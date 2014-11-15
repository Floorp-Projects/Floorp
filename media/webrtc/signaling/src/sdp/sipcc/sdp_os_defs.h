/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SDP_OS_DEFS_H_
#define _SDP_OS_DEFS_H_

#include <stdlib.h>

#include "cpr_types.h"
#include "cpr_string.h"


#define SDP_PRINT(format, ...) CSFLogError("sdp" , format , ## __VA_ARGS__ )

/* Use operating system malloc */
#define SDP_MALLOC(x) calloc(1, (x))
#define SDP_FREE free

typedef uint8_t    tinybool;
typedef unsigned short ushort;
typedef unsigned long  ulong;
#ifndef __GNUC_STDC_INLINE__
#define inline
#endif

#endif /* _SDP_OS_DEFS_H_ */
