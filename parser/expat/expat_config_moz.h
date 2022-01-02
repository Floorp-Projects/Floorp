/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __expat_config_moz_h__
#define __expat_config_moz_h__

#define MOZ_UNICODE
#include "nspr.h"

#ifdef IS_LITTLE_ENDIAN
#define BYTEORDER 1234
#else
#define BYTEORDER 4321
#endif /* IS_LITTLE_ENDIAN */

#if PR_BYTES_PER_INT != 4
#define int int32_t
#endif /* PR_BYTES_PER_INT != 4 */

#endif /* __expat_config_moz_h__ */
