// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* This file has been imported from
 * http://git.chromium.org/gitweb/?p=chromium.git;a=blob_plain;f=sandbox/linux/services/android_ucontext.h;hb=99b3e83972e478a42fa72da1ffefee58413e87d4
 */

#ifndef SANDBOX_LINUX_SERVICES_ANDROID_UCONTEXT_H_
#define SANDBOX_LINUX_SERVICES_ANDROID_UCONTEXT_H_

#if defined(__ANDROID__)

#if defined(__arm__)
#include "android_arm_ucontext.h"
#elif defined(__i386__)
#include "android_i386_ucontext.h"
#else
#error "No support for your architecture in Android header"
#endif

#else  // __ANDROID__
#error "Android header file included on non Android."
#endif  // __ANDROID__

#endif  // SANDBOX_LINUX_SERVICES_ANDROID_UCONTEXT_H_
