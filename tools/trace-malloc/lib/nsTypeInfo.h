/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef trace_malloc_nsTypeInfo_h_
#define trace_malloc_nsTypeInfo_h_

#include "prtypes.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const char* nsGetTypeName(void* ptr);

extern void RegisterTraceMallocShutdown();

#ifdef __cplusplus
}
#endif

#endif /* trace_malloc_nsTypeInfo_h_ */
