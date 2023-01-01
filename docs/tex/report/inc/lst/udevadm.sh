/* rules file */
SUBSYSTEM=="usb", ACTION=="add", ENV{DEVTYPE}=="usb_device",  RUN+="/bin/device_added.sh"
SUBSYSTEM=="usb", ACTION=="remove", ENV{DEVTYPE}=="usb_device", RUN+="/bin/device_removed.sh"

/* device_added.sh */
#!/bin/bash
echo "USB device added at $(date)" >>/tmp/scripts.log

/* device_removed.sh */
#!/bin/bash
echo "USB device removed  at $(date)" >>/tmp/scripts.log
