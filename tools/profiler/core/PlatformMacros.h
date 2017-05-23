/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PLATFORM_MACROS_H
#define PLATFORM_MACROS_H

// Define platform selection macros in a consistent way. Don't add anything
// else to this file, so it can remain freestanding. The primary factorisation
// is on (ARCH,OS) pairs ("PLATforms") but ARCH_ and OS_ macros are defined
// too, since they are sometimes convenient.
//
// Note: "GP" is short for "Gecko Profiler".

#undef GP_PLAT_x86_android
#undef GP_PLAT_arm_android
#undef GP_PLAT_aarch64_android
#undef GP_PLAT_x86_linux
#undef GP_PLAT_amd64_linux
#undef GP_PLAT_amd64_darwin
#undef GP_PLAT_x86_windows
#undef GP_PLAT_amd64_windows

#undef GP_ARCH_x86
#undef GP_ARCH_amd64
#undef GP_ARCH_arm
#undef GP_ARCH_aarch64

#undef GP_OS_android
#undef GP_OS_linux
#undef GP_OS_darwin
#undef GP_OS_windows

// We test __ANDROID__ before __linux__ because __linux__ is defined on both
// Android and Linux, whereas GP_OS_android is not defined on vanilla Linux.

#if defined(__ANDROID__) && defined(__i386__)
# define GP_PLAT_x86_android 1
# define GP_ARCH_x86 1
# define GP_OS_android 1

#elif defined(__ANDROID__) && defined(__arm__)
# define GP_PLAT_arm_android 1
# define GP_ARCH_arm 1
# define GP_OS_android 1

#elif defined(__ANDROID__) && defined(__aarch64__)
# define GP_PLAT_aarch64_android 1
# define GP_ARCH_aarch64 1
# define GP_OS_android 1

#elif defined(__linux__) && defined(__i386__)
# define GP_PLAT_x86_linux 1
# define GP_ARCH_x86 1
# define GP_OS_linux 1

#elif defined(__linux__) && defined(__x86_64__)
# define GP_PLAT_amd64_linux 1
# define GP_ARCH_amd64 1
# define GP_OS_linux 1

#elif defined(__APPLE__) && defined(__x86_64__)
# define GP_PLAT_amd64_darwin 1
# define GP_ARCH_amd64 1
# define GP_OS_darwin 1

#elif (defined(_MSC_VER) || defined(__MINGW32__)) && \
      (defined(_M_IX86) || defined(__i386__))
# define GP_PLAT_x86_windows 1
# define GP_ARCH_x86 1
# define GP_OS_windows 1

#elif (defined(_MSC_VER) || defined(__MINGW32__)) && \
      (defined(_M_X64) || defined(__x86_64__))
# define GP_PLAT_amd64_windows 1
# define GP_ARCH_amd64 1
# define GP_OS_windows 1

#else
# error "Unsupported platform"
#endif

#endif /* ndef PLATFORM_MACROS_H */
