/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __CPR_ANDROID_ALIGN_H__
#define __CPR_ANDROID_ALIGN_H__
#include "cpr_types.h"

/*
 * Macros to determine how an address is aligned
 */
#define is_16bit_aligned(addr) (!((uint32_t)(addr) & 0x01))
#define is_32bit_aligned(addr) (!((uint32_t)(addr) & 0x03))
#define is_64bit_aligned(addr) (!((uint32_t)(addr) & 0x07))

/*
 * Macro to get a mask for a specified byte alignment
 */
#define ALIGN_MASK(alignment)	(~((alignment) - 1))

/*
 * Macro to set the minimum alignment
 */
#define ALIGN_MIN(align,align_min) (((align) > (align_min)) ? (align) : (align_min))

/*
 * Macro to round up or down "val" to be a multiple of "align", assuming
 * "align" is a power of 2. if "align" is zero then no action will
 * be performed
 */
#ifdef __typeof__
#define ALIGN_UP(val,align)   ((align == 0) ? (val) : (__typeof__(val))(((uint32_t)(val)  +  ((align) - 1)) & ~((align) - 1)))
#define ALIGN_DOWN(val,align) ((align == 0) ? (val) : (__typeof__(val))(((uint32_t)(val)) & ~((align) - 1)))
#else
#define ALIGN_UP(val,align)   ((align == 0) ? (val) : (uint32_t)(((uint32_t)(val)  +  ((align) - 1)) & ~((align) - 1)))
#define ALIGN_DOWN(val,align) ((align == 0) ? (val) : (uint32_t)(((uint32_t)(val)) & ~((align) - 1)))
#endif

/**
 * Macro to safely write 4 bytes based on memory alignment setting
 */
#ifndef CPR_MEMORY_LITTLE_ENDIAN
#define WRITE_4BYTES_UNALIGNED(mem, value) \
    { ((char *)mem)[0] = (uint8_t)(((value) & 0xff000000) >> 24); \
      ((char *)mem)[1] = (uint8_t)(((value) & 0x00ff0000) >> 16); \
      ((char *)mem)[2] = (uint8_t)(((value) & 0x0000ff00) >>  8); \
      ((char *)mem)[3] = (uint8_t) ((value) & 0x000000ff);       \
    }
#else
#define WRITE_4BYTES_UNALIGNED(mem, value) \
    { ((char *)mem)[3] = (uint8_t)(((value) & 0xff000000) >> 24); \
      ((char *)mem)[2] = (uint8_t)(((value) & 0x00ff0000) >> 16); \
      ((char *)mem)[1] = (uint8_t)(((value) & 0x0000ff00) >>  8); \
      ((char *)mem)[0] = (uint8_t) ((value) & 0x000000ff);       \
    }
#endif
/**
 * Macro to safely read 4 bytes based on memory alignment setting
 */
#ifndef CPR_MEMORY_LITTLE_ENDIAN
#ifdef __typeof__
#define READ_4BYTES_UNALIGNED(mem, value) \
    __typeof__(value)((((uint8_t *)mem)[0] << 24) | (((uint8_t *)mem)[1] << 16) | \
                      (((uint8_t *)mem)[2] <<  8) |  ((uint8_t *)mem)[3])
#else
#define READ_4BYTES_UNALIGNED(mem, value) \
    (uint32_t)((((uint8_t *)mem)[0] << 24) | (((uint8_t *)mem)[1] << 16) | \
               (((uint8_t *)mem)[2] <<  8) |  ((uint8_t *)mem)[3])
#endif
#else
#ifdef __typeof__
#define READ_4BYTES_UNALIGNED(mem, value) \
    __typeof__(value)((((uint8_t *)mem)[3] << 24) | (((uint8_t *)mem)[2] << 16) | \
                      (((uint8_t *)mem)[1] <<  8) |  ((uint8_t *)mem)[0])
#else
#define READ_4BYTES_UNALIGNED(mem, value) \
    (uint32_t)((((uint8_t *)mem)[3] << 24) | (((uint8_t *)mem)[2] << 16) | \
               (((uint8_t *)mem)[1] <<  8) |  ((uint8_t *)mem)[0])
#endif
#endif

#endif /* __CPR_ANDROID_ALIGN_H__ */
