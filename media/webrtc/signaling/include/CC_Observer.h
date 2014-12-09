/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "CC_Common.h"

extern "C"
{
#include "ccapi_types.h"
}

namespace CSF
{
	/**
	 * These callbacks relate to the "core" Call Control API objects CC_Device, CC_Line and CC_Call.
	 */
    class ECC_API CC_Observer
    {
    public:
        virtual void onDeviceEvent  ( ccapi_device_event_e deviceEvent, CC_DevicePtr device, CC_DeviceInfoPtr info ) = 0;
        virtual void onFeatureEvent ( ccapi_device_event_e deviceEvent, CC_DevicePtr device, CC_FeatureInfoPtr feature_info) = 0;
        virtual void onLineEvent    ( ccapi_line_event_e lineEvent,     CC_LinePtr line,     CC_LineInfoPtr info ) = 0;
        virtual void onCallEvent    ( ccapi_call_event_e callEvent,     CC_CallPtr call,     CC_CallInfoPtr infog ) = 0;
    };

}
