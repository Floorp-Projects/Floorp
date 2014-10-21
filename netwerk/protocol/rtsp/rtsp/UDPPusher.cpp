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

#define LOG_TAG "UDPPusher"
#include <utils/Log.h>

#include "UDPPusher.h"

#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <utils/ByteOrder.h>

#include "mozilla/NullPtr.h"
#include "prnetdb.h"
#include "prerr.h"
#include "NetworkActivityMonitor.h"

using namespace mozilla::net;

namespace android {

UDPPusher::UDPPusher(const char *filename, unsigned port)
    : mFile(fopen(filename, "rb")),
      mFirstTimeMs(0),
      mFirstTimeUs(0) {
    CHECK(mFile != NULL);

    mSocket = PR_OpenUDPSocket(PR_AF_INET);
    if (!mSocket) {
        TRESPASS();
    }

    NetworkActivityMonitor::AttachIOLayer(mSocket);

    PRNetAddr addr;
    addr.inet.family = PR_AF_INET;
    addr.inet.ip = PR_htonl(PR_INADDR_ANY);
    addr.inet.port = PR_htons(0);

    CHECK_EQ(PR_SUCCESS, PR_Bind(mSocket, &addr));

    mRemoteAddr.inet.family = PR_AF_INET;
    mRemoteAddr.inet.ip = PR_htonl(PR_INADDR_ANY);
    mRemoteAddr.inet.port = PR_htons(port);
}

UDPPusher::~UDPPusher() {
    if (mSocket) {
        PR_Close(mSocket);
        mSocket = nullptr;
    }

    fclose(mFile);
    mFile = NULL;
}

void UDPPusher::start() {
    uint32_t timeMs;
    CHECK_EQ(fread(&timeMs, 1, sizeof(timeMs), mFile), sizeof(timeMs));
    mFirstTimeMs = fromlel(timeMs);
    mFirstTimeUs = ALooper::GetNowUs();

    (new AMessage(kWhatPush, id()))->post();
}

bool UDPPusher::onPush() {
    uint32_t length;
    if (fread(&length, 1, sizeof(length), mFile) < sizeof(length)) {
        LOGI("No more data to push.");
        return false;
    }

    length = fromlel(length);

    if (length <= 0u) {
        LOGE("Zero length");
        return false;
    }

    sp<ABuffer> buffer = new ABuffer(length);
    if (fread(buffer->data(), 1, length, mFile) < length) {
        LOGE("File truncated?.");
        return false;
    }

    ssize_t n = PR_SendTo(
            mSocket, buffer->data(), buffer->size(), 0,
            &mRemoteAddr, PR_INTERVAL_NO_WAIT);

    CHECK_EQ(n, (ssize_t)buffer->size());
    if (n != (ssize_t)buffer->size()) {
        LOGE("Sizes don't match");
        return false;
    }

    uint32_t timeMs;
    if (fread(&timeMs, 1, sizeof(timeMs), mFile) < sizeof(timeMs)) {
        LOGI("No more data to push.");
        return false;
    }

    timeMs = fromlel(timeMs);
    if (timeMs < mFirstTimeMs) {
        LOGE("Time is wrong");
        return false;
    }

    timeMs -= mFirstTimeMs;
    int64_t whenUs = mFirstTimeUs + timeMs * 1000ll;
    int64_t nowUs = ALooper::GetNowUs();
    (new AMessage(kWhatPush, id()))->post(whenUs - nowUs);

    return true;
}

void UDPPusher::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
        case kWhatPush:
        {
            if (!onPush() && !(ntohs(mRemoteAddr.sin_port) & 1)) {
                LOGI("emulating BYE packet");

                sp<ABuffer> buffer = new ABuffer(8);
                uint8_t *data = buffer->data();
                *data++ = (2 << 6) | 1;
                *data++ = 203;
                *data++ = 0;
                *data++ = 1;
                *data++ = 0x8f;
                *data++ = 0x49;
                *data++ = 0xc0;
                *data++ = 0xd0;
                buffer->setRange(0, 8);

                struct sockaddr_in tmp = mRemoteAddr;
                tmp.sin_port = htons(ntohs(mRemoteAddr.sin_port) | 1);

                ssize_t n = PR_SendTo(
                        mSocket, buffer->data(), buffer->size(), 0,
                        &tmp, PR_INTERVAL_NO_WAIT);

                MOZ_ASSERT(n, (ssize_t)buffer->size());
            }
            break;
        }

        default:
            TRESPASS();
            break;
    }
}

}  // namespace android

