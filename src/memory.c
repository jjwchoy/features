#include "memory.h"

#include <assert.h>

typedef struct features_switch_info_t {
    feature_switch_type_t type;
    void *data;
} features_switch_info_t;

#ifndef WORDS_BIGENDIAN
static uint16_t
features_swap_endian_16(uint16_t val);

static uint32_t
features_swap_endian_32(uint32_t val);

static uint64_t
features_swap_endian_64(uint64_t val);
#endif

static uint16_t
features_read_uint16(const void *data);

static void
features_write_uint16(void *data, uint16_t value);

static uint32_t
features_read_uint32(const void *data);

static void
features_write_uint32(void *data, uint32_t value);

static uint64_t
features_read_uint64(const void *data);

static void
features_write_uint64(void *data, uint64_t value);

static int16_t
features_read_int16(const void *data);

static void
features_write_int16(void *data, int16_t value);

static int32_t
features_read_int32(const void *data);

static void
features_write_int32(void *data, int32_t value);

static int64_t
features_read_int64(const void *data);

static void
features_write_int64(void *data, int64_t value);

static features_err_t
features_switch_info(
        features_switch_info_t *switch_info,
        features_data_t *data,
        features_switch_number_t switch_number);

features_switch_id_t
features_switch_id(features_switch_number_t switch_number) {
    features_switch_id_t switch_id;
    assert(switch_number <= FEATURES_MAX_SWITCH);

    switch_id.switch_number = switch_number % FEATURES_MAX_SWITCHES_PER_BLOCK;
    switch_number /= FEATURES_MAX_SWITCHES_PER_BLOCK;

    switch_id.block_number = switch_number % FEATURES_BLOCKS_PER_PAGE;
    switch_number /= FEATURES_BLOCKS_PER_PAGE;

    switch_id.page_number = switch_number;

    return switch_id;
}

features_err_t
features_data(
        features_data_t *data,
        void *raw) {
    features_err_t page_rc;
    features_page_t first_page;
    features_page_raw_t *page_raw;

    page_raw = raw;

    // Parse the first page to make sure it is valid
    page_rc = features_page(&first_page, page_raw);

    if (FEATURES_OK != page_rc) {
        return page_rc;
    }

    // The first page in the file contains the total page count
    data->page_count = features_read_uint64(&page_raw->header.page_count);
    data->page_offset = first_page.page_number;
    data->pages = page_raw;

    return FEATURES_OK;
}

features_err_t
features_page(
        features_page_t *page,
        features_page_raw_t *raw) {
    int c;
    // Check that the magic number is correct
    c = memcmp(FEATURES_MAGIC_13_10, raw->header.MAGIC, sizeof(raw->header.MAGIC));

    if (0 != c) {
        return FEATURES_ERR_INVALID;
    }
    
    page->page_number = features_read_uint32(&raw->header.page_count);
    page->blocks = raw->blocks;
    return FEATURES_OK;
}

features_err_t
features_block(
        features_block_t *block,
        features_page_t *page,
        uint8_t block_number) {
    uint8_t type_byte;
    features_page_header_t *page_header;

    assert(block_number < FEATURES_BLOCKS_PER_PAGE);

    memset(block, 0, sizeof(features_block_t));

    if (0 == page_block_number) {
        // The first block on every page is reserved for the page header
        // it is not a valid block number
        block->type = FEATURES_SWITCH_TYPE_INVALID;
        return FEATURES_OK;
    }

    page_header = (features_page_header_t*)page->blocks;
    type_byte = page_header->block_info.data[page_block_number / 2];
    
    // type byte has two block types packed into it
    // even block numbers have the least significant nybble
    // odd block number have the most significant nybble

    if (page_block_number % 2) {
        type_byte >>= 4;
    }

    block->type = (feature_switch_type_t)(type_byte & 0xf);
    // The switch properties is at the start of the data block
    block->properties = page->blocks + page_block_number;

    switch (block->type) {
        case FEATURES_SWITCH_TYPE_UNUSED:
        case FEATURES_SWITCH_TYPE_DEPRECATED:
        case FEATURES_SWITCH_TYPE_INVALID:
            // Nothing more to do
            break;
        case FEATURES_SWITCH_TYPE_FLAG:
            block->data.p8 = block->properties + FEATURES_FLAG_PROPERTIES_SIZE;
            break;
        case FEATURES_SWITCH_TYPE_UINT8:
        case FEATURES_SWITCH_TYPE_INT8:
            block->data.p8 = block->properties + FEATURES_UINT8_PROPERTIES_SIZE;
            break;
        case FEATURES_SWITCH_TYPE_UINT16:
        case FEATURES_SWITCH_TYPE_INT16:
            block->data.p16 = block->properties + FEATURES_UINT16_PROPERTIES_SIZE;
            break;
        case FEATURES_SWITCH_TYPE_UINT32:
        case FEATURES_SWITCH_TYPE_INT32:
            block->data.p32 = block->properties + FEATURES_UINT32_PROPERTIES_SIZE;
            break;
        case FEATURES_SWITCH_TYPE_UINT64:
        case FEATURES_SWITCH_TYPE_INT64:
            block->data.p64 = block->properties + FEATURES_UINT64_PROPERTIES_SIZE;
            break;
        default:
            return FEATURES_ERR_INVALID;
    }

    return FEATURES_OK;
}

features_err_t
features_switch_value(
        const features_data_t *data,
        features_switch_number_t switch_number,
        features_switch_value_t *value) {
    features_switch_info_t switch_info;
    features_err_t rc;
    uint8_t switch_properties;

    rc = features_switch_info(&switch_info, data, switch_number);

    if (FEATURES_OK != rc) {
        return rc;
    }

    value->type = switch_info.type;

    switch (switch_info.type) {
        case FEATURES_SWITCH_TYPE_UNUSED:
            return FEATURES_ERR_UNUSED;
        case FEATURES_SWITCH_TYPE_DEPRECATED:
            return FEATURES_ERR_DEPRECATED;
        case FEATURES_SWITCH_TYPE_FLAG:
            memcpy(&value->data.flag, switch_info.data, 1);
            value->data.flag &= (1 << (switch_number % 8));
            break;
        case FEATURES_SWITCH_TYPE_UINT8:
            memcpy(&value->data.uint8, switch_info.data, 1);
            break;
        case FEATURES_SWITCH_TYPE_UINT16:
            value->data.uint16 = features_read_uint16(switch_info->data);
            break;
        case FEATURES_SWITCH_TYPE_UINT32:
            value->data.uint32 = features_read_uint32(switch_info->data);
            break;
        case FEATURES_SWITCH_TYPE_UINT64:
            value->data.uint64 = features_read_uint64(switch_info->data);
            break;
        case FEATURES_SWITCH_TYPE_INT8:
            memcpy(&value->data.int8, switch_info.data, 1);
            break;
        case FEATURES_SWITCH_TYPE_INT16:
            value->data.int16 = features_read_int16(switch_info->data);
            break;
        case FEATURES_SWITCH_TYPE_INT32:
            value->data.int32 = features_read_int32(switch_info->data);
            break;
        case FEATURES_SWITCH_TYPE_INT64:
            value->data.int64 = features_read_int64(switch_info->data);
            break;
        case FEATURES_SWITCH_TYPE_INVALID:
        default:
            return FEATURES_ERR_INVALID;
    }

    switch_properties = value->switch_properties[switch_number / 4];
    switch_properties >>= switch_number % 4;

    if (!(switch_properties & 0x1)) {
        return FEATURES_ERR_UNUSED;
    }

    if (switch_properties & 0x2) {
        return FEATURES_ERR_DEPRECATED;
    }

    return FEATURES_OK;
}

#define FEATURE_RETURN_VALUE(expected_type, member)\
    features_switch_value_t value;\
    features_err_t rc;\
    rc = features_switch_value(data,switch_number,value);\
    if (FEATURES_OK == rc){\
        if (expected_type != value.type) {\
            rc = FEATURES_ERR_INCORRECT_TYPE;\
        } else {\
            *val = value.value.##member;\
        }\
    }\
    return rc

features_err_t
features_switch_flag_value(
        const features_data_t *data,
        features_switch_number_t switch_number,
        char *val) {
    FEATURE_RETURN_VALUE(FEATURES_SWITCH_TYPE_FLAG, flag);
}

features_err_t
features_switch_uint8_value(
        const features_data_t *data,
        features_switch_number_t switch_number,
        uint8_t *val) {
    FEATURE_RETURN_VALUE(FEATURES_SWITCH_TYPE_UINT8, uint8);
}

features_err_t
features_switch_uint16_value(
        const features_data_t *data,
        features_switch_number_t switch_number,
        uint16_t *val) {
    FEATURE_RETURN_VALUE(FEATURES_SWITCH_TYPE_UINT16, uint16);
}

features_err_t
features_switch_uint32_value(
        const features_data_t *data,
        features_switch_number_t switch_number,
        uint32_t *val) {
    FEATURE_RETURN_VALUE(FEATURES_SWITCH_TYPE_UINT32, uint32);
}

features_err_t
features_switch_uint64_value(
        const features_data_t *data,
        features_switch_number_t switch_number,
        uint64_t *val) {
    FEATURE_RETURN_VALUE(FEATURES_SWITCH_TYPE_UINT64, uint64);
}

features_err_t
features_switch_int8_value(
        const features_data_t *data,
        features_switch_number_t switch_number,
        int8_t *val) {
    FEATURE_RETURN_VALUE(FEATURES_SWITCH_TYPE_INT8, int8);
}

features_err_t
features_switch_int16_value(
        const features_data_t *data,
        features_switch_number_t switch_number,
        int16_t *val) {
    FEATURE_RETURN_VALUE(FEATURES_SWITCH_TYPE_INT16, int16);
}

features_err_t
features_switch_int32_value(
        const features_data_t *data,
        features_switch_number_t switch_number,
        int32_t *val) {
    FEATURE_RETURN_VALUE(FEATURES_SWITCH_TYPE_INT32, int32);
}

features_err_t
features_switch_int64_value(
        const features_data_t *data,
        features_switch_number_t switch_number,
        int64_t *val) {
    FEATURE_RETURN_VALUE(FEATURES_SWITCH_TYPE_INT64, int64);
}

static uint16_t
features_swap_endian_16(uint16_t val) {
    uint16_t out_val;
    uint8_t *in = (uint8_t *)&val;
    uint8_t *out = (uint8_t *)&out_val;

    out[0] = in[1];
    out[1] = in[0];

    return out_val;
}

static uint32_t
features_swap_endian_32(uint32_t val) {
    uint32_t out_val;
    uint8_t *in = (uint8_t *)&val;
    uint8_t *out = (uint8_t *)&out_val;

    out[0] = in[3];
    out[1] = in[2];
    out[2] = in[1];
    out[3] = in[0];

    return out_val;
}

static uint64_t
features_swap_endian_64(uint64_t val) {
    uint64_t out_val;
    uint8_t *in = (uint8_t *)&val;
    uint8_t *out = (uint8_t *)&out_val;

    out[0] = in[7];
    out[1] = in[6];
    out[2] = in[5];
    out[3] = in[4];
    out[4] = in[3];
    out[5] = in[2];
    out[6] = in[1];
    out[7] = in[0];

    return out_val;
}

static uint16_t
features_read_uint16(const void *data) {
    uint16_t val = *(uint16_t *)data;
#ifndef WORDS_BIGENDIAN
    val = features_swap_endian_16(val);
#endif
    return val;
}

static void
features_write_uint16(void *data, uint16_t value) {
    uint16_t val = value;
#ifndef WORDS_BIGENDIAN
    val = features_swap_endian_16(val);
#endif
    memcpy(data, &val, sizeof(uint16_t);
}

static uint32_t
features_read_uint32(const void *data) {
    uint32_t val = *(uint32_t *)data;
#ifndef WORDS_BIGENDIAN
    val = features_swap_endian_32(val);
#endif
    return val;
}

static void
features_write_uint32(void *data, uint32_t value) {
    uint32_t val = value;
#ifndef WORDS_BIGENDIAN
    val = features_swap_endian_32(val);
#endif
    memcpy(data, &val, sizeof(uint32_t));
}

static uint64_t
features_read_uint64(const void *data) {
    uint64_t val = *(uint64_t *)data;
#ifndef WORDS_BIGENDIAN
    val = features_swap_endian_64(val);
#endif
    return val;
}

static void
features_write_uint64(void *data, uint64_t value) {
    uint64_t val = value;
#ifndef WORDS_BIGENDIAN
    val = features_swap_endian_64(val);
#endif
    memcpy(data, &val, sizeof(uint64_t));
}

static int16_t
features_read_int16(const void *data) {
    return *(int16_t *)features_read_uint16(data);
}

static void
features_write_int16(void *data, int16_t value) {
    features_write_uint16(data, *(uint16_t *)&value);
}

static int32_t
features_read_int32(const void *data) {
    return *(int32_t *)features_read_uint32(data);
}

static void
features_write_int32(void *data, int32_t value) {
    return *(int32_t *)features_read_uint32(data);
}

static int64_t
features_read_int64(const void *data) {
    return *(int64_t *)features_read_uint64(data);
}

static void
features_write_int64(void *data, int64_t value) {
    return *(int64_t *)features_read_uint64(data);
}

static features_err_t
features_switch_info(
        features_switch_info_t *switch_info,
        features_data_t *data,
        features_switch_number_t switch_number) {
    features_switch_id_t switch_id;

    memset(switch_info, 0, sizeof(features_switch_info_t));

    switch_id = features_switch_id(switch_number);

    if (switch_id.page_number < data->page_offset) {
        switch_info->type = FEATURES_SWITCH_TYPE_DEPRECATED;
    } else if (switch_id.page_number > data->page_count) {
        switch_info->type = FEATURES_SWITCH_TYPE_UNUSED;
    } else {
        features_page_raw_t *page_raw;
        features_page_t page;
        features_block_t block;
        features_err_t rc;

        page_raw = data->pages + (switch_id.page_number - data->page_offset);
        
        rc = features_page(&page, page_raw);

        if (FEATURES_OK != rc) {
            return rc;
        }

        rc = features_block(&block, page, switch_id.block_number);

        if (FEATURES_OK != rc) {
            return rc;
        }

        switch (block->type) {
            case FEATURES_SWITCH_TYPE_UNUSED:
            case FEATURES_SWITCH_TYPE_DEPRECATED:
            case FEATURES_SWITCH_TYPE_INVALID:
                switch_info->type = block->type;
                break;
            case FEATURES_SWITCH_TYPE_FLAG:
                if (switch_info.switch_number > FEATURES_FLAGS_PER_BLOCK) {
                    switch_info->type = FEATURES_SWITCH_TYPE_INVALID;
                } else {
                    uint8_t switch_offset = switch_info.switch_number / 8;
                    switch_info->data = block->data.p8 + switch_offset;
                }
                break;
            case FEATURES_SWITCH_TYPE_UINT8:
            case FEATURES_SWITCH_TYPE_INT8:
                if (switch_info.switch_number > FEATURES_UINT8_PER_BLOCK) {
                    switch_info->type = FEATURES_SWITCH_TYPE_INVALID;
                } else {
                    switch_info->data = block->data.p8 + switch_info.switch_number;
                }
                break;
            case FEATURES_SWITCH_TYPE_UINT16:
            case FEATURES_SWITCH_TYPE_INT16:
                if (switch_info.switch_number > FEATURES_UINT16_PER_BLOCK) {
                    switch_info->type = FEATURES_SWITCH_TYPE_INVALID;
                } else {
                    switch_info->data = block->data.p16 + switch_info.switch_number;
                }
                break;
            case FEATURES_SWITCH_TYPE_UINT32:
            case FEATURES_SWITCH_TYPE_INT32:
                if (switch_info.switch_number > FEATURES_UINT32_PER_BLOCK) {
                    switch_info->type = FEATURES_SWITCH_TYPE_INVALID;
                } else {
                    switch_info->data = block->data.p32 + switch_info.switch_number;
                }
                break;
            case FEATURES_SWITCH_TYPE_UINT64:
            case FEATURES_SWITCH_TYPE_INT64:
                if (switch_info.switch_number > FEATURES_UINT64_PER_BLOCK) {
                    switch_info->type = FEATURES_SWITCH_TYPE_INVALID;
                } else {
                    switch_info->data = block->data.p64 + switch_info.switch_number;
                }
                break;
            default:
                switch_info->type = FEATURE_SWITCH_TYPE_INVALID;
        }
    }

    return FEATURES_OK;
}
