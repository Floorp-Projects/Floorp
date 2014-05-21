/*
 * Copyright 2012, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MEDIA_CODEC_H_

#define MEDIA_CODEC_H_

#include <gui/IGraphicBufferProducer.h>
#include <media/hardware/CryptoAPI.h>
#include <media/stagefright/foundation/AHandler.h>
#include <utils/Vector.h>

namespace android {

struct ABuffer;
struct ACodec;
struct AMessage;
struct AString;
struct ICrypto;
struct SoftwareRenderer;
struct Surface;

struct MediaCodec : public AHandler {
    enum ConfigureFlags {
        CONFIGURE_FLAG_ENCODE   = 1,
    };

    enum BufferFlags {
        BUFFER_FLAG_SYNCFRAME   = 1,
        BUFFER_FLAG_CODECCONFIG = 2,
        BUFFER_FLAG_EOS         = 4,
    };

    static sp<MediaCodec> CreateByType(
            const sp<ALooper> &looper, const char *mime, bool encoder);

    static sp<MediaCodec> CreateByComponentName(
            const sp<ALooper> &looper, const char *name);

    status_t configure(
            const sp<AMessage> &format,
            const sp<Surface> &nativeWindow,
            const sp<ICrypto> &crypto,
            uint32_t flags);

    status_t createInputSurface(sp<IGraphicBufferProducer>* bufferProducer);

    status_t start();

    // Returns to a state in which the component remains allocated but
    // unconfigured.
    status_t stop();

    // Client MUST call release before releasing final reference to this
    // object.
    status_t release();

    status_t flush();

    status_t queueInputBuffer(
            size_t index,
            size_t offset,
            size_t size,
            int64_t presentationTimeUs,
            uint32_t flags,
            AString *errorDetailMsg = NULL);

    status_t queueSecureInputBuffer(
            size_t index,
            size_t offset,
            const CryptoPlugin::SubSample *subSamples,
            size_t numSubSamples,
            const uint8_t key[16],
            const uint8_t iv[16],
            CryptoPlugin::Mode mode,
            int64_t presentationTimeUs,
            uint32_t flags,
            AString *errorDetailMsg = NULL);

    status_t dequeueInputBuffer(size_t *index, int64_t timeoutUs = 0ll);

    status_t dequeueOutputBuffer(
            size_t *index,
            size_t *offset,
            size_t *size,
            int64_t *presentationTimeUs,
            uint32_t *flags,
            int64_t timeoutUs = 0ll);

    status_t renderOutputBufferAndRelease(size_t index);
    status_t releaseOutputBuffer(size_t index);

    status_t signalEndOfInputStream();

    status_t getOutputFormat(sp<AMessage> *format) const;

    status_t getInputBuffers(Vector<sp<ABuffer> > *buffers) const;
    status_t getOutputBuffers(Vector<sp<ABuffer> > *buffers) const;

    status_t requestIDRFrame();

    // Notification will be posted once there "is something to do", i.e.
    // an input/output buffer has become available, a format change is
    // pending, an error is pending.
    void requestActivityNotification(const sp<AMessage> &notify);

    status_t getName(AString *componentName) const;

    status_t setParameters(const sp<AMessage> &params);

protected:
    virtual ~MediaCodec();
    virtual void onMessageReceived(const sp<AMessage> &msg);

private:
    enum State {
        UNINITIALIZED,
        INITIALIZING,
        INITIALIZED,
        CONFIGURING,
        CONFIGURED,
        STARTING,
        STARTED,
        FLUSHING,
        STOPPING,
        RELEASING,
    };

    enum {
        kPortIndexInput         = 0,
        kPortIndexOutput        = 1,
    };

    enum {
        kWhatInit                           = 'init',
        kWhatConfigure                      = 'conf',
        kWhatCreateInputSurface             = 'cisf',
        kWhatStart                          = 'strt',
        kWhatStop                           = 'stop',
        kWhatRelease                        = 'rele',
        kWhatDequeueInputBuffer             = 'deqI',
        kWhatQueueInputBuffer               = 'queI',
        kWhatDequeueOutputBuffer            = 'deqO',
        kWhatReleaseOutputBuffer            = 'relO',
        kWhatSignalEndOfInputStream         = 'eois',
        kWhatGetBuffers                     = 'getB',
        kWhatFlush                          = 'flus',
        kWhatGetOutputFormat                = 'getO',
        kWhatDequeueInputTimedOut           = 'dITO',
        kWhatDequeueOutputTimedOut          = 'dOTO',
        kWhatCodecNotify                    = 'codc',
        kWhatRequestIDRFrame                = 'ridr',
        kWhatRequestActivityNotification    = 'racN',
        kWhatGetName                        = 'getN',
        kWhatSetParameters                  = 'setP',
    };

    enum {
        kFlagIsSoftwareCodec            = 1,
        kFlagOutputFormatChanged        = 2,
        kFlagOutputBuffersChanged       = 4,
        kFlagStickyError                = 8,
        kFlagDequeueInputPending        = 16,
        kFlagDequeueOutputPending       = 32,
        kFlagIsSecure                   = 64,
        kFlagSawMediaServerDie          = 128,
        kFlagIsEncoder                  = 256,
        kFlagGatherCodecSpecificData    = 512,
    };

    struct BufferInfo {
        void *mBufferID;
        sp<ABuffer> mData;
        sp<ABuffer> mEncryptedData;
        sp<AMessage> mNotify;
        bool mOwnedByClient;
    };

    State mState;
    sp<ALooper> mLooper;
    sp<ALooper> mCodecLooper;
    sp<ACodec> mCodec;
    AString mComponentName;
    uint32_t mReplyID;
    uint32_t mFlags;
    sp<Surface> mNativeWindow;
    SoftwareRenderer *mSoftRenderer;
    sp<AMessage> mOutputFormat;

    List<size_t> mAvailPortBuffers[2];
    Vector<BufferInfo> mPortBuffers[2];

    int32_t mDequeueInputTimeoutGeneration;
    uint32_t mDequeueInputReplyID;

    int32_t mDequeueOutputTimeoutGeneration;
    uint32_t mDequeueOutputReplyID;

    sp<ICrypto> mCrypto;

    List<sp<ABuffer> > mCSD;

    sp<AMessage> mActivityNotify;

    bool mHaveInputSurface;

    MediaCodec(const sp<ALooper> &looper);

    static status_t PostAndAwaitResponse(
            const sp<AMessage> &msg, sp<AMessage> *response);

    status_t init(const char *name, bool nameIsType, bool encoder);

    void setState(State newState);
    void returnBuffersToCodec();
    void returnBuffersToCodecOnPort(int32_t portIndex);
    size_t updateBuffers(int32_t portIndex, const sp<AMessage> &msg);
    status_t onQueueInputBuffer(const sp<AMessage> &msg);
    status_t onReleaseOutputBuffer(const sp<AMessage> &msg);
    ssize_t dequeuePortBuffer(int32_t portIndex);

    bool handleDequeueInputBuffer(uint32_t replyID, bool newRequest = false);
    bool handleDequeueOutputBuffer(uint32_t replyID, bool newRequest = false);
    void cancelPendingDequeueOperations();

    void extractCSD(const sp<AMessage> &format);
    status_t queueCSDInputBuffer(size_t bufferIndex);

    status_t setNativeWindow(
            const sp<Surface> &surface);

    void postActivityNotificationIfPossible();

    status_t onSetParameters(const sp<AMessage> &params);

    status_t amendOutputFormatWithCodecSpecificData(const sp<ABuffer> &buffer);

    DISALLOW_EVIL_CONSTRUCTORS(MediaCodec);
};

}  // namespace android

#endif  // MEDIA_CODEC_H_
