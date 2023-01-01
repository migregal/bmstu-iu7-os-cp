struct usb_device_id {
    ...
    /* Used for product specific matches; range is inclusive */
    __u16           idVendor;
    __u16           idProduct;
    __u16           bcdDevice_lo;
    __u16           bcdDevice_hi;
    ...
};
