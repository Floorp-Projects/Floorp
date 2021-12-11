/*
 * altivec-types.h - shorter vector typedefs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ALTIVEC_TYPES_H_
#define _ALTIVEC_TYPES_H_ 1

#include <altivec.h>

typedef __vector unsigned char vec_u8;
typedef __vector signed char vec_s8;
typedef __vector unsigned short vec_u16;
typedef __vector signed short vec_s16;
typedef __vector unsigned int vec_u32;
typedef __vector signed int vec_s32;
#ifdef __VSX__
typedef __vector unsigned long long vec_u64;
typedef __vector signed long long vec_s64;
#endif
typedef __vector float vec_f;

#endif
