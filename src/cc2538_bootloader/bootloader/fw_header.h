#ifndef FW_HEADER_H_
#define FW_HEADER_H_

#define FW_HEADER_BYTES  0x800       // 2K on flash (one page), actual header material is significantly less
#define STANDARD_ID      0xdeadbeef

typedef union
{
    struct
    {
        uint32_t id;
    } f;

    uint8_t reserved[FW_HEADER_BYTES];
} fw_header_t;


#endif // FW_HEADER_H_
