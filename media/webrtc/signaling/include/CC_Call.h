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

#include "CC_Common.h"
#include "ECC_Types.h"

extern "C"
{
#include "ccapi_types.h"
}

namespace CSF
{
    class ECC_API CC_Call
    {
    protected:
        CC_Call () { }

    public:
        virtual ~CC_Call () {};

		virtual void setRemoteWindow (VideoWindowHandle window) = 0;

		virtual int setExternalRenderer(VideoFormat videoFormat, ExternalRendererHandle renderer) = 0;

		virtual void sendIFrame	() = 0;

        virtual CC_CallInfoPtr getCallInfo () = 0;

        virtual std::string toString() = 0;

        /**
           Originate call - API to go offhook and dial specified digits on a given call

           @param [in] video_pref - video direction desired on call
           @param [in] digits - digits to be dialed. can be empty then this API simply goes offhook

           @return true or false.
         */
        virtual bool originateCall (cc_sdp_direction_t video_pref, const std::string & digits, char* sdp, int audioPort, int videoPort) = 0;

        /**
           Use this function to answer an incoming call.

           @param[in] video_pref - video direction desired on call

           @return true or false.
         */
        virtual bool answerCall (cc_sdp_direction_t video_pref) = 0;

        /**
           Use this function to put an active call on hold.

           @param[in] reason - If the user chooses to put the call on hold then
                               CC_HOLD_REASON_NONE should be the value passed in here.

           @return true or false. If it's not appropriate to put this call on
                   hold at the moment then this function will return false.
         */
        virtual bool hold (cc_hold_reason_t reason) = 0;

        /**
           Use this function to resume a call that is currently on hold.

           @param [in] video_pref - video direction desired on call

           @return true or false
         */
        virtual bool resume (cc_sdp_direction_t video_pref) = 0;

        /**
           Use this function to end an active call.

           @return true or false
         */
        virtual bool endCall() = 0;
        
        /**
           Send digits on the call - can be invoked either to dial additional digits or send DTMF

           @param [in] digit - digit to be dialed

           @return true or false
         */
        virtual bool sendDigit (cc_digit_t digit) = 0;

        /**
           Send Backspace - Delete last digit dialed.

           @return true or false
         */
        virtual bool backspace() = 0;

        /**
           Redial

           @param [in] video_pref - video direction desired on call
           @return true or false
         */
        virtual bool redial (cc_sdp_direction_t video_pref) = 0;

        /**
           Initiate Call Forward All 

           @return true or false
         */
        virtual bool initiateCallForwardAll() = 0;

        /**
           end Consult leg - used to end consult leg when the user picks active calls list for xfer/conf

           @return true or false
         */
        virtual bool endConsultativeCall() = 0;

        /**
           Initiate a conference

           @param [in] video_pref - video direction desired on consult call

           @return true or false
         */
        virtual bool conferenceStart (cc_sdp_direction_t video_pref) = 0;

        /**
           complete conference

           @param [in] otherCall - CC_CallPtr of the other leg
           @param [in] video_pref - video direction desired on consult call

           @return true or false
         */
        virtual bool conferenceComplete (CC_CallPtr otherLog, cc_sdp_direction_t video_pref) = 0;

        /**
           start transfer

           @param [in] video_pref - video direction desired on consult call

           @return true or false
         */
        virtual bool transferStart (cc_sdp_direction_t video_pref) = 0;

        /**
           complete transfer

           @param [in] otherLeg - CC_CallPtr of the other leg
           @param [in] video_pref - video direction desired on consult call

           @return true or false
         */
        virtual bool transferComplete (CC_CallPtr otherLeg, 
                                       cc_sdp_direction_t video_pref) = 0;

        /**
           cancel conference or transfer

           @return true or false
         */
        virtual bool cancelTransferOrConferenceFeature() = 0;

        /**
           direct Transfer

           @param [in] target - call handle for transfer target call
           @return true or false
         */
        virtual bool directTransfer (CC_CallPtr target) = 0;

        /**
           Join Across line

           @param [in] target - join target
           @return true or false
         */
        virtual bool joinAcrossLine (CC_CallPtr target) = 0;

        /**
           BLF Call Pickup

           @param [in] video_pref - video direction preference
           @param [in] speed - speedDial Number
           @return true or false
         */
        virtual bool blfCallPickup (cc_sdp_direction_t video_pref, const std::string & speed) = 0;

        /**
           Select a call

           @return true or false
         */
        virtual bool select() = 0;

        /**
           Update Video Media Cap for the call

           @param [in] video_pref - video direction desired on call
           @return true or false
         */
        virtual bool updateVideoMediaCap (cc_sdp_direction_t video_pref) = 0;

        /**
           send INFO method for the call
           @param [in] handle - call handle
           @param [in] infopackage - Info-Package header value
           @param [in] infotype - Content-Type header val
           @param [in] infobody - Body of the INFO message
           @return true or false
         */
        virtual bool sendInfo (const std::string & infopackage, const std::string & infotype, const std::string & infobody) = 0;

        /**
           API to mute audio

           @return true if the operation succeeded

           NOTE: The mute state is persisted within the stack and shall be remembered across hold/resume.
         */
        virtual bool muteAudio(void) = 0;
        
        
        /**
           API to unmute audio

           @return true if the operation succeeded

           NOTE: The mute state is persisted within the stack and shall be remembered across hold/resume.
         */
        virtual bool unmuteAudio(void) = 0;
        /**
           API to mute video

           @return true if the operation succeeded

           NOTE: The mute state is persisted within the stack and shall be remembered across hold/resume.
         */
        virtual bool muteVideo(void) = 0;
        
        
        /**
           API to unmute video

           @return true if the operation succeeded

           NOTE: The mute state is persisted within the stack and shall be remembered across hold/resume.
         */
        virtual bool unmuteVideo(void) = 0;


        /**
        API to set the call volume, acceptable values are 0 - 100
        @return true if volume set successfully, false if value out of range or change failed
        */
        virtual bool setVolume(int volume) = 0;


        /**
           Originate P2P call - API to go offhook and dial specified digits\user on a given call

           @param [in] video_pref - video direction desired on call
           @param [in] digits - digits to be dialed. can be empty then this API simply goes offhook
           @param [in] ip address - the ip address of the peer to call

           @return true or false.
         */
        virtual bool originateP2PCall (cc_sdp_direction_t video_pref, const std::string & digits, const std::string & ip) = 0;

    };
};
