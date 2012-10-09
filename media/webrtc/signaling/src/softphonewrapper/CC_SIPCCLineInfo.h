/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _CC_SIPCC_LINEINFO_H
#define _CC_SIPCC_LINEINFO_H

#include "CC_LineInfo.h"

#include "common/Wrapper.h"

namespace CSF
{
	DECLARE_PTR(CC_SIPCCLineInfo);
    class CC_SIPCCLineInfo : public CC_LineInfo
    {
    public:
        ~CC_SIPCCLineInfo ();

    private:
        cc_lineinfo_ref_t lineinfo;
        CC_SIPCCLineInfo (cc_lineinfo_ref_t lineinfo);
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
