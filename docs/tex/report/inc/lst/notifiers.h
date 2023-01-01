struct notifier_block {
    notifier_fn_t notifier_call;
    struct notifier_block __rcu *next;
    int priority;
};

typedef	int (*notifier_fn_t)(struct notifier_block *nb, unsigned long action, void *data);

/* Events from the usb core */
#define USB_DEVICE_ADD      0x0001
#define USB_DEVICE_REMOVE   0x0002
#define USB_BUS_ADD         0x0003
#define USB_BUS_REMOVE      0x0004

extern void usb_register_notify(struct notifier_block *nb);
extern void usb_unregister_notify(struct notifier_block *nb);
