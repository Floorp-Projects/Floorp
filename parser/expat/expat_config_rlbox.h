/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __expat_config_rlbox_h__
#define __expat_config_rlbox_h__

/* Wasm is little endian */
#define BYTEORDER 1234
#define IS_LITTLE_ENDIAN 1

/* We don't redefine int as int32_t for our 32-bit Wasm machine. */
#ifdef __cplusplus
static_assert(sizeof(int) == sizeof(int32_t), "Expected int and int32_t to be of same size.");
#endif

/* We don't need to nor want to expose getpid() to expat */
#define getpid() 0

#endif /* __expat_config_rlbox_h__ */
