/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __CPR_DARWIN_ALIGN_H__
#define __CPR_DARWIN_ALIGN_H__
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

#endif /* __CPR_DARWIN_ALIGN_H__ */
