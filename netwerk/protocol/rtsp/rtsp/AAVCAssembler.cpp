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

#define LOG_TAG "AAVCAssembler"
#include "RtspPrlog.h"

#include "AAVCAssembler.h"

#include "ARTPSource.h"

#include "mozilla/Assertions.h"

#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/hexdump.h>

#include <stdint.h>

namespace android {

// static
AAVCAssembler::AAVCAssembler(const sp<AMessage> &notify)
    : mNotifyMsg(notify),
      mAccessUnitRTPTime(0),
      mNextExpectedSeqNoValid(false),
      mNextExpectedSeqNo(0),
      mAccessUnitDamaged(false) {
}

AAVCAssembler::~AAVCAssembler() {
}

ARTPAssembler::AssemblyStatus AAVCAssembler::addNALUnit(
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
        LOGV("Not the sequence number I expected");

        return WRONG_SEQUENCE_NUMBER;
    }

    const uint8_t *data = buffer->data();
    size_t size = buffer->size();

    if (size < 1 || (data[0] & 0x80)) {
        // Corrupt.

        LOGW("Ignoring corrupt buffer.");
        queue->erase(queue->begin());

        ++mNextExpectedSeqNo;
        return MALFORMED_PACKET;
    }

    unsigned nalType = data[0] & 0x1f;
    if (nalType >= 1 && nalType <= 23) {
        bool success = addSingleNALUnit(buffer);
        queue->erase(queue->begin());
        ++mNextExpectedSeqNo;

        return success ? OK : MALFORMED_PACKET;
    } else if (nalType == 28) {
        // FU-A
        return addFragmentedNALUnit(queue);
    } else if (nalType == 24) {
        // STAP-A
        bool success = addSingleTimeAggregationPacket(buffer);
        queue->erase(queue->begin());
        ++mNextExpectedSeqNo;

        return success ? OK : MALFORMED_PACKET;
    } else {
        LOGV("Ignoring unsupported buffer (nalType=%d)", nalType);

        queue->erase(queue->begin());
        ++mNextExpectedSeqNo;

        return MALFORMED_PACKET;
    }
}

bool AAVCAssembler::addSingleNALUnit(const sp<ABuffer> &buffer) {
    LOGV("addSingleNALUnit of size %d", buffer->size());
#if !LOG_NDEBUG
    hexdump(buffer->data(), buffer->size());
#endif

    uint32_t rtpTime;
    if (!buffer->meta()->findInt32("rtp-time", (int32_t *)&rtpTime)) {
        LOGW("Cannot find rtp-time");
        return false;
    }

    if (!mNALUnits.empty() && rtpTime != mAccessUnitRTPTime) {
        if (!submitAccessUnit()) {
            LOGW("Cannot find rtp-time. Malformed packet.");

            return false;
        }
    }
    mAccessUnitRTPTime = rtpTime;

    mNALUnits.push_back(buffer);
    return true;
}

bool AAVCAssembler::addSingleTimeAggregationPacket(const sp<ABuffer> &buffer) {
    const uint8_t *data = buffer->data();
    size_t size = buffer->size();

    if (size < 3) {
        LOGV("Discarding too small STAP-A packet.");
        return false;
    }

    ++data;
    --size;
    while (size >= 2) {
        size_t nalSize = (data[0] << 8) | data[1];

        if (size < nalSize + 2) {
            LOGV("Discarding malformed STAP-A packet.");
            return false;
        }

        sp<ABuffer> unit = new ABuffer(nalSize);
        memcpy(unit->data(), &data[2], nalSize);

        if (!CopyTimes(unit, buffer)) {
            return false;
        }

        if (!addSingleNALUnit(unit)) {
            LOGW("addSingleNALUnit() failed");
            return false;
        }

        data += 2 + nalSize;
        size -= 2 + nalSize;
    }

    if (size != 0) {
        LOGV("Unexpected padding at end of STAP-A packet.");
    }

    return true;
}

ARTPAssembler::AssemblyStatus AAVCAssembler::addFragmentedNALUnit(
        List<sp<ABuffer> > *queue) {
    MOZ_ASSERT(!queue->empty());

    sp<ABuffer> buffer = *queue->begin();
    const uint8_t *data = buffer->data();
    size_t size = buffer->size();

    if (size <= 0) {
        LOGW("Buffer is empty");

        queue->erase(queue->begin());
        ++mNextExpectedSeqNo;
        return MALFORMED_PACKET;
    }
    unsigned indicator = data[0];

    if ((indicator & 0x1f) != 28) {
        LOGW("Indicator is wrong");

        queue->erase(queue->begin());
        ++mNextExpectedSeqNo;
        return MALFORMED_PACKET;
    }

    if (size < 2) {
        LOGW("Ignoring malformed FU buffer (size = %d)", size);

        queue->erase(queue->begin());
        ++mNextExpectedSeqNo;
        return MALFORMED_PACKET;
    }

    if (!(data[1] & 0x80)) {
        // Start bit not set on the first buffer.

        LOGW("Start bit not set on first buffer");

        queue->erase(queue->begin());
        ++mNextExpectedSeqNo;
        return MALFORMED_PACKET;
    }

    uint32_t nalType = data[1] & 0x1f;
    uint32_t nri = (data[0] >> 5) & 3;

    uint32_t expectedSeqNo = (uint32_t)buffer->int32Data() + 1;
    size_t totalSize = size - 2;
    size_t totalCount = 1;
    bool complete = false;

    if (data[1] & 0x40) {
        // Huh? End bit also set on the first buffer.

        LOGV("Grrr. This isn't fragmented at all.");

        complete = true;
    } else {
        List<sp<ABuffer> >::iterator it = ++queue->begin();
        while (it != queue->end()) {
            LOGV("sequence length %d", totalCount);

            const sp<ABuffer> &buffer = *it;

            const uint8_t *data = buffer->data();
            size_t size = buffer->size();

            if ((uint32_t)buffer->int32Data() != expectedSeqNo) {
                LOGV("sequence not complete, expected seqNo %d, got %d",
                     expectedSeqNo, (uint32_t)buffer->int32Data());

                return WRONG_SEQUENCE_NUMBER;
            }

            if (size < 2
                    || data[0] != indicator
                    || (data[1] & 0x1f) != nalType
                    || (data[1] & 0x80)) {
                LOGW("Ignoring malformed FU buffer.");

                // Delete the whole start of the FU.

                it = queue->begin();
                for (size_t i = 0; i <= totalCount; ++i) {
                    it = queue->erase(it);
                }

                mNextExpectedSeqNo = expectedSeqNo + 1;

                return MALFORMED_PACKET;
            }

            totalSize += size - 2;
            ++totalCount;

            expectedSeqNo = expectedSeqNo + 1;

            if (data[1] & 0x40) {
                // This is the last fragment.
                complete = true;
                break;
            }

            ++it;
        }
    }

    if (!complete) {
        return NOT_ENOUGH_DATA;
    }

    mNextExpectedSeqNo = expectedSeqNo;

    // We found all the fragments that make up the complete NAL unit.

    // Leave room for the header. So far totalSize did not include the
    // header byte.
    ++totalSize;

    sp<ABuffer> unit = new ABuffer(totalSize);
    if (!CopyTimes(unit, *queue->begin())) {
        return MALFORMED_PACKET;
    }

    unit->data()[0] = (nri << 5) | nalType;

    size_t offset = 1;
    List<sp<ABuffer> >::iterator it = queue->begin();
    for (size_t i = 0; i < totalCount; ++i) {
        const sp<ABuffer> &buffer = *it;

        LOGV("piece #%d/%d", i + 1, totalCount);
#if !LOG_NDEBUG
        hexdump(buffer->data(), buffer->size());
#endif

        memcpy(unit->data() + offset, buffer->data() + 2, buffer->size() - 2);
        offset += buffer->size() - 2;

        it = queue->erase(it);
    }

    unit->setRange(0, totalSize);

    if (!addSingleNALUnit(unit)) {
        return MALFORMED_PACKET;
    }

    LOGV("successfully assembled a NAL unit from fragments.");

    return OK;
}

bool AAVCAssembler::submitAccessUnit() {
    MOZ_ASSERT(!mNALUnits.empty());

    LOGV("Access unit complete (%d nal units)", mNALUnits.size());

    size_t totalSize = 0;
    for (List<sp<ABuffer> >::iterator it = mNALUnits.begin();
         it != mNALUnits.end(); ++it) {
        totalSize += 4 + (*it)->size();
    }

    sp<ABuffer> accessUnit = new ABuffer(totalSize);
    size_t offset = 0;
    for (List<sp<ABuffer> >::iterator it = mNALUnits.begin();
         it != mNALUnits.end(); ++it) {
        memcpy(accessUnit->data() + offset, "\x00\x00\x00\x01", 4);
        offset += 4;

        sp<ABuffer> nal = *it;
        memcpy(accessUnit->data() + offset, nal->data(), nal->size());
        offset += nal->size();
    }

    if (!CopyTimes(accessUnit, *mNALUnits.begin())) {
        return false;
    }

#if 0
    printf(mAccessUnitDamaged ? "X" : ".");
    fflush(stdout);
#endif

    if (mAccessUnitDamaged) {
        accessUnit->meta()->setInt32("damaged", true);
    }

    mNALUnits.clear();
    mAccessUnitDamaged = false;

    sp<AMessage> msg = mNotifyMsg->dup();
    msg->setObject("access-unit", accessUnit);
    msg->post();
    return true;
}

ARTPAssembler::AssemblyStatus AAVCAssembler::assembleMore(
        const sp<ARTPSource> &source) {
    AssemblyStatus status = addNALUnit(source);
    if (status == MALFORMED_PACKET) {
        mAccessUnitDamaged = true;
    }
    return status;
}

void AAVCAssembler::packetLost() {
    CHECK(mNextExpectedSeqNoValid);
    LOGV("packetLost (expected %d)", mNextExpectedSeqNo);

    ++mNextExpectedSeqNo;

    mAccessUnitDamaged = true;
}

void AAVCAssembler::onByeReceived() {
    sp<AMessage> msg = mNotifyMsg->dup();
    msg->setInt32("eos", true);
    msg->post();
}

}  // namespace android
