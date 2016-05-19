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

#define LOG_TAG "FakeSurfaceComposer"
//#define LOG_NDEBUG 0

#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <cutils/atomic.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <private/android_filesystem_config.h>

#include <gui/IDisplayEventConnection.h>
#include <gui/GraphicBufferAlloc.h>
#include <gui/Surface.h>
#include <ui/DisplayInfo.h>

#if ANDROID_VERSION >= 21
#include <ui/Rect.h>
#endif

#include "../libdisplay/GonkDisplay.h"
#include "../nsScreenManagerGonk.h"
#include "FakeSurfaceComposer.h"
#include "gfxPrefs.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"

using namespace mozilla;

namespace android {

/* static */
void FakeSurfaceComposer::instantiate() {
    defaultServiceManager()->addService(
            String16("SurfaceFlinger"), new FakeSurfaceComposer());
}

FakeSurfaceComposer::FakeSurfaceComposer()
    : BnSurfaceComposer()
{
}

FakeSurfaceComposer::~FakeSurfaceComposer()
{
}

status_t FakeSurfaceComposer::onTransact(uint32_t code, const Parcel& data,
                                         Parcel* reply, uint32_t flags)
{
    switch (code) {
        case CREATE_CONNECTION:
        case CREATE_DISPLAY:
        case SET_TRANSACTION_STATE:
        case CAPTURE_SCREEN:
        {
            // codes that require permission check
            IPCThreadState* ipc = IPCThreadState::self();
            const int pid = ipc->getCallingPid();
            const int uid = ipc->getCallingUid();
            // Accept request only when uid is root.
            if (uid != AID_ROOT) {
                ALOGE("Permission Denial: "
                        "can't access SurfaceFlinger pid=%d, uid=%d", pid, uid);
                return PERMISSION_DENIED;
            }
            break;
        }
    }

    return BnSurfaceComposer::onTransact(code, data, reply, flags);
}

sp<ISurfaceComposerClient> FakeSurfaceComposer::createConnection()
{
    return nullptr;
}

sp<IGraphicBufferAlloc> FakeSurfaceComposer::createGraphicBufferAlloc()
{
    sp<GraphicBufferAlloc> gba(new GraphicBufferAlloc());
    return gba;
}

class DestroyDisplayRunnable : public Runnable {
public:
    DestroyDisplayRunnable(FakeSurfaceComposer* aComposer, ssize_t aIndex)
        : mComposer(aComposer), mIndex(aIndex) { }
    NS_IMETHOD Run() override {
        MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
        Mutex::Autolock _l(mComposer->mStateLock);
        RefPtr<nsScreenManagerGonk> screenManager =
            nsScreenManagerGonk::GetInstance();
        screenManager->RemoveScreen(GonkDisplay::DISPLAY_VIRTUAL);
        mComposer->mDisplays.removeItemsAt(mIndex);
        return NS_OK;
    }
    sp<FakeSurfaceComposer> mComposer;
    ssize_t mIndex;
};

sp<IBinder> FakeSurfaceComposer::createDisplay(const String8& displayName,
        bool secure)
{
#if ANDROID_VERSION >= 19
    class DisplayToken : public BBinder {
        sp<FakeSurfaceComposer> composer;
        virtual ~DisplayToken() {
            MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
            // no more references, this display must be terminated
            Mutex::Autolock _l(composer->mStateLock);
            ssize_t idx = composer->mDisplays.indexOfKey(this);
            if (idx >= 0) {
                nsCOMPtr<nsIRunnable> task(new DestroyDisplayRunnable(composer.get(), idx));
                NS_DispatchToMainThread(task);
            }
        }
    public:
        DisplayToken(const sp<FakeSurfaceComposer>& composer)
            : composer(composer) {
        }
    };

    sp<BBinder> token = new DisplayToken(this);

    Mutex::Autolock _l(mStateLock);
    DisplayDeviceState info(HWC_DISPLAY_VIRTUAL);
    info.displayName = displayName;
    info.displayId = GonkDisplay::DISPLAY_VIRTUAL;
    info.isSecure = secure;
    mDisplays.add(token, info);
    return token;
#else
    return nullptr;
#endif
}

#if ANDROID_VERSION >= 19
void FakeSurfaceComposer::destroyDisplay(const sp<IBinder>& display)
{
    Mutex::Autolock _l(mStateLock);

    ssize_t idx = mDisplays.indexOfKey(display);
    if (idx < 0) {
        ALOGW("destroyDisplay: invalid display token");
        return;
    }

    nsCOMPtr<nsIRunnable> task(new DestroyDisplayRunnable(this, idx));
    NS_DispatchToMainThread(task);
}
#endif

sp<IBinder> FakeSurfaceComposer::getBuiltInDisplay(int32_t id)
{
    // support only primary display
    if (uint32_t(id) != HWC_DISPLAY_PRIMARY) {
        return NULL;
    }

    if (!mPrimaryDisplay.get()) {
        mPrimaryDisplay = new BBinder();
    }
    return mPrimaryDisplay;
}

void FakeSurfaceComposer::setTransactionState(
        const Vector<ComposerState>& state,
        const Vector<DisplayState>& displays,
        uint32_t flags)
{
    Mutex::Autolock _l(mStateLock);
    size_t count = displays.size();
    for (size_t i=0 ; i<count ; i++) {
        const DisplayState& s(displays[i]);
        setDisplayStateLocked(s);
    }
}

uint32_t FakeSurfaceComposer::setDisplayStateLocked(const DisplayState& s)
{
    ssize_t dpyIdx = mDisplays.indexOfKey(s.token);
    if (dpyIdx < 0) {
        return 0;
    }

    uint32_t flags = 0;
    DisplayDeviceState& disp(mDisplays.editValueAt(dpyIdx));

    if (!disp.isValid()) {
        return 0;
    }

    const uint32_t what = s.what;
    if (what & DisplayState::eSurfaceChanged) {
        if (disp.surface->asBinder() != s.surface->asBinder()) {
            disp.surface = s.surface;
            flags |= eDisplayTransactionNeeded;
        }
    }
    if (what & DisplayState::eLayerStackChanged) {
        if (disp.layerStack != s.layerStack) {
            disp.layerStack = s.layerStack;
            flags |= eDisplayTransactionNeeded;
        }
    }
    if (what & DisplayState::eDisplayProjectionChanged) {
        if (disp.orientation != s.orientation) {
            disp.orientation = s.orientation;
            flags |= eDisplayTransactionNeeded;
        }
        if (disp.frame != s.frame) {
            disp.frame = s.frame;
            flags |= eDisplayTransactionNeeded;
        }
        if (disp.viewport != s.viewport) {
            disp.viewport = s.viewport;
            flags |= eDisplayTransactionNeeded;
        }
    }
#if ANDROID_VERSION >= 21
    if (what & DisplayState::eDisplaySizeChanged) {
        if (disp.width != s.width) {
            disp.width = s.width;
            flags |= eDisplayTransactionNeeded;
        }
        if (disp.height != s.height) {
            disp.height = s.height;
            flags |= eDisplayTransactionNeeded;
        }
    }
#endif

    if (what & DisplayState::eSurfaceChanged) {
        nsCOMPtr<nsIRunnable> runnable =
            NS_NewRunnableFunction([&]() {
                MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
                RefPtr<nsScreenManagerGonk> screenManager = nsScreenManagerGonk::GetInstance();
                screenManager->AddScreen(GonkDisplay::DISPLAY_VIRTUAL, disp.surface.get());
            });
        NS_DispatchToMainThread(runnable, NS_DISPATCH_SYNC);
    }

    return flags;
}

void FakeSurfaceComposer::bootFinished()
{
}

bool FakeSurfaceComposer::authenticateSurfaceTexture(
        const sp<IGraphicBufferProducer>& bufferProducer) const {
    return false;
}

sp<IDisplayEventConnection> FakeSurfaceComposer::createDisplayEventConnection() {
    return nullptr;
}

// ---------------------------------------------------------------------------
// Capture screen into an IGraphiBufferProducer
// ---------------------------------------------------------------------------

class Barrier {
public:
    inline Barrier() : state(CLOSED) { }
    inline ~Barrier() { }

    // Release any threads waiting at the Barrier.
    // Provides release semantics: preceding loads and stores will be visible
    // to other threads before they wake up.
    void open() {
        Mutex::Autolock _l(lock);
        state = OPENED;
        cv.broadcast();
    }

    // Reset the Barrier, so wait() will block until open() has been called.
    void close() {
        Mutex::Autolock _l(lock);
        state = CLOSED;
    }

    // Wait until the Barrier is OPEN.
    // Provides acquire semantics: no subsequent loads or stores will occur
    // until wait() returns.
    void wait() const {
        Mutex::Autolock _l(lock);
        while (state == CLOSED) {
            cv.wait(lock);
        }
    }
private:
    enum { OPENED, CLOSED };
    mutable     Mutex       lock;
    mutable     Condition   cv;
    volatile    int         state;
};

/* The code below is here to handle b/8734824
 *
 * We create a IGraphicBufferProducer wrapper that forwards all calls
 * to the calling binder thread, where they are executed. This allows
 * the calling thread to be reused (on the other side) and not
 * depend on having "enough" binder threads to handle the requests.
 *
 */

class GraphicProducerWrapper : public BBinder, public MessageHandler {
    sp<IGraphicBufferProducer> impl;
    sp<Looper> looper;
    status_t result;
    bool exitPending;
    bool exitRequested;
    mutable Barrier barrier;
    volatile int32_t memoryBarrier;
    uint32_t code;
    Parcel const* data;
    Parcel* reply;

    enum {
        MSG_API_CALL,
        MSG_EXIT
    };

    /*
     * this is called by our "fake" BpGraphicBufferProducer. We package the
     * data and reply Parcel and forward them to the calling thread.
     */
    virtual status_t transact(uint32_t code,
            const Parcel& data, Parcel* reply, uint32_t flags) {
        this->code = code;
        this->data = &data;
        this->reply = reply;
        android_atomic_acquire_store(0, &memoryBarrier);
        if (exitPending) {
            // if we've exited, we run the message synchronously right here
            handleMessage(Message(MSG_API_CALL));
        } else {
            barrier.close();
            looper->sendMessage(this, Message(MSG_API_CALL));
            barrier.wait();
        }
        return NO_ERROR;
    }

    /*
     * here we run on the binder calling thread. All we've got to do is
     * call the real BpGraphicBufferProducer.
     */
    virtual void handleMessage(const Message& message) {
        android_atomic_release_load(&memoryBarrier);
        if (message.what == MSG_API_CALL) {
            impl->asBinder()->transact(code, data[0], reply);
            barrier.open();
        } else if (message.what == MSG_EXIT) {
            exitRequested = true;
        }
    }

public:
    GraphicProducerWrapper(const sp<IGraphicBufferProducer>& impl) :
        impl(impl), looper(new Looper(true)), result(NO_ERROR),
        exitPending(false), exitRequested(false) {
    }

    status_t waitForResponse() {
        do {
            looper->pollOnce(-1);
        } while (!exitRequested);
        return result;
    }

    void exit(status_t result) {
        this->result = result;
        exitPending = true;
        looper->sendMessage(this, Message(MSG_EXIT));
    }
};

status_t
FakeSurfaceComposer::captureScreen(const sp<IBinder>& display
                                 , const sp<IGraphicBufferProducer>& producer
#if ANDROID_VERSION >= 21
                                 , Rect sourceCrop
#endif
                                 , uint32_t reqWidth
                                 , uint32_t reqHeight
                                 , uint32_t minLayerZ
                                 , uint32_t maxLayerZ
#if ANDROID_VERSION >= 21
                                 , bool useIdentityTransform
                                 , Rotation rotation
#elif ANDROID_VERSION < 19
                                 , bool isCpuConsumer
#endif
                                  )
{
    if (display == 0 || producer == 0) {
        return BAD_VALUE;
    }

    // Limit only to primary display
    if (display != mPrimaryDisplay) {
        return BAD_VALUE;
    }

    // this creates a "fake" BBinder which will serve as a "fake" remote
    // binder to receive the marshaled calls and forward them to the
    // real remote (a BpGraphicBufferProducer)
    sp<GraphicProducerWrapper> wrapper = new GraphicProducerWrapper(producer);
    // the asInterface() call below creates our "fake" BpGraphicBufferProducer
    // which does the marshaling work forwards to our "fake remote" above.
    sp<IGraphicBufferProducer> fakeProducer = IGraphicBufferProducer::asInterface(wrapper);

    nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableFunction([&]() {
            captureScreenImp(fakeProducer, reqWidth, reqHeight, wrapper.get());
        });
    NS_DispatchToMainThread(runnable);

    status_t result = wrapper->waitForResponse();

    return result;
}

class RunnableCallTask final : public Runnable
{
public:
    explicit RunnableCallTask(nsIRunnable* aRunnable)
        : mRunnable(aRunnable) {}

    NS_IMETHOD Run() override
    {
        return mRunnable->Run();
    }
protected:
    nsCOMPtr<nsIRunnable> mRunnable;
};

void
FakeSurfaceComposer::captureScreenImp(const sp<IGraphicBufferProducer>& producer,
                                      uint32_t reqWidth,
                                      uint32_t reqHeight,
                                      const sp<GraphicProducerWrapper>& wrapper)
{
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(wrapper.get());

    RefPtr<nsScreenGonk> screen = nsScreenManagerGonk::GetPrimaryScreen();

    // get screen geometry
    nsIntRect screenBounds = screen->GetNaturalBounds().ToUnknownRect();
    const uint32_t hw_w = screenBounds.width;
    const uint32_t hw_h = screenBounds.height;

    if (reqWidth > hw_w || reqHeight > hw_h) {
        ALOGE("size mismatch (%d, %d) > (%d, %d)",
                reqWidth, reqHeight, hw_w, hw_h);
        static_cast<GraphicProducerWrapper*>(producer->asBinder().get())->exit(BAD_VALUE);
        return;
    }

    reqWidth  = (!reqWidth)  ? hw_w : reqWidth;
    reqHeight = (!reqHeight) ? hw_h : reqHeight;

    nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableFunction([screen, reqWidth, reqHeight, producer, wrapper]() {
            // create a surface (because we're a producer, and we need to
            // dequeue/queue a buffer)
            sp<Surface> sur = new Surface(producer);
            ANativeWindow* window = sur.get();
            // The closure makes screen const and we can't call forget() on it.
            RefPtr<nsScreenGonk> screenAlias = screen;

            if (native_window_api_connect(window, NATIVE_WINDOW_API_EGL) != NO_ERROR) {
                static_cast<GraphicProducerWrapper*>(producer->asBinder().get())->exit(BAD_VALUE);
                NS_ReleaseOnMainThread(screenAlias.forget());
                return;
            }
            uint32_t usage = GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN |
                             GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_TEXTURE;

            int err = 0;
            err = native_window_set_buffers_dimensions(window, reqWidth, reqHeight);
            err |= native_window_set_scaling_mode(window, NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
            err |= native_window_set_buffers_format(window, HAL_PIXEL_FORMAT_RGBA_8888);
            err |= native_window_set_usage(window, usage);

            status_t result = NO_ERROR;
            if (err == NO_ERROR) {
                ANativeWindowBuffer* buffer;
                result = native_window_dequeue_buffer_and_wait(window,  &buffer);
                if (result == NO_ERROR) {
                    nsresult rv = screen->MakeSnapshot(buffer);
                    if (rv != NS_OK) {
                        result = INVALID_OPERATION;
                    }
                    window->queueBuffer(window, buffer, -1);
                }
            } else {
                result = BAD_VALUE;
            }
            native_window_api_disconnect(window, NATIVE_WINDOW_API_EGL);
            static_cast<GraphicProducerWrapper*>(producer->asBinder().get())->exit(result);
            NS_ReleaseOnMainThread(screenAlias.forget());
        });

    layers::CompositorThreadHolder::Loop()->PostTask(
        MakeAndAddRef<RunnableCallTask>(runnable));
}

#if ANDROID_VERSION >= 21
void
FakeSurfaceComposer::setPowerMode(const sp<IBinder>& display, int mode)
{
}

status_t
FakeSurfaceComposer::getDisplayConfigs(const sp<IBinder>& display, Vector<DisplayInfo>* configs)
{
    if (configs == NULL) {
        return BAD_VALUE;
    }

    // Limit DisplayConfigs only to primary display
    if (!display.get() || display != mPrimaryDisplay) {
        return NAME_NOT_FOUND;
    }

    configs->clear();
    DisplayInfo info = DisplayInfo();

    nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableFunction([&]() {
            MOZ_ASSERT(NS_IsMainThread());
            getPrimaryDisplayInfo(&info);
        });
    NS_DispatchToMainThread(runnable, NS_DISPATCH_SYNC);

    configs->push_back(info);
    return NO_ERROR;
}

status_t
FakeSurfaceComposer::getDisplayStats(const sp<IBinder>& display, DisplayStatInfo* stats)
{
    return INVALID_OPERATION;
}

int
FakeSurfaceComposer::getActiveConfig(const sp<IBinder>& display)
{
   // Only support primary display.
   if (display.get() && (display == mPrimaryDisplay)) {
       return 0;
   }
   return INVALID_OPERATION;
}

status_t
FakeSurfaceComposer::setActiveConfig(const sp<IBinder>& display, int id)
{
    return INVALID_OPERATION;
}

status_t
FakeSurfaceComposer::clearAnimationFrameStats()
{
    return INVALID_OPERATION;
}

status_t
FakeSurfaceComposer::getAnimationFrameStats(FrameStats* outStats) const
{
    return INVALID_OPERATION;
}
#else
void
FakeSurfaceComposer::blank(const sp<IBinder>& display)
{
}

void
FakeSurfaceComposer::unblank(const sp<IBinder>& display)
{
}

status_t
FakeSurfaceComposer::getDisplayInfo(const sp<IBinder>& display, DisplayInfo* info)
{
    if (info == NULL) {
        return BAD_VALUE;
    }

    // Limit DisplayConfigs only to primary display
    if (!display.get() || display != mPrimaryDisplay) {
        return NAME_NOT_FOUND;
    }

    nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableFunction([&]() {
            MOZ_ASSERT(NS_IsMainThread());
            getPrimaryDisplayInfo(info);
        });
    NS_DispatchToMainThread(runnable, NS_DISPATCH_SYNC);

    return NO_ERROR;
}
#endif

#define VSYNC_EVENT_PHASE_OFFSET_NS 0
#define SF_VSYNC_EVENT_PHASE_OFFSET_NS 0

void
FakeSurfaceComposer::getPrimaryDisplayInfo(DisplayInfo* info)
{
    MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");

    // Implementation mimic android SurfaceFlinger::getDisplayConfigs().

    class Density {
        static int getDensityFromProperty(char const* propName) {
            char property[PROPERTY_VALUE_MAX];
            int density = 0;
            if (property_get(propName, property, NULL) > 0) {
                density = atoi(property);
            }
            return density;
        }
    public:
        static int getEmuDensity() {
            return getDensityFromProperty("qemu.sf.lcd_density"); }
        static int getBuildDensity()  {
            return getDensityFromProperty("ro.sf.lcd_density"); }
    };

    RefPtr<nsScreenGonk> screen = nsScreenManagerGonk::GetPrimaryScreen();

    float xdpi = screen->GetDpi();
    float ydpi = screen->GetDpi();
    int fps = 60; // XXX set a value from hwc hal
    nsIntRect screenBounds = screen->GetNaturalBounds().ToUnknownRect();

    // The density of the device is provided by a build property
    float density = Density::getBuildDensity() / 160.0f;
    if (density == 0) {
        // the build doesn't provide a density -- this is wrong!
        // use xdpi instead
        ALOGE("ro.sf.lcd_density must be defined as a build property");
        density = xdpi / 160.0f;
    }
    info->density = density;
    info->orientation = screen->EffectiveScreenRotation();

    info->w = screenBounds.width;
    info->h = screenBounds.height;
    info->xdpi = xdpi;
    info->ydpi = ydpi;
    info->fps = fps;
#if ANDROID_VERSION >= 21
    info->appVsyncOffset = VSYNC_EVENT_PHASE_OFFSET_NS;

    // This is how far in advance a buffer must be queued for
    // presentation at a given time.  If you want a buffer to appear
    // on the screen at time N, you must submit the buffer before
    // (N - presentationDeadline).
    //
    // Normally it's one full refresh period (to give SF a chance to
    // latch the buffer), but this can be reduced by configuring a
    // DispSync offset.  Any additional delays introduced by the hardware
    // composer or panel must be accounted for here.
    //
    // We add an additional 1ms to allow for processing time and
    // differences between the ideal and actual refresh rate.
    info->presentationDeadline =
            (1e9 / fps) - SF_VSYNC_EVENT_PHASE_OFFSET_NS + 1000000;
#endif
    // All non-virtual displays are currently considered secure.
    info->secure = true;
}

}; // namespace android
