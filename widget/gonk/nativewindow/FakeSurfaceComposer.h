/*
 * Copyright (C) 2007 The Android Open Source Project
 * Copyright (C) 2013 Mozilla Foundation
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

#ifndef NATIVEWINDOW_FAKE_SURFACE_COMPOSER_H
#define NATIVEWINDOW_FAKE_SURFACE_COMPOSER_H

#include <stdint.h>
#include <sys/types.h>

#include <utils/Errors.h>
#include <utils/Looper.h>

#include <binder/BinderService.h>

#include <gui/IGraphicBufferProducer.h>
#include <gui/ISurfaceComposer.h>
#include <gui/ISurfaceComposerClient.h>
#include <hardware/hwcomposer.h>
#include <private/gui/LayerState.h>
#include <utils/KeyedVector.h>

class nsIWidget;

namespace android {

// ---------------------------------------------------------------------------

class GraphicProducerWrapper;
class IGraphicBufferAlloc;

enum {
    eTransactionNeeded        = 0x01,
    eTraversalNeeded          = 0x02,
    eDisplayTransactionNeeded = 0x04,
    eTransactionMask          = 0x07
};

class FakeSurfaceComposer : public BinderService<FakeSurfaceComposer>,
                            public BnSurfaceComposer
{
public:
    static char const* getServiceName() {
        return "FakeSurfaceComposer";
    }

    // Instantiate FakeSurfaceComposer and register to service manager.
    // If service manager is not present, wait until service manager becomes present.
    static  void instantiate();

#if ANDROID_VERSION >= 19
    virtual void destroyDisplay(const sp<android::IBinder>& display);
#endif

#if ANDROID_VERSION >= 21
    virtual status_t captureScreen(const sp<IBinder>& display,
                                   const sp<IGraphicBufferProducer>& producer,
                                   Rect sourceCrop, uint32_t reqWidth, uint32_t reqHeight,
                                   uint32_t minLayerZ, uint32_t maxLayerZ,
                                   bool useIdentityTransform,
                                   Rotation rotation = eRotateNone);
#elif ANDROID_VERSION >= 19
    virtual status_t captureScreen(const sp<IBinder>& display,
                                   const sp<IGraphicBufferProducer>& producer,
                                   uint32_t reqWidth, uint32_t reqHeight,
                                   uint32_t minLayerZ, uint32_t maxLayerZ);
#else
    virtual status_t captureScreen(const sp<IBinder>& display,
                                   const sp<IGraphicBufferProducer>& producer,
                                   uint32_t reqWidth, uint32_t reqHeight,
                                   uint32_t minLayerZ, uint32_t maxLayerZ, bool isCpuConsumer);
#endif

private:
    FakeSurfaceComposer();
    // We're reference counted, never destroy FakeSurfaceComposer directly
    virtual ~FakeSurfaceComposer();

    /* ------------------------------------------------------------------------
     * IBinder interface
     */
    virtual status_t onTransact(uint32_t code, const Parcel& data,
        Parcel* reply, uint32_t flags);

    /* ------------------------------------------------------------------------
     * ISurfaceComposer interface
     */
    virtual sp<ISurfaceComposerClient> createConnection();
    virtual sp<IGraphicBufferAlloc> createGraphicBufferAlloc();
    virtual sp<IBinder> createDisplay(const String8& displayName, bool secure);
    virtual sp<IBinder> getBuiltInDisplay(int32_t id);
    virtual void setTransactionState(const Vector<ComposerState>& state,
            const Vector<DisplayState>& displays, uint32_t flags);
    virtual void bootFinished();
    virtual bool authenticateSurfaceTexture(
        const sp<IGraphicBufferProducer>& bufferProducer) const;
    virtual sp<IDisplayEventConnection> createDisplayEventConnection();
#if ANDROID_VERSION >= 21
    virtual void setPowerMode(const sp<IBinder>& display, int mode);
    virtual status_t getDisplayConfigs(const sp<IBinder>& display, Vector<DisplayInfo>* configs);
    virtual status_t getDisplayStats(const sp<IBinder>& display, DisplayStatInfo* stats);
    virtual int getActiveConfig(const sp<IBinder>& display);
    virtual status_t setActiveConfig(const sp<IBinder>& display, int id);
    virtual status_t clearAnimationFrameStats();
    virtual status_t getAnimationFrameStats(FrameStats* outStats) const;
#elif ANDROID_VERSION >= 17
    // called when screen needs to turn off
    virtual void blank(const sp<IBinder>& display);
    // called when screen is turning back on
    virtual void unblank(const sp<IBinder>& display);
    virtual status_t getDisplayInfo(const sp<IBinder>& display, DisplayInfo* info);
#endif
    void getPrimaryDisplayInfo(DisplayInfo* info);

    /* ------------------------------------------------------------------------
     * Transactions
     */
    uint32_t setDisplayStateLocked(const DisplayState& s);

    void captureScreenImp(const sp<IGraphicBufferProducer>& producer,
                          uint32_t reqWidth,
                          uint32_t reqHeight,
                          const sp<GraphicProducerWrapper>& wrapper);

    sp<IBinder> mPrimaryDisplay;

    struct DisplayDeviceState {
        enum {
            NO_LAYER_STACK = 0xFFFFFFFF,
        };
        DisplayDeviceState()
            : type(-1), displayId(-1), width(0), height(0) {
        }
        DisplayDeviceState(int type)
            : type(type), displayId(-1), layerStack(NO_LAYER_STACK), orientation(0), width(0), height(0) {
            viewport.makeInvalid();
            frame.makeInvalid();
        }
        bool isValid() const { return type >= 0; }
        int type;
        int displayId;
        sp<IGraphicBufferProducer> surface;
        uint32_t layerStack;
        Rect viewport;
        Rect frame;
        uint8_t orientation;
        uint32_t width, height;
        String8 displayName;
        bool isSecure;
    };

    // access must be protected by mStateLock
    mutable Mutex mStateLock;
    DefaultKeyedVector<wp<IBinder>, DisplayDeviceState> mDisplays;

    friend class DestroyDisplayRunnable;
};

// ---------------------------------------------------------------------------
}; // namespace android

#endif // NATIVEWINDOW_FAKE_SURFACE_COMPOSER_H
