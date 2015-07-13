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
#include "mozilla/Services.h"
#include "mozilla/ProcessPriorityManager.h"
#include "nsIdleService.h"
#include "nsIObserverService.h"
#include "nsAppShell.h"
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
using namespace mozilla::dom;

namespace {

class ScreenOnOffEvent : public nsRunnable {
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
            mIsOn ? MOZ_UTF16("on") : MOZ_UTF16("off")
          );
        }

        nsRefPtr<nsScreenGonk> screen = nsScreenManagerGonk::GetPrimaryScreen();
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
    nsRefPtr<nsScreenManagerGonk> screenManager = nsScreenManagerGonk::GetInstance();
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
                           const GonkDisplay::NativeData& aNativeData)
    : mId(aId)
    , mNativeWindow(aNativeData.mNativeWindow)
    , mDpi(aNativeData.mXdpi)
    , mScreenRotation(nsIScreen::ROTATION_0_DEG)
    , mPhysicalScreenRotation(nsIScreen::ROTATION_0_DEG)
#if ANDROID_VERSION >= 17
    , mDisplaySurface(aNativeData.mDisplaySurface)
#endif
    , mDisplayType(aDisplayType)
    , mDpy(EGL_NO_DISPLAY)
    , mSur(EGL_NO_SURFACE)
    , mGLContext(nullptr)
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

nsScreenGonk::~nsScreenGonk()
{
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

nsIntRect
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
        mVirtualBounds = nsIntRect(0, 0,
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

nsIntRect
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
static ScreenOrientation
ComputeOrientation(uint32_t aRotation, const nsIntSize& aScreenSize)
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

ScreenConfiguration
nsScreenGonk::GetConfiguration()
{
    ScreenOrientation orientation = ComputeOrientation(mScreenRotation,
                                                       mNaturalBounds.Size());

    // NB: perpetuating colorDepth == pixelDepth illusion here, for
    // consistency.
    return ScreenConfiguration(mVirtualBounds, orientation,
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
nsScreenGonk::SetEGLInfo(hwc_display_t aDisplay, hwc_surface_t aSurface,
                         gl::GLContext* aGLContext)
{
    mDpy = aDisplay;
    mSur = aSurface;
    mGLContext = aGLContext;
}

hwc_display_t
nsScreenGonk::GetDpy()
{
    return mDpy;
}

hwc_surface_t
nsScreenGonk::GetSur()
{
    return mSur;
}

NS_IMPL_ISUPPORTS(nsScreenManagerGonk, nsIScreenManager)

nsScreenManagerGonk::nsScreenManagerGonk()
    : mInitialized(false)
{
}

nsScreenManagerGonk::~nsScreenManagerGonk()
{
}

/* static */ already_AddRefed<nsScreenManagerGonk>
nsScreenManagerGonk::GetInstance()
{
    nsCOMPtr<nsIScreenManager> manager;
    manager = do_GetService("@mozilla.org/gfx/screenmanager;1");
    MOZ_ASSERT(manager);
    return already_AddRefed<nsScreenManagerGonk>(
        static_cast<nsScreenManagerGonk*>(manager.forget().take()));
}

/* static */ already_AddRefed< nsScreenGonk>
nsScreenManagerGonk::GetPrimaryScreen()
{
    nsRefPtr<nsScreenManagerGonk> manager = nsScreenManagerGonk::GetInstance();
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
    if (gfxPrefs::HardwareVsyncEnabled()) {
        VsyncControl(aEnabled);
    }

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
    MOZ_ASSERT(gfxPrefs::HardwareVsyncEnabled());

    if (!NS_IsMainThread()) {
        NS_DispatchToMainThread(
            NS_NewRunnableMethodWithArgs<bool>(this,
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

class NotifyTask : public nsRunnable {
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
    nsRefPtr<DisplayInfo> mDisplayInfo;
};

void
NotifyDisplayChange(uint32_t aId, bool aConnected)
{
    NS_DispatchToMainThread(new NotifyTask(aId, aConnected));
}

} // end of unnamed namespace.

nsresult
nsScreenManagerGonk::AddScreen(GonkDisplay::DisplayType aDisplayType,
                               android::IGraphicBufferProducer* aProducer)
{
    MOZ_ASSERT(NS_IsMainThread());

    NS_ENSURE_TRUE(aDisplayType < GonkDisplay::DisplayType::NUM_DISPLAY_TYPES,
                   NS_ERROR_FAILURE);

    uint32_t id = GetIdFromType(aDisplayType);
    NS_ENSURE_TRUE(!IsScreenConnected(id), NS_ERROR_FAILURE);

    GonkDisplay::NativeData nativeData =
        GetGonkDisplay()->GetNativeData(aDisplayType, aProducer);
    nsScreenGonk* screen = new nsScreenGonk(id, aDisplayType, nativeData);

    mScreens.AppendElement(screen);

    NotifyDisplayChange(id, true);

    return NS_OK;
}

nsresult
nsScreenManagerGonk::RemoveScreen(GonkDisplay::DisplayType aDisplayType)
{
    MOZ_ASSERT(NS_IsMainThread());

    NS_ENSURE_TRUE(aDisplayType < GonkDisplay::DisplayType::NUM_DISPLAY_TYPES,
                   NS_ERROR_FAILURE);

    uint32_t screenId = GetIdFromType(aDisplayType);
    NS_ENSURE_TRUE(IsScreenConnected(screenId), NS_ERROR_FAILURE);

    for (size_t i = 0; i < mScreens.Length(); i++) {
        if (mScreens[i]->GetId() == screenId) {
            mScreens.RemoveElementAt(i);
            break;
        }
    }

    NotifyDisplayChange(screenId, false);

    return NS_OK;
}
