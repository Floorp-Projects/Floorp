/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// You can download Android source at
// http://source.android.com/source/downloading.html
// Original files are in ndk/sources/android/cpufeatures
// Revision is Change-Id: I9a0629efba36a6023f05e5f092e7addcc1b7d2a9

#ifndef CPU_FEATURES_H
#define CPU_FEATURES_H

#include <sys/cdefs.h>
#include <stdint.h>

__BEGIN_DECLS

typedef enum {
    ANDROID_CPU_FAMILY_UNKNOWN = 0,
    ANDROID_CPU_FAMILY_ARM,
    ANDROID_CPU_FAMILY_X86,

    ANDROID_CPU_FAMILY_MAX  /* do not remove */

} AndroidCpuFamily;

/* Return family of the device's CPU */
extern AndroidCpuFamily   android_getCpuFamily(void);

enum {
    ANDROID_CPU_ARM_FEATURE_ARMv7       = (1 << 0),
    ANDROID_CPU_ARM_FEATURE_VFPv3       = (1 << 1),
    ANDROID_CPU_ARM_FEATURE_NEON        = (1 << 2),
    ANDROID_CPU_ARM_FEATURE_LDREX_STREX = (1 << 3),
};

enum {
    ANDROID_CPU_X86_FEATURE_SSSE3  = (1 << 0),
    ANDROID_CPU_X86_FEATURE_POPCNT = (1 << 1),
    ANDROID_CPU_X86_FEATURE_MOVBE  = (1 << 2),
};

extern uint64_t    android_getCpuFeatures(void);

/* Return the number of CPU cores detected on this device. */
extern int         android_getCpuCount(void);

__END_DECLS

#endif /* CPU_FEATURES_H */
