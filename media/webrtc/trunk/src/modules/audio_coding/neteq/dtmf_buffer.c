/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * Implementation of packet buffer for DTMF messages.
 */

#include "dtmf_buffer.h"

#include "typedefs.h" /* to define endianness */
#include "signal_processing_library.h"

#include "neteq_error_codes.h"


#ifdef NETEQ_ATEVENT_DECODE

WebRtc_Word16 WebRtcNetEQ_DtmfRemoveEvent(dtmf_inst_t *DTMFdec_inst)
{

    int i;
    for (i = 0; i < 3; i++)
    {
        DTMFdec_inst->EventQueue[i] = DTMFdec_inst->EventQueue[i + 1];
        DTMFdec_inst->EventQueueVolume[i] = DTMFdec_inst->EventQueueVolume[i + 1];
        DTMFdec_inst->EventQueueEnded[i] = DTMFdec_inst->EventQueueEnded[i + 1];
        DTMFdec_inst->EventQueueStartTime[i] = DTMFdec_inst->EventQueueStartTime[i + 1];
        DTMFdec_inst->EventQueueEndTime[i] = DTMFdec_inst->EventQueueEndTime[i + 1];
    }
    DTMFdec_inst->EventBufferSize--;
    DTMFdec_inst->EventQueue[3] = -1;
    DTMFdec_inst->EventQueueVolume[3] = 0;
    DTMFdec_inst->EventQueueEnded[3] = 0;
    DTMFdec_inst->EventQueueStartTime[3] = 0;
    DTMFdec_inst->EventQueueEndTime[3] = 0;

    return 0;
}

WebRtc_Word16 WebRtcNetEQ_DtmfDecoderInit(dtmf_inst_t *DTMFdec_inst, WebRtc_UWord16 fs,
                                          WebRtc_Word16 MaxPLCtime)
{
    int i;
    if (((fs != 8000) && (fs != 16000) && (fs != 32000) && (fs != 48000)) || (MaxPLCtime < 0))
    {
        return DTMF_DEC_PARAMETER_ERROR;
    }
    if (fs == 8000)
        DTMFdec_inst->framelen = 80;
    else if (fs == 16000)
        DTMFdec_inst->framelen = 160;
    else if (fs == 32000)
        DTMFdec_inst->framelen = 320;
    else
        /* fs == 48000 */
        DTMFdec_inst->framelen = 480;

    DTMFdec_inst->MaxPLCtime = MaxPLCtime;
    DTMFdec_inst->CurrentPLCtime = 0;
    DTMFdec_inst->EventBufferSize = 0;
    for (i = 0; i < 4; i++)
    {
        DTMFdec_inst->EventQueue[i] = -1;
        DTMFdec_inst->EventQueueVolume[i] = 0;
        DTMFdec_inst->EventQueueEnded[i] = 0;
        DTMFdec_inst->EventQueueStartTime[i] = 0;
        DTMFdec_inst->EventQueueEndTime[i] = 0;
    }
    return 0;
}

WebRtc_Word16 WebRtcNetEQ_DtmfInsertEvent(dtmf_inst_t *DTMFdec_inst,
                                          const WebRtc_Word16 *encoded, WebRtc_Word16 len,
                                          WebRtc_UWord32 timeStamp)
{

    int i;
    WebRtc_Word16 value;
    const WebRtc_Word16 *EventStart;
    WebRtc_Word16 endEvent;
    WebRtc_Word16 Volume;
    WebRtc_Word16 Duration;
    WebRtc_Word16 position = -1;

    /* Extract event */
    if (len == 4)
    {
        EventStart = encoded;
#ifdef WEBRTC_BIG_ENDIAN
        value=((*EventStart)>>8);
        endEvent=((*EventStart)&0x80)>>7;
        Volume=((*EventStart)&0x3F);
        Duration=EventStart[1];
#else
        value = ((*EventStart) & 0xFF);
        endEvent = ((*EventStart) & 0x8000) >> 15;
        Volume = ((*EventStart) & 0x3F00) >> 8;
        Duration = (((((WebRtc_UWord16) EventStart[1]) >> 8) & 0xFF)
            | (((WebRtc_UWord16) (EventStart[1] & 0xFF)) << 8));
#endif
        /* Only events between 0-15 are supported (DTMF tones) */
        if ((value < 0) || (value > 15))
        {
            return 0;
        }

        /* Discard all DTMF tones with really low volume (<-36dbm0) */
        if (Volume > 36)
        {
            return 0;
        }

        /*Are there any unended events of the same type? */
        for (i = 0; i < DTMFdec_inst->EventBufferSize; i++)
        {
            /* Going through the whole queue even when we have found a match will
             ensure that we add to the latest applicable event  */
            if ((DTMFdec_inst->EventQueue[i] == value) && (!DTMFdec_inst->EventQueueEnded[i]
                || endEvent)) position = i;
        }
        if (position > -1)
        {
            DTMFdec_inst->EventQueueVolume[position] = Volume;
            if ((timeStamp + Duration) > DTMFdec_inst->EventQueueEndTime[position]) DTMFdec_inst->EventQueueEndTime[position]
                = DTMFdec_inst->EventQueueStartTime[position] + Duration;
            if (endEvent) DTMFdec_inst->EventQueueEnded[position] = 1;
        }
        else
        {
            if (DTMFdec_inst->EventBufferSize == MAX_DTMF_QUEUE_SIZE)
            { /* Buffer full */
                /* Remove one event */
                DTMFdec_inst->EventBufferSize--;
            }
            /* Store data in the instance on a new position*/
            DTMFdec_inst->EventQueue[DTMFdec_inst->EventBufferSize] = value;
            DTMFdec_inst->EventQueueVolume[DTMFdec_inst->EventBufferSize] = Volume;
            DTMFdec_inst->EventQueueEnded[DTMFdec_inst->EventBufferSize] = endEvent;
            DTMFdec_inst->EventQueueStartTime[DTMFdec_inst->EventBufferSize] = timeStamp;
            DTMFdec_inst->EventQueueEndTime[DTMFdec_inst->EventBufferSize] = timeStamp
                + Duration;
            DTMFdec_inst->EventBufferSize++;
        }
        return 0;
    }
    return DTMF_INSERT_ERROR;
}

WebRtc_Word16 WebRtcNetEQ_DtmfDecode(dtmf_inst_t *DTMFdec_inst, WebRtc_Word16 *event,
                                     WebRtc_Word16 *volume, WebRtc_UWord32 currTimeStamp)
{

    if (DTMFdec_inst->EventBufferSize < 1) return 0; /* No events to play */

    /* We have events, is it time to play them? */
    if (currTimeStamp < DTMFdec_inst->EventQueueStartTime[0])
    {
        /*No, just return zero */
        return 0;
    }

    /* Continue on the event that is currently ongoing */
    *event = DTMFdec_inst->EventQueue[0];
    *volume = DTMFdec_inst->EventQueueVolume[0];

    if (DTMFdec_inst->EventQueueEndTime[0] >= (currTimeStamp + DTMFdec_inst->framelen))
    {

        /* Still at least framLen to play */

        DTMFdec_inst->CurrentPLCtime = 0;
        if ((DTMFdec_inst->EventQueueEndTime[0] == (currTimeStamp + DTMFdec_inst->framelen))
            && (DTMFdec_inst->EventQueueEnded[0]))
        { /* We are done */
            /*Remove the event from Queue*/
            WebRtcNetEQ_DtmfRemoveEvent(DTMFdec_inst);
        }
        return DTMFdec_inst->framelen;

    }
    else
    {
        if ((DTMFdec_inst->EventQueueEnded[0]) || (DTMFdec_inst->EventQueue[1] > -1))
        {
            /*
             * Less than frameLen to play and end of event or already received next event.
             * Give our a whole frame size of audio to simplify things.
             */

            /*Remove the event from Queue*/
            WebRtcNetEQ_DtmfRemoveEvent(DTMFdec_inst);
            DTMFdec_inst->CurrentPLCtime = 0;

            return DTMFdec_inst->framelen;

        }
        else
        {
            /* Less than frameLen to play and not end of event. */
            DTMFdec_inst->CurrentPLCtime = (WebRtc_Word16) (currTimeStamp
                - DTMFdec_inst->EventQueueEndTime[0]);

            if ((DTMFdec_inst->CurrentPLCtime > DTMFdec_inst->MaxPLCtime)
                || (DTMFdec_inst->CurrentPLCtime < -DTMFdec_inst->MaxPLCtime))
            {
                /*Remove the event from queue*/
                WebRtcNetEQ_DtmfRemoveEvent(DTMFdec_inst);
                DTMFdec_inst->CurrentPLCtime = 0;
            }

            /* If we have a new event that it's time to play */
            if ((DTMFdec_inst->EventQueue[1] > -1) && (DTMFdec_inst->EventQueueStartTime[1]
                >= (currTimeStamp + DTMFdec_inst->framelen)))
            {
                /*Remove the event from queue*/
                WebRtcNetEQ_DtmfRemoveEvent(DTMFdec_inst);
                DTMFdec_inst->CurrentPLCtime = 0;
            }

            return DTMFdec_inst->framelen;
        }
    }
}

#endif
