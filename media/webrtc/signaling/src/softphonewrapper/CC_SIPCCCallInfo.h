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

#ifndef _CC_SIPCC_CALLINFO_H
#define _CC_SIPCC_CALLINFO_H

#include "CC_CallInfo.h"
#include "CC_SIPCCCall.h"

#include "common/Wrapper.h"

extern "C" {
    void CCAPI_CallListener_onCallEvent(ccapi_call_event_e eventType, cc_call_handle_t handle, cc_callinfo_ref_t info, char* sdp);
}

namespace CSF
{
	DECLARE_PTR(CC_SIPCCCallInfo);
    class CC_SIPCCCallInfo : public CC_CallInfo
    {

    private:
    	cc_callinfo_ref_t callinfo_ref;

        CC_SIPCCCallInfo (cc_callinfo_ref_t callinfo);

        CSF_DECLARE_WRAP(CC_SIPCCCallInfo, cc_callinfo_ref_t);

        CC_SIPCCCallMediaDataPtr  pMediaData;

    public:
        ~CC_SIPCCCallInfo ();

        cc_call_state_t getCallState ();
        bool getRingerState();

        virtual cc_call_attr_t getCallAttr();

        virtual CC_LinePtr getline ();

        virtual cc_call_type_t getCallType();
        virtual std::string getCalledPartyName();
        virtual std::string getCalledPartyNumber();
        virtual std::string getCallingPartyName();
        virtual std::string getCallingPartyNumber();
        virtual std::string getAlternateNumber();
        virtual bool hasCapability (CC_CallCapabilityEnum::CC_CallCapability capability);
        virtual std::set<CC_CallCapabilityEnum::CC_CallCapability> getCapabilitySet();
        virtual std::string getOriginalCalledPartyName();
        virtual std::string getOriginalCalledPartyNumber();
        virtual std::string getLastRedirectingPartyName();
        virtual std::string getLastRedirectingPartyNumber();
        virtual std::string getPlacedCallPartyName();
        virtual std::string getPlacedCallPartyNumber();
        virtual cc_int32_t getCallInstance();
        virtual std::string getStatus();
        virtual cc_call_security_t getSecurity();
        virtual cc_int32_t getSelectionStatus();
        virtual std::string getGCID();
        virtual bool getIsRingOnce();
        virtual int getRingerMode();
        virtual cc_int32_t  getOnhookReason();
        virtual bool getIsConference();
        virtual std::set<cc_int32_t> getStreamStatistics();
        virtual bool isCallSelected();
        virtual std::string getINFOPack();
        virtual std::string getINFOType();
        virtual std::string getINFOBody();
        virtual cc_calllog_ref_t getCallLogRef();
        virtual cc_sdp_direction_t getVideoDirection();
        virtual int getVolume();
        virtual bool isMediaStateAvailable();
        virtual bool isAudioMuted();
        virtual bool isVideoMuted();

        virtual void setMediaData(CC_SIPCCCallMediaDataPtr  pMediaData);

    private:
        // Helper to generate the caps once - then we serve then from this cache.
        // This is fine because the info object is a snapshot, and there's no use generating this
        // multiple times if the client makes successive calls to hasCapability().
        void generateCapabilities();
        std::set<CC_CallCapabilityEnum::CC_CallCapability> caps;
        bool hasFeature(ccapi_call_capability_e cap);
    };
};


#endif
