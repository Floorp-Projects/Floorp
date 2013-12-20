/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "CC_Common.h"
#include "ECC_Types.h"
#include "mozilla/RefPtr.h"

extern "C"
{
#include "ccapi_types.h"
}

#if defined(__cplusplus) && __cplusplus >= 201103L
typedef struct Timecard Timecard;
#else
#include "timecard.h"
#endif

namespace CSF
{
    class ECC_API CC_Call
    {
    public:
        NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CC_Call)

    protected:
        CC_Call () { }

    public:
        virtual ~CC_Call () {}

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
        virtual bool originateCall (cc_sdp_direction_t video_pref, const std::string & digits) = 0;

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

           @return void
          */
        virtual void originateP2PCall (cc_sdp_direction_t video_pref, const std::string & digits, const std::string & ip) = 0;

        virtual void createOffer (cc_media_constraints_t* constraints, Timecard *) = 0;

        virtual void createAnswer(cc_media_constraints_t* constraints, Timecard *) = 0;

        virtual void setLocalDescription(cc_jsep_action_t action, const std::string & sdp, Timecard *) = 0;

        virtual void setRemoteDescription(cc_jsep_action_t action, const std::string & sdp, Timecard *) = 0;

        virtual void setPeerConnection(const std::string& handle) = 0;

        virtual void addStream(cc_media_stream_id_t stream_id,
                               cc_media_track_id_t track_id,
                               cc_media_type_t media_type,
                               cc_media_constraints_t *constraints) = 0;

        virtual void removeStream(cc_media_stream_id_t stream_id, cc_media_track_id_t track_id, cc_media_type_t media_type) = 0;

        virtual const std::string& getPeerConnection() const = 0;

        virtual void addICECandidate(const std::string & candidate, const std::string & mid, unsigned short level, Timecard *) = 0;

    };
}

