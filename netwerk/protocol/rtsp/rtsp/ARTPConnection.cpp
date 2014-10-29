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

#define LOG_TAG "ARTPConnection"
#include "RtspPrlog.h"

#include "ARTPConnection.h"

#include "ARTPSource.h"
#include "ASessionDescription.h"

#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/AString.h>
#include <media/stagefright/foundation/hexdump.h>

#include <arpa/inet.h>

#include "mozilla/NullPtr.h"
#include "mozilla/mozalloc.h"
#include "nsTArray.h"
#include "prnetdb.h"
#include "prerr.h"
#include "prerror.h"
#include "NetworkActivityMonitor.h"

using namespace mozilla::net;

namespace android {

static const size_t kMaxUDPSize = 1500;

static uint16_t u16at(const uint8_t *data) {
    return data[0] << 8 | data[1];
}

static uint32_t u32at(const uint8_t *data) {
    return u16at(data) << 16 | u16at(&data[2]);
}

static uint64_t u64at(const uint8_t *data) {
    return (uint64_t)(u32at(data)) << 32 | u32at(&data[4]);
}

// static
const uint32_t ARTPConnection::kSocketPollTimeoutUs = 1000ll;

struct ARTPConnection::StreamInfo {
    PRFileDesc *mRTPSocket;
    PRFileDesc *mRTCPSocket;
    int mInterleavedRTPIdx;
    int mInterleavedRTCPIdx;
    sp<ASessionDescription> mSessionDesc;
    size_t mIndex;
    sp<AMessage> mNotifyMsg;
    KeyedVector<uint32_t, sp<ARTPSource> > mSources;

    int64_t mNumRTCPPacketsReceived;
    int64_t mNumRTPPacketsReceived;
    PRNetAddr mRemoteRTCPAddr;

    bool mIsInjected;
};

ARTPConnection::ARTPConnection(uint32_t flags)
    : mFlags(flags),
      mPollEventPending(false),
      mLastReceiverReportTimeUs(-1) {
}

ARTPConnection::~ARTPConnection() {
}

void ARTPConnection::addStream(
        PRFileDesc *rtpSocket, PRFileDesc *rtcpSocket,
        int interleavedRTPIdx, int interleavedRTCPIdx,
        const sp<ASessionDescription> &sessionDesc,
        size_t index,
        const sp<AMessage> &notify,
        bool injected) {
    sp<AMessage> msg = new AMessage(kWhatAddStream, id());
    msg->setPointer("rtp-socket", rtpSocket);
    msg->setPointer("rtcp-socket", rtcpSocket);
    msg->setInt32("interleaved-rtp", interleavedRTPIdx);
    msg->setInt32("interleaved-rtcp", interleavedRTCPIdx);
    msg->setObject("session-desc", sessionDesc);
    msg->setSize("index", index);
    msg->setMessage("notify", notify);
    msg->setInt32("injected", injected);
    msg->post();
}

void ARTPConnection::removeStream(PRFileDesc *rtpSocket, PRFileDesc *rtcpSocket) {
    sp<AMessage> msg = new AMessage(kWhatRemoveStream, id());
    msg->setPointer("rtp-socket", rtpSocket);
    msg->setPointer("rtcp-socket", rtcpSocket);

    // Since the caller will close the sockets after this function
    // returns, we need to use a blocking post to prevent from polling
    // closed sockets.
    sp<AMessage> response;
    msg->postAndAwaitResponse(&response);
}

static void bumpSocketBufferSize(PRFileDesc *s) {
    uint32_t size = 256 * 1024;
    PRSocketOptionData opt;

    opt.option = PR_SockOpt_RecvBufferSize;
    opt.value.recv_buffer_size = size;
    CHECK_EQ(PR_SetSocketOption(s, &opt), PR_SUCCESS);
}

// static
void ARTPConnection::MakePortPair(
        PRFileDesc **rtpSocket, PRFileDesc **rtcpSocket, uint16_t *rtpPort) {
    *rtpSocket = PR_OpenUDPSocket(PR_AF_INET);
    if (!*rtpSocket) {
        TRESPASS();
    }

    bumpSocketBufferSize(*rtpSocket);

    *rtcpSocket = PR_OpenUDPSocket(PR_AF_INET);
    if (!*rtcpSocket) {
        TRESPASS();
    }

    bumpSocketBufferSize(*rtcpSocket);

    NetworkActivityMonitor::AttachIOLayer(*rtpSocket);
    NetworkActivityMonitor::AttachIOLayer(*rtcpSocket);

    // Reduce the chance of using duplicate port numbers.
    srand(time(NULL));
    // rand() * 1000 may overflow int type, use long long.
    unsigned start = (unsigned)((rand() * 1000ll) / RAND_MAX) + 15550;
    start &= ~1;

    for (uint32_t port = start; port < 65536; port += 2) {
        PRNetAddr addr;
        addr.inet.family = PR_AF_INET;
        addr.inet.ip = PR_htonl(PR_INADDR_ANY);
        addr.inet.port = PR_htons(port);

        if (PR_Bind(*rtpSocket, &addr) == PR_FAILURE) {
            continue;
        }

        addr.inet.port = PR_htons(port + 1);

        if (PR_Bind(*rtcpSocket, &addr) == PR_SUCCESS) {
            *rtpPort = port;
            return;
        }
    }

    TRESPASS();
}

void ARTPConnection::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
        case kWhatAddStream:
        {
            onAddStream(msg);
            break;
        }

        case kWhatRemoveStream:
        {
            onRemoveStream(msg);
            sp<AMessage> ack = new AMessage;
            uint32_t replyID;
            CHECK(msg->senderAwaitsResponse(&replyID));
            ack->postReply(replyID);
            break;
        }

        case kWhatPollStreams:
        {
            onPollStreams();
            break;
        }

        case kWhatInjectPacket:
        {
            onInjectPacket(msg);
            break;
        }

        default:
        {
            TRESPASS();
            break;
        }
    }
}

void ARTPConnection::onAddStream(const sp<AMessage> &msg) {
    mStreams.push_back(StreamInfo());
    StreamInfo *info = &*--mStreams.end();

    void *s;
    CHECK(msg->findPointer("rtp-socket", &s));
    info->mRTPSocket = (PRFileDesc*)s;
    CHECK(msg->findPointer("rtcp-socket", &s));
    info->mRTCPSocket = (PRFileDesc*)s;

    CHECK(msg->findInt32("interleaved-rtp", &info->mInterleavedRTPIdx));
    CHECK(msg->findInt32("interleaved-rtcp", &info->mInterleavedRTCPIdx));

    int32_t injected;
    CHECK(msg->findInt32("injected", &injected));

    info->mIsInjected = injected;

    sp<RefBase> obj;
    CHECK(msg->findObject("session-desc", &obj));
    info->mSessionDesc = static_cast<ASessionDescription *>(obj.get());

    CHECK(msg->findSize("index", &info->mIndex));
    CHECK(msg->findMessage("notify", &info->mNotifyMsg));

    info->mNumRTCPPacketsReceived = 0;
    info->mNumRTPPacketsReceived = 0;
    PR_InitializeNetAddr(PR_IpAddrNull, 0, &info->mRemoteRTCPAddr);

    if (!injected) {
        postPollEvent();
    }
}

void ARTPConnection::onRemoveStream(const sp<AMessage> &msg) {
    PRFileDesc *rtpSocket = nullptr, *rtcpSocket = nullptr;
    void *s;
    CHECK(msg->findPointer("rtp-socket", &s));
    rtpSocket = (PRFileDesc*)s;
    CHECK(msg->findPointer("rtcp-socket", &s));
    rtcpSocket = (PRFileDesc*)s;

    List<StreamInfo>::iterator it = mStreams.begin();
    while (it != mStreams.end()
           && (it->mRTPSocket != rtpSocket || it->mRTCPSocket != rtcpSocket)) {
        ++it;
    }

    if (it == mStreams.end()) {
        return;
    }

    mStreams.erase(it);
}

void ARTPConnection::postPollEvent() {
    if (mPollEventPending) {
        return;
    }

    sp<AMessage> msg = new AMessage(kWhatPollStreams, id());
    msg->post();

    mPollEventPending = true;
}

void ARTPConnection::onPollStreams() {
    mPollEventPending = false;

    if (mStreams.empty()) {
        return;
    }

    uint32_t pollCount = mStreams.size() * 2;
    nsTArray<PRPollDesc> pollList;
    pollList.AppendElements(pollCount);
    memset(pollList.Elements(), 0, sizeof(PRPollDesc) * pollCount);

    // |pollIndex| is used to map different RTP & RTCP socket pairs.
    uint32_t numSocketsToPoll = 0, pollIndex = 0;
    for (List<StreamInfo>::iterator it = mStreams.begin();
         it != mStreams.end(); ++it, pollIndex += 2) {
        if (pollIndex >= pollCount) {
            // |pollIndex| should never equal or exceed |pollCount|.
            TRESPASS();
        }

        if ((*it).mIsInjected) {
            continue;
        }

        if (it->mRTPSocket) {
            pollList[pollIndex].fd = it->mRTPSocket;
            pollList[pollIndex].in_flags = PR_POLL_READ;
            pollList[pollIndex].out_flags = 0;
            numSocketsToPoll++;
        }
        if (it->mRTCPSocket) {
            pollList[pollIndex + 1].fd = it->mRTCPSocket;
            pollList[pollIndex + 1].in_flags = PR_POLL_READ;
            pollList[pollIndex + 1].out_flags = 0;
            numSocketsToPoll++;
        }
    }

    if (numSocketsToPoll == 0) {
        // No sockets need to poll. return.
        return;
    }

    const int32_t numSocketsReadyToRead =
        PR_Poll(pollList.Elements(), pollList.Length(),
                PR_MicrosecondsToInterval(kSocketPollTimeoutUs));

    if (numSocketsReadyToRead > 0) {
        pollIndex = 0;
        List<StreamInfo>::iterator it = mStreams.begin();
        while (it != mStreams.end()) {
            if ((*it).mIsInjected) {
                ++it;
                pollIndex += 2;
                continue;
            }

            status_t err = OK;
            if (pollList[pollIndex].out_flags != 0) {
                err = receive(&*it, true);
            }
            if (err == OK && pollList[pollIndex + 1].out_flags != 0) {
                err = receive(&*it, false);
            }

            if (err == -ECONNRESET) {
                // socket failure, this stream is dead, Jim.

                LOGW("failed to receive RTP/RTCP datagram.");
                it = mStreams.erase(it);
                pollIndex += 2;
                continue;
            }

            ++it;
            pollIndex += 2;
        }
    }

    int64_t nowUs = ALooper::GetNowUs();
    if (mLastReceiverReportTimeUs <= 0
            || mLastReceiverReportTimeUs + 5000000ll <= nowUs) {
        sp<ABuffer> buffer = new ABuffer(kMaxUDPSize);
        List<StreamInfo>::iterator it = mStreams.begin();
        while (it != mStreams.end()) {
            StreamInfo *s = &*it;

            if (s->mIsInjected) {
                ++it;
                continue;
            }

            if (s->mNumRTCPPacketsReceived == 0) {
                // We have never received any RTCP packets on this stream,
                // we don't even know where to send a report.
                ++it;
                continue;
            }

            buffer->setRange(0, 0);

            for (size_t i = 0; i < s->mSources.size(); ++i) {
                sp<ARTPSource> source = s->mSources.valueAt(i);

                source->addReceiverReport(buffer);

                if (mFlags & kRegularlyRequestFIR) {
                    source->addFIR(buffer);
                }
            }

            if (buffer->size() > 0) {
                LOGV("Sending RR...");

                ssize_t n;
                PRErrorCode errorCode;
                do {
                    n = PR_SendTo(s->mRTCPSocket, buffer->data(), buffer->size(),
                                  0, &s->mRemoteRTCPAddr, PR_INTERVAL_NO_WAIT);
                    errorCode = PR_GetError();
                } while (n < 0 && errorCode == PR_PENDING_INTERRUPT_ERROR);

                if (n <= 0) {
                    LOGW("failed to send RTCP receiver report (%s).",
                         n == 0 ? "connection gone" : "interrupt error");

                    it = mStreams.erase(it);
                    continue;
                }

                CHECK_EQ(n, (ssize_t)buffer->size());

                mLastReceiverReportTimeUs = nowUs;
            }

            ++it;
        }
    }

    if (!mStreams.empty()) {
        postPollEvent();
    }
}

status_t ARTPConnection::receive(StreamInfo *s, bool receiveRTP) {
    LOGV("receiving %s", receiveRTP ? "RTP" : "RTCP");

    CHECK(!s->mIsInjected);

    sp<ABuffer> buffer = new ABuffer(65536);

    int32_t remoteAddrLen =
        (!receiveRTP && s->mNumRTCPPacketsReceived == 0)
            ? sizeof(s->mRemoteRTCPAddr) : 0;

    ssize_t nbytes;
    PRErrorCode errorCode;
    do {
        nbytes = PR_RecvFrom(receiveRTP ? s->mRTPSocket : s->mRTCPSocket,
                             buffer->data(),
                             buffer->size(),
                             0,
                             remoteAddrLen > 0 ? &s->mRemoteRTCPAddr : NULL,
                             PR_INTERVAL_NO_WAIT);
        errorCode = PR_GetError();
    } while (nbytes < 0 && errorCode == PR_PENDING_INTERRUPT_ERROR);

    if (nbytes <= 0) {
        return -PR_CONNECT_RESET_ERROR;
    }

    buffer->setRange(0, nbytes);

    // LOGI("received %d bytes.", buffer->size());

    status_t err;
    if (receiveRTP) {
        err = parseRTP(s, buffer);
    } else {
        err = parseRTCP(s, buffer);
    }

    return err;
}

status_t ARTPConnection::parseRTP(StreamInfo *s, const sp<ABuffer> &buffer) {
    if (s->mNumRTPPacketsReceived++ == 0) {
        sp<AMessage> notify = s->mNotifyMsg->dup();
        notify->setInt32("first-rtp", true);
        notify->post();
    }

    size_t size = buffer->size();

    if (size < 12) {
        // Too short to be a valid RTP header.
        return -1;
    }

    const uint8_t *data = buffer->data();

    if ((data[0] >> 6) != 2) {
        // Unsupported version.
        return -1;
    }

    if (data[0] & 0x20) {
        // Padding present.

        size_t paddingLength = data[size - 1];

        if (paddingLength + 12 > size) {
            // If we removed this much padding we'd end up with something
            // that's too short to be a valid RTP header.
            return -1;
        }

        size -= paddingLength;
    }

    int numCSRCs = data[0] & 0x0f;

    size_t payloadOffset = 12 + 4 * numCSRCs;

    if (size < payloadOffset) {
        // Not enough data to fit the basic header and all the CSRC entries.
        return -1;
    }

    if (data[0] & 0x10) {
        // Header eXtension present.

        if (size < payloadOffset + 4) {
            // Not enough data to fit the basic header, all CSRC entries
            // and the first 4 bytes of the extension header.

            return -1;
        }

        const uint8_t *extensionData = &data[payloadOffset];

        size_t extensionLength =
            4 * (extensionData[2] << 8 | extensionData[3]);

        if (size < payloadOffset + 4 + extensionLength) {
            return -1;
        }

        payloadOffset += 4 + extensionLength;
    }

    uint32_t srcId = u32at(&data[8]);

    sp<ARTPSource> source = findSource(s, srcId);

    uint32_t rtpTime = u32at(&data[4]);

    sp<AMessage> meta = buffer->meta();
    meta->setInt32("ssrc", srcId);
    meta->setInt32("rtp-time", rtpTime);
    meta->setInt32("PT", data[1] & 0x7f);
    meta->setInt32("M", data[1] >> 7);

    buffer->setInt32Data(u16at(&data[2]));
    buffer->setRange(payloadOffset, size - payloadOffset);

    source->processRTPPacket(buffer);

    return OK;
}

status_t ARTPConnection::parseRTCP(StreamInfo *s, const sp<ABuffer> &buffer) {
    if (s->mNumRTCPPacketsReceived++ == 0) {
        sp<AMessage> notify = s->mNotifyMsg->dup();
        notify->setInt32("first-rtcp", true);
        notify->post();
    }

    const uint8_t *data = buffer->data();
    size_t size = buffer->size();

    while (size > 0) {
        if (size < 8) {
            // Too short to be a valid RTCP header
            return -1;
        }

        if ((data[0] >> 6) != 2) {
            // Unsupported version.
            return -1;
        }

        if (data[0] & 0x20) {
            // Padding present.

            size_t paddingLength = data[size - 1];

            if (paddingLength + 12 > size) {
                // If we removed this much padding we'd end up with something
                // that's too short to be a valid RTP header.
                return -1;
            }

            size -= paddingLength;
        }

        size_t headerLength = 4 * (data[2] << 8 | data[3]) + 4;

        if (size < headerLength) {
            // Only received a partial packet?
            return -1;
        }

        switch (data[1]) {
            case 200:
            {
                parseSR(s, data, headerLength);
                break;
            }

            case 201:  // RR
            case 202:  // SDES
            case 204:  // APP
                break;

            case 205:  // TSFB (transport layer specific feedback)
            case 206:  // PSFB (payload specific feedback)
                // hexdump(data, headerLength);
                break;

            case 203:
            {
                parseBYE(s, data, headerLength);
                break;
            }

            default:
            {
                LOGW("Unknown RTCP packet type %u of size %d",
                     (unsigned)data[1], headerLength);
                break;
            }
        }

        data += headerLength;
        size -= headerLength;
    }

    return OK;
}

status_t ARTPConnection::parseBYE(
        StreamInfo *s, const uint8_t *data, size_t size) {
    size_t SC = data[0] & 0x3f;

    if (SC == 0 || size < (4 + SC * 4)) {
        // Packet too short for the minimal BYE header.
        return -1;
    }

    uint32_t id = u32at(&data[4]);

    sp<ARTPSource> source = findSource(s, id);

    source->byeReceived();

    return OK;
}

status_t ARTPConnection::parseSR(
        StreamInfo *s, const uint8_t *data, size_t size) {
    size_t RC = data[0] & 0x1f;

    if (size < (7 + RC * 6) * 4) {
        // Packet too short for the minimal SR header.
        return -1;
    }

    uint32_t id = u32at(&data[4]);
    uint64_t ntpTime = u64at(&data[8]);
    uint32_t rtpTime = u32at(&data[16]);

#if 0
    LOGI("XXX timeUpdate: ssrc=0x%08x, rtpTime %u == ntpTime %.3f",
         id,
         rtpTime,
         (ntpTime >> 32) + (double)(ntpTime & 0xffffffff) / (1ll << 32));
#endif

    sp<ARTPSource> source = findSource(s, id);

    source->timeUpdate(rtpTime, ntpTime);

    return 0;
}

sp<ARTPSource> ARTPConnection::findSource(StreamInfo *info, uint32_t srcId) {
    sp<ARTPSource> source;
    ssize_t index = info->mSources.indexOfKey(srcId);
    if (index < 0) {
        index = info->mSources.size();

        source = new ARTPSource(
                srcId, info->mSessionDesc, info->mIndex, info->mNotifyMsg);

        info->mSources.add(srcId, source);
    } else {
        source = info->mSources.valueAt(index);
    }

    return source;
}

void ARTPConnection::injectPacket(int index, const sp<ABuffer> &buffer) {
    sp<AMessage> msg = new AMessage(kWhatInjectPacket, id());
    msg->setInt32("index", index);
    msg->setObject("buffer", buffer);
    msg->post();
}

void ARTPConnection::onInjectPacket(const sp<AMessage> &msg) {
    int32_t index;
    CHECK(msg->findInt32("index", &index));

    sp<RefBase> obj;
    CHECK(msg->findObject("buffer", &obj));

    sp<ABuffer> buffer = static_cast<ABuffer *>(obj.get());

    List<StreamInfo>::iterator it = mStreams.begin();
    while (it != mStreams.end()
           && it->mInterleavedRTPIdx != index && it->mInterleavedRTCPIdx != index) {
        ++it;
    }

    if (it == mStreams.end()) {
        TRESPASS();
    }

    StreamInfo *s = &*it;

    status_t err;
    if (it->mInterleavedRTPIdx == index) {
        err = parseRTP(s, buffer);
    } else {
        err = parseRTCP(s, buffer);
    }
}

}  // namespace android
