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
    class ECC_API CC_Line
    {
    public:
        NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CC_Line)
    protected:
        CC_Line () { }

    public:
        virtual ~CC_Line () {};

        virtual std::string toString() = 0;

        virtual cc_lineid_t getID() = 0;
        virtual CC_LineInfoPtr getLineInfo () = 0;
        virtual CC_CallPtr createCall () = 0;
    };
};
