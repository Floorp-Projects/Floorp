/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * Implementation of the actual packet buffer data structure.
 */

#include "packet_buffer.h"

#include <string.h> /* to define NULL */

#include "signal_processing_library.h"

#include "neteq_error_codes.h"

#ifdef NETEQ_DELAY_LOGGING
/* special code for offline delay logging */
#include "delay_logging.h"
#include <stdio.h>

extern FILE *delay_fid2; /* file pointer to delay log file */
extern WebRtc_UWord32 tot_received_packets;
#endif /* NETEQ_DELAY_LOGGING */


int WebRtcNetEQ_PacketBufferInit(PacketBuf_t *bufferInst, int maxNoOfPackets,
                                 WebRtc_Word16 *pw16_memory, int memorySize)
{
    int i;
    int pos = 0;

    /* Sanity check */
    if ((memorySize < PBUFFER_MIN_MEMORY_SIZE) || (pw16_memory == NULL)
        || (maxNoOfPackets < 2) || (maxNoOfPackets > 600))
    {
        /* Invalid parameters */
        return (PBUFFER_INIT_ERROR);
    }

    /* Clear the buffer instance */
    WebRtcSpl_MemSetW16((WebRtc_Word16*) bufferInst, 0,
        sizeof(PacketBuf_t) / sizeof(WebRtc_Word16));

    /* Clear the buffer memory */
    WebRtcSpl_MemSetW16((WebRtc_Word16*) pw16_memory, 0, memorySize);

    /* Set maximum number of packets */
    bufferInst->maxInsertPositions = maxNoOfPackets;

    /* Initialize array pointers */
    /* After each pointer has been set, the index pos is advanced to point immediately
     * after the the recently allocated vector. Note that one step for the pos index
     * corresponds to a WebRtc_Word16.
     */

    bufferInst->timeStamp = (WebRtc_UWord32*) &pw16_memory[pos];
    pos += maxNoOfPackets << 1; /* advance maxNoOfPackets * WebRtc_UWord32 */

    bufferInst->payloadLocation = (WebRtc_Word16**) &pw16_memory[pos];
    pos += maxNoOfPackets * (sizeof(WebRtc_Word16*) / sizeof(WebRtc_Word16)); /* advance */

    bufferInst->seqNumber = (WebRtc_UWord16*) &pw16_memory[pos];
    pos += maxNoOfPackets; /* advance maxNoOfPackets * WebRtc_UWord16 */

    bufferInst->payloadType = &pw16_memory[pos];
    pos += maxNoOfPackets; /* advance maxNoOfPackets * WebRtc_Word16 */

    bufferInst->payloadLengthBytes = &pw16_memory[pos];
    pos += maxNoOfPackets; /* advance maxNoOfPackets * WebRtc_Word16 */

    bufferInst->rcuPlCntr = &pw16_memory[pos];
    pos += maxNoOfPackets; /* advance maxNoOfPackets * WebRtc_Word16 */

    bufferInst->waitingTime = (int*) (&pw16_memory[pos]);
    /* Advance maxNoOfPackets * sizeof(waitingTime element). */
    pos += maxNoOfPackets *
        sizeof(*bufferInst->waitingTime) / sizeof(*pw16_memory);

    /* The payload memory starts after the slot arrays */
    bufferInst->startPayloadMemory = &pw16_memory[pos];
    bufferInst->currentMemoryPos = bufferInst->startPayloadMemory;
    bufferInst->memorySizeW16 = (memorySize - pos); /* Remaining memory */

    /* Initialize each payload slot as empty with infinite delay */
    for (i = 0; i < bufferInst->maxInsertPositions; i++)
    {
        bufferInst->payloadType[i] = -1;
    }

    /* Reset buffer parameters */
    bufferInst->numPacketsInBuffer = 0;
    bufferInst->packSizeSamples = 0;
    bufferInst->insertPosition = 0;

    /* Reset buffer statistics */
    bufferInst->discardedPackets = 0;

    return (0);
}


int WebRtcNetEQ_PacketBufferFlush(PacketBuf_t *bufferInst)
{
    int i;

    /* Sanity check */
    if (bufferInst->startPayloadMemory == NULL)
    {
        /* Packet buffer has not been initialized */
        /* Don't do the flushing operation, since we do not
         know the state of the struct variables */
        return (0);
    }

    /* Set all payload lengths to zero */
    WebRtcSpl_MemSetW16(bufferInst->payloadLengthBytes, 0, bufferInst->maxInsertPositions);

    /* Reset buffer variables */
    bufferInst->numPacketsInBuffer = 0;
    bufferInst->currentMemoryPos = bufferInst->startPayloadMemory;
    bufferInst->insertPosition = 0;

    /* Clear all slots, starting with the last one */
    for (i = (bufferInst->maxInsertPositions - 1); i >= 0; i--)
    {
        bufferInst->payloadType[i] = -1;
        bufferInst->timeStamp[i] = 0;
        bufferInst->seqNumber[i] = 0;
    }

    return (0);
}


int WebRtcNetEQ_PacketBufferInsert(PacketBuf_t *bufferInst, const RTPPacket_t *RTPpacket,
                                   WebRtc_Word16 *flushed)
{
    int nextPos;
    int i;

#ifdef NETEQ_DELAY_LOGGING
    /* special code for offline delay logging */
    int temp_var;
#endif /* NETEQ_DELAY_LOGGING */

    /* Initialize to "no flush" */
    *flushed = 0;

    /* Sanity check */
    if (bufferInst->startPayloadMemory == NULL)
    {
        /* packet buffer has not been initialized */
        return (-1);
    }

    /* Sanity check for payload length
     (payloadLen in bytes and memory size in WebRtc_Word16) */
    if ((RTPpacket->payloadLen > (bufferInst->memorySizeW16 << 1)) || (RTPpacket->payloadLen
        <= 0))
    {
        /* faulty or too long payload length */
        return (-1);
    }

    /* Find a position in the buffer for this packet */
    if (bufferInst->numPacketsInBuffer != 0)
    {
        /* Get the next slot */
        bufferInst->insertPosition++;
        if (bufferInst->insertPosition >= bufferInst->maxInsertPositions)
        {
            /* "Wrap around" and start from the beginning */
            bufferInst->insertPosition = 0;
        }

        /* Check if there is enough space for the new packet */
        if (bufferInst->currentMemoryPos + ((RTPpacket->payloadLen + 1) >> 1)
            >= &bufferInst->startPayloadMemory[bufferInst->memorySizeW16])
        {
            WebRtc_Word16 *tempMemAddress;

            /*
             * Payload does not fit at the end of the memory, put it in the beginning
             * instead
             */
            bufferInst->currentMemoryPos = bufferInst->startPayloadMemory;

            /*
             * Now, we must search for the next non-empty payload,
             * finding the one with the lowest start address for the payload
             */
            tempMemAddress = &bufferInst->startPayloadMemory[bufferInst->memorySizeW16];
            nextPos = -1;

            /* Loop through all slots again */
            for (i = 0; i < bufferInst->maxInsertPositions; i++)
            {
                /* Look for the non-empty slot with the lowest
                 payload location address */
                if (bufferInst->payloadLengthBytes[i] != 0 && bufferInst->payloadLocation[i]
                    < tempMemAddress)
                {
                    tempMemAddress = bufferInst->payloadLocation[i];
                    nextPos = i;
                }
            }

            /* Check that we did find a previous payload */
            if (nextPos == -1)
            {
                /* The buffer is corrupt => flush and return error */
                WebRtcNetEQ_PacketBufferFlush(bufferInst);
                *flushed = 1;
                return (-1);
            }
        }
        else
        {
            /* Payload fits at the end of memory. */

            /* Find the next non-empty slot. */
            nextPos = bufferInst->insertPosition + 1;

            /* Increase nextPos until a non-empty slot is found or end of array is encountered*/
            while ((bufferInst->payloadLengthBytes[nextPos] == 0) && (nextPos
                < bufferInst->maxInsertPositions))
            {
                nextPos++;
            }

            if (nextPos == bufferInst->maxInsertPositions)
            {
                /*
                 * Reached the end of the array, so there must be a packet in the first
                 * position instead
                 */
                nextPos = 0;

                /* Increase nextPos until a non-empty slot is found */
                while (bufferInst->payloadLengthBytes[nextPos] == 0)
                {
                    nextPos++;
                }
            }
        } /* end if-else */

        /*
         * Check if the new payload will extend into a payload later in memory.
         * If so, the buffer is full.
         */
        if ((bufferInst->currentMemoryPos <= bufferInst->payloadLocation[nextPos])
            && ((&bufferInst->currentMemoryPos[(RTPpacket->payloadLen + 1) >> 1])
                > bufferInst->payloadLocation[nextPos]))
        {
            /* Buffer is full, so the buffer must be flushed */
            WebRtcNetEQ_PacketBufferFlush(bufferInst);
            *flushed = 1;
        }

        if (bufferInst->payloadLengthBytes[bufferInst->insertPosition] != 0)
        {
            /* All positions are already taken and entire buffer should be flushed */
            WebRtcNetEQ_PacketBufferFlush(bufferInst);
            *flushed = 1;
        }

    }
    else
    {
        /* Buffer is empty, just insert the packet at the beginning */
        bufferInst->currentMemoryPos = bufferInst->startPayloadMemory;
        bufferInst->insertPosition = 0;
    }

    /* Insert packet in the found position */
    if (RTPpacket->starts_byte1 == 0)
    {
        /* Payload is 16-bit aligned => just copy it */

        WEBRTC_SPL_MEMCPY_W16(bufferInst->currentMemoryPos,
            RTPpacket->payload, (RTPpacket->payloadLen + 1) >> 1);
    }
    else
    {
        /* Payload is not 16-bit aligned => align it during copy operation */
        for (i = 0; i < RTPpacket->payloadLen; i++)
        {
            /* copy the (i+1)-th byte to the i-th byte */

            WEBRTC_SPL_SET_BYTE(bufferInst->currentMemoryPos,
                (WEBRTC_SPL_GET_BYTE(RTPpacket->payload, (i + 1))), i);
        }
    }

    /* Copy the packet information */
    bufferInst->payloadLocation[bufferInst->insertPosition] = bufferInst->currentMemoryPos;
    bufferInst->payloadLengthBytes[bufferInst->insertPosition] = RTPpacket->payloadLen;
    bufferInst->payloadType[bufferInst->insertPosition] = RTPpacket->payloadType;
    bufferInst->seqNumber[bufferInst->insertPosition] = RTPpacket->seqNumber;
    bufferInst->timeStamp[bufferInst->insertPosition] = RTPpacket->timeStamp;
    bufferInst->rcuPlCntr[bufferInst->insertPosition] = RTPpacket->rcuPlCntr;
    bufferInst->waitingTime[bufferInst->insertPosition] = 0;
    /* Update buffer parameters */
    bufferInst->numPacketsInBuffer++;
    bufferInst->currentMemoryPos += (RTPpacket->payloadLen + 1) >> 1;

#ifdef NETEQ_DELAY_LOGGING
    /* special code for offline delay logging */
    if (*flushed)
    {
        temp_var = NETEQ_DELAY_LOGGING_SIGNAL_FLUSH;
        if (fwrite(&temp_var, sizeof(int), 1, delay_fid2) != 1) {
          return -1;
        }
    }
    temp_var = NETEQ_DELAY_LOGGING_SIGNAL_RECIN;
    if ((fwrite(&temp_var, sizeof(int),
                1, delay_fid2) != 1) ||
        (fwrite(&RTPpacket->timeStamp, sizeof(WebRtc_UWord32),
                1, delay_fid2) != 1) ||
        (fwrite(&RTPpacket->seqNumber, sizeof(WebRtc_UWord16),
                1, delay_fid2) != 1) ||
        (fwrite(&RTPpacket->payloadType, sizeof(int),
                1, delay_fid2) != 1) ||
        (fwrite(&RTPpacket->payloadLen, sizeof(WebRtc_Word16),
                1, delay_fid2) != 1)) {
      return -1;
    }
    tot_received_packets++;
#endif /* NETEQ_DELAY_LOGGING */

    return (0);
}


int WebRtcNetEQ_PacketBufferExtract(PacketBuf_t *bufferInst, RTPPacket_t *RTPpacket,
                                    int bufferPosition, int *waitingTime)
{

    /* Sanity check */
    if (bufferInst->startPayloadMemory == NULL)
    {
        /* packet buffer has not been initialized */
        return (PBUFFER_NOT_INITIALIZED);
    }

    if (bufferPosition < 0 || bufferPosition >= bufferInst->maxInsertPositions)
    {
        /* buffer position is outside valid range */
        return (NETEQ_OTHER_ERROR);
    }

    /* Check that there is a valid payload in the specified position */
    if (bufferInst->payloadLengthBytes[bufferPosition] <= 0)
    {
        /* The position does not contain a valid payload */
        RTPpacket->payloadLen = 0; /* Set zero length */
        return (PBUFFER_NONEXISTING_PACKET); /* Return error */
    }

    /* Payload exists => extract payload data */

    /* Copy the actual data payload to RTP packet struct */

    WEBRTC_SPL_MEMCPY_W16((WebRtc_Word16*) RTPpacket->payload,
        bufferInst->payloadLocation[bufferPosition],
        (bufferInst->payloadLengthBytes[bufferPosition] + 1) >> 1); /*length in WebRtc_Word16*/

    /* Copy payload parameters */
    RTPpacket->payloadLen = bufferInst->payloadLengthBytes[bufferPosition];
    RTPpacket->payloadType = bufferInst->payloadType[bufferPosition];
    RTPpacket->seqNumber = bufferInst->seqNumber[bufferPosition];
    RTPpacket->timeStamp = bufferInst->timeStamp[bufferPosition];
    RTPpacket->rcuPlCntr = bufferInst->rcuPlCntr[bufferPosition];
    *waitingTime = bufferInst->waitingTime[bufferPosition];
    RTPpacket->starts_byte1 = 0; /* payload is 16-bit aligned */

    /* Clear the position in the packet buffer */
    bufferInst->payloadType[bufferPosition] = -1;
    bufferInst->payloadLengthBytes[bufferPosition] = 0;
    bufferInst->seqNumber[bufferPosition] = 0;
    bufferInst->timeStamp[bufferPosition] = 0;
    bufferInst->waitingTime[bufferPosition] = 0;
    bufferInst->payloadLocation[bufferPosition] = bufferInst->startPayloadMemory;

    /* Reduce packet counter with one */
    bufferInst->numPacketsInBuffer--;

    return (0);
}

int WebRtcNetEQ_PacketBufferFindLowestTimestamp(PacketBuf_t* buffer_inst,
                                                uint32_t current_time_stamp,
                                                uint32_t* time_stamp,
                                                int* buffer_position,
                                                int erase_old_packets,
                                                int16_t* payload_type) {
  int32_t time_stamp_diff = WEBRTC_SPL_WORD32_MAX;  /* Smallest diff found. */
  int32_t new_diff;
  int i;
  int16_t rcu_payload_cntr;

  if (buffer_inst->startPayloadMemory == NULL) {
    /* Packet buffer has not been initialized. */
    return PBUFFER_NOT_INITIALIZED;
  }

  /* Initialize all return values. */
  *time_stamp = 0;
  *payload_type = -1;  /* Indicates that no packet was found. */
  *buffer_position = -1;  /* Indicates that no packet was found. */
  rcu_payload_cntr = WEBRTC_SPL_WORD16_MAX;  /* Indicates no packet found. */

  /* Check if buffer is empty. */
  if (buffer_inst->numPacketsInBuffer <= 0) {
    return 0;
  }

  /* Loop through all slots in buffer. */
  if (erase_old_packets) {  /* If old payloads should be discarded. */
    for (i = 0; i < buffer_inst->maxInsertPositions; ++i) {
      /* Calculate difference between this slot and current_time_stamp. */
      new_diff = (int32_t)(buffer_inst->timeStamp[i] - current_time_stamp);

      /* Check if payload should be discarded. */
      if ((new_diff < 0)  /* Payload is too old */
          && (new_diff > -30000)  /* Account for TS wrap-around. */
          && (buffer_inst->payloadLengthBytes[i] > 0)) {  /* Payload exists. */
        /* Throw away old packet. */

        /* Clear the position in the buffer. */
        buffer_inst->payloadType[i] = -1;
        buffer_inst->payloadLengthBytes[i] = 0;

        /* Reduce packet counter by one. */
        buffer_inst->numPacketsInBuffer--;
        /* Increase discard counter for in-call statistics. */
        buffer_inst->discardedPackets++;
      } else if (((new_diff < time_stamp_diff) 
                  || ((new_diff == time_stamp_diff)
                      && (buffer_inst->rcuPlCntr[i] < rcu_payload_cntr)))
                      && (buffer_inst->payloadLengthBytes[i] > 0)) {
        /* New diff is smaller than previous diffs or we have a candidate with a
         * time stamp as previous candidate but better RCU-counter; 
         * and the payload exists. 
         */ 
        /* Save this position as the best candidate. */
        *buffer_position = i;
        time_stamp_diff = new_diff;
        *payload_type = buffer_inst->payloadType[i];
        rcu_payload_cntr = buffer_inst->rcuPlCntr[i];
      }
    }
  } else {
    for (i = 0; i < buffer_inst->maxInsertPositions; ++i) {
      /* Calculate difference between this slot and current_time_stamp. */
      new_diff = (int32_t)(buffer_inst->timeStamp[i] - current_time_stamp);

      /* Check if this is the oldest packet. */
      if (((new_diff < time_stamp_diff) 
           || ((new_diff == time_stamp_diff)
               && (buffer_inst->rcuPlCntr[i] < rcu_payload_cntr)))
               && (buffer_inst->payloadLengthBytes[i] > 0)) {
        /* New diff is smaller than previous diffs or we have a candidate with a
         * time_stamp as previous candidate but better RCU-counter; 
         * and the payload exists. 
         */ 
        /* Save this position as the best candidate. */
        *buffer_position = i;
        time_stamp_diff = new_diff;
        *payload_type = buffer_inst->payloadType[i];
        rcu_payload_cntr = buffer_inst->rcuPlCntr[i];
      }
    }
  }

  /* Check that we did find a real position. */
  if (*buffer_position >= 0) {
    /* Get the time_stamp for the best position. */
    *time_stamp = buffer_inst->timeStamp[*buffer_position];
  }

  return 0;
}

int WebRtcNetEQ_PacketBufferGetPacketSize(const PacketBuf_t* buffer_inst,
                                          int buffer_pos,
                                          const CodecDbInst_t* codec_database,
                                          int codec_pos, int last_duration) {
  if (codec_database->funcDurationEst[codec_pos] == NULL) {
    return last_duration;
  }
  return (*codec_database->funcDurationEst[codec_pos])(
    codec_database->codec_state[codec_pos],
    (const uint8_t *)buffer_inst->payloadLocation[buffer_pos],
    buffer_inst->payloadLengthBytes[buffer_pos]);
}

WebRtc_Word32 WebRtcNetEQ_PacketBufferGetSize(const PacketBuf_t* buffer_inst,
                                              const CodecDbInst_t*
                                              codec_database) {
  int i, count;
  int last_duration;
  int last_codec_pos;
  int last_payload_type;
  WebRtc_Word32 size_samples;

  count = 0;
  last_duration = buffer_inst->packSizeSamples;
  last_codec_pos = -1;
  last_payload_type = -1;
  size_samples = 0;

  /* Loop through all slots in the buffer */
  for (i = 0; i < buffer_inst->maxInsertPositions; i++) {
    /* Only count the packets with non-zero size */
    if (buffer_inst->payloadLengthBytes[i] != 0) {
      int payload_type;
      int codec_pos;
      /* Figure out the codec database entry for this payload_type. */
      payload_type = buffer_inst->payloadType[i];
      /* Remember the last one, to avoid the database search. */
      if(payload_type == last_payload_type) {
        codec_pos = last_codec_pos;
      }
      else {
        codec_pos = WebRtcNetEQ_DbGetCodec(codec_database,
          payload_type);
        if (codec_pos >= 0) {
          codec_pos = codec_database->position[codec_pos];
        }
      }
      last_codec_pos = codec_pos;
      last_payload_type = payload_type;
      if (codec_pos >= 0) {
        /*
         * Right now WebRtcNetEQ_PacketBufferGetPacketSize either always
         * returns last_duration or always computes the real duration without
         * looking at last_duration. If an implementation really wanted to use
         * last_duration to compute a changing duration, we would have to
         * iterate through the packets in chronological order by timestamp.
         */
         last_duration = WebRtcNetEQ_PacketBufferGetPacketSize(
           buffer_inst, i, codec_database, codec_pos,
           last_duration);
      }
      /* Add in the size of this packet. */
      size_samples += last_duration;
      count++;
    }
  }

  /* Sanity check; size cannot be negative */
  if (size_samples < 0) {
    size_samples = 0;
  }

  return size_samples;
}

void WebRtcNetEQ_IncrementWaitingTimes(PacketBuf_t *buffer_inst) {
  int i;
  /* Loop through all slots in the buffer. */
  for (i = 0; i < buffer_inst->maxInsertPositions; ++i) {
    /* Only increment waiting time for the packets with non-zero size. */
    if (buffer_inst->payloadLengthBytes[i] != 0) {
      buffer_inst->waitingTime[i]++;
    }
  }
}

int WebRtcNetEQ_GetDefaultCodecSettings(const enum WebRtcNetEQDecoder *codecID,
                                        int noOfCodecs, int *maxBytes, int *maxSlots)
{
    int i;
    int ok = 0;
    WebRtc_Word16 w16_tmp;
    WebRtc_Word16 codecBytes;
    WebRtc_Word16 codecBuffers;

    /* Initialize return variables to zero */
    *maxBytes = 0;
    *maxSlots = 0;

    /* Loop through all codecs supplied to function */
    for (i = 0; i < noOfCodecs; i++)
    {
        /* Find current codec and set parameters accordingly */

        if ((codecID[i] == kDecoderPCMu) || (codecID[i] == kDecoderPCMu_2ch))
        {
            codecBytes = 1680; /* Up to 210ms @ 64kbps */
            codecBuffers = 30; /* Down to 5ms frames */
        }
        else if ((codecID[i] == kDecoderPCMa) ||
            (codecID[i] == kDecoderPCMa_2ch))
        {
            codecBytes = 1680; /* Up to 210ms @ 64kbps */
            codecBuffers = 30; /* Down to 5ms frames */
        }
        else if (codecID[i] == kDecoderILBC)
        {
            codecBytes = 380; /* 200ms @ 15.2kbps (20ms frames) */
            codecBuffers = 10;
        }
        else if (codecID[i] == kDecoderISAC)
        {
            codecBytes = 960; /* 240ms @ 32kbps (60ms frames) */
            codecBuffers = 8;
        }
        else if ((codecID[i] == kDecoderISACswb) ||
            (codecID[i] == kDecoderISACfb))
        {
            codecBytes = 1560; /* 240ms @ 52kbps (30ms frames) */
            codecBuffers = 8;
        }
        else if (codecID[i] == kDecoderOpus)
        {
            codecBytes = 15300; /* 240ms @ 510kbps (60ms frames) */
            codecBuffers = 30;  /* Replicating the value for PCMu/a */
        }
        else if ((codecID[i] == kDecoderPCM16B) ||
            (codecID[i] == kDecoderPCM16B_2ch))
        {
            codecBytes = 3360; /* 210ms */
            codecBuffers = 15;
        }
        else if ((codecID[i] == kDecoderPCM16Bwb) ||
            (codecID[i] == kDecoderPCM16Bwb_2ch))
        {
            codecBytes = 6720; /* 210ms */
            codecBuffers = 15;
        }
        else if ((codecID[i] == kDecoderPCM16Bswb32kHz) ||
            (codecID[i] == kDecoderPCM16Bswb32kHz_2ch))
        {
            codecBytes = 13440; /* 210ms */
            codecBuffers = 15;
        }
        else if (codecID[i] == kDecoderPCM16Bswb48kHz)
        {
            codecBytes = 20160; /* 210ms */
            codecBuffers = 15;
        }
        else if ((codecID[i] == kDecoderG722) ||
            (codecID[i] == kDecoderG722_2ch))
        {
            codecBytes = 1680; /* 210ms @ 64kbps */
            codecBuffers = 15;
        }
        else if (codecID[i] == kDecoderRED)
        {
            codecBytes = 0; /* Should not be max... */
            codecBuffers = 0;
        }
        else if (codecID[i] == kDecoderAVT)
        {
            codecBytes = 0; /* Should not be max... */
            codecBuffers = 0;
        }
        else if (codecID[i] == kDecoderCNG)
        {
            codecBytes = 0; /* Should not be max... */
            codecBuffers = 0;
        }
        else if (codecID[i] == kDecoderG729)
        {
            codecBytes = 210; /* 210ms @ 8kbps */
            codecBuffers = 20; /* max 200ms supported for 10ms frames */
        }
        else if (codecID[i] == kDecoderG729_1)
        {
            codecBytes = 840; /* 210ms @ 32kbps */
            codecBuffers = 10; /* max 200ms supported for 20ms frames */
        }
        else if (codecID[i] == kDecoderG726_16)
        {
            codecBytes = 400; /* 200ms @ 16kbps */
            codecBuffers = 10;
        }
        else if (codecID[i] == kDecoderG726_24)
        {
            codecBytes = 600; /* 200ms @ 24kbps */
            codecBuffers = 10;
        }
        else if (codecID[i] == kDecoderG726_32)
        {
            codecBytes = 800; /* 200ms @ 32kbps */
            codecBuffers = 10;
        }
        else if (codecID[i] == kDecoderG726_40)
        {
            codecBytes = 1000; /* 200ms @ 40kbps */
            codecBuffers = 10;
        }
        else if (codecID[i] == kDecoderG722_1_16)
        {
            codecBytes = 420; /* 210ms @ 16kbps */
            codecBuffers = 10;
        }
        else if (codecID[i] == kDecoderG722_1_24)
        {
            codecBytes = 630; /* 210ms @ 24kbps */
            codecBuffers = 10;
        }
        else if (codecID[i] == kDecoderG722_1_32)
        {
            codecBytes = 840; /* 210ms @ 32kbps */
            codecBuffers = 10;
        }
        else if (codecID[i] == kDecoderG722_1C_24)
        {
            codecBytes = 630; /* 210ms @ 24kbps */
            codecBuffers = 10;
        }
        else if (codecID[i] == kDecoderG722_1C_32)
        {
            codecBytes = 840; /* 210ms @ 32kbps */
            codecBuffers = 10;
        }
        else if (codecID[i] == kDecoderG722_1C_48)
        {
            codecBytes = 1260; /* 210ms @ 48kbps */
            codecBuffers = 10;
        }
        else if (codecID[i] == kDecoderSPEEX_8)
        {
            codecBytes = 1250; /* 210ms @ 50kbps */
            codecBuffers = 10;
        }
        else if (codecID[i] == kDecoderSPEEX_16)
        {
            codecBytes = 1250; /* 210ms @ 50kbps */
            codecBuffers = 10;
        }
        else if ((codecID[i] == kDecoderCELT_32) ||
            (codecID[i] == kDecoderCELT_32_2ch))
        {
            codecBytes = 1250; /* 210ms @ 50kbps */
            codecBuffers = 10;
        }
        else if (codecID[i] == kDecoderGSMFR)
        {
            codecBytes = 340; /* 200ms */
            codecBuffers = 10;
        }
        else if (codecID[i] == kDecoderAMR)
        {
            codecBytes = 384; /* 240ms @ 12.2kbps+headers (60ms frames) */
            codecBuffers = 10;
        }
        else if (codecID[i] == kDecoderAMRWB)
        {
            codecBytes = 744;
            codecBuffers = 10;
        }
        else if (codecID[i] == kDecoderArbitrary)
        {
            codecBytes = 6720; /* Assume worst case uncompressed WB 210ms */
            codecBuffers = 15;
        }
        else
        {
            /*Unknow codec */
            codecBytes = 0;
            codecBuffers = 0;
            ok = CODEC_DB_UNKNOWN_CODEC;
        }

        /* Update max variables */
        *maxBytes = WEBRTC_SPL_MAX((*maxBytes), codecBytes);
        *maxSlots = WEBRTC_SPL_MAX((*maxSlots), codecBuffers);

    } /* end of for loop */

    /*
     * Add size needed by the additional pointers for each slot inside struct,
     * as indicated on each line below.
     */
    w16_tmp = (sizeof(WebRtc_UWord32) /* timeStamp */
    + sizeof(WebRtc_Word16*) /* payloadLocation */
    + sizeof(WebRtc_UWord16) /* seqNumber */
    + sizeof(WebRtc_Word16)  /* payloadType */
    + sizeof(WebRtc_Word16)  /* payloadLengthBytes */
    + sizeof(WebRtc_Word16)  /* rcuPlCntr   */
    + sizeof(int));          /* waitingTime */
    /* Add the extra size per slot to the memory count */
    *maxBytes += w16_tmp * (*maxSlots);

    return ok;
}
