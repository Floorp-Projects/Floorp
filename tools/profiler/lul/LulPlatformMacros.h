/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LulPlatformMacros_h
#define LulPlatformMacros_h

#include <stdint.h>
#include <stdlib.h>

// Define platform selection macros in a consistent way.  The primary
// factorisation is on (ARCH,OS) pairs ("PLATforms") but ARCH_ and OS_
// macros are defined too, since they are sometimes convenient.

#undef LUL_PLAT_x64_linux
#undef LUL_PLAT_x86_linux
#undef LUL_PLAT_arm_android
#undef LUL_PLAT_x86_android

#undef LUL_ARCH_arm
#undef LUL_ARCH_x86
#undef LUL_ARCH_x64

#undef LUL_OS_android
#undef LUL_OS_linux

#if defined(__linux__) && defined(__x86_64__)
# define LUL_PLAT_x64_linux 1
# define LUL_ARCH_x64 1
# define LUL_OS_linux 1

#elif defined(__linux__) && defined(__i386__) && !defined(__ANDROID__)
# define LUL_PLAT_x86_linux 1
# define LUL_ARCH_x86 1
# define LUL_OS_linux 1

#elif defined(__ANDROID__) && defined(__arm__)
# define LUL_PLAT_arm_android 1
# define LUL_ARCH_arm 1
# define LUL_OS_android 1

#elif defined(__ANDROID__) && defined(__i386__)
# define LUL_PLAT_x86_android 1
# define LUL_ARCH_x86 1
# define LUL_OS_android 1

#else
# error "Unsupported platform"
#endif

#endif // LulPlatformMacros_h
