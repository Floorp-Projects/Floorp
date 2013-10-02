/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SDP_OS_DEFS_H_
#define _SDP_OS_DEFS_H_


#include "cpr_types.h"
#include "cpr_stdio.h"
#include "cpr_stdlib.h"
#include "cpr_string.h"
#include "phone_debug.h"


#define SDP_PRINT(format, ...) CSFLogError("sdp" , format , ## __VA_ARGS__ )
#define SDP_MALLOC(x) cpr_calloc(1, (x))
#define SDP_FREE      cpr_free

typedef uint8_t    tinybool;
typedef uint8_t    u8;
typedef uint16_t   u16;
typedef uint16_t   uint16;
typedef uint32_t   u32;
typedef uint32_t   uint32;
typedef int32_t    int32;
typedef int16_t    int16;
typedef unsigned short ushort;
typedef unsigned long  ulong;
#ifndef __GNUC_STDC_INLINE__
#define inline
#endif

#endif /* _SDP_OS_DEFS_H_ */
