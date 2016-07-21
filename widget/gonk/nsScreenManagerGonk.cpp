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

#include "android/log.h"
#include "GLContext.h"
#include "gfxPrefs.h"
#include "gfxUtils.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/Hal.h"
#include "libdisplay/BootAnimation.h"
#include "libdisplay/GonkDisplay.h"
#include "nsScreenManagerGonk.h"
#include "nsThreadUtils.h"
#include "HwcComposer2D.h"
#include "VsyncSource.h"
#include "nsWindow.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/Services.h"
#include "mozilla/ProcessPriorityManager.h"
#include "nsIdleService.h"
#include "nsIObserverService.h"
#include "nsAppShell.h"
#include "nsProxyRelease.h"
#include "nsTArray.h"
#include "pixelflinger/format.h"
#include "nsIDisplayInfo.h"

#if ANDROID_VERSION >= 17
#include "libdisplay/DisplaySurface.h"
#endif

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "nsScreenGonk" , ## args)
#define LOGW(args...) __android_log_print(ANDROID_LOG_WARN, "nsScreenGonk", ## args)
#define LOGE(args...) __android_log_print(ANDROID_LOG_ERROR, "nsScreenGonk", ## args)

using namespace mozilla;
using namespace mozilla::hal;
using namespace mozilla::gfx;
using namespace mozilla::gl;
using namespace mozilla::layers;
using namespace mozilla::dom;

namespace {

class ScreenOnOffEvent : public mozilla::Runnable {
public:
    ScreenOnOffEvent(bool on)
        : mIsOn(on)
    {}

    NS_IMETHOD Run() {
        // Notify observers that the screen state has just changed.
        nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
        if (observerService) {
          observerService->NotifyObservers(
            nullptr, "screen-state-changed",
            mIsOn ? u"on" : u"off"
          );
        }

        RefPtr<nsScreenGonk> screen = nsScreenManagerGonk::GetPrimaryScreen();
        const nsTArray<nsWindow*>& windows = screen->GetTopWindows();

        for (uint32_t i = 0; i < windows.Length(); i++) {
            nsWindow *win = windows[i];

            if (nsIWidgetListener* listener = win->GetWidgetListener()) {
                listener->SizeModeChanged(mIsOn ? nsSizeMode_Fullscreen : nsSizeMode_Minimized);
            }
        }

        return NS_OK;
    }

private:
    bool mIsOn;
};

static void
displayEnabledCallback(bool enabled)
{
    RefPtr<nsScreenManagerGonk> screenManager = nsScreenManagerGonk::GetInstance();
    screenManager->DisplayEnabled(enabled);
}

} // namespace

static uint32_t
SurfaceFormatToColorDepth(int32_t aSurfaceFormat)
{
    switch (aSurfaceFormat) {
    case GGL_PIXEL_FORMAT_RGB_565:
        return 16;
    case GGL_PIXEL_FORMAT_RGBA_8888:
        return 32;
    }
    return 24; // GGL_PIXEL_FORMAT_RGBX_8888
}

// nsScreenGonk.cpp

nsScreenGonk::nsScreenGonk(uint32_t aId,
                           GonkDisplay::DisplayType aDisplayType,
                           const GonkDisplay::NativeData& aNativeData,
                           NotifyDisplayChangedEvent aEventVisibility)
    : mId(aId)
    , mEventVisibility(aEventVisibility)
    , mNativeWindow(aNativeData.mNativeWindow)
    , mDpi(aNativeData.mXdpi)
    , mScreenRotation(nsIScreen::ROTATION_0_DEG)
    , mPhysicalScreenRotation(nsIScreen::ROTATION_0_DEG)
#if ANDROID_VERSION >= 17
    , mDisplaySurface(aNativeData.mDisplaySurface)
#endif
    , mIsMirroring(false)
    , mDisplayType(aDisplayType)
    , mEGLDisplay(EGL_NO_DISPLAY)
    , mEGLSurface(EGL_NO_SURFACE)
    , mGLContext(nullptr)
    , mFramebuffer(nullptr)
    , mMappedBuffer(nullptr)
{
    if (mNativeWindow->query(mNativeWindow.get(), NATIVE_WINDOW_WIDTH, &mVirtualBounds.width) ||
        mNativeWindow->query(mNativeWindow.get(), NATIVE_WINDOW_HEIGHT, &mVirtualBounds.height) ||
        mNativeWindow->query(mNativeWindow.get(), NATIVE_WINDOW_FORMAT, &mSurfaceFormat)) {
        NS_RUNTIMEABORT("Failed to get native window size, aborting...");
    }

    mNaturalBounds = mVirtualBounds;

    if (IsPrimaryScreen()) {
        char propValue[PROPERTY_VALUE_MAX];
        property_get("ro.sf.hwrotation", propValue, "0");
        mPhysicalScreenRotation = atoi(propValue) / 90;
    }

    mColorDepth = SurfaceFormatToColorDepth(mSurfaceFormat);
}

static void
ReleaseGLContextSync(mozilla::gl::GLContext* aGLContext)
{
    MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
    aGLContext->Release();
}

nsScreenGonk::~nsScreenGonk()
{
    // Release GLContext on compositor thread
    if (mGLContext) {
        CompositorThreadHolder::Loop()->PostTask(
            NewRunnableFunction(&ReleaseGLContextSync,
                                mGLContext.forget().take()));
        mGLContext = nullptr;
    }
}

bool
nsScreenGonk::IsPrimaryScreen()
{
    return mDisplayType == GonkDisplay::DISPLAY_PRIMARY;
}

NS_IMETHODIMP
nsScreenGonk::GetId(uint32_t *outId)
{
    *outId = mId;
    return NS_OK;
}

uint32_t
nsScreenGonk::GetId()
{
    return mId;
}

NotifyDisplayChangedEvent
nsScreenGonk::GetEventVisibility()
{
    return mEventVisibility;
}

NS_IMETHODIMP
nsScreenGonk::GetRect(int32_t *outLeft,  int32_t *outTop,
                      int32_t *outWidth, int32_t *outHeight)
{
    *outLeft = mVirtualBounds.x;
    *outTop = mVirtualBounds.y;

    *outWidth = mVirtualBounds.width;
    *outHeight = mVirtualBounds.height;

    return NS_OK;
}

LayoutDeviceIntRect
nsScreenGonk::GetRect()
{
    return mVirtualBounds;
}

NS_IMETHODIMP
nsScreenGonk::GetAvailRect(int32_t *outLeft,  int32_t *outTop,
                           int32_t *outWidth, int32_t *outHeight)
{
    return GetRect(outLeft, outTop, outWidth, outHeight);
}

NS_IMETHODIMP
nsScreenGonk::GetPixelDepth(int32_t *aPixelDepth)
{
    // XXX: this should actually return 32 when we're using 24-bit
    // color, because we use RGBX.
    *aPixelDepth = mColorDepth;
    return NS_OK;
}

NS_IMETHODIMP
nsScreenGonk::GetColorDepth(int32_t *aColorDepth)
{
    *aColorDepth = mColorDepth;
    return NS_OK;
}

NS_IMETHODIMP
nsScreenGonk::GetRotation(uint32_t* aRotation)
{
    *aRotation = mScreenRotation;
    return NS_OK;
}

float
nsScreenGonk::GetDpi()
{
    return mDpi;
}

int32_t
nsScreenGonk::GetSurfaceFormat()
{
    return mSurfaceFormat;
}

ANativeWindow*
nsScreenGonk::GetNativeWindow()
{
    return mNativeWindow.get();
}

NS_IMETHODIMP
nsScreenGonk::SetRotation(uint32_t aRotation)
{
    if (!(aRotation <= ROTATION_270_DEG)) {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    if (mScreenRotation == aRotation) {
        return NS_OK;
    }

    mScreenRotation = aRotation;
    uint32_t rotation = EffectiveScreenRotation();
    if (rotation == nsIScreen::ROTATION_90_DEG ||
        rotation == nsIScreen::ROTATION_270_DEG) {
        mVirtualBounds = LayoutDeviceIntRect(0, 0,
                                             mNaturalBounds.height,
                                             mNaturalBounds.width);
    } else {
        mVirtualBounds = mNaturalBounds;
    }

    nsAppShell::NotifyScreenRotation();

    for (unsigned int i = 0; i < mTopWindows.Length(); i++) {
        mTopWindows[i]->Resize(mVirtualBounds.width,
                               mVirtualBounds.height,
                               true);
    }

    return NS_OK;
}

LayoutDeviceIntRect
nsScreenGonk::GetNaturalBounds()
{
    return mNaturalBounds;
}

uint32_t
nsScreenGonk::EffectiveScreenRotation()
{
    return (mScreenRotation + mPhysicalScreenRotation) % (360 / 90);
}

// NB: This isn't gonk-specific, but gonk is the only widget backend
// that does this calculation itself, currently.
static ScreenOrientationInternal
ComputeOrientation(uint32_t aRotation, const LayoutDeviceIntSize& aScreenSize)
{
    bool naturallyPortrait = (aScreenSize.height > aScreenSize.width);
    switch (aRotation) {
    case nsIScreen::ROTATION_0_DEG:
        return (naturallyPortrait ? eScreenOrientation_PortraitPrimary :
                eScreenOrientation_LandscapePrimary);
    case nsIScreen::ROTATION_90_DEG:
        // Arbitrarily choosing 90deg to be primary "unnatural"
        // rotation.
        return (naturallyPortrait ? eScreenOrientation_LandscapePrimary :
                eScreenOrientation_PortraitPrimary);
    case nsIScreen::ROTATION_180_DEG:
        return (naturallyPortrait ? eScreenOrientation_PortraitSecondary :
                eScreenOrientation_LandscapeSecondary);
    case nsIScreen::ROTATION_270_DEG:
        return (naturallyPortrait ? eScreenOrientation_LandscapeSecondary :
                eScreenOrientation_PortraitSecondary);
    default:
        MOZ_CRASH("Gonk screen must always have a known rotation");
    }
}

static uint16_t
RotationToAngle(uint32_t aRotation)
{
    uint16_t angle = 90 * aRotation;
    MOZ_ASSERT(angle == 0 || angle == 90 || angle == 180 || angle == 270);
    return angle;
}

ScreenConfiguration
nsScreenGonk::GetConfiguration()
{
    ScreenOrientationInternal orientation =
        ComputeOrientation(mScreenRotation, mNaturalBounds.Size());

    // NB: perpetuating colorDepth == pixelDepth illusion here, for
    // consistency.
    return ScreenConfiguration(mVirtualBounds.ToUnknownRect(), orientation,
                               RotationToAngle(mScreenRotation),
                               mColorDepth, mColorDepth);
}

void
nsScreenGonk::RegisterWindow(nsWindow* aWindow)
{
    mTopWindows.AppendElement(aWindow);
}

void
nsScreenGonk::UnregisterWindow(nsWindow* aWindow)
{
    mTopWindows.RemoveElement(aWindow);
}

void
nsScreenGonk::BringToTop(nsWindow* aWindow)
{
    mTopWindows.RemoveElement(aWindow);
    mTopWindows.InsertElementAt(0, aWindow);
}

static gralloc_module_t const*
gralloc_module()
{
    hw_module_t const *module;
    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module)) {
        return nullptr;
    }
    return reinterpret_cast<gralloc_module_t const*>(module);
}

static SurfaceFormat
HalFormatToSurfaceFormat(int aHalFormat)
{
    switch (aHalFormat) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
        // Needs RB swap
        return SurfaceFormat::B8G8R8A8;
    case HAL_PIXEL_FORMAT_RGBX_8888:
        // Needs RB swap
        return SurfaceFormat::B8G8R8X8;
    case HAL_PIXEL_FORMAT_BGRA_8888:
        return SurfaceFormat::B8G8R8A8;
    case HAL_PIXEL_FORMAT_RGB_565:
        return SurfaceFormat::R5G6B5_UINT16;
    default:
        MOZ_CRASH("Unhandled HAL pixel format");
        return SurfaceFormat::UNKNOWN; // not reached
    }
}

static bool
NeedsRBSwap(int aHalFormat)
{
    switch (aHalFormat) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
        return true;
    case HAL_PIXEL_FORMAT_RGBX_8888:
        return true;
    case HAL_PIXEL_FORMAT_BGRA_8888:
        return false;
    case HAL_PIXEL_FORMAT_RGB_565:
        return false;
    default:
        MOZ_CRASH("Unhandled HAL pixel format");
        return false; // not reached
    }
}

already_AddRefed<DrawTarget>
nsScreenGonk::StartRemoteDrawing()
{
    MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
    MOZ_ASSERT(!mFramebuffer);
    MOZ_ASSERT(!mMappedBuffer);

    mFramebuffer = DequeueBuffer();
    int width = mFramebuffer->width, height = mFramebuffer->height;
    if (gralloc_module()->lock(gralloc_module(), mFramebuffer->handle,
                               GRALLOC_USAGE_SW_READ_NEVER |
                               GRALLOC_USAGE_SW_WRITE_OFTEN |
                               GRALLOC_USAGE_HW_FB,
                               0, 0, width, height,
                               reinterpret_cast<void**>(&mMappedBuffer))) {
        EndRemoteDrawing();
        return nullptr;
    }
    SurfaceFormat format = HalFormatToSurfaceFormat(GetSurfaceFormat());
    mFramebufferTarget = Factory::CreateDrawTargetForData(
        BackendType::CAIRO,
        mMappedBuffer,
        IntSize(width, height),
        mFramebuffer->stride * gfx::BytesPerPixel(format),
        format);
    if (!mFramebufferTarget) {
        MOZ_CRASH("nsWindow::StartRemoteDrawing failed in CreateDrawTargetForData");
    }
    if (!mBackBuffer ||
        mBackBuffer->GetSize() != mFramebufferTarget->GetSize() ||
        mBackBuffer->GetFormat() != mFramebufferTarget->GetFormat()) {
        mBackBuffer = mFramebufferTarget->CreateSimilarDrawTarget(
            mFramebufferTarget->GetSize(), mFramebufferTarget->GetFormat());
    }
    RefPtr<DrawTarget> buffer(mBackBuffer);
    return buffer.forget();
}

void
nsScreenGonk::EndRemoteDrawing()
{
    MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

    if (mFramebufferTarget && mFramebuffer) {
        IntSize size = mFramebufferTarget->GetSize();
        Rect rect(0, 0, size.width, size.height);
        RefPtr<SourceSurface> source = mBackBuffer->Snapshot();
        mFramebufferTarget->DrawSurface(source, rect, rect);

        // Convert from BGR to RGB
        // XXX this is a temporary solution. It consumes extra cpu cycles,
        // it should not be used on product device.
        if (NeedsRBSwap(GetSurfaceFormat())) {
            LOGE("Very slow composition path, it should not be used on product!!!");
            SurfaceFormat format = HalFormatToSurfaceFormat(GetSurfaceFormat());
            gfxUtils::ConvertBGRAtoRGBA(
                mMappedBuffer,
                mFramebuffer->stride * mFramebuffer->height * gfx::BytesPerPixel(format));
        }
    }
    if (mMappedBuffer) {
        MOZ_ASSERT(mFramebuffer);
        gralloc_module()->unlock(gralloc_module(), mFramebuffer->handle);
        mMappedBuffer = nullptr;
    }
    if (mFramebuffer) {
        QueueBuffer(mFramebuffer);
    }
    mFramebuffer = nullptr;
    mFramebufferTarget = nullptr;
}

ANativeWindowBuffer*
nsScreenGonk::DequeueBuffer()
{
    ANativeWindowBuffer* buf = nullptr;
#if ANDROID_VERSION >= 17
    int fenceFd = -1;
    mNativeWindow->dequeueBuffer(mNativeWindow.get(), &buf, &fenceFd);
    android::sp<android::Fence> fence(new android::Fence(fenceFd));
#if ANDROID_VERSION == 17
    fence->waitForever(1000, "nsScreenGonk_DequeueBuffer");
    // 1000 is what Android uses. It is a warning timeout in ms.
    // This timeout was removed in ANDROID_VERSION 18.
#else
    fence->waitForever("nsScreenGonk_DequeueBuffer");
#endif
#else
    mNativeWindow->dequeueBuffer(mNativeWindow.get(), &buf);
#endif
    return buf;
}

bool
nsScreenGonk::QueueBuffer(ANativeWindowBuffer* buf)
{
#if ANDROID_VERSION >= 17
  int ret = mNativeWindow->queueBuffer(mNativeWindow.get(), buf, -1);
  return ret == 0;
#else
  int ret = mNativeWindow->queueBuffer(mNativeWindow.get(), buf);
  return ret == 0;
#endif
}

nsresult
nsScreenGonk::MakeSnapshot(ANativeWindowBuffer* aBuffer)
{
    MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
    MOZ_ASSERT(aBuffer);

    layers::CompositorBridgeParent* compositorParent = mCompositorBridgeParent;
    if (!compositorParent) {
        return NS_ERROR_FAILURE;
    }

    int width = aBuffer->width, height = aBuffer->height;
    uint8_t* mappedBuffer = nullptr;
    if (gralloc_module()->lock(gralloc_module(), aBuffer->handle,
                               GRALLOC_USAGE_SW_READ_OFTEN |
                               GRALLOC_USAGE_SW_WRITE_OFTEN,
                               0, 0, width, height,
                               reinterpret_cast<void**>(&mappedBuffer))) {
        return NS_ERROR_FAILURE;
    }

    SurfaceFormat format = HalFormatToSurfaceFormat(GetSurfaceFormat());
    RefPtr<DrawTarget> mTarget =
        Factory::CreateDrawTargetForData(
            BackendType::CAIRO,
            mappedBuffer,
            IntSize(width, height),
            aBuffer->stride * gfx::BytesPerPixel(format),
            format);
    if (!mTarget) {
        return NS_ERROR_FAILURE;
    }

    gfx::IntRect rect = GetRect().ToUnknownRect();
    compositorParent->ForceComposeToTarget(mTarget, &rect);

    // Convert from BGR to RGB
    // XXX this is a temporary solution. It consumes extra cpu cycles,
    if (NeedsRBSwap(GetSurfaceFormat())) {
        LOGE("Slow path of making Snapshot!!!");
        SurfaceFormat format = HalFormatToSurfaceFormat(GetSurfaceFormat());
        gfxUtils::ConvertBGRAtoRGBA(
            mappedBuffer,
            aBuffer->stride * aBuffer->height * gfx::BytesPerPixel(format));
        mappedBuffer = nullptr;
    }
    gralloc_module()->unlock(gralloc_module(), aBuffer->handle);
    return NS_OK;
}

void
nsScreenGonk::SetCompositorBridgeParent(layers::CompositorBridgeParent* aCompositorBridgeParent)
{
    MOZ_ASSERT(NS_IsMainThread());
    mCompositorBridgeParent = aCompositorBridgeParent;
}

#if ANDROID_VERSION >= 17
android::DisplaySurface*
nsScreenGonk::GetDisplaySurface()
{
    return mDisplaySurface.get();
}

int
nsScreenGonk::GetPrevDispAcquireFd()
{
    if (!mDisplaySurface.get()) {
        return -1;
    }
    return mDisplaySurface->GetPrevDispAcquireFd();
}
#endif

GonkDisplay::DisplayType
nsScreenGonk::GetDisplayType()
{
    return mDisplayType;
}

void
nsScreenGonk::SetEGLInfo(hwc_display_t aDisplay,
                         hwc_surface_t aSurface,
                         gl::GLContext* aGLContext)
{
    MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
    mEGLDisplay = aDisplay;
    mEGLSurface = aSurface;
    mGLContext = aGLContext;
}

hwc_display_t
nsScreenGonk::GetEGLDisplay()
{
    MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
    return mEGLDisplay;
}

hwc_surface_t
nsScreenGonk::GetEGLSurface()
{
    MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
    return mEGLSurface;
}

already_AddRefed<mozilla::gl::GLContext>
nsScreenGonk::GetGLContext()
{
    MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
    RefPtr<mozilla::gl::GLContext>glContext = mGLContext;
    return glContext.forget();
}

static void
UpdateMirroringWidgetSync(nsMainThreadPtrHandle<nsScreenGonk>&& aScreen, nsWindow* aWindow)
{
    MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
    already_AddRefed<nsWindow> window(aWindow);
    aScreen->UpdateMirroringWidget(window);
}

bool
nsScreenGonk::EnableMirroring()
{
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!IsPrimaryScreen());

    RefPtr<nsScreenGonk> primaryScreen = nsScreenManagerGonk::GetPrimaryScreen();
    NS_ENSURE_TRUE(primaryScreen, false);

    bool ret = primaryScreen->SetMirroringScreen(this);
    NS_ENSURE_TRUE(ret, false);

    // Create a widget for mirroring
    nsWidgetInitData initData;
    initData.mScreenId = mId;
    RefPtr<nsWindow> window = new nsWindow();
    window->Create(nullptr, nullptr, mNaturalBounds, &initData);
    MOZ_ASSERT(static_cast<nsWindow*>(window)->GetScreen() == this);

    // Update mMirroringWidget on compositor thread
    nsMainThreadPtrHandle<nsScreenGonk> primary =
      nsMainThreadPtrHandle<nsScreenGonk>(new nsMainThreadPtrHolder<nsScreenGonk>(primaryScreen, false));
    CompositorThreadHolder::Loop()->PostTask(
        NewRunnableFunction(&UpdateMirroringWidgetSync,
                            primary,
                            window.forget().take()));

    mIsMirroring = true;
    return true;
}

bool
nsScreenGonk::DisableMirroring()
{
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!IsPrimaryScreen());

    mIsMirroring = false;
    RefPtr<nsScreenGonk> primaryScreen = nsScreenManagerGonk::GetPrimaryScreen();
    NS_ENSURE_TRUE(primaryScreen, false);

    bool ret = primaryScreen->ClearMirroringScreen(this);
    NS_ENSURE_TRUE(ret, false);

    // Update mMirroringWidget on compositor thread
    nsMainThreadPtrHandle<nsScreenGonk> primary =
      nsMainThreadPtrHandle<nsScreenGonk>(new nsMainThreadPtrHolder<nsScreenGonk>(primaryScreen, false));
    CompositorThreadHolder::Loop()->PostTask(
        NewRunnableFunction(&UpdateMirroringWidgetSync,
                            primary,
                            nullptr));
    return true;
}

bool
nsScreenGonk::SetMirroringScreen(nsScreenGonk* aScreen)
{
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(IsPrimaryScreen());

    if (mMirroringScreen) {
        return false;
    }
    mMirroringScreen = aScreen;
    return true;
}

bool
nsScreenGonk::ClearMirroringScreen(nsScreenGonk* aScreen)
{
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(IsPrimaryScreen());

    if (mMirroringScreen != aScreen) {
        return false;
    }
    mMirroringScreen = nullptr;
    return true;
}

void
nsScreenGonk::UpdateMirroringWidget(already_AddRefed<nsWindow>& aWindow)
{
    MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
    MOZ_ASSERT(IsPrimaryScreen());

    if (mMirroringWidget) {
        nsCOMPtr<nsIWidget> widget = mMirroringWidget.forget();
        NS_ReleaseOnMainThread(widget.forget());
    }
    mMirroringWidget = aWindow;
}

nsWindow*
nsScreenGonk::GetMirroringWidget()
{
    MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
    MOZ_ASSERT(IsPrimaryScreen());

    return mMirroringWidget;
}

NS_IMPL_ISUPPORTS(nsScreenManagerGonk, nsIScreenManager)

nsScreenManagerGonk::nsScreenManagerGonk()
    : mInitialized(false)
#if ANDROID_VERSION >= 19
    , mDisplayEnabled(false)
#endif
{
}

nsScreenManagerGonk::~nsScreenManagerGonk()
{
}

static StaticRefPtr<nsScreenManagerGonk> sScreenManagerGonk;

/* static */ already_AddRefed<nsScreenManagerGonk>
nsScreenManagerGonk::GetInstance()
{
    MOZ_ASSERT(NS_IsMainThread());

    // Avoid creating nsScreenManagerGonk from content process.
    if (!XRE_IsParentProcess()) {
        MOZ_CRASH("Non-chrome processes should not get here.");
    }

    // Avoid creating multiple nsScreenManagerGonk instance inside main process.
    if (!sScreenManagerGonk) {
      sScreenManagerGonk = new nsScreenManagerGonk();
      ClearOnShutdown(&sScreenManagerGonk);
    }

    RefPtr<nsScreenManagerGonk> screenMgr = sScreenManagerGonk.get();
    return screenMgr.forget();
}

/* static */ already_AddRefed< nsScreenGonk>
nsScreenManagerGonk::GetPrimaryScreen()
{
    MOZ_ASSERT(NS_IsMainThread());

    RefPtr<nsScreenManagerGonk> manager = nsScreenManagerGonk::GetInstance();
    nsCOMPtr<nsIScreen> screen;
    manager->GetPrimaryScreen(getter_AddRefs(screen));
    MOZ_ASSERT(screen);
    return already_AddRefed<nsScreenGonk>(
        static_cast<nsScreenGonk*>(screen.forget().take()));
}

void
nsScreenManagerGonk::Initialize()
{
    if (mInitialized) {
        return;
    }

    mScreenOnEvent = new ScreenOnOffEvent(true);
    mScreenOffEvent = new ScreenOnOffEvent(false);
    GetGonkDisplay()->OnEnabled(displayEnabledCallback);

    AddScreen(GonkDisplay::DISPLAY_PRIMARY);

    nsAppShell::NotifyScreenInitialized();
    mInitialized = true;
}

void
nsScreenManagerGonk::DisplayEnabled(bool aEnabled)
{
    MOZ_ASSERT(NS_IsMainThread());

#if ANDROID_VERSION >= 19
    /* Bug 1244044
     * This function could be called before |mCompositorVsyncScheduler| is set.
     * To avoid this issue, keep the value stored in |mDisplayEnabled|.
     */
    mDisplayEnabled = aEnabled;
    if (mCompositorVsyncScheduler) {
        mCompositorVsyncScheduler->SetDisplay(mDisplayEnabled);
    }
#endif

    VsyncControl(aEnabled);
    NS_DispatchToMainThread(aEnabled ? mScreenOnEvent : mScreenOffEvent);
}

NS_IMETHODIMP
nsScreenManagerGonk::GetPrimaryScreen(nsIScreen **outScreen)
{
    NS_IF_ADDREF(*outScreen = mScreens[0].get());
    return NS_OK;
}

NS_IMETHODIMP
nsScreenManagerGonk::ScreenForId(uint32_t aId,
                                 nsIScreen **outScreen)
{
    for (size_t i = 0; i < mScreens.Length(); i++) {
        if (mScreens[i]->GetId() == aId) {
            NS_IF_ADDREF(*outScreen = mScreens[i].get());
            return NS_OK;
        }
    }

    *outScreen = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsScreenManagerGonk::ScreenForRect(int32_t inLeft,
                                   int32_t inTop,
                                   int32_t inWidth,
                                   int32_t inHeight,
                                   nsIScreen **outScreen)
{
    // Since all screens have independent coordinate system, we could
    // only return the primary screen no matter what rect is given.
    return GetPrimaryScreen(outScreen);
}

NS_IMETHODIMP
nsScreenManagerGonk::ScreenForNativeWidget(void *aWidget, nsIScreen **outScreen)
{
    for (size_t i = 0; i < mScreens.Length(); i++) {
        if (aWidget == mScreens[i]->GetNativeWindow()) {
            NS_IF_ADDREF(*outScreen = mScreens[i].get());
            return NS_OK;
        }
    }

    *outScreen = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsScreenManagerGonk::GetNumberOfScreens(uint32_t *aNumberOfScreens)
{
    *aNumberOfScreens = mScreens.Length();
    return NS_OK;
}

NS_IMETHODIMP
nsScreenManagerGonk::GetSystemDefaultScale(float *aDefaultScale)
{
    *aDefaultScale = 1.0f;
    return NS_OK;
}

void
nsScreenManagerGonk::VsyncControl(bool aEnabled)
{
    if (!NS_IsMainThread()) {
        NS_DispatchToMainThread(
            NewRunnableMethod<bool>(this,
                                    &nsScreenManagerGonk::VsyncControl,
                                    aEnabled));
        return;
    }

    MOZ_ASSERT(NS_IsMainThread());
    VsyncSource::Display &display = gfxPlatform::GetPlatform()->GetHardwareVsync()->GetGlobalDisplay();
    if (aEnabled) {
        display.EnableVsync();
    } else {
        display.DisableVsync();
    }
}

uint32_t
nsScreenManagerGonk::GetIdFromType(GonkDisplay::DisplayType aDisplayType)
{
    // This is the only place where we make the assumption that
    // display type is equivalent to screen id.

    // Bug 1138287 will address the conversion from type to id.
    return aDisplayType;
}

bool
nsScreenManagerGonk::IsScreenConnected(uint32_t aId)
{
    for (size_t i = 0; i < mScreens.Length(); ++i) {
        if (mScreens[i]->GetId() == aId) {
            return true;
        }
    }

    return false;
}

namespace {

// A concrete class as a subject for 'display-changed' observer event.
class DisplayInfo : public nsIDisplayInfo {
public:
    NS_DECL_ISUPPORTS

    DisplayInfo(uint32_t aId, bool aConnected)
        : mId(aId)
        , mConnected(aConnected)
    {
    }

    NS_IMETHODIMP GetId(int32_t *aId)
    {
        *aId = mId;
        return NS_OK;
    }

    NS_IMETHODIMP GetConnected(bool *aConnected)
    {
        *aConnected = mConnected;
        return NS_OK;
    }

private:
    virtual ~DisplayInfo() {}

    uint32_t mId;
    bool mConnected;
};

NS_IMPL_ISUPPORTS(DisplayInfo, nsIDisplayInfo, nsISupports)

class NotifyTask : public mozilla::Runnable {
public:
    NotifyTask(uint32_t aId, bool aConnected)
        : mDisplayInfo(new DisplayInfo(aId, aConnected))
    {
    }

    NS_IMETHOD Run()
    {
        nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
        if (os) {
          os->NotifyObservers(mDisplayInfo, "display-changed", nullptr);
        }

        return NS_OK;
    }
private:
    RefPtr<DisplayInfo> mDisplayInfo;
};

void
NotifyDisplayChange(uint32_t aId, bool aConnected)
{
    NS_DispatchToMainThread(new NotifyTask(aId, aConnected));
}

} // end of unnamed namespace.

nsresult
nsScreenManagerGonk::AddScreen(GonkDisplay::DisplayType aDisplayType,
                               android::IGraphicBufferProducer* aSink,
                               NotifyDisplayChangedEvent aEventVisibility)
{
    MOZ_ASSERT(NS_IsMainThread());

    NS_ENSURE_TRUE(aDisplayType < GonkDisplay::DisplayType::NUM_DISPLAY_TYPES,
                   NS_ERROR_FAILURE);

    uint32_t id = GetIdFromType(aDisplayType);
    NS_ENSURE_TRUE(!IsScreenConnected(id), NS_ERROR_FAILURE);

    GonkDisplay::NativeData nativeData =
        GetGonkDisplay()->GetNativeData(aDisplayType, aSink);
    nsScreenGonk* screen = new nsScreenGonk(id,
                                            aDisplayType,
                                            nativeData,
                                            aEventVisibility);
    mScreens.AppendElement(screen);

    if (aEventVisibility == NotifyDisplayChangedEvent::Observable) {
        NotifyDisplayChange(id, true);
    }

    // By default, non primary screen does mirroring.
    if (aDisplayType != GonkDisplay::DISPLAY_PRIMARY &&
        gfxPrefs::ScreenMirroringEnabled()) {
        screen->EnableMirroring();
    }

    return NS_OK;
}

nsresult
nsScreenManagerGonk::RemoveScreen(GonkDisplay::DisplayType aDisplayType)
{
    MOZ_ASSERT(NS_IsMainThread());

    NS_ENSURE_TRUE(aDisplayType < GonkDisplay::DisplayType::NUM_DISPLAY_TYPES,
                   NS_ERROR_FAILURE);

    NotifyDisplayChangedEvent eventVisibility = NotifyDisplayChangedEvent::Observable;
    uint32_t screenId = GetIdFromType(aDisplayType);
    NS_ENSURE_TRUE(IsScreenConnected(screenId), NS_ERROR_FAILURE);

    for (size_t i = 0; i < mScreens.Length(); i++) {
        if (mScreens[i]->GetId() == screenId) {
            if (mScreens[i]->IsMirroring()) {
                mScreens[i]->DisableMirroring();
            }
            eventVisibility = mScreens[i]->GetEventVisibility();
            mScreens.RemoveElementAt(i);
            break;
        }
    }

    if (eventVisibility == NotifyDisplayChangedEvent::Observable) {
      NotifyDisplayChange(screenId, false);
    }
    return NS_OK;
}

#if ANDROID_VERSION >= 19
void
nsScreenManagerGonk::SetCompositorVsyncScheduler(mozilla::layers::CompositorVsyncScheduler *aObserver)
{
    MOZ_ASSERT(NS_IsMainThread());

    // We assume on b2g that there is only 1 CompositorBridgeParent
    MOZ_ASSERT(mCompositorVsyncScheduler == nullptr);
    MOZ_ASSERT(aObserver);
    mCompositorVsyncScheduler = aObserver;
    mCompositorVsyncScheduler->SetDisplay(mDisplayEnabled);
}
#endif
