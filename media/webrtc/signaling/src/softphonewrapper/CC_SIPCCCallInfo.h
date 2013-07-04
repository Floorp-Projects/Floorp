/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CC_SIPCC_CALLINFO_H
#define _CC_SIPCC_CALLINFO_H

#include "CC_CallInfo.h"
#include "CC_SIPCCCall.h"


#include "common/Wrapper.h"

extern "C" {
    void CCAPI_CallListener_onCallEvent(ccapi_call_event_e eventType, cc_call_handle_t handle, cc_callinfo_ref_t info);
}

namespace CSF
{

	DECLARE_NS_PTR(CC_SIPCCCallInfo);
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
        fsmdef_states_t getFsmState () const;
        bool getRingerState();

        virtual cc_call_attr_t getCallAttr();


        virtual CC_LinePtr getline ();
        virtual std::string callStateToString (cc_call_state_t state);
        virtual std::string fsmStateToString (fsmdef_states_t state) const;
        virtual std::string callEventToString (ccapi_call_event_e callEvent);
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
        virtual std::string getSDP();
        virtual cc_int32_t getStatusCode();
        virtual MediaStreamTable* getMediaStreams() const;

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
