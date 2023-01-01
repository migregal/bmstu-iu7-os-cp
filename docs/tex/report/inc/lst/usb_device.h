struct usb_device {
    ...
    struct usb_device_descriptor descriptor;
    ...
    /* static strings from the device */
    char *product;
    char *manufacturer;
    char *serial;
    ...
};
