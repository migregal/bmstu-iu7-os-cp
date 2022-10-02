#include <linux/module.h>
#include <linux/usb.h>
#include <linux/keyboard.h>
#include <linux/slab.h> // for kmalloc, kfree

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gregory Mironov");
MODULE_VERSION("1.0.0");

// params ----------------------------------------------------------------------

static char *net_modules[16] = {"iwlwifi"};
static int net_modules_n = 1;
module_param_array(net_modules, charp, &net_modules_n, S_IRUGO);
MODULE_PARM_DESC(net_modules, "List of modules to manipulate with");

static char *password = {"qwery"};
module_param(password, charp, 0000);
MODULE_PARM_DESC(password, "Password to reenable network manually");

// params ^---------------------------------------------------------------------

// network ---------------------------------------------------------------------

static bool is_network_down = false;

static char *envp[] = {"HOME=/", "TERM=linux", "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL};

static void
disable_network(void)
{
    int i = 0;
    for (; i < net_modules_n; i++)
    {
        char *argv[] = {"/sbin/modprobe", "-r", net_modules[i], NULL};
        if (call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC > 0))
        {
            pr_warn("netpmod: unable to kill network\n");
        }
        else
        {
            pr_info("netpmod: network is killed\n");
            is_network_down = true;
        }
    }
}

static void
enable_network(void)
{
    int i = 0;
    for (; i < net_modules_n; i++)
    {
        char *argv[] = {"/sbin/modprobe", net_modules[i], NULL};
        if (call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC > 0))
        {
            pr_warn("netpmod: unable to bring network back\n");
        }
        else
        {
            pr_info("netpmod: network is available now\n");
            is_network_down = false;
        }
    }
}

// usb handler -----------------------------------------------------------------

static struct usb_device_id allowed_devs[] = {
    // {USB_DEVICE(0x0781, 0x5571)},
};

// Wrapper for usb_device_id with added list_head field to track devices.
typedef struct int_usb_device
{
    struct usb_device_id dev_id;
    struct list_head list_node;
} int_usb_device_t;


// Declare and init the head node of the linked list.
LIST_HEAD(connected_devices);

// Match device with device id.
static bool
is_dev_matched(const struct usb_device * const dev, const struct usb_device_id *const dev_id)
{
    // Check idVendor and idProduct, which are used.
    return dev_id->idVendor == dev->descriptor.idVendor
            && dev_id->idProduct == dev->descriptor.idProduct;
}

// Match device id with device id.
static bool
is_dev_id_matched(const struct usb_device_id * const new_dev_id, const struct usb_device_id * const dev_id)
{
    // Check idVendor and idProduct, which are used.
    return dev_id->idVendor == new_dev_id->idVendor
            && dev_id->idProduct == new_dev_id->idProduct;
}

// Check if device is in allowed devices list.
static bool
is_dev_allowed(const struct usb_device_id * const dev)
{
    unsigned long allowed_devs_len = sizeof(allowed_devs) / sizeof(struct usb_device_id);

    int i = 0;
    for (; i < allowed_devs_len; i++)
        if (is_dev_id_matched(dev, &allowed_devs[i]))
            return true;

    return false;
}

// Check if changed device is acknowledged.
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

//  Add connected device to list of tracked devices.
static void
add_int_usb_dev(const struct usb_device * const dev)
{
    int_usb_device_t *new_usb_device = (int_usb_device_t *)kmalloc(sizeof(int_usb_device_t), GFP_KERNEL);
    struct usb_device_id new_id = {
        USB_DEVICE(dev->descriptor.idVendor, dev->descriptor.idProduct),
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
    pr_info("netpmod: device connected with PID '%d' and VID '%d'\n",
           dev->descriptor.idProduct, dev->descriptor.idVendor);
    add_int_usb_dev(dev);

    int not_acked_devs = count_not_acked_devs();
    if (!not_acked_devs)
    {
        pr_info("netpmod: allowed device connected, skipping network killing\n");
        return;
    }

    pr_info("netpmod: %d not allowed devices connected, killing network\n", not_acked_devs);
    if (is_network_down)
        return;

    disable_network();
}

// Handler for USB removal.
static void
usb_dev_remove(const struct usb_device * const dev)
{
    pr_info("netpmod: device disconnected with PID '%d' and VID '%d'\n",
           dev->descriptor.idProduct, dev->descriptor.idVendor);
    delete_int_usb_dev(dev);

    if (!is_network_down)
        return;

    int not_acked_devs = count_not_acked_devs();
    if (not_acked_devs)
    {
        pr_info("netpmod: %d not allowed devices connected, nothing to do\n", not_acked_devs);
        return;
    }

    pr_info("netpmod: all not allowed devices are disconnected, bringing network back\n");

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

// React on different notifies.
static struct notifier_block usb_notify = {
    .notifier_call = usb_notifier_call,
};

// usb handler ^----------------------------------------------------------------

// keyboard handler ------------------------------------------------------------

static size_t matched_password_len = 0;
static size_t password_len = 0;

int kbd_notifier_call(struct notifier_block *nblock, unsigned long code, void *_param)
{
    if (!is_network_down)
        return NOTIFY_OK;

    struct keyboard_notifier_param *param = _param;
    if (code != KBD_KEYSYM || !param->down)
        return NOTIFY_OK;

    char c = param->value;
    // printable ASCII range is between ' ' (0x20) and '~' (0x7e)
    if (c < ' ' && c > 0x7e)
        return NOTIFY_OK;

    if (!password_len)
        password_len = strlen(password);

    if (!password_len)
        return NOTIFY_OK;

    if (c == password[matched_password_len])
        matched_password_len++;

    if (matched_password_len == password_len)
    {
        pr_info("netpmod: password matched, bringing network back\n");

        matched_password_len = 0;
        enable_network();
    }

    return NOTIFY_OK;
}

static struct notifier_block kbd_notify = {
    .notifier_call = kbd_notifier_call,
};

// keyboard handler ^-----------------------------------------------------------

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
