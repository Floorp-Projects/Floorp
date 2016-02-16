/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
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

#ifndef nsScreenManagerGonk_h___
#define nsScreenManagerGonk_h___

#include "cutils/properties.h"
#include "hardware/hwcomposer.h"

#include "libdisplay/GonkDisplay.h"
#include "mozilla/Atomics.h"
#include "mozilla/Hal.h"
#include "mozilla/Mutex.h"
#include "nsBaseScreen.h"
#include "nsCOMPtr.h"
#include "nsIScreenManager.h"
#include "nsProxyRelease.h"

#include <android/native_window.h>

class nsRunnable;
class nsWindow;

namespace android {
    class DisplaySurface;
    class IGraphicBufferProducer;
};

namespace mozilla {
namespace gl {
    class GLContext;
}
namespace layers {
class CompositorVsyncScheduler;
class CompositorParent;
}
}

enum class NotifyDisplayChangedEvent : int8_t {
  Observable,
  Suppressed
};

class nsScreenGonk : public nsBaseScreen
{
    typedef mozilla::hal::ScreenConfiguration ScreenConfiguration;
    typedef mozilla::GonkDisplay GonkDisplay;
    typedef mozilla::LayoutDeviceIntRect LayoutDeviceIntRect;
    typedef mozilla::layers::CompositorParent CompositorParent;
    typedef mozilla::gfx::DrawTarget DrawTarget;

public:
    nsScreenGonk(uint32_t aId,
                 GonkDisplay::DisplayType aDisplayType,
                 const GonkDisplay::NativeData& aNativeData,
                 NotifyDisplayChangedEvent aEventVisibility);

    ~nsScreenGonk();

    NS_IMETHOD GetId(uint32_t* aId);
    NS_IMETHOD GetRect(int32_t* aLeft, int32_t* aTop, int32_t* aWidth, int32_t* aHeight);
    NS_IMETHOD GetAvailRect(int32_t* aLeft, int32_t* aTop, int32_t* aWidth, int32_t* aHeight);
    NS_IMETHOD GetPixelDepth(int32_t* aPixelDepth);
    NS_IMETHOD GetColorDepth(int32_t* aColorDepth);
    NS_IMETHOD GetRotation(uint32_t* aRotation);
    NS_IMETHOD SetRotation(uint32_t  aRotation);

    uint32_t GetId();
    NotifyDisplayChangedEvent GetEventVisibility();
    LayoutDeviceIntRect GetRect();
    float GetDpi();
    int32_t GetSurfaceFormat();
    ANativeWindow* GetNativeWindow();
    LayoutDeviceIntRect GetNaturalBounds();
    uint32_t EffectiveScreenRotation();
    ScreenConfiguration GetConfiguration();
    bool IsPrimaryScreen();

    already_AddRefed<DrawTarget> StartRemoteDrawing();
    void EndRemoteDrawing();

    nsresult MakeSnapshot(ANativeWindowBuffer* aBuffer);
    void SetCompositorParent(CompositorParent* aCompositorParent);

#if ANDROID_VERSION >= 17
    android::DisplaySurface* GetDisplaySurface();
    int GetPrevDispAcquireFd();
#endif
    GonkDisplay::DisplayType GetDisplayType();

    void RegisterWindow(nsWindow* aWindow);
    void UnregisterWindow(nsWindow* aWindow);
    void BringToTop(nsWindow* aWindow);

    const nsTArray<nsWindow*>& GetTopWindows() const
    {
        return mTopWindows;
    }

    // Non-primary screen only
    bool EnableMirroring();
    bool DisableMirroring();
    bool IsMirroring()
    {
        return mIsMirroring;
    }

    // Primary screen only
    bool SetMirroringScreen(nsScreenGonk* aScreen);
    bool ClearMirroringScreen(nsScreenGonk* aScreen);

    // Called only on compositor thread
    void SetEGLInfo(hwc_display_t aDisplay, hwc_surface_t aSurface,
                    mozilla::gl::GLContext* aGLContext);
    hwc_display_t GetEGLDisplay();
    hwc_surface_t GetEGLSurface();
    already_AddRefed<mozilla::gl::GLContext> GetGLContext();
    void UpdateMirroringWidget(already_AddRefed<nsWindow>& aWindow); // Primary screen only
    nsWindow* GetMirroringWidget(); // Primary screen only

protected:
    ANativeWindowBuffer* DequeueBuffer();
    bool QueueBuffer(ANativeWindowBuffer* buf);

    uint32_t mId;
    NotifyDisplayChangedEvent mEventVisibility;
    int32_t mColorDepth;
    android::sp<ANativeWindow> mNativeWindow;
    float mDpi;
    int32_t mSurfaceFormat;
    LayoutDeviceIntRect mNaturalBounds; // Screen bounds w/o rotation taken into account.
    LayoutDeviceIntRect mVirtualBounds; // Screen bounds w/ rotation taken into account.
    uint32_t mScreenRotation;
    uint32_t mPhysicalScreenRotation;
    nsTArray<nsWindow*> mTopWindows;
#if ANDROID_VERSION >= 17
    android::sp<android::DisplaySurface> mDisplaySurface;
#endif
    bool mIsMirroring; // Non-primary screen only
    RefPtr<nsScreenGonk> mMirroringScreen; // Primary screen only
    mozilla::Atomic<CompositorParent*> mCompositorParent;

    // Accessed and updated only on compositor thread
    GonkDisplay::DisplayType mDisplayType;
    hwc_display_t mEGLDisplay;
    hwc_surface_t mEGLSurface;
    RefPtr<mozilla::gl::GLContext> mGLContext;
    RefPtr<nsWindow> mMirroringWidget; // Primary screen only

    // If we're using a BasicCompositor, these fields are temporarily
    // set during frame composition.  They wrap the hardware
    // framebuffer.
    RefPtr<DrawTarget> mFramebufferTarget;
    ANativeWindowBuffer* mFramebuffer;
    /**
     * Points to a mapped gralloc buffer between calls to lock and unlock.
     * Should be null outside of the lock-unlock pair.
     */
    uint8_t* mMappedBuffer;
    // If we're using a BasicCompositor, this is our window back
    // buffer.  The gralloc framebuffer driver expects us to draw the
    // entire framebuffer on every frame, but gecko expects the
    // windowing system to be tracking buffer updates for invalidated
    // regions.  We get stuck holding that bag.
    //
    // Only accessed on the compositor thread, except during
    // destruction.
    RefPtr<DrawTarget> mBackBuffer;
};

class nsScreenManagerGonk final : public nsIScreenManager
{
public:
    typedef mozilla::GonkDisplay GonkDisplay;

public:
    nsScreenManagerGonk();

    NS_DECL_ISUPPORTS
    NS_DECL_NSISCREENMANAGER

    static already_AddRefed<nsScreenManagerGonk> GetInstance();
    static already_AddRefed<nsScreenGonk> GetPrimaryScreen();

    void Initialize();
    void DisplayEnabled(bool aEnabled);

    nsresult AddScreen(GonkDisplay::DisplayType aDisplayType,
                       android::IGraphicBufferProducer* aSink = nullptr,
                       NotifyDisplayChangedEvent aEventVisibility = NotifyDisplayChangedEvent::Observable);

    nsresult RemoveScreen(GonkDisplay::DisplayType aDisplayType);

#if ANDROID_VERSION >= 19
    void SetCompositorVsyncScheduler(mozilla::layers::CompositorVsyncScheduler* aObserver);
#endif

protected:
    ~nsScreenManagerGonk();
    void VsyncControl(bool aEnabled);
    uint32_t GetIdFromType(GonkDisplay::DisplayType aDisplayType);
    bool IsScreenConnected(uint32_t aId);

    bool mInitialized;
    nsTArray<RefPtr<nsScreenGonk>> mScreens;
    RefPtr<nsRunnable> mScreenOnEvent;
    RefPtr<nsRunnable> mScreenOffEvent;

#if ANDROID_VERSION >= 19
    bool mDisplayEnabled;
    RefPtr<mozilla::layers::CompositorVsyncScheduler> mCompositorVsyncScheduler;
#endif
};

#endif /* nsScreenManagerGonk_h___ */
