/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CC_SIPCC_LINEINFO_H
#define _CC_SIPCC_LINEINFO_H

#include "CC_LineInfo.h"

#include "common/Wrapper.h"

namespace CSF
{
	DECLARE_NS_PTR(CC_SIPCCLineInfo);
    class CC_SIPCCLineInfo : public CC_LineInfo
    {
    public:
        ~CC_SIPCCLineInfo ();

    private:
        cc_lineinfo_ref_t lineinfo;
        explicit CC_SIPCCLineInfo (cc_lineinfo_ref_t lineinfo);
        CSF_DECLARE_WRAP(CC_SIPCCLineInfo, cc_lineinfo_ref_t);

    public:
        virtual std::string getName();
        virtual std::string getNumber();
        virtual cc_uint32_t getButton();
        virtual cc_line_feature_t getLineType();
        virtual bool getRegState();
        virtual bool isCFWDActive();
        virtual std::string getCFWDName();
        virtual std::vector<CC_CallPtr> getCalls (CC_LinePtr linePtr);
        virtual std::vector<CC_CallPtr> getCallsByState (CC_LinePtr linePtr, cc_call_state_t state);
        virtual bool getMWIStatus();
        virtual cc_uint32_t getMWIType();
        virtual cc_uint32_t getMWINewMsgCount();
        virtual cc_uint32_t getMWIOldMsgCount();
        virtual cc_uint32_t getMWIPrioNewMsgCount();
        virtual cc_uint32_t getMWIPrioOldMsgCount();
        virtual bool hasCapability(ccapi_call_capability_e capability);
        virtual std::bitset<CCAPI_CALL_CAP_MAX> getCapabilitySet();
    };
};


#endif
