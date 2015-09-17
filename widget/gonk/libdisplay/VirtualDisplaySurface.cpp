/*
 * Copyright 2013 The Android Open Source Project
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

// #define LOG_NDEBUG 0
#include "VirtualDisplaySurface.h"

// ---------------------------------------------------------------------------
namespace android {
// ---------------------------------------------------------------------------

#if defined(FORCE_HWC_COPY_FOR_VIRTUAL_DISPLAYS)
static const bool sForceHwcCopy = true;
#else
static const bool sForceHwcCopy = false;
#endif

#define VDS_LOGE(msg, ...) ALOGE("[%s] " msg, \
        mDisplayName.string(), ##__VA_ARGS__)
#define VDS_LOGW_IF(cond, msg, ...) ALOGW_IF(cond, "[%s] " msg, \
        mDisplayName.string(), ##__VA_ARGS__)
#define VDS_LOGV(msg, ...) ALOGV("[%s] " msg, \
        mDisplayName.string(), ##__VA_ARGS__)

__attribute__((unused))
static const char* dbgCompositionTypeStr(DisplaySurface::CompositionType type) {
    switch (type) {
        case DisplaySurface::COMPOSITION_UNKNOWN: return "UNKNOWN";
        case DisplaySurface::COMPOSITION_GLES:    return "GLES";
        case DisplaySurface::COMPOSITION_HWC:     return "HWC";
        case DisplaySurface::COMPOSITION_MIXED:   return "MIXED";
        default:                                  return "<INVALID>";
    }
}

VirtualDisplaySurface::VirtualDisplaySurface(int32_t dispId,
        const sp<IGraphicBufferProducer>& sink,
        const sp<IGraphicBufferProducer>& bqProducer,
        const sp<StreamConsumer>& bqConsumer,
        const String8& name)
:   DisplaySurface(bqConsumer),
    mDisplayId(dispId),
    mDisplayName(name),
    mOutputUsage(GRALLOC_USAGE_HW_COMPOSER),
    mProducerSlotSource(0),
    mDbgState(DBG_STATE_IDLE),
    mDbgLastCompositionType(COMPOSITION_UNKNOWN),
    mMustRecompose(false)
{
    mSource[SOURCE_SINK] = sink;
    mSource[SOURCE_SCRATCH] = bqProducer;

    resetPerFrameState();

    int sinkWidth, sinkHeight;
    sink->query(NATIVE_WINDOW_WIDTH, &sinkWidth);
    sink->query(NATIVE_WINDOW_HEIGHT, &sinkHeight);
    mSinkBufferWidth = sinkWidth;
    mSinkBufferHeight = sinkHeight;

    // Pick the buffer format to request from the sink when not rendering to it
    // with GLES. If the consumer needs CPU access, use the default format
    // set by the consumer. Otherwise allow gralloc to decide the format based
    // on usage bits.
    int sinkUsage;
    sink->query(NATIVE_WINDOW_CONSUMER_USAGE_BITS, &sinkUsage);
    if (sinkUsage & (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK)) {
        int sinkFormat;
        sink->query(NATIVE_WINDOW_FORMAT, &sinkFormat);
        mDefaultOutputFormat = sinkFormat;
    } else {
        mDefaultOutputFormat = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
    }
    mOutputFormat = mDefaultOutputFormat;

    ConsumerBase::mName = String8::format("VDS: %s", mDisplayName.string());
    mConsumer->setConsumerName(ConsumerBase::mName);
    mConsumer->setConsumerUsageBits(GRALLOC_USAGE_HW_COMPOSER);
    mConsumer->setDefaultBufferSize(sinkWidth, sinkHeight);
    mConsumer->setDefaultMaxBufferCount(2);
}

VirtualDisplaySurface::~VirtualDisplaySurface() {
}

status_t VirtualDisplaySurface::beginFrame(bool mustRecompose) {
    if (mDisplayId < 0)
        return NO_ERROR;

    mMustRecompose = mustRecompose;

    VDS_LOGW_IF(mDbgState != DBG_STATE_IDLE,
            "Unexpected beginFrame() in %s state", dbgStateStr());
    mDbgState = DBG_STATE_BEGUN;

    return refreshOutputBuffer();
}

status_t VirtualDisplaySurface::prepareFrame(CompositionType compositionType) {
    if (mDisplayId < 0)
        return NO_ERROR;

    VDS_LOGW_IF(mDbgState != DBG_STATE_BEGUN,
            "Unexpected prepareFrame() in %s state", dbgStateStr());
    mDbgState = DBG_STATE_PREPARED;

    mCompositionType = compositionType;
    if (sForceHwcCopy && mCompositionType == COMPOSITION_GLES) {
        // Some hardware can do RGB->YUV conversion more efficiently in hardware
        // controlled by HWC than in hardware controlled by the video encoder.
        // Forcing GLES-composed frames to go through an extra copy by the HWC
        // allows the format conversion to happen there, rather than passing RGB
        // directly to the consumer.
        //
        // On the other hand, when the consumer prefers RGB or can consume RGB
        // inexpensively, this forces an unnecessary copy.
        mCompositionType = COMPOSITION_MIXED;
    }

    if (mCompositionType != mDbgLastCompositionType) {
        VDS_LOGV("prepareFrame: composition type changed to %s",
                dbgCompositionTypeStr(mCompositionType));
        mDbgLastCompositionType = mCompositionType;
    }

    if (mCompositionType != COMPOSITION_GLES &&
            (mOutputFormat != mDefaultOutputFormat ||
             mOutputUsage != GRALLOC_USAGE_HW_COMPOSER)) {
        // We must have just switched from GLES-only to MIXED or HWC
        // composition. Stop using the format and usage requested by the GLES
        // driver; they may be suboptimal when HWC is writing to the output
        // buffer. For example, if the output is going to a video encoder, and
        // HWC can write directly to YUV, some hardware can skip a
        // memory-to-memory RGB-to-YUV conversion step.
        //
        // If we just switched *to* GLES-only mode, we'll change the
        // format/usage and get a new buffer when the GLES driver calls
        // dequeueBuffer().
        mOutputFormat = mDefaultOutputFormat;
        mOutputUsage = GRALLOC_USAGE_HW_COMPOSER;
        refreshOutputBuffer();
    }

    return NO_ERROR;
}

status_t VirtualDisplaySurface::compositionComplete() {
    return NO_ERROR;
}

status_t VirtualDisplaySurface::advanceFrame() {
    return NO_ERROR;

// XXX Add HWC support

#if 0
    if (mDisplayId < 0)
        return NO_ERROR;

    if (mCompositionType == COMPOSITION_HWC) {
        VDS_LOGW_IF(mDbgState != DBG_STATE_PREPARED,
                "Unexpected advanceFrame() in %s state on HWC frame",
                dbgStateStr());
    } else {
        VDS_LOGW_IF(mDbgState != DBG_STATE_GLES_DONE,
                "Unexpected advanceFrame() in %s state on GLES/MIXED frame",
                dbgStateStr());
    }
    mDbgState = DBG_STATE_HWC;

    if (mOutputProducerSlot < 0 ||
            (mCompositionType != COMPOSITION_HWC && mFbProducerSlot < 0)) {
        // Last chance bailout if something bad happened earlier. For example,
        // in a GLES configuration, if the sink disappears then dequeueBuffer
        // will fail, the GLES driver won't queue a buffer, but SurfaceFlinger
        // will soldier on. So we end up here without a buffer. There should
        // be lots of scary messages in the log just before this.
        VDS_LOGE("advanceFrame: no buffer, bailing out");
        return NO_MEMORY;
    }

    sp<GraphicBuffer> fbBuffer = mFbProducerSlot >= 0 ?
            mProducerBuffers[mFbProducerSlot] : sp<GraphicBuffer>(NULL);
    sp<GraphicBuffer> outBuffer = mProducerBuffers[mOutputProducerSlot];
    VDS_LOGV("advanceFrame: fb=%d(%p) out=%d(%p)",
            mFbProducerSlot, fbBuffer.get(),
            mOutputProducerSlot, outBuffer.get());

    // At this point we know the output buffer acquire fence,
    // so update HWC state with it.
    mHwc.setOutputBuffer(mDisplayId, mOutputFence, outBuffer);

    status_t result = NO_ERROR;
    if (fbBuffer != NULL) {
        result = mHwc.fbPost(mDisplayId, mFbFence, fbBuffer);
    }

    return result;
#endif
}

void VirtualDisplaySurface::onFrameCommitted() {
    return;

// XXX Add HWC support

#if 0
    if (mDisplayId < 0)
        return;

    VDS_LOGW_IF(mDbgState != DBG_STATE_HWC,
            "Unexpected onFrameCommitted() in %s state", dbgStateStr());
    mDbgState = DBG_STATE_IDLE;

    sp<Fence> fbFence = mHwc.getAndResetReleaseFence(mDisplayId);
    if (mCompositionType == COMPOSITION_MIXED && mFbProducerSlot >= 0) {
        // release the scratch buffer back to the pool
        Mutex::Autolock lock(mMutex);
        int sslot = mapProducer2SourceSlot(SOURCE_SCRATCH, mFbProducerSlot);
        VDS_LOGV("onFrameCommitted: release scratch sslot=%d", sslot);
        addReleaseFenceLocked(sslot, mProducerBuffers[mFbProducerSlot], fbFence);
        releaseBufferLocked(sslot, mProducerBuffers[mFbProducerSlot],
                EGL_NO_DISPLAY, EGL_NO_SYNC_KHR);
    }

    if (mOutputProducerSlot >= 0) {
        int sslot = mapProducer2SourceSlot(SOURCE_SINK, mOutputProducerSlot);
        QueueBufferOutput qbo;
        sp<Fence> outFence = mHwc.getLastRetireFence(mDisplayId);
        VDS_LOGV("onFrameCommitted: queue sink sslot=%d", sslot);
        if (mMustRecompose) {
            status_t result = mSource[SOURCE_SINK]->queueBuffer(sslot,
                    QueueBufferInput(
                        systemTime(), false /* isAutoTimestamp */,
                        Rect(mSinkBufferWidth, mSinkBufferHeight),
                        NATIVE_WINDOW_SCALING_MODE_FREEZE, 0 /* transform */,
                        true /* async*/,
                        outFence),
                    &qbo);
            if (result == NO_ERROR) {
                updateQueueBufferOutput(qbo);
            }
        } else {
            // If the surface hadn't actually been updated, then we only went
            // through the motions of updating the display to keep our state
            // machine happy. We cancel the buffer to avoid triggering another
            // re-composition and causing an infinite loop.
            mSource[SOURCE_SINK]->cancelBuffer(sslot, outFence);
        }
    }

    resetPerFrameState();
#endif
}

void VirtualDisplaySurface::resizeBuffers(const uint32_t w, const uint32_t h) {
    uint32_t tmpW, tmpH, transformHint, numPendingBuffers;
    mQueueBufferOutput.deflate(&tmpW, &tmpH, &transformHint, &numPendingBuffers);
    mQueueBufferOutput.inflate(w, h, transformHint, numPendingBuffers);

    mSinkBufferWidth = w;
    mSinkBufferHeight = h;
}

status_t VirtualDisplaySurface::requestBuffer(int pslot,
        sp<GraphicBuffer>* outBuf) {
    if (mDisplayId < 0)
        return mSource[SOURCE_SINK]->requestBuffer(pslot, outBuf);

    VDS_LOGW_IF(mDbgState != DBG_STATE_GLES,
            "Unexpected requestBuffer pslot=%d in %s state",
            pslot, dbgStateStr());

    *outBuf = mProducerBuffers[pslot];
    return NO_ERROR;
}

status_t VirtualDisplaySurface::setBufferCount(int bufferCount) {
    return mSource[SOURCE_SINK]->setBufferCount(bufferCount);
}

status_t VirtualDisplaySurface::dequeueBuffer(Source source,
        uint32_t format, uint32_t usage, int* sslot, sp<Fence>* fence) {
    LOG_FATAL_IF(mDisplayId < 0, "mDisplayId=%d but should not be < 0.", mDisplayId);
    // Don't let a slow consumer block us
    bool async = (source == SOURCE_SINK);

    status_t result = mSource[source]->dequeueBuffer(sslot, fence, async,
            mSinkBufferWidth, mSinkBufferHeight, format, usage);
    if (result < 0)
        return result;
    int pslot = mapSource2ProducerSlot(source, *sslot);
    VDS_LOGV("dequeueBuffer(%s): sslot=%d pslot=%d result=%d",
            dbgSourceStr(source), *sslot, pslot, result);
    uint64_t sourceBit = static_cast<uint64_t>(source) << pslot;

    if ((mProducerSlotSource & (1ULL << pslot)) != sourceBit) {
        // This slot was previously dequeued from the other source; must
        // re-request the buffer.
        result |= BUFFER_NEEDS_REALLOCATION;
        mProducerSlotSource &= ~(1ULL << pslot);
        mProducerSlotSource |= sourceBit;
    }

    if (result & RELEASE_ALL_BUFFERS) {
        for (uint32_t i = 0; i < BufferQueue::NUM_BUFFER_SLOTS; i++) {
            if ((mProducerSlotSource & (1ULL << i)) == sourceBit)
                mProducerBuffers[i].clear();
        }
    }
    if (result & BUFFER_NEEDS_REALLOCATION) {
        result = mSource[source]->requestBuffer(*sslot, &mProducerBuffers[pslot]);
        if (result < 0) {
            mProducerBuffers[pslot].clear();
            mSource[source]->cancelBuffer(*sslot, *fence);
            return result;
        }
        VDS_LOGV("dequeueBuffer(%s): buffers[%d]=%p fmt=%d usage=%#x",
                dbgSourceStr(source), pslot, mProducerBuffers[pslot].get(),
                mProducerBuffers[pslot]->getPixelFormat(),
                mProducerBuffers[pslot]->getUsage());
    }

    return result;
}

status_t VirtualDisplaySurface::dequeueBuffer(int* pslot, sp<Fence>* fence, bool async,
        uint32_t w, uint32_t h, uint32_t format, uint32_t usage) {
    if (mDisplayId < 0)
        return mSource[SOURCE_SINK]->dequeueBuffer(pslot, fence, async, w, h, format, usage);

    VDS_LOGW_IF(mDbgState != DBG_STATE_PREPARED,
            "Unexpected dequeueBuffer() in %s state", dbgStateStr());
    mDbgState = DBG_STATE_GLES;

    VDS_LOGW_IF(!async, "EGL called dequeueBuffer with !async despite eglSwapInterval(0)");
    VDS_LOGV("dequeueBuffer %dx%d fmt=%d usage=%#x", w, h, format, usage);

    status_t result = NO_ERROR;
    Source source = fbSourceForCompositionType(mCompositionType);

    if (source == SOURCE_SINK) {

        if (mOutputProducerSlot < 0) {
            // Last chance bailout if something bad happened earlier. For example,
            // in a GLES configuration, if the sink disappears then dequeueBuffer
            // will fail, the GLES driver won't queue a buffer, but SurfaceFlinger
            // will soldier on. So we end up here without a buffer. There should
            // be lots of scary messages in the log just before this.
            VDS_LOGE("dequeueBuffer: no buffer, bailing out");
            return NO_MEMORY;
        }

        // We already dequeued the output buffer. If the GLES driver wants
        // something incompatible, we have to cancel and get a new one. This
        // will mean that HWC will see a different output buffer between
        // prepare and set, but since we're in GLES-only mode already it
        // shouldn't matter.

        usage |= GRALLOC_USAGE_HW_COMPOSER;
        const sp<GraphicBuffer>& buf = mProducerBuffers[mOutputProducerSlot];
        if ((usage & ~buf->getUsage()) != 0 ||
                (format != 0 && format != (uint32_t)buf->getPixelFormat()) ||
                (w != 0 && w != mSinkBufferWidth) ||
                (h != 0 && h != mSinkBufferHeight)) {
            VDS_LOGV("dequeueBuffer: dequeueing new output buffer: "
                    "want %dx%d fmt=%d use=%#x, "
                    "have %dx%d fmt=%d use=%#x",
                    w, h, format, usage,
                    mSinkBufferWidth, mSinkBufferHeight,
                    buf->getPixelFormat(), buf->getUsage());
            mOutputFormat = format;
            mOutputUsage = usage;
            result = refreshOutputBuffer();
            if (result < 0)
                return result;
        }
    }

    if (source == SOURCE_SINK) {
        *pslot = mOutputProducerSlot;
        *fence = mOutputFence;
    } else {
        int sslot;
        result = dequeueBuffer(source, format, usage, &sslot, fence);
        if (result >= 0) {
            *pslot = mapSource2ProducerSlot(source, sslot);
        }
    }
    return result;
}

status_t VirtualDisplaySurface::detachBuffer(int /* slot */) {
    VDS_LOGE("detachBuffer is not available for VirtualDisplaySurface");
    return INVALID_OPERATION;
}

status_t VirtualDisplaySurface::detachNextBuffer(
        sp<GraphicBuffer>* /* outBuffer */, sp<Fence>* /* outFence */) {
    VDS_LOGE("detachNextBuffer is not available for VirtualDisplaySurface");
    return INVALID_OPERATION;
}

status_t VirtualDisplaySurface::attachBuffer(int* /* outSlot */,
        const sp<GraphicBuffer>& /* buffer */) {
    VDS_LOGE("attachBuffer is not available for VirtualDisplaySurface");
    return INVALID_OPERATION;
}

status_t VirtualDisplaySurface::queueBuffer(int pslot,
        const QueueBufferInput& input, QueueBufferOutput* output) {
    if (mDisplayId < 0)
        return mSource[SOURCE_SINK]->queueBuffer(pslot, input, output);

    VDS_LOGW_IF(mDbgState != DBG_STATE_GLES,
            "Unexpected queueBuffer(pslot=%d) in %s state", pslot,
            dbgStateStr());
    mDbgState = DBG_STATE_GLES_DONE;

    VDS_LOGV("queueBuffer pslot=%d", pslot);

    status_t result;
    if (mCompositionType == COMPOSITION_MIXED) {
        // Queue the buffer back into the scratch pool
        QueueBufferOutput scratchQBO;
        int sslot = mapProducer2SourceSlot(SOURCE_SCRATCH, pslot);
        result = mSource[SOURCE_SCRATCH]->queueBuffer(sslot, input, &scratchQBO);
        if (result != NO_ERROR)
            return result;

        // Now acquire the buffer from the scratch pool -- should be the same
        // slot and fence as we just queued.
        Mutex::Autolock lock(mMutex);
        BufferQueue::BufferItem item;
        result = acquireBufferLocked(&item, 0);
        if (result != NO_ERROR)
            return result;
        VDS_LOGW_IF(item.mBuf != sslot,
                "queueBuffer: acquired sslot %d from SCRATCH after queueing sslot %d",
                item.mBuf, sslot);
        mFbProducerSlot = mapSource2ProducerSlot(SOURCE_SCRATCH, item.mBuf);
        mFbFence = mSlots[item.mBuf].mFence;

    } else {
        LOG_FATAL_IF(mCompositionType != COMPOSITION_GLES,
                "Unexpected queueBuffer in state %s for compositionType %s",
                dbgStateStr(), dbgCompositionTypeStr(mCompositionType));

        // Extract the GLES release fence for HWC to acquire
        int64_t timestamp;
        bool isAutoTimestamp;
        Rect crop;
        int scalingMode;
        uint32_t transform;
        bool async;
        input.deflate(&timestamp, &isAutoTimestamp, &crop, &scalingMode,
                &transform, &async, &mFbFence);

        mFbProducerSlot = pslot;
        mOutputFence = mFbFence;
    }

    *output = mQueueBufferOutput;
    return NO_ERROR;
}

void VirtualDisplaySurface::cancelBuffer(int pslot, const sp<Fence>& fence) {
    if (mDisplayId < 0)
        return mSource[SOURCE_SINK]->cancelBuffer(mapProducer2SourceSlot(SOURCE_SINK, pslot), fence);

    VDS_LOGW_IF(mDbgState != DBG_STATE_GLES,
            "Unexpected cancelBuffer(pslot=%d) in %s state", pslot,
            dbgStateStr());
    VDS_LOGV("cancelBuffer pslot=%d", pslot);
    Source source = fbSourceForCompositionType(mCompositionType);
    return mSource[source]->cancelBuffer(
            mapProducer2SourceSlot(source, pslot), fence);
}

int VirtualDisplaySurface::query(int what, int* value) {
    switch (what) {
        case NATIVE_WINDOW_WIDTH:
            *value = mSinkBufferWidth;
            break;
        case NATIVE_WINDOW_HEIGHT:
            *value = mSinkBufferHeight;
            break;
        default:
            return mSource[SOURCE_SINK]->query(what, value);
    }
    return NO_ERROR;
}

#if ANDROID_VERSION >= 21
status_t VirtualDisplaySurface::connect(const sp<IProducerListener>& listener,
        int api, bool producerControlledByApp,
        QueueBufferOutput* output) {
    QueueBufferOutput qbo;
    status_t result = mSource[SOURCE_SINK]->connect(listener, api,
            producerControlledByApp, &qbo);
    if (result == NO_ERROR) {
        updateQueueBufferOutput(qbo);
        *output = mQueueBufferOutput;
    }
    return result;
}
#else
status_t VirtualDisplaySurface::connect(const sp<IBinder>& token,
        int api, bool producerControlledByApp,
        QueueBufferOutput* output) {
    QueueBufferOutput qbo;
    status_t result = mSource[SOURCE_SINK]->connect(token, api, producerControlledByApp, &qbo);
    if (result == NO_ERROR) {
        updateQueueBufferOutput(qbo);
        *output = mQueueBufferOutput;
    }
    return result;
}
#endif

status_t VirtualDisplaySurface::disconnect(int api) {
    return mSource[SOURCE_SINK]->disconnect(api);
}

#if ANDROID_VERSION >= 21
status_t VirtualDisplaySurface::setSidebandStream(const sp<NativeHandle>& /*stream*/) {
    return INVALID_OPERATION;
}
#endif

void VirtualDisplaySurface::allocateBuffers(bool /* async */,
        uint32_t /* width */, uint32_t /* height */, uint32_t /* format */,
        uint32_t /* usage */) {
    // TODO: Should we actually allocate buffers for a virtual display?
}

void VirtualDisplaySurface::updateQueueBufferOutput(
        const QueueBufferOutput& qbo) {
    uint32_t w, h, transformHint, numPendingBuffers;
    qbo.deflate(&w, &h, &transformHint, &numPendingBuffers);
    mQueueBufferOutput.inflate(w, h, 0, numPendingBuffers);
}

void VirtualDisplaySurface::resetPerFrameState() {
    mCompositionType = COMPOSITION_UNKNOWN;
    mFbFence = Fence::NO_FENCE;
    mOutputFence = Fence::NO_FENCE;
    mOutputProducerSlot = -1;
    mFbProducerSlot = -1;
}

status_t VirtualDisplaySurface::refreshOutputBuffer() {

    return INVALID_OPERATION;

// XXX Add HWC support

#if 0
    if (mOutputProducerSlot >= 0) {
        mSource[SOURCE_SINK]->cancelBuffer(
                mapProducer2SourceSlot(SOURCE_SINK, mOutputProducerSlot),
                mOutputFence);
    }

    int sslot;
    status_t result = dequeueBuffer(SOURCE_SINK, mOutputFormat, mOutputUsage,
            &sslot, &mOutputFence);
    if (result < 0)
        return result;
    mOutputProducerSlot = mapSource2ProducerSlot(SOURCE_SINK, sslot);

    // On GLES-only frames, we don't have the right output buffer acquire fence
    // until after GLES calls queueBuffer(). So here we just set the buffer
    // (for use in HWC prepare) but not the fence; we'll call this again with
    // the proper fence once we have it.
    result = mHwc.setOutputBuffer(mDisplayId, Fence::NO_FENCE,
            mProducerBuffers[mOutputProducerSlot]);

    return result;
#endif
}

// This slot mapping function is its own inverse, so two copies are unnecessary.
// Both are kept to make the intent clear where the function is called, and for
// the (unlikely) chance that we switch to a different mapping function.
int VirtualDisplaySurface::mapSource2ProducerSlot(Source source, int sslot) {
    if (source == SOURCE_SCRATCH) {
        return BufferQueue::NUM_BUFFER_SLOTS - sslot - 1;
    } else {
        return sslot;
    }
}
int VirtualDisplaySurface::mapProducer2SourceSlot(Source source, int pslot) {
    return mapSource2ProducerSlot(source, pslot);
}

VirtualDisplaySurface::Source
VirtualDisplaySurface::fbSourceForCompositionType(CompositionType type) {
    return type == COMPOSITION_MIXED ? SOURCE_SCRATCH : SOURCE_SINK;
}

const char* VirtualDisplaySurface::dbgStateStr() const {
    switch (mDbgState) {
        case DBG_STATE_IDLE:      return "IDLE";
        case DBG_STATE_PREPARED:  return "PREPARED";
        case DBG_STATE_GLES:      return "GLES";
        case DBG_STATE_GLES_DONE: return "GLES_DONE";
        case DBG_STATE_HWC:       return "HWC";
        default:                  return "INVALID";
    }
}

const char* VirtualDisplaySurface::dbgSourceStr(Source s) {
    switch (s) {
        case SOURCE_SINK:    return "SINK";
        case SOURCE_SCRATCH: return "SCRATCH";
        default:             return "INVALID";
    }
}

// ---------------------------------------------------------------------------
} // namespace android
// ---------------------------------------------------------------------------
