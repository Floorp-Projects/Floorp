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
 * RTP related functions.
 */

#include "rtp.h"

#include "typedefs.h" /* to define endianness */

#include "neteq_error_codes.h"

int WebRtcNetEQ_RTPPayloadInfo(WebRtc_Word16* pw16_Datagram, int i_DatagramLen,
                               RTPPacket_t* RTPheader)
{
    int i_P, i_X, i_CC, i_startPosition;
    int i_IPver;
    int i_extlength = -1; /* Default value is there is no extension */
    int i_padlength = 0; /* Default value if there is no padding */

    if (i_DatagramLen < 12)
    {
        return RTP_TOO_SHORT_PACKET;
    }

#ifdef WEBRTC_BIG_ENDIAN
    i_IPver = (((WebRtc_UWord16) (pw16_Datagram[0] & 0xC000)) >> 14); /* Extract the version */
    i_P = (((WebRtc_UWord16) (pw16_Datagram[0] & 0x2000)) >> 13); /* Extract the P bit */
    i_X = (((WebRtc_UWord16) (pw16_Datagram[0] & 0x1000)) >> 12); /* Extract the X bit */
    i_CC = ((WebRtc_UWord16) (pw16_Datagram[0] >> 8) & 0xF); /* Get the CC number */
    RTPheader->payloadType = pw16_Datagram[0] & 0x7F; /* Get the coder type	*/
    RTPheader->seqNumber = pw16_Datagram[1]; /* Get the sequence number	*/
    RTPheader->timeStamp = ((((WebRtc_UWord32) ((WebRtc_UWord16) pw16_Datagram[2])) << 16)
        | (WebRtc_UWord16) (pw16_Datagram[3])); /* Get timestamp */
    RTPheader->ssrc = (((WebRtc_UWord32) pw16_Datagram[4]) << 16)
        + (((WebRtc_UWord32) pw16_Datagram[5])); /* Get the SSRC */

    if (i_X == 1)
    {
        /* Extension header exists. Find out how many WebRtc_Word32 it consists of. */
        i_extlength = pw16_Datagram[7 + 2 * i_CC];
    }
    if (i_P == 1)
    {
        /* Padding exists. Find out how many bytes the padding consists of. */
        if (i_DatagramLen & 0x1)
        {
            /* odd number of bytes => last byte in higher byte */
            i_padlength = (((WebRtc_UWord16) pw16_Datagram[i_DatagramLen >> 1]) >> 8);
        }
        else
        {
            /* even number of bytes => last byte in lower byte */
            i_padlength = ((pw16_Datagram[(i_DatagramLen >> 1) - 1]) & 0xFF);
        }
    }
#else /* WEBRTC_LITTLE_ENDIAN */
    i_IPver = (((WebRtc_UWord16) (pw16_Datagram[0] & 0xC0)) >> 6); /* Extract the IP version */
    i_P = (((WebRtc_UWord16) (pw16_Datagram[0] & 0x20)) >> 5); /* Extract the P bit */
    i_X = (((WebRtc_UWord16) (pw16_Datagram[0] & 0x10)) >> 4); /* Extract the X bit */
    i_CC = (WebRtc_UWord16) (pw16_Datagram[0] & 0xF); /* Get the CC number */
    RTPheader->payloadType = (pw16_Datagram[0] >> 8) & 0x7F; /* Get the coder type */
    RTPheader->seqNumber = (((((WebRtc_UWord16) pw16_Datagram[1]) >> 8) & 0xFF)
        | (((WebRtc_UWord16) (pw16_Datagram[1] & 0xFF)) << 8)); /* Get the packet number */
    RTPheader->timeStamp = ((((WebRtc_UWord16) pw16_Datagram[2]) & 0xFF) << 24)
        | ((((WebRtc_UWord16) pw16_Datagram[2]) & 0xFF00) << 8)
        | ((((WebRtc_UWord16) pw16_Datagram[3]) >> 8) & 0xFF)
        | ((((WebRtc_UWord16) pw16_Datagram[3]) & 0xFF) << 8); /* Get timestamp */
    RTPheader->ssrc = ((((WebRtc_UWord16) pw16_Datagram[4]) & 0xFF) << 24)
        | ((((WebRtc_UWord16) pw16_Datagram[4]) & 0xFF00) << 8)
        | ((((WebRtc_UWord16) pw16_Datagram[5]) >> 8) & 0xFF)
        | ((((WebRtc_UWord16) pw16_Datagram[5]) & 0xFF) << 8); /* Get the SSRC */

    if (i_X == 1)
    {
        /* Extension header exists. Find out how many WebRtc_Word32 it consists of. */
        i_extlength = (((((WebRtc_UWord16) pw16_Datagram[7 + 2 * i_CC]) >> 8) & 0xFF)
            | (((WebRtc_UWord16) (pw16_Datagram[7 + 2 * i_CC] & 0xFF)) << 8));
    }
    if (i_P == 1)
    {
        /* Padding exists. Find out how many bytes the padding consists of. */
        if (i_DatagramLen & 0x1)
        {
            /* odd number of bytes => last byte in higher byte */
            i_padlength = (pw16_Datagram[i_DatagramLen >> 1] & 0xFF);
        }
        else
        {
            /* even number of bytes => last byte in lower byte */
            i_padlength = (((WebRtc_UWord16) pw16_Datagram[(i_DatagramLen >> 1) - 1]) >> 8);
        }
    }
#endif

    i_startPosition = 12 + 4 * (i_extlength + 1) + 4 * i_CC;
    RTPheader->payload = &pw16_Datagram[i_startPosition >> 1];
    RTPheader->payloadLen = i_DatagramLen - i_startPosition - i_padlength;
    RTPheader->starts_byte1 = 0;

    if ((i_IPver != 2) || (RTPheader->payloadLen <= 0) || (RTPheader->payloadLen >= 16000)
        || (i_startPosition < 12) || (i_startPosition > i_DatagramLen))
    {
        return RTP_CORRUPT_PACKET;
    }

    return 0;
}

#ifdef NETEQ_RED_CODEC

int WebRtcNetEQ_RedundancySplit(RTPPacket_t* RTPheader[], int i_MaximumPayloads,
                                int *i_No_Of_Payloads)
{
    const WebRtc_Word16 *pw16_data = RTPheader[0]->payload; /* Pointer to the data */
    WebRtc_UWord16 uw16_offsetTimeStamp = 65535, uw16_secondPayload = 65535;
    int i_blockLength, i_k;
    int i_discardedBlockLength = 0;
    int singlePayload = 0;

#ifdef WEBRTC_BIG_ENDIAN
    if ((pw16_data[0] & 0x8000) == 0)
    {
        /* Only one payload in this packet*/
        singlePayload = 1;
        /* set the blocklength to -4 to deduce the non-existent 4-byte RED header */
        i_blockLength = -4;
        RTPheader[0]->payloadType = ((((WebRtc_UWord16)pw16_data[0]) & 0x7F00) >> 8);
    }
    else
    {
        /* Discard all but the two last payloads. */
        while (((pw16_data[2] & 0x8000) == 1)&&
            (pw16_data<((RTPheader[0]->payload)+((RTPheader[0]->payloadLen+1)>>1))))
        {
            i_discardedBlockLength += (4+(((WebRtc_UWord16)pw16_data[1]) & 0x3FF));
            pw16_data+=2;
        }
        if (pw16_data>=(RTPheader[0]->payload+((RTPheader[0]->payloadLen+1)>>1)))
        {
            return RED_SPLIT_ERROR2; /* Error, we are outside the packet */
        }
        singlePayload = 0; /* the packet contains more than one payload */
        uw16_secondPayload = ((((WebRtc_UWord16)pw16_data[0]) & 0x7F00) >> 8);
        RTPheader[0]->payloadType = ((((WebRtc_UWord16)pw16_data[2]) & 0x7F00) >> 8);
        uw16_offsetTimeStamp = ((((WebRtc_UWord16)pw16_data[0]) & 0xFF) << 6) +
        ((((WebRtc_UWord16)pw16_data[1]) & 0xFC00) >> 10);
        i_blockLength = (((WebRtc_UWord16)pw16_data[1]) & 0x3FF);
    }
#else /* WEBRTC_LITTLE_ENDIAN */
    if ((pw16_data[0] & 0x80) == 0)
    {
        /* Only one payload in this packet */
        singlePayload = 1;
        /* set the blocklength to -4 to deduce the non-existent 4-byte RED header */
        i_blockLength = -4;
        RTPheader[0]->payloadType = (((WebRtc_UWord16) pw16_data[0]) & 0x7F);
    }
    else
    {
        /* Discard all but the two last payloads. */
        while (((pw16_data[2] & 0x80) == 1) && (pw16_data < ((RTPheader[0]->payload)
            + ((RTPheader[0]->payloadLen + 1) >> 1))))
        {
            i_discardedBlockLength += (4 + ((((WebRtc_UWord16) pw16_data[1]) & 0x3) << 8)
                + ((((WebRtc_UWord16) pw16_data[1]) & 0xFF00) >> 8));
            pw16_data += 2;
        }
        if (pw16_data >= (RTPheader[0]->payload + ((RTPheader[0]->payloadLen + 1) >> 1)))
        {
            return RED_SPLIT_ERROR2; /* Error, we are outside the packet */;
        }
        singlePayload = 0; /* the packet contains more than one payload */
        uw16_secondPayload = (((WebRtc_UWord16) pw16_data[0]) & 0x7F);
        RTPheader[0]->payloadType = (((WebRtc_UWord16) pw16_data[2]) & 0x7F);
        uw16_offsetTimeStamp = ((((WebRtc_UWord16) pw16_data[0]) & 0xFF00) >> 2)
            + ((((WebRtc_UWord16) pw16_data[1]) & 0xFC) >> 2);
        i_blockLength = ((((WebRtc_UWord16) pw16_data[1]) & 0x3) << 8)
            + ((((WebRtc_UWord16) pw16_data[1]) & 0xFF00) >> 8);
    }
#endif

    if (i_MaximumPayloads < 2 || singlePayload == 1)
    {
        /* Reject the redundancy; or no redundant payload present. */
        for (i_k = 1; i_k < i_MaximumPayloads; i_k++)
        {
            RTPheader[i_k]->payloadType = -1;
            RTPheader[i_k]->payloadLen = 0;
        }

        /* update the pointer for the main data */
        pw16_data = &pw16_data[(5 + i_blockLength) >> 1];
        RTPheader[0]->starts_byte1 = (5 + i_blockLength) & 0x1;
        RTPheader[0]->payloadLen = RTPheader[0]->payloadLen - (i_blockLength + 5)
            - i_discardedBlockLength;
        RTPheader[0]->payload = pw16_data;

        *i_No_Of_Payloads = 1;

    }
    else
    {
        /* Redundancy accepted, put the redundancy in second RTPheader. */
        RTPheader[1]->payloadType = uw16_secondPayload;
        RTPheader[1]->payload = &pw16_data[5 >> 1];
        RTPheader[1]->starts_byte1 = 5 & 0x1;
        RTPheader[1]->seqNumber = RTPheader[0]->seqNumber;
        RTPheader[1]->timeStamp = RTPheader[0]->timeStamp - uw16_offsetTimeStamp;
        RTPheader[1]->ssrc = RTPheader[0]->ssrc;
        RTPheader[1]->payloadLen = i_blockLength;

        /* Modify first RTP packet, so that it contains the main data. */
        RTPheader[0]->payload = &pw16_data[(5 + i_blockLength) >> 1];
        RTPheader[0]->starts_byte1 = (5 + i_blockLength) & 0x1;
        RTPheader[0]->payloadLen = RTPheader[0]->payloadLen - (i_blockLength + 5)
            - i_discardedBlockLength;

        /* Clear the following payloads. */
        for (i_k = 2; i_k < i_MaximumPayloads; i_k++)
        {
            RTPheader[i_k]->payloadType = -1;
            RTPheader[i_k]->payloadLen = 0;
        }

        *i_No_Of_Payloads = 2;
    }
    return 0;
}

#endif

