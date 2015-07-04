/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SPS_PLATFORM_MACROS_H
#define SPS_PLATFORM_MACROS_H

/* Define platform selection macros in a consistent way.  Don't add
   anything else to this file, so it can remain freestanding.  The
   primary factorisation is on (ARCH,OS) pairs ("PLATforms") but ARCH_
   and OS_ macros are defined too, since they are sometimes
   convenient. */

#undef SPS_PLAT_arm_android
#undef SPS_PLAT_amd64_linux
#undef SPS_PLAT_x86_linux
#undef SPS_PLAT_amd64_darwin
#undef SPS_PLAT_x86_darwin
#undef SPS_PLAT_x86_windows
#undef SPS_PLAT_amd64_windows

#undef SPS_ARCH_arm
#undef SPS_ARCH_x86
#undef SPS_ARCH_amd64

#undef SPS_OS_android
#undef SPS_OS_linux
#undef SPS_OS_darwin
#undef SPS_OS_windows

#if defined(__linux__) && defined(__x86_64__)
#  define SPS_PLAT_amd64_linux 1
#  define SPS_ARCH_amd64 1
#  define SPS_OS_linux 1

#elif defined(__ANDROID__) && defined(__arm__)
#  define SPS_PLAT_arm_android 1
#  define SPS_ARCH_arm 1
#  define SPS_OS_android 1

#elif defined(__ANDROID__) && defined(__i386__)
#  define SPS_PLAT_x86_android 1
#  define SPS_ARCH_x86 1
#  define SPS_OS_android 1

#elif defined(__linux__) && defined(__i386__)
#  define SPS_PLAT_x86_linux 1
#  define SPS_ARCH_x86 1
#  define SPS_OS_linux 1

#elif defined(__APPLE__) && defined(__x86_64__)
#  define SPS_PLAT_amd64_darwin 1
#  define SPS_ARCH_amd64 1
#  define SPS_OS_darwin 1

#elif defined(__APPLE__) && defined(__i386__)
#  define SPS_PLAT_x86_darwin 1
#  define SPS_ARCH_x86 1
#  define SPS_OS_darwin 1

#elif (defined(_MSC_VER) || defined(__MINGW32__)) && (defined(_M_IX86) || defined(__i386__))
#  define SPS_PLAT_x86_windows 1
#  define SPS_ARCH_x86 1
#  define SPS_OS_windows 1

#elif (defined(_MSC_VER) || defined(__MINGW32__)) && (defined(_M_X64) || defined(__x86_64__))
#  define SPS_PLAT_amd64_windows 1
#  define SPS_ARCH_amd64 1
#  define SPS_OS_windows 1

#else
#  error "Unsupported platform"
#endif

#endif /* ndef SPS_PLATFORM_MACROS_H */
