/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CC_SIPCCLINE_H
#define _CC_SIPCCLINE_H

#include "CC_Line.h"
#include <map>

#include "common/csf_common.h"
#include "common/Wrapper.h"

namespace CSF
{
	DECLARE_NS_PTR(CC_SIPCCLine);
    class CC_SIPCCLine : public CC_Line
    {
    private:
        CSF_DECLARE_WRAP(CC_SIPCCLine, cc_lineid_t);

        cc_lineid_t lineId;
        explicit CC_SIPCCLine (cc_lineid_t aLineId) : lineId(aLineId) { }

    public:
        virtual std::string toString() {
            std::string result;
            char tmpString[11];
            csf_sprintf(tmpString, sizeof(tmpString), "%X", lineId);
            result = tmpString;
            return result;
        };
        virtual cc_lineid_t getID();
        virtual CC_LineInfoPtr getLineInfo ();
        virtual CC_CallPtr createCall ();
    };

};


#endif
