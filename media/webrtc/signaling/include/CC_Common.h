/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "SharedPtr.h"

#ifndef ECC_API
#ifdef ECC_EXPORT
#define ECC_API _declspec(dllexport)
#elif ECC_IMPORT
#define ECC_API _declspec(dllimport)
#else
#define ECC_API
#endif
#endif

namespace CSF
{
    DECLARE_NS_PTR(CallControlManager)
    DECLARE_NS_PTR_VECTOR(PhoneDetails)
    DECLARE_NS_PTR(CC_Service)
    DECLARE_NS_PTR(VideoControl)
    DECLARE_NS_PTR(AudioControl)
    DECLARE_NS_PTR_VECTOR(CC_Device)
    DECLARE_NS_PTR(CC_DeviceInfo)
    DECLARE_NS_PTR(CC_CallServerInfo)
    DECLARE_NS_PTR(CC_FeatureInfo)
    DECLARE_NS_PTR_VECTOR(CC_Line)
    DECLARE_NS_PTR(CC_LineInfo)
    DECLARE_NS_PTR_VECTOR(CC_Call)
    DECLARE_NS_PTR(CC_CallInfo)
}
