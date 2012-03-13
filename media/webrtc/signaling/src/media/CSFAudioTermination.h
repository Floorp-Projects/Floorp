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
    AudioPayloadType_ISAC = 41

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
