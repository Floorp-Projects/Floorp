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

#ifndef ANDROID_SF_VIRTUAL_DISPLAY_SURFACE_H
#define ANDROID_SF_VIRTUAL_DISPLAY_SURFACE_H

#include <gui/IGraphicBufferProducer.h>

#include "DisplaySurface.h"

// ---------------------------------------------------------------------------
namespace android {
// ---------------------------------------------------------------------------

class HWComposer;
class IProducerListener;

/* This DisplaySurface implementation supports virtual displays, where GLES
 * and/or HWC compose into a buffer that is then passed to an arbitrary
 * consumer (the sink) running in another process.
 *
 * The simplest case is when the virtual display will never use the h/w
 * composer -- either the h/w composer doesn't support writing to buffers, or
 * there are more virtual displays than it supports simultaneously. In this
 * case, the GLES driver works directly with the output buffer queue, and
 * calls to the VirtualDisplay from SurfaceFlinger and DisplayHardware do
 * nothing.
 *
 * If h/w composer might be used, then each frame will fall into one of three
 * configurations: GLES-only, HWC-only, and MIXED composition. In all of these,
 * we must provide a FB target buffer and output buffer for the HWC set() call.
 *
 * In GLES-only composition, the GLES driver is given a buffer from the sink to
 * render into. When the GLES driver queues the buffer to the
 * VirtualDisplaySurface, the VirtualDisplaySurface holds onto it instead of
 * immediately queueing it to the sink. The buffer is used as both the FB
 * target and output buffer for HWC, though on these frames the HWC doesn't
 * do any work for this display and doesn't write to the output buffer. After
 * composition is complete, the buffer is queued to the sink.
 *
 * In HWC-only composition, the VirtualDisplaySurface dequeues a buffer from
 * the sink and passes it to HWC as both the FB target buffer and output
 * buffer. The HWC doesn't need to read from the FB target buffer, but does
 * write to the output buffer. After composition is complete, the buffer is
 * queued to the sink.
 *
 * On MIXED frames, things become more complicated, since some h/w composer
 * implementations can't read from and write to the same buffer. This class has
 * an internal BufferQueue that it uses as a scratch buffer pool. The GLES
 * driver is given a scratch buffer to render into. When it finishes rendering,
 * the buffer is queued and then immediately acquired by the
 * VirtualDisplaySurface. The scratch buffer is then used as the FB target
 * buffer for HWC, and a separate buffer is dequeued from the sink and used as
 * the HWC output buffer. When HWC composition is complete, the scratch buffer
 * is released and the output buffer is queued to the sink.
 */
class VirtualDisplaySurface : public DisplaySurface,
                              public BnGraphicBufferProducer {
public:
    VirtualDisplaySurface(int32_t dispId,
            const sp<IGraphicBufferProducer>& sink,
            const sp<IGraphicBufferProducer>& bqProducer,
            const sp<StreamConsumer>& bqConsumer,
            const String8& name);

    //
    // DisplaySurface interface
    //
    virtual status_t beginFrame(bool mustRecompose);
    virtual status_t prepareFrame(CompositionType compositionType);
    virtual status_t compositionComplete();
    virtual status_t advanceFrame();
    virtual void onFrameCommitted();
    virtual void resizeBuffers(const uint32_t w, const uint32_t h);

    virtual status_t setReleaseFenceFd(int fenceFd) { return INVALID_OPERATION; }
    virtual int GetPrevDispAcquireFd() { return -1; };

private:
    enum Source {SOURCE_SINK = 0, SOURCE_SCRATCH = 1};

    virtual ~VirtualDisplaySurface();

    //
    // IGraphicBufferProducer interface, used by the GLES driver.
    //
    virtual status_t requestBuffer(int pslot, sp<GraphicBuffer>* outBuf);
    virtual status_t setBufferCount(int bufferCount);
    virtual status_t dequeueBuffer(int* pslot, sp<Fence>* fence, bool async,
            uint32_t w, uint32_t h, uint32_t format, uint32_t usage);
    virtual status_t detachBuffer(int slot);
    virtual status_t detachNextBuffer(sp<GraphicBuffer>* outBuffer,
            sp<Fence>* outFence);
    virtual status_t attachBuffer(int* slot, const sp<GraphicBuffer>& buffer);
    virtual status_t queueBuffer(int pslot,
            const QueueBufferInput& input, QueueBufferOutput* output);
    virtual void cancelBuffer(int pslot, const sp<Fence>& fence);
    virtual int query(int what, int* value);
#if ANDROID_VERSION >= 21
    virtual status_t connect(const sp<IProducerListener>& listener,
            int api, bool producerControlledByApp, QueueBufferOutput* output);
#else
    virtual status_t connect(const sp<IBinder>& token,
            int api, bool producerControlledByApp, QueueBufferOutput* output);
#endif
    virtual status_t disconnect(int api);
#if ANDROID_VERSION >= 21
    virtual status_t setSidebandStream(const sp<NativeHandle>& stream);
#endif
    virtual void allocateBuffers(bool async, uint32_t width, uint32_t height,
            uint32_t format, uint32_t usage);

    //
    // Utility methods
    //
    static Source fbSourceForCompositionType(CompositionType type);
    status_t dequeueBuffer(Source source, uint32_t format, uint32_t usage,
            int* sslot, sp<Fence>* fence);
    void updateQueueBufferOutput(const QueueBufferOutput& qbo);
    void resetPerFrameState();
    status_t refreshOutputBuffer();

    // Both the sink and scratch buffer pools have their own set of slots
    // ("source slots", or "sslot"). We have to merge these into the single
    // set of slots used by the GLES producer ("producer slots" or "pslot") and
    // internally in the VirtualDisplaySurface. To minimize the number of times
    // a producer slot switches which source it comes from, we map source slot
    // numbers to producer slot numbers differently for each source.
    static int mapSource2ProducerSlot(Source source, int sslot);
    static int mapProducer2SourceSlot(Source source, int pslot);

    //
    // Immutable after construction
    //
    const int32_t mDisplayId;
    const String8 mDisplayName;
    sp<IGraphicBufferProducer> mSource[2]; // indexed by SOURCE_*
    uint32_t mDefaultOutputFormat;

    //
    // Inter-frame state
    //

    // To avoid buffer reallocations, we track the buffer usage and format
    // we used on the previous frame and use it again on the new frame. If
    // the composition type changes or the GLES driver starts requesting
    // different usage/format, we'll get a new buffer.
    uint32_t mOutputFormat;
    uint32_t mOutputUsage;

    // Since we present a single producer interface to the GLES driver, but
    // are internally muxing between the sink and scratch producers, we have
    // to keep track of which source last returned each producer slot from
    // dequeueBuffer. Each bit in mProducerSlotSource corresponds to a producer
    // slot. Both mProducerSlotSource and mProducerBuffers are indexed by a
    // "producer slot"; see the mapSlot*() functions.
    uint64_t mProducerSlotSource;
    sp<GraphicBuffer> mProducerBuffers[BufferQueue::NUM_BUFFER_SLOTS];

    // The QueueBufferOutput with the latest info from the sink, and with the
    // transform hint cleared. Since we defer queueBuffer from the GLES driver
    // to the sink, we have to return the previous version.
    QueueBufferOutput mQueueBufferOutput;

    // Details of the current sink buffer. These become valid when a buffer is
    // dequeued from the sink, and are used when queueing the buffer.
    uint32_t mSinkBufferWidth, mSinkBufferHeight;

    //
    // Intra-frame state
    //

    // Composition type and GLES buffer source for the current frame.
    // Valid after prepareFrame(), cleared in onFrameCommitted.
    CompositionType mCompositionType;

    // mFbFence is the fence HWC should wait for before reading the framebuffer
    // target buffer.
    sp<Fence> mFbFence;

    // mOutputFence is the fence HWC should wait for before writing to the
    // output buffer.
    sp<Fence> mOutputFence;

    // Producer slot numbers for the buffers to use for HWC framebuffer target
    // and output.
    int mFbProducerSlot;
    int mOutputProducerSlot;

    // Debug only -- track the sequence of events in each frame so we can make
    // sure they happen in the order we expect. This class implicitly models
    // a state machine; this enum/variable makes it explicit.
    //
    // +-----------+-------------------+-------------+
    // | State     | Event             || Next State |
    // +-----------+-------------------+-------------+
    // | IDLE      | beginFrame        || BEGUN      |
    // | BEGUN     | prepareFrame      || PREPARED   |
    // | PREPARED  | dequeueBuffer [1] || GLES       |
    // | PREPARED  | advanceFrame [2]  || HWC        |
    // | GLES      | queueBuffer       || GLES_DONE  |
    // | GLES_DONE | advanceFrame      || HWC        |
    // | HWC       | onFrameCommitted  || IDLE       |
    // +-----------+-------------------++------------+
    // [1] COMPOSITION_GLES and COMPOSITION_MIXED frames.
    // [2] COMPOSITION_HWC frames.
    //
    enum DbgState {
        // no buffer dequeued, don't know anything about the next frame
        DBG_STATE_IDLE,
        // output buffer dequeued, framebuffer source not yet known
        DBG_STATE_BEGUN,
        // output buffer dequeued, framebuffer source known but not provided
        // to GLES yet.
        DBG_STATE_PREPARED,
        // GLES driver has a buffer dequeued
        DBG_STATE_GLES,
        // GLES driver has queued the buffer, we haven't sent it to HWC yet
        DBG_STATE_GLES_DONE,
        // HWC has the buffer for this frame
        DBG_STATE_HWC,
    };
    DbgState mDbgState;
    CompositionType mDbgLastCompositionType;

    const char* dbgStateStr() const;
    static const char* dbgSourceStr(Source s);

    bool mMustRecompose;
};

// ---------------------------------------------------------------------------
} // namespace android
// ---------------------------------------------------------------------------

#endif // ANDROID_SF_VIRTUAL_DISPLAY_SURFACE_H

