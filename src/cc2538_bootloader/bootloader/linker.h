#ifndef LINKER_H_
#define LINKER_H_

extern char __BOOTLOADER_BASE;

extern char __MAIN_HEADER_BASE;
extern char __MAIN_HEADER_BYTES;

extern char __MAIN_BASE;
extern char __MAIN_BYTES;

extern char __DOWNLOAD_HEADER_BASE;
extern char __DOWNLOAD_BASE;
extern char __STORAGE_BASE;


#define BOOTLOADER_BASE        ((uint32_t)&__BOOTLOADER_BASE)

#define MAIN_HEADER_BASE       ((uint32_t)&__MAIN_HEADER_BASE)
#define MAIN_HEADER_BYTES      ((uint32_t)&__MAIN_HEADER_BYTES)

#define MAIN_BASE              ((uint32_t)&__MAIN_BASE)
#define MAIN_BYTES             ((uint32_t)&__MAIN_BYTES)

#define DOWNLOAD_HEADER_BASE   ((uint32_t)&__DOWNLOAD_HEADER_BASE)
#define DOWNLOAD_BASE          ((uint32_t)&__DOWNLOAD_BASE)
#define STORAGE_BASE           ((uint32_t)&__STORAGE_BASE)

#endif // LINKER_H_
