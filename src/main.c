#include <linux/module.h>
#include <linux/usb.h>
#include <linux/slab.h> // for kmalloc, kfree

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gregory Mironov");
MODULE_VERSION("1.0.0");

// Wrapper for usb_device_id with added list_head field to track devices.
typedef struct int_udev
{
    struct usb_device_id dev_id;
    struct list_head list_node;
} int_udev_t;

static bool is_network_down = false;

static struct usb_device_id allowed_devs[] = {
    {USB_DEVICE(0x0781, 0x5571)},
};

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

    int_udev_t *temp;
    list_for_each_entry(temp, &connected_devices, list_node)
        if (!is_dev_allowed(&temp->dev_id))
            count++;

    return count;
}

//  Add connected device to list of tracked devices.
static void
add_int_usb_dev(const struct usb_device * const dev)
{
    int_udev_t *new_usb_device = (int_udev_t *)kmalloc(sizeof(int_udev_t), GFP_KERNEL);
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
    int_udev_t *device, *temp;
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

    char *argv[] = {"/sbin/modprobe", "-r", "iwlwifi", NULL};
    char *envp[] = {"HOME=/", "TERM=linux", "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL};
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

// Handler for USB removal.
static void
usb_dev_remove(const struct usb_device * const dev)
{
    pr_info("netpmod: device disconnected with PID '%d' and VID '%d'\n",
           dev->descriptor.idProduct, dev->descriptor.idVendor);
    delete_int_usb_dev(dev);

    int not_acked_devs = count_not_acked_devs();
    if (not_acked_devs)
    {
        pr_info("netpmod: %d not allowed devices connected, nothing to do\n", not_acked_devs);
        return;
    }

    if (!is_network_down)
        return;

    pr_info("netpmod: all not allowed devices are disconnected, bringing network back\n");
    char *argv[] = {"/sbin/modprobe", "iwlwifi", NULL};
    char *envp[] = {"HOME=/", "TERM=linux", "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL};
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

// Handler for event's notifier.
static int
notify(struct notifier_block *self, unsigned long action, void *dev)
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

    return 0;
}

// React on different notifies.
static struct notifier_block usb_notify = {
    .notifier_call = notify,
};

// Module init function.
static int
__init netpmod_init(void)
{
    usb_register_notify(&usb_notify);
    pr_info("netpmod: module loaded\n");
    return 0;
}

// Module exit function.
static void
__exit netpmod_exit(void)
{
    usb_unregister_notify(&usb_notify);
    pr_info("netpmod: module unloaded\n");
}

module_init(netpmod_init);
module_exit(netpmod_exit);
