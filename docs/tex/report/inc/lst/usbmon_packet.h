struct usbmon_packet {
    u64 id;                 //  0: URB ID - from submission to callback
    unsigned char type;     //  8: Same as text; extensible.
    unsigned char xfer_type; //    ISO (0), Intr, Control, Bulk (3)
    unsigned char epnum;    //     Endpoint number and transfer direction
    unsigned char devnum;   //     Device address
    u16 busnum;             // 12: Bus number
    char flag_setup;        // 14: Same as text
    char flag_data;         // 15: Same as text; Binary zero is OK.
    s64 ts_sec;             // 16: gettimeofday
    s32 ts_usec;            // 24: gettimeofday
    int status;             // 28:
    unsigned int length;    // 32: Length of data (submitted or actual)
    unsigned int len_cap;   // 36: Delivered length
    union {                 // 40:
        unsigned char setup[SETUP_LEN]; // Only for Control S-type
        struct iso_rec {                // Only for ISO
            int error_count;
            int numdesc;
        } iso;
    } s;
    int interval;           // 48: Only for Interrupt and ISO
    int start_frame;        // 52: For ISO
    unsigned int xfer_flags; // 56: copy of URB's transfer_flags
    unsigned int ndesc;     // 60: Actual number of ISO descriptors
};                          // 64 total length
