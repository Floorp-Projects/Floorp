/*
  Copyright (c) INRIA and Microsoft Corporation. All rights reserved.
  Licensed under the Apache 2.0 License.
*/

#include <inttypes.h>
#include <stdbool.h>
#include "kremlin/internal/compat.h"
#include "kremlin/lowstar_endianness.h"
#include "kremlin/internal/types.h"
#include "kremlin/internal/target.h"

#ifndef __FStar_UInt_8_16_32_64_H
#define __FStar_UInt_8_16_32_64_H

extern Prims_int FStar_UInt64_n;

extern bool FStar_UInt64_uu___is_Mk(uint64_t projectee);

extern Prims_int FStar_UInt64___proj__Mk__item__v(uint64_t projectee);

extern Prims_int FStar_UInt64_v(uint64_t x);

extern uint64_t FStar_UInt64_uint_to_t(Prims_int x);

extern uint64_t FStar_UInt64_minus(uint64_t a);

extern uint32_t FStar_UInt64_n_minus_one;

static inline uint64_t
FStar_UInt64_eq_mask(uint64_t a, uint64_t b)
{
    uint64_t x = a ^ b;
    uint64_t minus_x = ~x + (uint64_t)1U;
    uint64_t x_or_minus_x = x | minus_x;
    uint64_t xnx = x_or_minus_x >> (uint32_t)63U;
    return xnx - (uint64_t)1U;
}

static inline uint64_t
FStar_UInt64_gte_mask(uint64_t a, uint64_t b)
{
    uint64_t x = a;
    uint64_t y = b;
    uint64_t x_xor_y = x ^ y;
    uint64_t x_sub_y = x - y;
    uint64_t x_sub_y_xor_y = x_sub_y ^ y;
    uint64_t q = x_xor_y | x_sub_y_xor_y;
    uint64_t x_xor_q = x ^ q;
    uint64_t x_xor_q_ = x_xor_q >> (uint32_t)63U;
    return x_xor_q_ - (uint64_t)1U;
}

extern Prims_string FStar_UInt64_to_string(uint64_t uu____888);

extern Prims_string FStar_UInt64_to_string_hex(uint64_t uu____899);

extern Prims_string FStar_UInt64_to_string_hex_pad(uint64_t uu____910);

extern uint64_t FStar_UInt64_of_string(Prims_string uu____921);

extern Prims_int FStar_UInt32_n;

extern bool FStar_UInt32_uu___is_Mk(uint32_t projectee);

extern Prims_int FStar_UInt32___proj__Mk__item__v(uint32_t projectee);

extern Prims_int FStar_UInt32_v(uint32_t x);

extern uint32_t FStar_UInt32_uint_to_t(Prims_int x);

extern uint32_t FStar_UInt32_minus(uint32_t a);

extern uint32_t FStar_UInt32_n_minus_one;

static inline uint32_t
FStar_UInt32_eq_mask(uint32_t a, uint32_t b)
{
    uint32_t x = a ^ b;
    uint32_t minus_x = ~x + (uint32_t)1U;
    uint32_t x_or_minus_x = x | minus_x;
    uint32_t xnx = x_or_minus_x >> (uint32_t)31U;
    return xnx - (uint32_t)1U;
}

static inline uint32_t
FStar_UInt32_gte_mask(uint32_t a, uint32_t b)
{
    uint32_t x = a;
    uint32_t y = b;
    uint32_t x_xor_y = x ^ y;
    uint32_t x_sub_y = x - y;
    uint32_t x_sub_y_xor_y = x_sub_y ^ y;
    uint32_t q = x_xor_y | x_sub_y_xor_y;
    uint32_t x_xor_q = x ^ q;
    uint32_t x_xor_q_ = x_xor_q >> (uint32_t)31U;
    return x_xor_q_ - (uint32_t)1U;
}

extern Prims_string FStar_UInt32_to_string(uint32_t uu____888);

extern Prims_string FStar_UInt32_to_string_hex(uint32_t uu____899);

extern Prims_string FStar_UInt32_to_string_hex_pad(uint32_t uu____910);

extern uint32_t FStar_UInt32_of_string(Prims_string uu____921);

extern Prims_int FStar_UInt16_n;

extern bool FStar_UInt16_uu___is_Mk(uint16_t projectee);

extern Prims_int FStar_UInt16___proj__Mk__item__v(uint16_t projectee);

extern Prims_int FStar_UInt16_v(uint16_t x);

extern uint16_t FStar_UInt16_uint_to_t(Prims_int x);

extern uint16_t FStar_UInt16_minus(uint16_t a);

extern uint32_t FStar_UInt16_n_minus_one;

static inline uint16_t
FStar_UInt16_eq_mask(uint16_t a, uint16_t b)
{
    uint16_t x = a ^ b;
    uint16_t minus_x = ~x + (uint16_t)1U;
    uint16_t x_or_minus_x = x | minus_x;
    uint16_t xnx = x_or_minus_x >> (uint32_t)15U;
    return xnx - (uint16_t)1U;
}

static inline uint16_t
FStar_UInt16_gte_mask(uint16_t a, uint16_t b)
{
    uint16_t x = a;
    uint16_t y = b;
    uint16_t x_xor_y = x ^ y;
    uint16_t x_sub_y = x - y;
    uint16_t x_sub_y_xor_y = x_sub_y ^ y;
    uint16_t q = x_xor_y | x_sub_y_xor_y;
    uint16_t x_xor_q = x ^ q;
    uint16_t x_xor_q_ = x_xor_q >> (uint32_t)15U;
    return x_xor_q_ - (uint16_t)1U;
}

extern Prims_string FStar_UInt16_to_string(uint16_t uu____888);

extern Prims_string FStar_UInt16_to_string_hex(uint16_t uu____899);

extern Prims_string FStar_UInt16_to_string_hex_pad(uint16_t uu____910);

extern uint16_t FStar_UInt16_of_string(Prims_string uu____921);

extern Prims_int FStar_UInt8_n;

extern bool FStar_UInt8_uu___is_Mk(uint8_t projectee);

extern Prims_int FStar_UInt8___proj__Mk__item__v(uint8_t projectee);

extern Prims_int FStar_UInt8_v(uint8_t x);

extern uint8_t FStar_UInt8_uint_to_t(Prims_int x);

extern uint8_t FStar_UInt8_minus(uint8_t a);

extern uint32_t FStar_UInt8_n_minus_one;

static inline uint8_t
FStar_UInt8_eq_mask(uint8_t a, uint8_t b)
{
    uint8_t x = a ^ b;
    uint8_t minus_x = ~x + (uint8_t)1U;
    uint8_t x_or_minus_x = x | minus_x;
    uint8_t xnx = x_or_minus_x >> (uint32_t)7U;
    return xnx - (uint8_t)1U;
}

static inline uint8_t
FStar_UInt8_gte_mask(uint8_t a, uint8_t b)
{
    uint8_t x = a;
    uint8_t y = b;
    uint8_t x_xor_y = x ^ y;
    uint8_t x_sub_y = x - y;
    uint8_t x_sub_y_xor_y = x_sub_y ^ y;
    uint8_t q = x_xor_y | x_sub_y_xor_y;
    uint8_t x_xor_q = x ^ q;
    uint8_t x_xor_q_ = x_xor_q >> (uint32_t)7U;
    return x_xor_q_ - (uint8_t)1U;
}

extern Prims_string FStar_UInt8_to_string(uint8_t uu____888);

extern Prims_string FStar_UInt8_to_string_hex(uint8_t uu____899);

extern Prims_string FStar_UInt8_to_string_hex_pad(uint8_t uu____910);

extern uint8_t FStar_UInt8_of_string(Prims_string uu____921);

typedef uint8_t FStar_UInt8_byte;

#define __FStar_UInt_8_16_32_64_H_DEFINED
#endif
