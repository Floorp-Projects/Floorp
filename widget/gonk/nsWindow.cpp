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
#include "mozilla/ProcessPriorityManager.h"
#include "mozilla/Services.h"
#include "mozilla/FileUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "GLContextProvider.h"
#include "GLContext.h"
#include "nsAutoPtr.h"
#include "nsAppShell.h"
#include "nsIdleService.h"
#include "nsIObserverService.h"
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
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/InputAPZContext.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TouchEvents.h"
#include "nsThreadUtils.h"
#include "HwcComposer2D.h"

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk" , ## args)
#define LOGW(args...) __android_log_print(ANDROID_LOG_WARN, "Gonk", ## args)
#define LOGE(args...) __android_log_print(ANDROID_LOG_ERROR, "Gonk", ## args)

#define IS_TOPLEVEL() (mWindowType == eWindowType_toplevel || mWindowType == eWindowType_dialog)

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::hal;
using namespace mozilla::gfx;
using namespace mozilla::gl;
using namespace mozilla::layers;
using namespace mozilla::widget;

nsIntRect gScreenBounds;
static uint32_t sScreenRotation;
static uint32_t sPhysicalScreenRotation;
static nsIntRect sVirtualBounds;

static nsTArray<nsWindow *> sTopWindows;
static nsWindow *gFocusedWindow = nullptr;
static bool sUsingHwc;
static bool sScreenInitialized;

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
        // When the screen is off prevent priority changes.
        if (mIsOn) {
          ProcessPriorityManager::Unfreeze();
        } else {
          ProcessPriorityManager::Freeze();
        }

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
    HwcComposer2D::GetInstance()->EnableVsync(enabled);
    NS_DispatchToMainThread(enabled ? sScreenOnEvent : sScreenOffEvent);
}

} // anonymous namespace

NS_IMPL_ISUPPORTS_INHERITED0(nsWindow, nsBaseWidget)

nsWindow::nsWindow()
{
    mFramebuffer = nullptr;

    if (sScreenInitialized)
        return;

    sScreenOnEvent = new ScreenOnOffEvent(true);
    ClearOnShutdown(&sScreenOnEvent);
    sScreenOffEvent = new ScreenOnOffEvent(false);
    ClearOnShutdown(&sScreenOffEvent);
    GetGonkDisplay()->OnEnabled(displayEnabledCallback);

    nsIntSize screenSize;

    ANativeWindow *win = GetGonkDisplay()->GetNativeWindow();

    if (win->query(win, NATIVE_WINDOW_WIDTH, &screenSize.width) ||
        win->query(win, NATIVE_WINDOW_HEIGHT, &screenSize.height)) {
        NS_RUNTIMEABORT("Failed to get native window size, aborting...");
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
    if (!ShouldUseOffMainThreadCompositing()) {
        MOZ_CRASH("How can we render apps, then?");
    }
    // Update sUsingHwc whenever layers.composer2d.enabled changes
    Preferences::AddBoolVarCache(&sUsingHwc, "layers.composer2d.enabled");
}

nsWindow::~nsWindow()
{
    HwcComposer2D::GetInstance()->SetCompositorParent(nullptr);
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

    nsIWidgetListener* listener = targetWindow->GetWidgetListener();
    if (listener) {
        listener->WillPaintWindow(targetWindow);
    }

    LayerManager* lm = targetWindow->GetLayerManager();
    if (mozilla::layers::LayersBackend::LAYERS_CLIENT == lm->GetBackendType()) {
      // No need to do anything, the compositor will handle drawing
    } else {
        NS_RUNTIMEABORT("Unexpected layer manager type");
    }

    listener = targetWindow->GetWidgetListener();
    if (listener) {
        listener->DidPaintWindow();
    }
}

void
nsWindow::ConfigureAPZControllerThread()
{
  APZThreadUtils::SetControllerThread(CompositorParent::CompositorLoop());
}

/*static*/ nsEventStatus
nsWindow::DispatchInputEvent(WidgetGUIEvent& aEvent)
{
    if (!gFocusedWindow) {
        return nsEventStatus_eIgnore;
    }

    gFocusedWindow->UserActivity();

    nsEventStatus status;
    aEvent.widget = gFocusedWindow;
    gFocusedWindow->DispatchEvent(&aEvent, status);
    return status;
}

/*static*/ void
nsWindow::DispatchTouchInput(MultiTouchInput& aInput)
{
    APZThreadUtils::AssertOnControllerThread();

    if (!gFocusedWindow) {
        return;
    }

    gFocusedWindow->DispatchTouchInputViaAPZ(aInput);
}

class DispatchTouchInputOnMainThread : public nsRunnable
{
public:
    DispatchTouchInputOnMainThread(const MultiTouchInput& aInput,
                                   const ScrollableLayerGuid& aGuid,
                                   const uint64_t& aInputBlockId,
                                   nsEventStatus aApzResponse)
      : mInput(aInput)
      , mGuid(aGuid)
      , mInputBlockId(aInputBlockId)
      , mApzResponse(aApzResponse)
    {}

    NS_IMETHOD Run() {
        if (gFocusedWindow) {
            gFocusedWindow->DispatchTouchEventForAPZ(mInput, mGuid, mInputBlockId, mApzResponse);
        }
        return NS_OK;
    }

private:
    MultiTouchInput mInput;
    ScrollableLayerGuid mGuid;
    uint64_t mInputBlockId;
    nsEventStatus mApzResponse;
};

void
nsWindow::DispatchTouchInputViaAPZ(MultiTouchInput& aInput)
{
    APZThreadUtils::AssertOnControllerThread();

    if (!mAPZC) {
        // In general mAPZC should not be null, but during initial setup
        // it might be, so we handle that case by ignoring touch input there.
        return;
    }

    // First send it through the APZ code
    mozilla::layers::ScrollableLayerGuid guid;
    uint64_t inputBlockId;
    nsEventStatus result = mAPZC->ReceiveInputEvent(aInput, &guid, &inputBlockId);
    // If the APZ says to drop it, then we drop it
    if (result == nsEventStatus_eConsumeNoDefault) {
        return;
    }

    // Can't use NS_NewRunnableMethod because it only takes up to one arg and
    // we need more. Also we can't pass in |this| to the task because nsWindow
    // refcounting is not threadsafe. Instead we just use the gFocusedWindow
    // static ptr inside the task.
    NS_DispatchToMainThread(new DispatchTouchInputOnMainThread(
        aInput, guid, inputBlockId, result));
}

void
nsWindow::DispatchTouchEventForAPZ(const MultiTouchInput& aInput,
                                   const ScrollableLayerGuid& aGuid,
                                   const uint64_t aInputBlockId,
                                   nsEventStatus aApzResponse)
{
    MOZ_ASSERT(NS_IsMainThread());
    UserActivity();

    // Convert it to an event we can send to Gecko
    WidgetTouchEvent event = aInput.ToWidgetTouchEvent(this);

    // Dispatch the event into the gecko root process for "normal" flow.
    // The event might get sent to a child process,
    // but if it doesn't we need to notify the APZ of various things.
    // All of that happens in ProcessUntransformedAPZEvent
    ProcessUntransformedAPZEvent(&event, aGuid, aInputBlockId, aApzResponse);
}

class DispatchTouchInputOnControllerThread : public Task
{
public:
    DispatchTouchInputOnControllerThread(const MultiTouchInput& aInput)
      : Task()
      , mInput(aInput)
    {}

    virtual void Run() override {
        if (gFocusedWindow) {
            gFocusedWindow->DispatchTouchInputViaAPZ(mInput);
        }
    }

private:
    MultiTouchInput mInput;
};

nsresult
nsWindow::SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                     TouchPointerState aPointerState,
                                     nsIntPoint aPointerScreenPoint,
                                     double aPointerPressure,
                                     uint32_t aPointerOrientation,
                                     nsIObserver* aObserver)
{
    AutoObserverNotifier notifier(aObserver, "touchpoint");

    if (aPointerState == TOUCH_HOVER) {
        return NS_ERROR_UNEXPECTED;
    }

    if (!mSynthesizedTouchInput) {
        mSynthesizedTouchInput = new MultiTouchInput();
    }

    // We can't dispatch mSynthesizedTouchInput directly because (a) dispatching
    // it might inadvertently modify it and (b) in the case of touchend or
    // touchcancel events mSynthesizedTouchInput will hold the touches that are
    // still down whereas the input dispatched needs to hold the removed
    // touch(es). We use |inputToDispatch| for this purpose.
    MultiTouchInput inputToDispatch;
    inputToDispatch.mInputType = MULTITOUCH_INPUT;

    int32_t index = mSynthesizedTouchInput->IndexOfTouch((int32_t)aPointerId);
    if (aPointerState == TOUCH_CONTACT) {
        if (index >= 0) {
            // found an existing touch point, update it
            SingleTouchData& point = mSynthesizedTouchInput->mTouches[index];
            point.mScreenPoint = ScreenIntPoint::FromUntyped(aPointerScreenPoint);
            point.mRotationAngle = (float)aPointerOrientation;
            point.mForce = (float)aPointerPressure;
            inputToDispatch.mType = MultiTouchInput::MULTITOUCH_MOVE;
        } else {
            // new touch point, add it
            mSynthesizedTouchInput->mTouches.AppendElement(SingleTouchData(
                (int32_t)aPointerId,
                ScreenIntPoint::FromUntyped(aPointerScreenPoint),
                ScreenSize(0, 0),
                (float)aPointerOrientation,
                (float)aPointerPressure));
            inputToDispatch.mType = MultiTouchInput::MULTITOUCH_START;
        }
        inputToDispatch.mTouches = mSynthesizedTouchInput->mTouches;
    } else {
        MOZ_ASSERT(aPointerState == TOUCH_REMOVE || aPointerState == TOUCH_CANCEL);
        // a touch point is being lifted, so remove it from the stored list
        if (index >= 0) {
            mSynthesizedTouchInput->mTouches.RemoveElementAt(index);
        }
        inputToDispatch.mType = (aPointerState == TOUCH_REMOVE
            ? MultiTouchInput::MULTITOUCH_END
            : MultiTouchInput::MULTITOUCH_CANCEL);
        inputToDispatch.mTouches.AppendElement(SingleTouchData(
            (int32_t)aPointerId,
            ScreenIntPoint::FromUntyped(aPointerScreenPoint),
            ScreenSize(0, 0),
            (float)aPointerOrientation,
            (float)aPointerPressure));
    }

    // Can't use NewRunnableMethod here because that will pass a const-ref
    // argument to DispatchTouchInputViaAPZ whereas that function takes a
    // non-const ref. At this callsite we don't care about the mutations that
    // the function performs so this is fine. Also we can't pass |this| to the
    // task because nsWindow refcounting is not threadsafe. Instead we just use
    // the gFocusedWindow static ptr instead the task.
    APZThreadUtils::RunOnControllerThread(new DispatchTouchInputOnControllerThread(inputToDispatch));

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Create(nsIWidget *aParent,
                 void *aNativeParent,
                 const nsIntRect &aRect,
                 nsWidgetInitData *aInitData)
{
    BaseCreate(aParent, IS_TOPLEVEL() ? sVirtualBounds : aRect,
               aInitData);

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

    gDrawRequest = true;
    mozilla::NotifyEvent();
    return NS_OK;
}

LayoutDeviceIntPoint
nsWindow::WidgetToScreenOffset()
{
    LayoutDeviceIntPoint p(0, 0);
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
nsWindow::MakeFullScreen(bool aFullScreen, nsIScreen*)
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
HalFormatToSurfaceFormat(int aHalFormat, int* bytepp)
{
    switch (aHalFormat) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
        *bytepp = 4;
        return SurfaceFormat::R8G8B8A8;
    case HAL_PIXEL_FORMAT_RGBX_8888:
        *bytepp = 4;
        return SurfaceFormat::R8G8B8X8;
    case HAL_PIXEL_FORMAT_BGRA_8888:
        *bytepp = 4;
        return SurfaceFormat::B8G8R8A8;
    case HAL_PIXEL_FORMAT_RGB_565:
        *bytepp = 2;
        return SurfaceFormat::R5G6B5;
    default:
        MOZ_CRASH("Unhandled HAL pixel format");
        return SurfaceFormat::UNKNOWN; // not reached
    }
}

TemporaryRef<DrawTarget>
nsWindow::StartRemoteDrawing()
{
    GonkDisplay* display = GetGonkDisplay();
    mFramebuffer = display->DequeueBuffer();
    int width = mFramebuffer->width, height = mFramebuffer->height;
    void *vaddr;
    if (gralloc_module()->lock(gralloc_module(), mFramebuffer->handle,
                               GRALLOC_USAGE_SW_READ_NEVER |
                               GRALLOC_USAGE_SW_WRITE_OFTEN |
                               GRALLOC_USAGE_HW_FB,
                               0, 0, width, height, &vaddr)) {
        EndRemoteDrawing();
        return nullptr;
    }
    int bytepp;
    SurfaceFormat format = HalFormatToSurfaceFormat(display->surfaceformat,
                                                    &bytepp);
    mFramebufferTarget = Factory::CreateDrawTargetForData(
         BackendType::CAIRO, (uint8_t*)vaddr,
         IntSize(width, height), mFramebuffer->stride * bytepp, format);
    if (!mFramebufferTarget) {
        MOZ_CRASH("nsWindow::StartRemoteDrawing failed in CreateDrawTargetForData");
    }
    if (!mBackBuffer ||
        mBackBuffer->GetSize() != mFramebufferTarget->GetSize() ||
        mBackBuffer->GetFormat() != mFramebufferTarget->GetFormat()) {
        mBackBuffer = mFramebufferTarget->CreateSimilarDrawTarget(
            mFramebufferTarget->GetSize(), mFramebufferTarget->GetFormat());
    }
    return mBackBuffer;
}

void
nsWindow::EndRemoteDrawing()
{
    if (mFramebufferTarget) {
        IntSize size = mFramebufferTarget->GetSize();
        Rect rect(0, 0, size.width, size.height);
        RefPtr<SourceSurface> source = mBackBuffer->Snapshot();
        mFramebufferTarget->DrawSurface(source, rect, rect);
        gralloc_module()->unlock(gralloc_module(), mFramebuffer->handle);
    }
    if (mFramebuffer) {
        GetGonkDisplay()->QueueBuffer(mFramebuffer);
    }
    mFramebuffer = nullptr;
    mFramebufferTarget = nullptr;
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
    return floor(dpi / 150.0 + 0.5);
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
        if (mLayerManager->GetBackendType() == LayersBackend::LAYERS_CLIENT) {
            ClientLayerManager* manager =
                static_cast<ClientLayerManager*>(mLayerManager.get());
            manager->SetDefaultTargetConfiguration(mozilla::layers::BufferMode::BUFFER_NONE,
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

    CreateCompositor();
    if (mCompositorParent) {
        HwcComposer2D::GetInstance()->SetCompositorParent(mCompositorParent);
    }
    MOZ_ASSERT(mLayerManager);
    return mLayerManager;
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
        mLayerManager->GetBackendType() == mozilla::layers::LayersBackend::LAYERS_OPENGL) {
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
nsScreenGonk::GetId(uint32_t *outId)
{
    *outId = 1;
    return NS_OK;
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

NS_IMPL_ISUPPORTS(nsScreenManagerGonk, nsIScreenManager)

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
nsScreenManagerGonk::ScreenForId(uint32_t aId,
                                 nsIScreen **outScreen)
{
    return GetPrimaryScreen(outScreen);
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
