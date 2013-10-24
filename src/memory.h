#ifndef FEATURES_MEMORY_H
#define FEATURES_MEMORY_H

#define FEATURES_MAGIC_13_10 "FEAT1310"

#include <stdint.h>

#include "config.h"

enum {
    FEATURES_PAGE_SIZE = 4096,
    FEATURES_BLOCK_SIZE = 64,
    FEATURES_BLOCKS_PER_PAGE = 64,
    FEATURES_MAX_SWITCHES_PER_BLOCK = 256,
    FEATURES_MAX_PAGE = UINT32_MAX,
    FEATURES_MAX_BLOCK = FEATURES_MAX_PAGE * FEATURES_BLOCKS_PER_PAGE,
    FEATURES_MAX_SWITCH = FEATURES_MAX_BLOCK * FEATURES_MAX_SWITCHES_PER_BLOCK,
    FEATURES_FLAGS_PER_BLOCK = 168,
    FEATURES_UINT8_PER_BLOCK = 51,
    FEATURES_UINT16_PER_BLOCK = 28,
    FEATURES_UINT32_PER_BLOCK = 15,
    FEATURES_UINT64_PER_BLOCK = 7,

    FEATURES_INT8_PER_BLOCK = FEATURES_UINT8_PER_BLOCK,
    FEATURES_INT16_PER_BLOCK = FEATURES_UINT16_PER_BLOCK,
    FEATURES_INT32_PER_BLOCK = FEATURES_UINT32_PER_BLOCK,
    FEATURES_INT64_PER_BLOCK = FEATURES_UINT64_PER_BLOCK

    FEATURES_FLAG_PROPERTIES_SIZE = (FEATURES_FLAGS_PER_BLOCK * 2 - 1) / 8 + 1,
    FEATURES_UINT8_PROPERTIES_SIZE = (FEATURES_UINT8_PER_BLOCK * 2 - 1) / 8 + 1,
    FEATURES_UINT16_PROPERTIES_SIZE = (FEATURES_UINT16_PER_BLOCK * 2 - 1) / 8 + 1,
    FEATURES_UINT32_PROPERTIES_SIZE = (FEATURES_UINT32_PER_BLOCK * 2 - 1) / 8 + 1,
    FEATURES_UINT64_PROPERTIES_SIZE = (FEATURES_UINT64_PER_BLOCK * 2 - 1) / 8 + 1,

    FEATURES_INT8_PROPERTIES_SIZE = FEATURES_UINT8_PROPERTIES_SIZE,
    FEATURES_INT16_PROPERTIES_SIZE = FEATURES_UINT16_PROPERTIES_SIZE,
    FEATURES_INT32_PROPERTIES_SIZE = FEATURES_UINT32_PROPERTIES_SIZE,
    FEATURES_INT64_PROPERTIES_SIZE = FEATURES_UINT64_PROPERTIES_SIZE
};

typedef enum features_err_t {
    FEATURES_OK,
    FEATURES_ERR_UNINITIALISED,
    FEATURES_ERR_INVALID,
    FEATURES_ERR_UNUSED,
    FEATURES_ERR_DEPRECATED,
    FEATURES_ERR_INCORRECT_TYPE
} features_err_t;

typedef enum features_switch_type_t {
    FEATURES_SWITCH_TYPE_UNUSED = 0x0,
    FEATURES_SWITCH_TYPE_DEPRECATED = 0x1,
    FEATURES_SWITCH_TYPE_FLAG = 0x2,
    FEATURES_SWITCH_TYPE_UINT8 = 0x3,
    FEATURES_SWITCH_TYPE_UINT16 = 0x4,
    FEATURES_SWITCH_TYPE_UINT32 = 0x5,
    FEATURES_SWITCH_TYPE_UINT64 = 0x6,
    FEATURES_SWITCH_TYPE_INT8 = 0x7,
    FEATURES_SWITCH_TYPE_INT16 = 0x8,
    FEATURES_SWITCH_TYPE_INT32 = 0x9,
    FEATURES_SWITCH_TYPE_INT64 = 0xa
    FEATURES_SWITCH_TYPE_INVALID = 0xf
} features_switch_type_t;

typedef uint64_t features_block_number_t;
typedef uint32_t features_block_offset_t;
typedef uint64_t features_switch_number_t;

typedef struct features_switch_id_t {
    uint32_t page_number;
    uint8_t block_number;
    uint8_t switch_number;
} features_switch_id_t;

typedef struct features_block_t {
    features_switch_type_t type;
    uint8_t *switch_properties;

    union {
        uint8_t *p8;
        uint16_t *p16;
        uint32_t *p32;
        uint64_t *p64;
    } data;
} features_block_t;

typedef struct features_block_raw_t {
    uint8_t data[FEATURES_BLOCK_SIZE];
} features_block_raw_t;

typedef struct features_page_header_block_info_t {
    uint8_t data[32]; // Each element contains the type info of 2 blocks
} features_page_header_block_info_t;

// Page header is a 64 byte value
typedef struct features_page_header_t {
    char *MAGIC[8];
    uint32_t page_number; // stored in big_endian
    uint32_t page_count; // stored in big_endian
    uint8_t unused[16];
    features_page_header_block_info_t block_info;
} features_page_header_t;

typedef struct features_page_raw_t {
    features_page_header_t header;
    features_block_raw_t blocks[FEATURES_BLOCKS_PER_PAGE-1];
} features_page_raw_t;

typedef struct features_page_t {
    uint32_t page_number;
    features_block_raw_t *blocks;
} features_page_t;

typedef struct features_data_t {
    uint64_t page_count;
    uint64_t page_offset;
    features_page_raw_t *pages;
} features_data_t;

features_switch_id_t
features_switch_id(features_switch_number_t switch_number);

features_err_t
features_data(
        features_data_t *data,
        void *raw);

features_err_t
features_page(
        features_page_t *page,
        features_page_raw_t *raw);

features_err_t
features_block(
        features_block_t *block,
        features_page_t *page,
        uint8_t block_number);

uint8_t
features_switch_uint8_value(
        const features_data_t *data,
        features_switch_number_t switch_number);

uint16_t
features_switch_uint16_value(
        const features_data_t *data,
        features_switch_number_t switch_number);

uint32_t
features_switch_uint32_value(
        const features_data_t *data,
        features_switch_number_t switch_number);

uint64_t
features_switch_uint64_value(
        const features_data_t *data,
        features_switch_number_t switch_number);

#endif
