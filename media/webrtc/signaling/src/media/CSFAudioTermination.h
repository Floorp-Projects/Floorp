/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CSFAUDIOMEDIATERMINATION_H_
#define CSFAUDIOMEDIATERMINATION_H_

#include <CSFMediaTermination.h>

typedef enum
{
    AudioCodecMask_G711 = 1,
    AudioCodecMask_LINEAR = 2,
    AudioCodecMask_G722 = 4,
    AudioCodecMask_iLBC = 16,
    AudioCodecMask_iSAC = 32

} AudioCodecMask;

typedef enum
{
    RingMode_INSIDE_RING,
    RingMode_OUTSIDE_RING,
    RingMode_FEATURE_RING,
    RingMode_BELLCORE_DR1,
    RingMode_BELLCORE_DR2,
    RingMode_BELLCORE_DR3,
    RingMode_BELLCORE_DR4,
    RingMode_BELLCORE_DR5,
    RingMode_FLASHONLY_RING,
    RingMode_PRECEDENCE_RING

} RingMode;

typedef enum
{
    ToneType_INSIDE_DIAL_TONE,
    ToneType_OUTSIDE_DIAL_TONE,
    ToneType_LINE_BUSY_TONE,
    ToneType_ALERTING_TONE,
    ToneType_BUSY_VERIFY_TONE,
    ToneType_STUTTER_TONE,
    ToneType_MSG_WAITING_TONE,
    ToneType_REORDER_TONE,
    ToneType_CALL_WAITING_TONE,
    ToneType_CALL_WAITING_2_TONE,
    ToneType_CALL_WAITING_3_TONE,
    ToneType_CALL_WAITING_4_TONE,
    ToneType_HOLD_TONE,
    ToneType_CONFIRMATION_TONE,
    ToneType_PERMANENT_SIGNAL_TONE,
    ToneType_REMINDER_RING_TONE,
    ToneType_NO_TONE,
    ToneType_ZIP_ZIP,
    ToneType_ZIP,
    ToneType_BEEP_BONK,
    ToneType_RECORDERWARNING_TONE,
    ToneType_RECORDERDETECTED_TONE,
    ToneType_MONITORWARNING_TONE,
    ToneType_SECUREWARNING_TONE

} ToneType;

typedef enum
{
    ToneDirection_PLAY_TONE_TO_EAR = 1,
    ToneDirection_PLAY_TONE_TO_NET = 2,
    ToneDirection_PLAY_TONE_TO_ALL = 3

} ToneDirection;

typedef enum
{
    AudioPayloadType_G711ALAW64K =  2,
    AudioPayloadType_G711ALAW56K =  3,
    AudioPayloadType_G711ULAW64K =  4,
    AudioPayloadType_G711ULAW56K =  5,
    AudioPayloadType_G722_64K = 6,
    AudioPayloadType_G722_56K = 7,
    AudioPayloadType_G722_48K = 8,
    AudioPayloadType_RFC2833 = 38,
    AudioPayloadType_ILBC20 = 39,
    AudioPayloadType_ILBC30 = 40,
    AudioPayloadType_ISAC = 41,
    AudioPayloadType_OPUS = 109

} AudioPayloadType;

#if __cplusplus

namespace CSF
{
    //AudioTermination adds to the core MediaTermination
    class AudioTermination : public MediaTermination
    {
    public:
        virtual int  toneStart	( ToneType type, ToneDirection direction, int alertInfo, int groupId, int streamId, bool useBackup ) = 0;
        virtual int  toneStop	( ToneType type, int groupId, int streamId ) = 0;
        virtual int  ringStart	( int lineId, RingMode mode, bool once ) = 0;
        virtual int  ringStop	( int lineId ) = 0;

        virtual int  sendDtmf 	( int streamId, int digit ) = 0;
        virtual bool mute	    ( int streamId, bool mute ) = 0;
        virtual bool isMuted	( int streamId ) = 0;

        virtual bool setVolume  ( int streamId, int volume ) = 0;
        virtual int  getVolume  ( int streamId ) = 0;

        virtual void setVADEnabled (bool vadEnabled) = 0;

    };

} // namespace

#endif // __cplusplus

#endif /* CSFAUDIOMEDIATERMINATION_H_ */
