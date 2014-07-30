/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "AH263Assembler.h"

#include "ARTPSource.h"
#include "RtspPrlog.h"

#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/hexdump.h>
#include <media/stagefright/Utils.h>

namespace android {

AH263Assembler::AH263Assembler(const sp<AMessage> &notify)
    : mNotifyMsg(notify),
      mAccessUnitRTPTime(0),
      mNextExpectedSeqNoValid(false),
      mNextExpectedSeqNo(0),
      mAccessUnitDamaged(false) {
}

AH263Assembler::~AH263Assembler() {
}

ARTPAssembler::AssemblyStatus AH263Assembler::assembleMore(
        const sp<ARTPSource> &source) {
    AssemblyStatus status = addPacket(source);
    if (status == MALFORMED_PACKET) {
        mAccessUnitDamaged = true;
    }
    return status;
}

ARTPAssembler::AssemblyStatus AH263Assembler::addPacket(
        const sp<ARTPSource> &source) {
    List<sp<ABuffer> > *queue = source->queue();

    if (queue->empty()) {
        return NOT_ENOUGH_DATA;
    }

    if (mNextExpectedSeqNoValid) {
        List<sp<ABuffer> >::iterator it = queue->begin();
        while (it != queue->end()) {
            if ((uint32_t)(*it)->int32Data() >= mNextExpectedSeqNo) {
                break;
            }

            it = queue->erase(it);
        }

        if (queue->empty()) {
            return NOT_ENOUGH_DATA;
        }
    }

    sp<ABuffer> buffer = *queue->begin();

    if (!mNextExpectedSeqNoValid) {
        mNextExpectedSeqNoValid = true;
        mNextExpectedSeqNo = (uint32_t)buffer->int32Data();
    } else if ((uint32_t)buffer->int32Data() != mNextExpectedSeqNo) {
#if VERBOSE
        LOG(VERBOSE) << "Not the sequence number I expected";
#endif

        return WRONG_SEQUENCE_NUMBER;
    }

    uint32_t rtpTime;
    CHECK(buffer->meta()->findInt32("rtp-time", (int32_t *)&rtpTime));

    if (mPackets.size() > 0 && rtpTime != mAccessUnitRTPTime) {
        submitAccessUnit();
    }
    mAccessUnitRTPTime = rtpTime;

    // hexdump(buffer->data(), buffer->size());

    if (buffer->size() < 2) {
        queue->erase(queue->begin());
        ++mNextExpectedSeqNo;

        return MALFORMED_PACKET;
    }

    // RFC 4629, Sec. 5.1 General H.263+ Payload Header.
    // The H.263+ payload header is structured as follows:
    //  0                   1
    //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |   RR    |P|V|   PLEN    |PEBIT|
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //
    // RR: 5 bits
    //     Reserved bits. It SHALL be zero and MUST be ignored by receivers.
    // P: 1 bit
    //     Indicates the picture start or a picture segment (GOB/Slice) start or
    //     a video sequence end (EOS or EOSBS). Two bytes of zero bits then have
    //     to be prefixed to the payload of such a packet to compose a complete
    //     picture/GOB/slice/EOS/EOSBS start code. This bit allows the omission
    //     of the two first bytes of the start code, thus improving the
    //     compression ratio.
    // V: 1 bit
    //     Indicates the presence of an 8-bit field containing information for
    //     Video Redundancy Coding (VRC), which follows immediately after the
    //     initial 16 bits of the payload header.
    // PLEN: 6 bits
    //     Length, in bytes, of the extra picture header. If no extra picture
    //     header is attached, PLEN is 0. If PLEN>0, the extra picture header is
    //     attached immediately following the rest of the payload header. Note
    //     that the length reflects the omission of the first two bytes of the
    //     picture start code (PSC).
    // PEBIT: 3 bits
    //     Indicates the number of bits that shall be ignored in the last byte
    //     of the picture header. If PLEN is not zero, the ignored bits shall be
    //     the least significant bits of the byte. If PLEN is zero, then PEBIT
    //     shall also be zero.

    unsigned payloadHeader = U16_AT(buffer->data());
    unsigned P = (payloadHeader >> 10) & 1;
    unsigned V = (payloadHeader >> 9) & 1;
    unsigned PLEN = (payloadHeader >> 3) & 0x3f;
    unsigned PEBIT = payloadHeader & 7;

    // V = 0
    // We do not support VRC header extension for now, so just discard it if
    // present.
    if (V != 0u) {
        queue->erase(queue->begin());
        ++mNextExpectedSeqNo;
        LOGW("Packet discarded due to VRC (V != 0)");
        return MALFORMED_PACKET;
    }

    // If PLEN is zero, then PEBIT shall also be zero.
    if (PLEN == 0u && PEBIT != 0u) {
        queue->erase(queue->begin());
        ++mNextExpectedSeqNo;
        LOGW("Packet discarded (PEBIT != 0)");
        return MALFORMED_PACKET;
    }

    size_t skip = PLEN + (P ? 0: 2);

    buffer->setRange(buffer->offset() + skip, buffer->size() - skip);

    if (P) {
        buffer->data()[0] = 0x00;
        buffer->data()[1] = 0x00;
    }

    mPackets.push_back(buffer);

    queue->erase(queue->begin());
    ++mNextExpectedSeqNo;

    return OK;
}

void AH263Assembler::submitAccessUnit() {
    CHECK(!mPackets.empty());

#if VERBOSE
    LOG(VERBOSE) << "Access unit complete (" << mPackets.size() << " packets)";
#endif

    size_t totalSize = 0;
    List<sp<ABuffer> >::iterator it = mPackets.begin();
    while (it != mPackets.end()) {
        const sp<ABuffer> &unit = *it;

        totalSize += unit->size();
        ++it;
    }

    sp<ABuffer> accessUnit = new ABuffer(totalSize);
    size_t offset = 0;
    it = mPackets.begin();
    while (it != mPackets.end()) {
        const sp<ABuffer> &unit = *it;

        memcpy((uint8_t *)accessUnit->data() + offset,
               unit->data(), unit->size());

        offset += unit->size();

        ++it;
    }

    CopyTimes(accessUnit, *mPackets.begin());

#if 0
    printf(mAccessUnitDamaged ? "X" : ".");
    fflush(stdout);
#endif

    if (mAccessUnitDamaged) {
        accessUnit->meta()->setInt32("damaged", true);
    }

    mPackets.clear();
    mAccessUnitDamaged = false;

    sp<AMessage> msg = mNotifyMsg->dup();
    msg->setObject("access-unit", accessUnit);
    msg->post();
}

void AH263Assembler::packetLost() {
    CHECK(mNextExpectedSeqNoValid);
    ++mNextExpectedSeqNo;

    mAccessUnitDamaged = true;
}

void AH263Assembler::onByeReceived() {
    sp<AMessage> msg = mNotifyMsg->dup();
    msg->setInt32("eos", true);
    msg->post();
}

}  // namespace android

