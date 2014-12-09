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

    class ECC_API CC_Device
    {
    public:
        NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CC_Device)
    protected:
        CC_Device() {}

        virtual ~CC_Device() {}

    public:
        virtual std::string toString() = 0;

        virtual CC_DeviceInfoPtr getDeviceInfo () = 0;

        /**
           Create a call on the device. Line selection is on the first available line.
           Lines that have their MNC reached will be skipped. If you have a specific line
           you want to make a call on (assuming the device has more than available) then
           you should use CC_Line::createCall() to do that.

           @return CC_CallPtr - the created call object wrapped in a smart_ptr.
         */
        virtual CC_CallPtr createCall() = 0;

        virtual void enableVideo(bool enable) = 0;
        virtual void enableCamera(bool enable) = 0;
		virtual void setDigestNamePasswd (char *name, char *pw) = 0;

    private:
		// Cannot copy - clients should be passing the pointer not the object.
		CC_Device& operator=(const CC_Device& rhs);
		CC_Device(const CC_Device&);
    };
}
