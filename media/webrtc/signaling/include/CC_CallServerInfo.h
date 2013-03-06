/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "CC_Common.h"

#include <vector>

extern "C"
{
#include "ccapi_types.h"
}

namespace CSF
{
    class ECC_API CC_CallServerInfo
    {
    public:
        NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CC_CallServerInfo)
    protected:
        CC_CallServerInfo() { }

    public:
        //Base class needs dtor to be declared as virtual
        virtual ~CC_CallServerInfo() {};

        /**
           gets call server name

           @returns name of the call server
         */
        virtual std::string getCallServerName() = 0;

        /**
           gets call server mode

           @returns - mode of the call server
         */
        virtual cc_cucm_mode_t getCallServerMode() = 0;

        /**
           gets calls erver name

           @returns status of the call server
         */
        virtual cc_ccm_status_t getCallServerStatus() = 0;
    };
};
