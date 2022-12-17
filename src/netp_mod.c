#include <linux/module.h>
#include <linux/usb.h>
#include <linux/keyboard.h>
#include <linux/slab.h> // for kmalloc, kfree
#include <linux/string.h>

#include "netp.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gregory Mironov");
MODULE_VERSION("1.0.0");

// params ----------------------------------------------------------------------

static char *password = {"qwery"};
module_param(password, charp, 0000);
MODULE_PARM_DESC(password, "Password to reenable network manually");

// params ^---------------------------------------------------------------------

// init ------------------------------------------------------------------------

static int
usb_notifier_call(struct notifier_block *self, unsigned long action, void *dev);

static struct notifier_block usb_notify = {
  .notifier_call = usb_notifier_call,
};

static int
kbd_notifier_call(struct notifier_block *self, unsigned long action, void *_param);

static struct notifier_block kbd_notify = {
  .notifier_call = kbd_notifier_call,
};

// Module init function.
static int
__init netpmod_init(void)
{
  usb_register_notify(&usb_notify);
  register_keyboard_notifier(&kbd_notify);

  pr_info("netpmod: module loaded\n");

  return 0;
}

// Module exit function.
static void
__exit netpmod_exit(void)
{
  unregister_keyboard_notifier(&kbd_notify);
  usb_unregister_notify(&usb_notify);

  pr_info("netpmod: module unloaded\n");
}

module_init(netpmod_init);
module_exit(netpmod_exit);

// init ^-----------------------------------------------------------------------

// usb handler -----------------------------------------------------------------

typedef struct int_usb_device_id {
  struct usb_device_id id;
  char                 *serial;
} int_usb_device_id_t;

#define INT_USB_DEVICE(v, p, s)\
  .id={USB_DEVICE(v, p)},\
  .serial=(s)

typedef struct int_usb_device
{
  int_usb_device_id_t dev_id;
  struct list_head    list_node;
} int_usb_device_t;

LIST_HEAD(connected_devices);

static int_usb_device_id_t allowed_devs[] = {
  {INT_USB_DEVICE(0x0781, 0x5571, "03021524050621080032")},
};

static bool
is_dev_matched(const struct usb_device * const dev, const int_usb_device_id_t *const dev_id)
{
  return dev_id->id.idVendor == dev->descriptor.idVendor
      && dev_id->id.idProduct == dev->descriptor.idProduct
      && !strcmp(dev_id->serial, dev->serial);
}

static bool
is_dev_id_matched(const int_usb_device_id_t * const new_dev_id, const int_usb_device_id_t * const dev_id)
{
  return dev_id->id.idVendor == new_dev_id->id.idVendor
      && dev_id->id.idProduct == new_dev_id->id.idProduct
      && !strcmp(dev_id->serial, new_dev_id->serial);
}

static bool
is_dev_allowed(const int_usb_device_id_t * const dev)
{
  unsigned long allowed_devs_len = sizeof(allowed_devs) / sizeof(int_usb_device_id_t);

  int i = 0;
  for (; i < allowed_devs_len; i++)
    if (is_dev_id_matched(dev, &allowed_devs[i]))
      return true;

  return false;
}

static int
count_not_acked_devs(void)
{
  int count = 0;

  int_usb_device_t *temp;
  list_for_each_entry(temp, &connected_devices, list_node)
    if (!is_dev_allowed(&temp->dev_id))
      count++;

  return count;
}

static void
add_int_usb_dev(const struct usb_device * const dev)
{
  int_usb_device_t *new_usb_device = (int_usb_device_t *)kmalloc(sizeof(int_usb_device_t), GFP_KERNEL);
  int_usb_device_id_t new_id = {
    INT_USB_DEVICE(dev->descriptor.idVendor, dev->descriptor.idProduct, dev->serial),
  };

  new_usb_device->dev_id = new_id;
  list_add_tail(&new_usb_device->list_node, &connected_devices);
}

//  Delete device from list of tracked devices.
static void
delete_int_usb_dev(const struct usb_device * const dev)
{
  int_usb_device_t *device, *temp;
  list_for_each_entry_safe(device, temp, &connected_devices, list_node)
    if (is_dev_matched(dev, &device->dev_id))
    {
      list_del(&device->list_node);
      kfree(device);
    }
}

// Handler for USB insertion.
static void
usb_dev_insert(const struct usb_device * const dev)
{
  pr_info("netpmod: dev connected with PID '%d' and VID '%d' and SERIAL '%s'\n",
       dev->descriptor.idProduct, dev->descriptor.idVendor, dev->serial);

  add_int_usb_dev(dev);

  int not_acked_devs = count_not_acked_devs();
  if (!not_acked_devs)
  {
    pr_info("netpmod: allowed dev connected, skipping network killing\n");
    return;
  }

  pr_info("netpmod: %d not allowed devs connected, killing network\n", not_acked_devs);

  if (is_network_disabled())
    return;

  disable_network();
}

// Handler for USB removal.
static void
usb_dev_remove(const struct usb_device * const dev)
{
  pr_info("netpmod: dev disconnected with PID '%d' and VID '%d' and SERIAL '%s'\n",
       dev->descriptor.idProduct, dev->descriptor.idVendor, dev->serial);
  delete_int_usb_dev(dev);

  if (!is_network_disabled())
    return;

  int not_acked_devs = count_not_acked_devs();
  if (not_acked_devs)
  {
    pr_info("netpmod: %d not allowed devs connected, nothing to do\n", not_acked_devs);
    return;
  }

  pr_info("netpmod: all not allowed devs are disconnected, bringing network back\n");

  enable_network();
}

// Handler for event's notifier.
static int
usb_notifier_call(struct notifier_block *self, unsigned long action, void *dev)
{
  // Events, which our notifier react.
  switch (action)
  {
  case USB_DEVICE_ADD:
    usb_dev_insert(dev);
    break;
  case USB_DEVICE_REMOVE:
    usb_dev_remove(dev);
    break;
  default:
    break;
  }

  return NOTIFY_OK;
}

// usb handler ^----------------------------------------------------------------

// keyboard handler ------------------------------------------------------------

static size_t matched_password_len = 0;
static size_t password_len = 0;

static int
kbd_notifier_verify_action(unsigned long action, void *_param)
{
  if (!is_network_disabled())
    return 0;

  struct keyboard_notifier_param *param = _param;
  if (action != KBD_KEYSYM || !param->down)
    return 0;

  return 1;
}

static int
kbd_notifier_verify_pwd_len(void)
{
  if (!password_len)
    password_len = strlen(password);

  if (!password_len)
    return 0;

  return 1;
}

static void
kbd_notifier_process_action(char symbol)
{
  if (symbol < ' ' || symbol > '~')
    return;

  if (symbol != password[matched_password_len])
  {
    matched_password_len = 0;
    return;
  }

  if (++matched_password_len == password_len)
  {
    pr_info("netpmod: password matched, bringing network back\n");

    matched_password_len = 0;
    enable_network();
  }
}

static int
kbd_notifier_call(struct notifier_block *self, unsigned long action, void *_param)
{
  if (!kbd_notifier_verify_action(action, _param))
    return NOTIFY_OK;

  if (!kbd_notifier_verify_pwd_len())
    return NOTIFY_OK;

  struct keyboard_notifier_param *param = _param;
  char symbol = param->value;

  kbd_notifier_process_action(symbol);

  return NOTIFY_OK;
}

// keyboard handler ^-----------------------------------------------------------
