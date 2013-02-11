/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_INTERFACE_ASM_DEFINES_H_
#define WEBRTC_SYSTEM_WRAPPERS_INTERFACE_ASM_DEFINES_H_

#if defined(__linux__) && defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif

// Define the macros used in ARM assembly code, so that for Mac or iOS builds
// we add leading underscores for the function names.
#ifdef __APPLE__
.macro GLOBAL_FUNCTION name
.global _\name
.endm
.macro DEFINE_FUNCTION name
_\name:
.endm
#else
.macro GLOBAL_FUNCTION name
.global \name
.endm
.macro DEFINE_FUNCTION name
\name:
.endm
#endif

.text

#endif  // WEBRTC_SYSTEM_WRAPPERS_INTERFACE_COMPILE_ASSERT_H_
