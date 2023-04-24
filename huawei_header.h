#ifndef HUAWEI_HEADER_H
#define HUAWEI_HEADER_H

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

enum HW_OFF {
    CRC32_HDR = 0x14,
    CRC32_ALL = 0x0C,
    SZ_BIN    = 0x4C
};

struct huawei_header {
    uint32_t magic_huawei;

    uint32_t raw_sz;
    uint32_t raw_crc32;

    uint32_t hdr_sz;
    uint32_t hdr_crc32;

    uint32_t item_counts;

    uint8_t _unknow_data_1;
    uint8_t _unknow_data_2;

    uint16_t prod_list_sz;

    uint32_t item_sz;

    uint32_t reserved;
};

struct huawei_item {
    uint32_t iter;
    uint32_t item_crc32;
    uint32_t data_off;
    uint32_t data_sz;

    char item[256];
    char section[16];
    char version[64];

    uint32_t policy;

    uint32_t reserved;
};

#endif // HUAWEI_HEADER_H
