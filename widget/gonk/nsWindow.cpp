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

#include "mozilla/DebugOnly.h"

#include <fcntl.h>

#include "android/log.h"

#include "mozilla/dom/TabParent.h"
#include "mozilla/Hal.h"
#include "mozilla/Preferences.h"
#include "mozilla/FileUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "Framebuffer.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "GLContextProvider.h"
#include "GLContext.h"
#include "nsAutoPtr.h"
#include "nsAppShell.h"
#include "nsIdleService.h"
#include "nsScreenManagerGonk.h"
#include "nsTArray.h"
#include "nsWindow.h"
#include "nsIWidgetListener.h"
#include "cutils/properties.h"
#include "ClientLayerManager.h"
#include "BasicLayers.h"
#include "libdisplay/GonkDisplay.h"
#include "pixelflinger/format.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/layers/CompositorParent.h"
#include "ParentProcessController.h"
#include "nsThreadUtils.h"
#include "HwcComposer2D.h"

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk" , ## args)
#define LOGW(args...) __android_log_print(ANDROID_LOG_WARN, "Gonk", ## args)
#define LOGE(args...) __android_log_print(ANDROID_LOG_ERROR, "Gonk", ## args)

#define IS_TOPLEVEL() (mWindowType == eWindowType_toplevel || mWindowType == eWindowType_dialog)

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::hal;
using namespace mozilla::gl;
using namespace mozilla::layers;
using namespace mozilla::widget;

nsIntRect gScreenBounds;
static uint32_t sScreenRotation;
static uint32_t sPhysicalScreenRotation;
static nsIntRect sVirtualBounds;

static nsRefPtr<GLContext> sGLContext;
static nsTArray<nsWindow *> sTopWindows;
static nsWindow *gFocusedWindow = nullptr;
static bool sFramebufferOpen;
static bool sUsingOMTC;
static bool sUsingHwc;
static bool sScreenInitialized;
static nsRefPtr<gfxASurface> sOMTCSurface;

namespace {

static uint32_t
EffectiveScreenRotation()
{
    return (sScreenRotation + sPhysicalScreenRotation) % (360 / 90);
}

class ScreenOnOffEvent : public nsRunnable {
public:
    ScreenOnOffEvent(bool on)
        : mIsOn(on)
    {}

    NS_IMETHOD Run() {
        for (uint32_t i = 0; i < sTopWindows.Length(); i++) {
            nsWindow *win = sTopWindows[i];

            if (nsIWidgetListener* listener = win->GetWidgetListener()) {
                listener->SizeModeChanged(mIsOn ? nsSizeMode_Fullscreen : nsSizeMode_Minimized);
            }
        }

        // Notify observers that the screen state has just changed.
        nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
        if (observerService) {
          observerService->NotifyObservers(
            nullptr, "screen-state-changed",
            mIsOn ? MOZ_UTF16("on") : MOZ_UTF16("off")
          );
        }

        return NS_OK;
    }

private:
    bool mIsOn;
};

static StaticRefPtr<ScreenOnOffEvent> sScreenOnEvent;
static StaticRefPtr<ScreenOnOffEvent> sScreenOffEvent;

static void
displayEnabledCallback(bool enabled)
{
    NS_DispatchToMainThread(enabled ? sScreenOnEvent : sScreenOffEvent);
}

} // anonymous namespace

nsWindow::nsWindow()
{
    if (!sScreenInitialized) {
        sScreenOnEvent = new ScreenOnOffEvent(true);
        ClearOnShutdown(&sScreenOnEvent);
        sScreenOffEvent = new ScreenOnOffEvent(false);
        ClearOnShutdown(&sScreenOffEvent);
        GetGonkDisplay()->OnEnabled(displayEnabledCallback);

        nsIntSize screenSize;
        bool gotFB = Framebuffer::GetSize(&screenSize);
        if (!gotFB) {
            NS_RUNTIMEABORT("Failed to get size from framebuffer, aborting...");
        }
        gScreenBounds = nsIntRect(nsIntPoint(0, 0), screenSize);

        char propValue[PROPERTY_VALUE_MAX];
        property_get("ro.sf.hwrotation", propValue, "0");
        sPhysicalScreenRotation = atoi(propValue) / 90;

        sVirtualBounds = gScreenBounds;

        sScreenInitialized = true;

        nsAppShell::NotifyScreenInitialized();

        // This is a hack to force initialization of the compositor
        // resources, if we're going to use omtc.
        //
        // NB: GetPlatform() will create the gfxPlatform, which wants
        // to know the color depth, which asks our native window.
        // This has to happen after other init has finished.
        gfxPlatform::GetPlatform();
        sUsingOMTC = ShouldUseOffMainThreadCompositing();

        property_get("ro.display.colorfill", propValue, "0");

        //Update sUsingHwc whenever layers.composer2d.enabled changes
        Preferences::AddBoolVarCache(&sUsingHwc, "layers.composer2d.enabled");

        if (sUsingOMTC) {
          sOMTCSurface = new gfxImageSurface(gfxIntSize(1, 1),
                                             gfxImageFormatRGB24);
        }
    }
}

nsWindow::~nsWindow()
{
}

void
nsWindow::DoDraw(void)
{
    if (!hal::GetScreenEnabled()) {
        gDrawRequest = true;
        return;
    }

    if (sTopWindows.IsEmpty()) {
        LOG("  no window to draw, bailing");
        return;
    }

    nsWindow *targetWindow = (nsWindow *)sTopWindows[0];
    while (targetWindow->GetLastChild())
        targetWindow = (nsWindow *)targetWindow->GetLastChild();

    nsIntRegion region = sTopWindows[0]->mDirtyRegion;
    sTopWindows[0]->mDirtyRegion.SetEmpty();

    nsIWidgetListener* listener = targetWindow->GetWidgetListener();
    if (listener) {
        listener->WillPaintWindow(targetWindow);
    }

    LayerManager* lm = targetWindow->GetLayerManager();
    if (mozilla::layers::LAYERS_CLIENT == lm->GetBackendType()) {
      // No need to do anything, the compositor will handle drawing
    } else if (mozilla::layers::LAYERS_BASIC == lm->GetBackendType()) {
        MOZ_ASSERT(sFramebufferOpen || sUsingOMTC);
        nsRefPtr<gfxASurface> targetSurface;

        if(sUsingOMTC)
            targetSurface = sOMTCSurface;
        else
            targetSurface = Framebuffer::BackBuffer();

        {
            nsRefPtr<gfxContext> ctx = new gfxContext(targetSurface);
            gfxUtils::PathFromRegion(ctx, region);
            ctx->Clip();

            // No double-buffering needed.
            AutoLayerManagerSetup setupLayerManager(
                targetWindow, ctx, mozilla::layers::BUFFER_NONE,
                ScreenRotation(EffectiveScreenRotation()));

            listener = targetWindow->GetWidgetListener();
            if (listener) {
                listener->PaintWindow(targetWindow, region);
            }
        }

        if (!sUsingOMTC) {
            targetSurface->Flush();
            Framebuffer::Present(region);
        }
    } else {
        NS_RUNTIMEABORT("Unexpected layer manager type");
    }

    listener = targetWindow->GetWidgetListener();
    if (listener) {
        listener->DidPaintWindow();
    }
}

nsEventStatus
nsWindow::DispatchInputEvent(WidgetGUIEvent& aEvent, bool* aWasCaptured)
{
    if (aWasCaptured) {
        *aWasCaptured = false;
    }
    if (!gFocusedWindow) {
        return nsEventStatus_eIgnore;
    }

    gFocusedWindow->UserActivity();

    aEvent.widget = gFocusedWindow;

    if (TabParent* capturer = TabParent::GetEventCapturer()) {
        bool captured = capturer->TryCapture(aEvent);
        if (aWasCaptured) {
            *aWasCaptured = captured;
        }
        if (captured) {
            return nsEventStatus_eConsumeNoDefault;
        }
    }

    nsEventStatus status;
    gFocusedWindow->DispatchEvent(&aEvent, status);
    return status;
}

NS_IMETHODIMP
nsWindow::Create(nsIWidget *aParent,
                 void *aNativeParent,
                 const nsIntRect &aRect,
                 nsDeviceContext *aContext,
                 nsWidgetInitData *aInitData)
{
    BaseCreate(aParent, IS_TOPLEVEL() ? sVirtualBounds : aRect,
               aContext, aInitData);

    mBounds = aRect;

    mParent = (nsWindow *)aParent;
    mVisible = false;

    if (!aParent) {
        mBounds = sVirtualBounds;
    }

    if (!IS_TOPLEVEL())
        return NS_OK;

    sTopWindows.AppendElement(this);

    Resize(0, 0, sVirtualBounds.width, sVirtualBounds.height, false);
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Destroy(void)
{
    mOnDestroyCalled = true;
    sTopWindows.RemoveElement(this);
    if (this == gFocusedWindow)
        gFocusedWindow = nullptr;
    nsBaseWidget::OnDestroy();
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Show(bool aState)
{
    if (mWindowType == eWindowType_invisible)
        return NS_OK;

    if (mVisible == aState)
        return NS_OK;

    mVisible = aState;
    if (!IS_TOPLEVEL())
        return mParent ? mParent->Show(aState) : NS_OK;

    if (aState) {
        BringToTop();
    } else {
        for (unsigned int i = 0; i < sTopWindows.Length(); i++) {
            nsWindow *win = sTopWindows[i];
            if (!win->mVisible)
                continue;

            win->BringToTop();
            break;
        }
    }

    return NS_OK;
}

bool
nsWindow::IsVisible() const
{
    return mVisible;
}

NS_IMETHODIMP
nsWindow::ConstrainPosition(bool aAllowSlop,
                            int32_t *aX,
                            int32_t *aY)
{
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Move(double aX,
               double aY)
{
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Resize(double aWidth,
                 double aHeight,
                 bool   aRepaint)
{
    return Resize(0, 0, aWidth, aHeight, aRepaint);
}

NS_IMETHODIMP
nsWindow::Resize(double aX,
                 double aY,
                 double aWidth,
                 double aHeight,
                 bool   aRepaint)
{
    mBounds = nsIntRect(NSToIntRound(aX), NSToIntRound(aY),
                        NSToIntRound(aWidth), NSToIntRound(aHeight));
    if (mWidgetListener)
        mWidgetListener->WindowResized(this, mBounds.width, mBounds.height);

    if (aRepaint)
        Invalidate(sVirtualBounds);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Enable(bool aState)
{
    return NS_OK;
}

bool
nsWindow::IsEnabled() const
{
    return true;
}

NS_IMETHODIMP
nsWindow::SetFocus(bool aRaise)
{
    if (aRaise)
        BringToTop();

    gFocusedWindow = this;
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::ConfigureChildren(const nsTArray<nsIWidget::Configuration>&)
{
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Invalidate(const nsIntRect &aRect)
{
    nsWindow *top = mParent;
    while (top && top->mParent)
        top = top->mParent;
    if (top != sTopWindows[0] && this != sTopWindows[0])
        return NS_OK;

    mDirtyRegion.Or(mDirtyRegion, aRect);
    gDrawRequest = true;
    mozilla::NotifyEvent();
    return NS_OK;
}

nsIntPoint
nsWindow::WidgetToScreenOffset()
{
    nsIntPoint p(0, 0);
    nsWindow *w = this;

    while (w && w->mParent) {
        p.x += w->mBounds.x;
        p.y += w->mBounds.y;

        w = w->mParent;
    }

    return p;
}

void*
nsWindow::GetNativeData(uint32_t aDataType)
{
    switch (aDataType) {
    case NS_NATIVE_WINDOW:
        return GetGonkDisplay()->GetNativeWindow();
    }
    return nullptr;
}

NS_IMETHODIMP
nsWindow::DispatchEvent(WidgetGUIEvent* aEvent, nsEventStatus& aStatus)
{
    if (mWidgetListener)
      aStatus = mWidgetListener->HandleEvent(aEvent, mUseAttachedEvents);
    return NS_OK;
}

NS_IMETHODIMP_(void)
nsWindow::SetInputContext(const InputContext& aContext,
                          const InputContextAction& aAction)
{
    mInputContext = aContext;
}

NS_IMETHODIMP_(InputContext)
nsWindow::GetInputContext()
{
    // There is only one IME context on Gonk.
    mInputContext.mNativeIMEContext = nullptr;
    return mInputContext;
}

NS_IMETHODIMP
nsWindow::ReparentNativeWidget(nsIWidget* aNewParent)
{
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::MakeFullScreen(bool aFullScreen)
{
    if (mWindowType != eWindowType_toplevel) {
        // Ignore fullscreen request for non-toplevel windows.
        NS_WARNING("MakeFullScreen() on a dialog or child widget?");
        return nsBaseWidget::MakeFullScreen(aFullScreen);
    }

    if (aFullScreen) {
        // Fullscreen is "sticky" for toplevel widgets on gonk: we
        // must paint the entire screen, and should only have one
        // toplevel widget, so it doesn't make sense to ever "exit"
        // fullscreen.  If we do, we can leave parts of the screen
        // unpainted.
        Resize(sVirtualBounds.x, sVirtualBounds.y,
               sVirtualBounds.width, sVirtualBounds.height,
               /*repaint*/true);
    }
    return NS_OK;
}

float
nsWindow::GetDPI()
{
    return GetGonkDisplay()->xdpi;
}

double
nsWindow::GetDefaultScaleInternal()
{
    float dpi = GetDPI();
    // The mean pixel density for mdpi devices is 160dpi, 240dpi for hdpi,
    // and 320dpi for xhdpi, respectively.
    // We'll take the mid-value between these three numbers as the boundary.
    if (dpi < 200.0) {
        return 1.0; // mdpi devices.
    }
    if (dpi < 300.0) {
        return 1.5; // hdpi devices.
    }
    // xhdpi devices and beyond.
    return floor(dpi / 150.0);
}

LayerManager *
nsWindow::GetLayerManager(PLayerTransactionChild* aShadowManager,
                          LayersBackend aBackendHint,
                          LayerManagerPersistence aPersistence,
                          bool* aAllowRetaining)
{
    if (aAllowRetaining)
        *aAllowRetaining = true;
    if (mLayerManager) {
        // This layer manager might be used for painting outside of DoDraw(), so we need
        // to set the correct rotation on it.
        if (mLayerManager->GetBackendType() == LAYERS_BASIC) {
            BasicLayerManager* manager =
                static_cast<BasicLayerManager*>(mLayerManager.get());
            manager->SetDefaultTargetConfiguration(mozilla::layers::BUFFER_NONE,
                                                   ScreenRotation(EffectiveScreenRotation()));
        } else if (mLayerManager->GetBackendType() == LAYERS_CLIENT) {
            ClientLayerManager* manager =
                static_cast<ClientLayerManager*>(mLayerManager.get());
            manager->SetDefaultTargetConfiguration(mozilla::layers::BUFFER_NONE,
                                                   ScreenRotation(EffectiveScreenRotation()));
        }
        return mLayerManager;
    }

    // Set mUseLayersAcceleration here to make it consistent with
    // nsBaseWidget::GetLayerManager
    mUseLayersAcceleration = ComputeShouldAccelerate(mUseLayersAcceleration);
    nsWindow *topWindow = sTopWindows[0];

    if (!topWindow) {
        LOGW(" -- no topwindow\n");
        return nullptr;
    }

    if (sUsingOMTC) {
        CreateCompositor();
        if (mCompositorParent) {
            uint64_t rootLayerTreeId = mCompositorParent->RootLayerTreeId();
            CompositorParent::SetControllerForLayerTree(rootLayerTreeId, new ParentProcessController());
        }
        if (mLayerManager)
            return mLayerManager;
    }

    if (mUseLayersAcceleration) {
        DebugOnly<nsIntRect> fbBounds = gScreenBounds;
        if (!sGLContext) {
            sGLContext = GLContextProvider::CreateForWindow(this);
        }

        MOZ_ASSERT(fbBounds.value == gScreenBounds);
    }

    // Fall back to software rendering.
    sFramebufferOpen = Framebuffer::Open();
    if (sFramebufferOpen) {
        LOG("Falling back to framebuffer software rendering");
    } else {
        LOGE("Failed to mmap fb(?!?), aborting ...");
        NS_RUNTIMEABORT("Can't open GL context and can't fall back on /dev/graphics/fb0 ...");
    }

    mLayerManager = new ClientLayerManager(this);
    mUseLayersAcceleration = false;

    return mLayerManager;
}

gfxASurface *
nsWindow::GetThebesSurface()
{
    /* This is really a dummy surface; this is only used when doing reflow, because
     * we need a RenderingContext to measure text against.
     */

    // XXX this really wants to return already_AddRefed, but this only really gets used
    // on direct assignment to a gfxASurface
    return new gfxImageSurface(gfxIntSize(5,5), gfxImageFormatRGB24);
}

void
nsWindow::BringToTop()
{
    if (!sTopWindows.IsEmpty()) {
        if (nsIWidgetListener* listener = sTopWindows[0]->GetWidgetListener())
            listener->WindowDeactivated();
    }

    sTopWindows.RemoveElement(this);
    sTopWindows.InsertElementAt(0, this);

    if (mWidgetListener)
        mWidgetListener->WindowActivated();
    Invalidate(sVirtualBounds);
}

void
nsWindow::UserActivity()
{
    if (!mIdleService) {
        mIdleService = do_GetService("@mozilla.org/widget/idleservice;1");
    }

    if (mIdleService) {
        mIdleService->ResetIdleTimeOut(0);
    }
}

uint32_t
nsWindow::GetGLFrameBufferFormat()
{
    if (mLayerManager &&
        mLayerManager->GetBackendType() == mozilla::layers::LAYERS_OPENGL) {
        // We directly map the hardware fb on Gonk.  The hardware fb
        // has RGB format.
        return LOCAL_GL_RGB;
    }
    return LOCAL_GL_NONE;
}

nsIntRect
nsWindow::GetNaturalBounds()
{
    return gScreenBounds;
}

bool
nsWindow::NeedsPaint()
{
  if (!mLayerManager) {
    return false;
  }
  return nsIWidget::NeedsPaint();
}

Composer2D*
nsWindow::GetComposer2D()
{
    if (!sUsingHwc) {
        return nullptr;
    }

    if (HwcComposer2D* hwc = HwcComposer2D::GetInstance()) {
        return hwc->Initialized() ? hwc : nullptr;
    }

    return nullptr;
}

// nsScreenGonk.cpp

nsScreenGonk::nsScreenGonk(void *nativeScreen)
{
}

nsScreenGonk::~nsScreenGonk()
{
}

NS_IMETHODIMP
nsScreenGonk::GetRect(int32_t *outLeft,  int32_t *outTop,
                      int32_t *outWidth, int32_t *outHeight)
{
    *outLeft = sVirtualBounds.x;
    *outTop = sVirtualBounds.y;

    *outWidth = sVirtualBounds.width;
    *outHeight = sVirtualBounds.height;

    return NS_OK;
}

NS_IMETHODIMP
nsScreenGonk::GetAvailRect(int32_t *outLeft,  int32_t *outTop,
                           int32_t *outWidth, int32_t *outHeight)
{
    return GetRect(outLeft, outTop, outWidth, outHeight);
}

static uint32_t
ColorDepth()
{
    switch (GetGonkDisplay()->surfaceformat) {
    case GGL_PIXEL_FORMAT_RGB_565:
        return 16;
    case GGL_PIXEL_FORMAT_RGBA_8888:
        return 32;
    }
    return 24; // GGL_PIXEL_FORMAT_RGBX_8888
}

NS_IMETHODIMP
nsScreenGonk::GetPixelDepth(int32_t *aPixelDepth)
{
    // XXX: this should actually return 32 when we're using 24-bit
    // color, because we use RGBX.
    *aPixelDepth = ColorDepth();
    return NS_OK;
}

NS_IMETHODIMP
nsScreenGonk::GetColorDepth(int32_t *aColorDepth)
{
    return GetPixelDepth(aColorDepth);
}

NS_IMETHODIMP
nsScreenGonk::GetRotation(uint32_t* aRotation)
{
    *aRotation = sScreenRotation;
    return NS_OK;
}

NS_IMETHODIMP
nsScreenGonk::SetRotation(uint32_t aRotation)
{
    if (!(aRotation <= ROTATION_270_DEG))
        return NS_ERROR_ILLEGAL_VALUE;

    if (sScreenRotation == aRotation)
        return NS_OK;

    sScreenRotation = aRotation;
    uint32_t rotation = EffectiveScreenRotation();
    if (rotation == nsIScreen::ROTATION_90_DEG ||
        rotation == nsIScreen::ROTATION_270_DEG) {
        sVirtualBounds = nsIntRect(0, 0, gScreenBounds.height,
                                   gScreenBounds.width);
    } else {
        sVirtualBounds = gScreenBounds;
    }

    nsAppShell::NotifyScreenRotation();

    for (unsigned int i = 0; i < sTopWindows.Length(); i++)
        sTopWindows[i]->Resize(sVirtualBounds.width,
                               sVirtualBounds.height,
                               true);

    return NS_OK;
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

/*static*/ uint32_t
nsScreenGonk::GetRotation()
{
    return sScreenRotation;
}

/*static*/ ScreenConfiguration
nsScreenGonk::GetConfiguration()
{
    ScreenOrientation orientation = ComputeOrientation(sScreenRotation,
                                                       gScreenBounds.Size());
    uint32_t colorDepth = ColorDepth();
    // NB: perpetuating colorDepth == pixelDepth illusion here, for
    // consistency.
    return ScreenConfiguration(sVirtualBounds, orientation,
                               colorDepth, colorDepth);
}

NS_IMPL_ISUPPORTS1(nsScreenManagerGonk, nsIScreenManager)

nsScreenManagerGonk::nsScreenManagerGonk()
{
    mOneScreen = new nsScreenGonk(nullptr);
}

nsScreenManagerGonk::~nsScreenManagerGonk()
{
}

NS_IMETHODIMP
nsScreenManagerGonk::GetPrimaryScreen(nsIScreen **outScreen)
{
    NS_IF_ADDREF(*outScreen = mOneScreen.get());
    return NS_OK;
}

NS_IMETHODIMP
nsScreenManagerGonk::ScreenForRect(int32_t inLeft,
                                   int32_t inTop,
                                   int32_t inWidth,
                                   int32_t inHeight,
                                   nsIScreen **outScreen)
{
    return GetPrimaryScreen(outScreen);
}

NS_IMETHODIMP
nsScreenManagerGonk::ScreenForNativeWidget(void *aWidget, nsIScreen **outScreen)
{
    return GetPrimaryScreen(outScreen);
}

NS_IMETHODIMP
nsScreenManagerGonk::GetNumberOfScreens(uint32_t *aNumberOfScreens)
{
    *aNumberOfScreens = 1;
    return NS_OK;
}

NS_IMETHODIMP
nsScreenManagerGonk::GetSystemDefaultScale(float *aDefaultScale)
{
    *aDefaultScale = 1.0f;
    return NS_OK;
}
