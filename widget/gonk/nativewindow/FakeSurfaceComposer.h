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

#include <binder/BinderService.h>

#include <gui/ISurfaceComposer.h>
#include <gui/ISurfaceComposerClient.h>

namespace android {

// ---------------------------------------------------------------------------

class IGraphicBufferAlloc;

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

};

// ---------------------------------------------------------------------------
}; // namespace android

#endif // NATIVEWINDOW_FAKE_SURFACE_COMPOSER_H
