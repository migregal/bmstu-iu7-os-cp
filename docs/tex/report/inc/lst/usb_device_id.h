struct usb_device_id {
    /* which fields to match against? */
    __u16           match_flags;

    /* Used for product specific matches; range is inclusive */
    __u16           idVendor;
    __u16           idProduct;
    __u16           bcdDevice_lo;
    __u16           bcdDevice_hi;

    /* Used for device class matches */
    __u8            bDeviceClass;
    __u8            bDeviceSubClass;
    __u8            bDeviceProtocol;

    /* Used for interface class matches */
    __u8            bInterfaceClass;
    __u8            bInterfaceSubClass;
    __u8            bInterfaceProtocol;

    /* Used for vendor-specific interface matches */
    __u8            bInterfaceNumber;

    /* not matched against */
    kernel_ulong_t  driver_info
            __attribute__((aligned(sizeof(kernel_ulong_t))));
};
