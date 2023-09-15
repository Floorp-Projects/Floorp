/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SECURITY_SANDBOX_BASE_SHIM_SDKDECLS_H_
#define _SECURITY_SANDBOX_BASE_SHIM_SDKDECLS_H_

#include <windows.h>

// This file contains definitions required for things dynamically loaded
// while building or targetting lower platform versions or lower SDKs.

#if defined(__MINGW32__)
//
// Define Font Disable Policy.  When enabled, this option will
// block loading Non System Fonts.
//

#define PROCESS_CREATION_MITIGATION_POLICY_FONT_DISABLE_MASK                              (0x00000003uLL << 48)
#define PROCESS_CREATION_MITIGATION_POLICY_FONT_DISABLE_DEFER                             (0x00000000uLL << 48)
#define PROCESS_CREATION_MITIGATION_POLICY_FONT_DISABLE_ALWAYS_ON                         (0x00000001uLL << 48)
#define PROCESS_CREATION_MITIGATION_POLICY_FONT_DISABLE_ALWAYS_OFF                        (0x00000002uLL << 48)
#define PROCESS_CREATION_MITIGATION_POLICY_AUDIT_NONSYSTEM_FONTS                          (0x00000003uLL << 48)

//
// Define remote image load options.  When enabled, this option will
// block mapping of images from remote devices.
//

#define PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_NO_REMOTE_MASK                      (0x00000003uLL << 52)
#define PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_NO_REMOTE_DEFER                     (0x00000000uLL << 52)
#define PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_NO_REMOTE_ALWAYS_ON                 (0x00000001uLL << 52)
#define PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_NO_REMOTE_ALWAYS_OFF                (0x00000002uLL << 52)
#define PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_NO_REMOTE_RESERVED                  (0x00000003uLL << 52)

//
// Define low IL image load options.  When enabled, this option will
// block mapping of images that have the low mandatory label.
//

#define PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_NO_LOW_LABEL_MASK                   (0x00000003uLL << 56)
#define PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_NO_LOW_LABEL_DEFER                  (0x00000000uLL << 56)
#define PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_NO_LOW_LABEL_ALWAYS_ON              (0x00000001uLL << 56)
#define PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_NO_LOW_LABEL_ALWAYS_OFF             (0x00000002uLL << 56)
#define PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_NO_LOW_LABEL_RESERVED               (0x00000003uLL << 56)

//
// Define image load options to prefer System32 images compared to
// the same images in application directory. When enabled, this option
// will prefer loading images from system32 folder.
//

#define PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_MASK                (0x00000003uLL << 60)
#define PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_DEFER               (0x00000000uLL << 60)
#define PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_ON           (0x00000001uLL << 60)
#define PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_OFF          (0x00000002uLL << 60)
#define PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_RESERVED            (0x00000003uLL << 60)

//
// Define the restricted indirect branch prediction mitigation policy options.
//

#define PROCESS_CREATION_MITIGATION_POLICY2_RESTRICT_INDIRECT_BRANCH_PREDICTION_MASK        (0x00000003ui64 << 16)
#define PROCESS_CREATION_MITIGATION_POLICY2_RESTRICT_INDIRECT_BRANCH_PREDICTION_DEFER       (0x00000000ui64 << 16)
#define PROCESS_CREATION_MITIGATION_POLICY2_RESTRICT_INDIRECT_BRANCH_PREDICTION_ALWAYS_ON   (0x00000001ui64 << 16)
#define PROCESS_CREATION_MITIGATION_POLICY2_RESTRICT_INDIRECT_BRANCH_PREDICTION_ALWAYS_OFF  (0x00000002ui64 << 16)
#define PROCESS_CREATION_MITIGATION_POLICY2_RESTRICT_INDIRECT_BRANCH_PREDICTION_RESERVED    (0x00000003ui64 << 16)

//
// Define the user-mode shadow stack mitigation policy options.
//

#define PROCESS_CREATION_MITIGATION_POLICY2_CET_USER_SHADOW_STACKS_MASK                    (0x00000003ui64 << 28)
#define PROCESS_CREATION_MITIGATION_POLICY2_CET_USER_SHADOW_STACKS_DEFER                   (0x00000000ui64 << 28)
#define PROCESS_CREATION_MITIGATION_POLICY2_CET_USER_SHADOW_STACKS_ALWAYS_ON               (0x00000001ui64 << 28)
#define PROCESS_CREATION_MITIGATION_POLICY2_CET_USER_SHADOW_STACKS_ALWAYS_OFF              (0x00000002ui64 << 28)
#define PROCESS_CREATION_MITIGATION_POLICY2_CET_USER_SHADOW_STACKS_RESERVED                (0x00000003ui64 << 28)
#define PROCESS_CREATION_MITIGATION_POLICY2_CET_USER_SHADOW_STACKS_STRICT_MODE             (0x00000003ui64 << 28)

//
// Define Attribute to disable creation of child process
//

#define PROCESS_CREATION_CHILD_PROCESS_RESTRICTED                                         0x01
#define PROCESS_CREATION_CHILD_PROCESS_OVERRIDE                                           0x02

//
// Define Attribute for Desktop Appx Overide.
//

#define PROCESS_CREATION_DESKTOP_APPX_OVERRIDE                                            0x04

#define ProcThreadAttributeChildProcessPolicy 14

#define PROC_THREAD_ATTRIBUTE_CHILD_PROCESS_POLICY \
    ProcThreadAttributeValue (ProcThreadAttributeChildProcessPolicy, FALSE, TRUE, FALSE)

//
// Define Attribute to opt out of matching All Application Packages
//

#define PROCESS_CREATION_ALL_APPLICATION_PACKAGES_OPT_OUT                                 0x01

#define ProcThreadAttributeAllApplicationPackagesPolicy 15

#define PROC_THREAD_ATTRIBUTE_ALL_APPLICATION_PACKAGES_POLICY \
    ProcThreadAttributeValue (ProcThreadAttributeAllApplicationPackagesPolicy, FALSE, TRUE, FALSE)

//
// Define functions declared only when _WIN32_WINNT >= 0x0A00
//

WINBASEAPI
BOOL
WINAPI
IsWow64Process2(
    _In_ HANDLE hProcess,
    _Out_ USHORT* pProcessMachine,
    _Out_opt_ USHORT* pNativeMachine
    );

WINBASEAPI
BOOL
WINAPI
IsUserCetAvailableInEnvironment(
    _In_ DWORD UserCetEnvironment
    );

#define USER_CET_ENVIRONMENT_WIN32_PROCESS        0x00000000

#endif // defined(__MINGW32__)

#endif // _SECURITY_SANDBOX_BASE_SHIM_SDKDECLS_H_
