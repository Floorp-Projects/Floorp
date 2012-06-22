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
 * Interface for the actual packet buffer data structure.
 */

#ifndef PACKET_BUFFER_H
#define PACKET_BUFFER_H

#include "typedefs.h"

#include "webrtc_neteq.h"
#include "rtp.h"

/* Define minimum allowed buffer memory, in 16-bit words */
#define PBUFFER_MIN_MEMORY_SIZE	150

/****************************/
/* The packet buffer struct */
/****************************/

typedef struct
{

    /* Variables common to the entire buffer */
    WebRtc_UWord16 packSizeSamples; /* packet size in samples of last decoded packet */
    WebRtc_Word16 *startPayloadMemory; /* pointer to the payload memory */
    int memorySizeW16; /* the size (in WebRtc_Word16) of the payload memory */
    WebRtc_Word16 *currentMemoryPos; /* The memory position to insert next payload */
    int numPacketsInBuffer; /* The number of packets in the buffer */
    int insertPosition; /* The position to insert next packet */
    int maxInsertPositions; /* Maximum number of packets allowed */

    /* Arrays with one entry per packet slot */
    /* NOTE: If these are changed, the changes must be accounted for at the end of
     the function WebRtcNetEQ_GetDefaultCodecSettings(). */
    WebRtc_UWord32 *timeStamp; /* Timestamp in slot n */
    WebRtc_Word16 **payloadLocation; /* Memory location of payload in slot n */
    WebRtc_UWord16 *seqNumber; /* Sequence number in slot n */
    WebRtc_Word16 *payloadType; /* Payload type of packet in slot n */
    WebRtc_Word16 *payloadLengthBytes; /* Payload length of packet in slot n */
    WebRtc_Word16 *rcuPlCntr; /* zero for non-RCU payload, 1 for main payload
     2 for redundant payload */
    int *waitingTime;


    /* Statistics counter */
    WebRtc_UWord16 discardedPackets; /* Number of discarded packets */

} PacketBuf_t;

/*************************/
/* Function declarations */
/*************************/

/****************************************************************************
 * WebRtcNetEQ_PacketBufferInit(...)
 *
 * This function initializes the packet buffer.
 *
 * Input:
 *		- bufferInst	: Buffer instance to be initialized
 *		- noOfPackets	: Maximum number of packets that buffer should hold
 *		- memory		: Pointer to the storage memory for the payloads
 *		- memorySize	: The size of the payload memory (in WebRtc_Word16)
 *
 * Output:
 *      - bufferInst    : Updated buffer instance
 *
 * Return value			:  0 - Ok
 *						  <0 - Error
 */

int WebRtcNetEQ_PacketBufferInit(PacketBuf_t *bufferInst, int maxNoOfPackets,
                                 WebRtc_Word16 *pw16_memory, int memorySize);

/****************************************************************************
 * WebRtcNetEQ_PacketBufferFlush(...)
 *
 * This function flushes all the packets in the buffer.
 *
 * Input:
 *		- bufferInst	: Buffer instance to be flushed
 *
 * Output:
 *      - bufferInst    : Flushed buffer instance
 *
 * Return value			:  0 - Ok
 */

int WebRtcNetEQ_PacketBufferFlush(PacketBuf_t *bufferInst);

/****************************************************************************
 * WebRtcNetEQ_PacketBufferInsert(...)
 *
 * This function inserts an RTP packet into the packet buffer.
 *
 * Input:
 *		- bufferInst	: Buffer instance
 *		- RTPpacket		: An RTP packet struct (with payload, sequence
 *						  number, etc.)
 *
 * Output:
 *      - bufferInst    : Updated buffer instance
 *		- flushed		: 1 if buffer was flushed, 0 otherwise
 *
 * Return value			:  0 - Ok
 *						  -1 - Error
 */

int WebRtcNetEQ_PacketBufferInsert(PacketBuf_t *bufferInst, const RTPPacket_t *RTPpacket,
                                   WebRtc_Word16 *flushed);

/****************************************************************************
 * WebRtcNetEQ_PacketBufferExtract(...)
 *
 * This function extracts a payload from the buffer.
 *
 * Input:
 *		- bufferInst	: Buffer instance
 *		- bufferPosition: Position of the packet that should be extracted
 *
 * Output:
 *		- RTPpacket		: An RTP packet struct (with payload, sequence 
 *						  number, etc)
 *      - bufferInst    : Updated buffer instance
 *
 * Return value			:  0 - Ok
 *						  <0 - Error
 */

int WebRtcNetEQ_PacketBufferExtract(PacketBuf_t *bufferInst, RTPPacket_t *RTPpacket,
                                    int bufferPosition, int *waitingTime);

/****************************************************************************
 * WebRtcNetEQ_PacketBufferFindLowestTimestamp(...)
 *
 * This function finds the next packet with the lowest timestamp.
 *
 * Input:
 *       - buffer_inst        : Buffer instance.
 *       - current_time_stamp : The timestamp to compare packet timestamps with.
 *       - erase_old_packets  : If non-zero, erase packets older than currentTS.
 *
 * Output:
 *       - time_stamp         : Lowest timestamp that was found.
 *       - buffer_position    : Position of this packet (-1 if there are no
 *                              packets in the buffer).
 *       - payload_type       : Payload type of the found payload.
 *
 * Return value               :  0 - Ok;
 *                             < 0 - Error.
 */

int WebRtcNetEQ_PacketBufferFindLowestTimestamp(PacketBuf_t* buffer_inst,
                                                uint32_t current_time_stamp,
                                                uint32_t* time_stamp,
                                                int* buffer_position,
                                                int erase_old_packets,
                                                int16_t* payload_type);

/****************************************************************************
 * WebRtcNetEQ_PacketBufferGetSize(...)
 *
 * Calculate and return an estimate of the total data length (in samples)
 * currently in the buffer. The estimate is calculated as the number of
 * packets currently in the buffer (which does not have any remaining waiting
 * time), multiplied with the number of samples obtained from the last
 * decoded packet.
 *
 * Input:
 *		- bufferInst	: Buffer instance
 *
 * Return value			: The buffer size in samples
 */

WebRtc_Word32 WebRtcNetEQ_PacketBufferGetSize(const PacketBuf_t *bufferInst);

/****************************************************************************
 * WebRtcNetEQ_IncrementWaitingTimes(...)
 *
 * Increment the waiting time for all packets in the buffer by one.
 *
 * Input:
 *    - bufferInst  : Buffer instance
 *
 * Return value     : n/a
 */

void WebRtcNetEQ_IncrementWaitingTimes(PacketBuf_t *buffer_inst);

/****************************************************************************
 * WebRtcNetEQ_GetDefaultCodecSettings(...)
 *
 * Calculates a recommended buffer size for a specific set of codecs.
 *
 * Input:
 *		- codecID	    : An array of codec types that will be used
 *      - noOfCodecs    : Number of codecs in array codecID
 *
 * Output:
 *		- maxBytes	    : Recommended buffer memory size in bytes
 *      - maxSlots      : Recommended number of slots in buffer
 *
 * Return value			:  0 - Ok
 *						  <0 - Error
 */

int WebRtcNetEQ_GetDefaultCodecSettings(const enum WebRtcNetEQDecoder *codecID,
                                        int noOfCodecs, int *maxBytes, int *maxSlots);

#endif /* PACKET_BUFFER_H */
