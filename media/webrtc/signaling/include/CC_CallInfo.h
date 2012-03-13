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

#pragma once

#include <set>

extern "C"
{
#include "ccapi_types.h"
}

#include "CC_Common.h"
#include "CC_CallTypes.h"

namespace CSF
{

	class ECC_API CC_CallInfo
    {
    protected:
        CC_CallInfo() { }

    public:
        //Base class needs dtor to be declared as virtual
        virtual ~CC_CallInfo() {};

        /**
           Get the line object associated with this call.

           @return CC_LinePtr - line ID
         */
        virtual CC_LinePtr getline () = 0;

        /**
           get Call state
           @param [in] handle - call info handle
           @return call state
         */
        virtual cc_call_state_t getCallState () = 0;

        /**
           Get ringer state.

           @return bool ringer state.
         */
        virtual bool getRingerState() = 0;

        /**
           Get call attributes

           @return cc_call_attr_t.
         */
        virtual cc_call_attr_t getCallAttr() = 0;

        /**
           Get the call type

           @return cc_call_type_t for this call. Supported values inlude:
                   CC_CALL_TYPE_INCOMING, CC_CALL_TYPE_OUTGOING and CC_CALL_TYPE_FORWARDED.
         */
        virtual cc_call_type_t getCallType() = 0;

        /**
           Get called party name

           @return called party name
         */
        virtual std::string getCalledPartyName() = 0;

        /**
           Get called party number

           @return called party number as a string.
         */
        virtual std::string getCalledPartyNumber() = 0;

        /**
           Get calling party name

           @return calling party name
         */
        virtual std::string getCallingPartyName() = 0;

        /**
           Get calling party number
           @return calling party number as a string
                   Note: this is a const reference to a string that's owned by the 
          */
        virtual std::string getCallingPartyNumber() = 0;

        /**
           Get alternate number

           @return calling party number as a string.
         */
        virtual std::string getAlternateNumber() = 0;

        /**
           This function is used to check if a given capability is supported
           based on the information in this CC_CallInfo object.

           @param [in] capability - the capability that is to be checked for availability.
           @return boolean - returns true if the given capability is available, false otherwise.
         */
        virtual bool hasCapability (CC_CallCapabilityEnum::CC_CallCapability capability) = 0;

        /**
           If you need the complete set of capabilities 

           @return cc_return_t - set of Call Capabilities.
         */
        virtual std::set<CC_CallCapabilityEnum::CC_CallCapability> getCapabilitySet() = 0;

        /**
           get Original Called party name
           @param [in] handle - call info handle
           @return original called party name
         */
        virtual std::string getOriginalCalledPartyName() = 0;

        /**
           get Original Called party number
           @param [in] handle - call info handle
           @return original called party number
         */
        virtual std::string getOriginalCalledPartyNumber() = 0;

        /**
           get last redirecting party name
           @param [in] handle - call info handle
           @return last redirecting party name
         */
        virtual std::string getLastRedirectingPartyName() = 0;

        /**
           get past redirecting party number
           @param [in] handle - call info handle
           @return last redirecting party number
         */
        virtual std::string getLastRedirectingPartyNumber() = 0;

        /**
           get placed call party name
           @param [in] handle - call info handle
           @return placed party name
         */
        virtual std::string getPlacedCallPartyName() = 0;

        /**
           get placed call party number
           @param [in] handle - call info handle
           @return placed party number
         */
        virtual std::string getPlacedCallPartyNumber() = 0;

        /**
           get call instance number
           @param [in] handle - call info handle
           @return 
         */
        virtual cc_int32_t getCallInstance() = 0;

        /**
           get call status prompt
           @param [in] handle - call info handle
           @return call status
         */
        virtual std::string getStatus() = 0;

        /**
           get call security   // TODO XLS has callagent security and endtoend security on call?
           @param [in] handle - call info handle
           @return call security status
         */
        virtual cc_call_security_t getSecurity() = 0;

        /**
           get Call Selection Status
           @param [in] handle - call info handle
           @return bool - TRUE => selected
         */
        virtual cc_int32_t getSelectionStatus() = 0;

        /**
           get GCID
           @param [in] handle - call info handle
           @return GCID
         */
        virtual std::string getGCID() = 0;

        /**
           get ringer loop count
           @param handle - call handle
           @return once Vs continuous
         */
        virtual bool getIsRingOnce() = 0;

        /**
           get ringer mode
           @param handle - call handle
           @return ringer mode
         */
        virtual int getRingerMode() = 0;

        /**
           get onhook reason
           @param [in] handle - call info handle
           @return onhook reason
         */
        virtual cc_int32_t getOnhookReason() = 0;

        /**
           is Conference Call?
           @param [in] handle - call info handle
           @return boolean - is Conference
         */
        virtual bool getIsConference() = 0;

        /**
           getStream Statistics
           @param [in] handle - call info handle
           @param [in,out] stats - Array to get the stats
           @param [in,out] count - in len of stats arraysize of stats / out stats copied
           @return cc_return_t - CC_SUCCESS or CC_FAILURE
         */
        virtual std::set<cc_int32_t> getStreamStatistics() = 0;

        /**
           Call selection status
           @param [in] handle - call info handle
           @return bool - selection status
         */
        virtual bool isCallSelected() = 0;

        /**
           INFO Package for RECEIVED_INFO event
           @param [in] handle - call info handle
           @return string - Info package header
         */
        virtual std::string getINFOPack() = 0;

        /**
           INFO type for RECEIVED_INFO event

           @return string - content-type  header
         */
        virtual std::string getINFOType() = 0;

        /**
           INFO body for RECEIVED_INFO event

           @return string - INFO body
         */
        virtual std::string getINFOBody() = 0;

        /**
           Get the call log reference

           //TODO NEED TO DO SOMETHING WRAP CALL LOG REF.
           @return string - INFO body
           NOTE: Memory associated with the call log is tied to the  
           this would be freed when the callinfo ref is freed.
         */
        virtual cc_calllog_ref_t  getCallLogRef() = 0;

        /**
           returns the negotiated video direction for this call

           @return cc_sdp_direction_t - video direction
         */
        virtual cc_sdp_direction_t getVideoDirection() = 0;

         /**
           Find out if this call is capable of querying the media state, which includes mute state and video direction
           @return  bool  
          */       
        virtual bool isMediaStateAvailable() = 0;
        
        /**
           Get the audio mute state if available (check availability with isMediaStateAvailable())
           @return  bool  - the current audio state of the call
          */
        virtual bool isAudioMuted(void) = 0;
        
         /**
           Get the video mute state if available (check availability with isMediaStateAvailable())
           @return  bool  - the current video state of the call
          */
        virtual bool isVideoMuted(void) = 0;

        /**
          Get the current call volume level
          @return int - the current call volume level, or -1 if it cannot be determined
        */
        virtual int getVolume() = 0;
    };
};
