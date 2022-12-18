struct usb_device_descriptor {
    __le16 idVendor;
    __le16 idProduct;
    ...
} __attribute__ ((packed));
