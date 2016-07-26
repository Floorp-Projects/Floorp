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

#include "nsWindow.h"

#include "mozilla/DebugOnly.h"

#include <fcntl.h>

#include "android/log.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/Preferences.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Services.h"
#include "mozilla/FileUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "GLContextProvider.h"
#include "GLContext.h"
#include "GLContextEGL.h"
#include "nsAppShell.h"
#include "nsScreenManagerGonk.h"
#include "nsTArray.h"
#include "nsIWidgetListener.h"
#include "ClientLayerManager.h"
#include "BasicLayers.h"
#include "libdisplay/GonkDisplay.h"
#include "mozilla/TextEvents.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/CompositorSession.h"
#include "mozilla/TouchEvents.h"
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

static nsWindow *gFocusedWindow = nullptr;

NS_IMPL_ISUPPORTS_INHERITED0(nsWindow, nsBaseWidget)

nsWindow::nsWindow()
{
    RefPtr<nsScreenManagerGonk> screenManager = nsScreenManagerGonk::GetInstance();
    screenManager->Initialize();

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
}

nsWindow::~nsWindow()
{
    if (mScreen->IsPrimaryScreen()) {
        mComposer2D->SetCompositorBridgeParent(nullptr);
    }
}

void
nsWindow::DoDraw(void)
{
    if (!hal::GetScreenEnabled()) {
        gDrawRequest = true;
        return;
    }

    RefPtr<nsScreenGonk> screen = nsScreenManagerGonk::GetPrimaryScreen();
    const nsTArray<nsWindow*>& windows = screen->GetTopWindows();

    if (windows.IsEmpty()) {
        LOG("  no window to draw, bailing");
        return;
    }

    RefPtr<nsWindow> targetWindow = (nsWindow *)windows[0];
    while (targetWindow->GetLastChild()) {
        targetWindow = (nsWindow *)targetWindow->GetLastChild();
    }

    nsIWidgetListener* listener = targetWindow->GetWidgetListener();
    if (listener) {
        listener->WillPaintWindow(targetWindow);
    }

    listener = targetWindow->GetWidgetListener();
    if (listener) {
        LayerManager* lm = targetWindow->GetLayerManager();
        if (mozilla::layers::LayersBackend::LAYERS_CLIENT == lm->GetBackendType()) {
            // No need to do anything, the compositor will handle drawing
        } else {
            NS_RUNTIMEABORT("Unexpected layer manager type");
        }

        listener->DidPaintWindow();
    }
}

void
nsWindow::ConfigureAPZControllerThread()
{
    APZThreadUtils::SetControllerThread(CompositorThreadHolder::Loop());
}

/*static*/ nsEventStatus
nsWindow::DispatchKeyInput(WidgetKeyboardEvent& aEvent)
{
    if (!gFocusedWindow) {
        return nsEventStatus_eIgnore;
    }

    gFocusedWindow->UserActivity();

    nsEventStatus status;
    aEvent.mWidget = gFocusedWindow;
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

class DispatchTouchInputOnMainThread : public mozilla::Runnable
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

class DispatchTouchInputOnControllerThread : public Runnable
{
public:
    DispatchTouchInputOnControllerThread(const MultiTouchInput& aInput)
      : mInput(aInput)
    {}

    NS_IMETHOD Run() override {
        if (gFocusedWindow) {
            gFocusedWindow->DispatchTouchInputViaAPZ(mInput);
        }
        return NS_OK;
    }

private:
    MultiTouchInput mInput;
};

nsresult
nsWindow::SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                     TouchPointerState aPointerState,
                                     LayoutDeviceIntPoint aPoint,
                                     double aPointerPressure,
                                     uint32_t aPointerOrientation,
                                     nsIObserver* aObserver)
{
    AutoObserverNotifier notifier(aObserver, "touchpoint");

    if (aPointerState == TOUCH_HOVER) {
        return NS_ERROR_UNEXPECTED;
    }

    if (!mSynthesizedTouchInput) {
        mSynthesizedTouchInput = MakeUnique<MultiTouchInput>();
    }

    MultiTouchInput inputToDispatch = UpdateSynthesizedTouchState(
        mSynthesizedTouchInput.get(), aPointerId, aPointerState,
        aPoint, aPointerPressure, aPointerOrientation);

    // Can't use NewRunnableMethod here because that will pass a const-ref
    // argument to DispatchTouchInputViaAPZ whereas that function takes a
    // non-const ref. At this callsite we don't care about the mutations that
    // the function performs so this is fine. Also we can't pass |this| to the
    // task because nsWindow refcounting is not threadsafe. Instead we just use
    // the gFocusedWindow static ptr instead the task.
    APZThreadUtils::RunOnControllerThread(
      MakeAndAddRef<DispatchTouchInputOnControllerThread>(inputToDispatch));

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Create(nsIWidget* aParent,
                 void* aNativeParent,
                 const LayoutDeviceIntRect& aRect,
                 nsWidgetInitData* aInitData)
{
    BaseCreate(aParent, aInitData);

    nsCOMPtr<nsIScreen> screen;

    uint32_t screenId = aParent ? ((nsWindow*)aParent)->mScreen->GetId() :
                                  aInitData->mScreenId;

    RefPtr<nsScreenManagerGonk> screenManager = nsScreenManagerGonk::GetInstance();
    screenManager->ScreenForId(screenId, getter_AddRefs(screen));

    mScreen = static_cast<nsScreenGonk*>(screen.get());

    mBounds = aRect;

    mParent = (nsWindow *)aParent;
    mVisible = false;

    if (!aParent) {
        mBounds = mScreen->GetRect();
    }

    mComposer2D = HwcComposer2D::GetInstance();

    if (!IS_TOPLEVEL()) {
        return NS_OK;
    }

    mScreen->RegisterWindow(this);

    Resize(0, 0, mBounds.width, mBounds.height, false);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Destroy(void)
{
    mOnDestroyCalled = true;
    mScreen->UnregisterWindow(this);
    if (this == gFocusedWindow) {
        gFocusedWindow = nullptr;
    }
    nsBaseWidget::OnDestroy();
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Show(bool aState)
{
    if (mWindowType == eWindowType_invisible) {
        return NS_OK;
    }

    if (mVisible == aState) {
        return NS_OK;
    }

    mVisible = aState;
    if (!IS_TOPLEVEL()) {
        return mParent ? mParent->Show(aState) : NS_OK;
    }

    if (aState) {
        BringToTop();
    } else {
        const nsTArray<nsWindow*>& windows =
            mScreen->GetTopWindows();
        for (unsigned int i = 0; i < windows.Length(); i++) {
            nsWindow *win = windows[i];
            if (!win->mVisible) {
                continue;
            }
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
    mBounds = LayoutDeviceIntRect(NSToIntRound(aX), NSToIntRound(aY),
                                  NSToIntRound(aWidth), NSToIntRound(aHeight));
    if (mWidgetListener) {
        mWidgetListener->WindowResized(this, mBounds.width, mBounds.height);
    }

    if (aRepaint) {
        Invalidate(mBounds);
    }

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
    if (aRaise) {
        BringToTop();
    }

    if (!IS_TOPLEVEL() && mScreen->IsPrimaryScreen()) {
        // We should only set focused window on non-toplevel primary window.
        gFocusedWindow = this;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::ConfigureChildren(const nsTArray<nsIWidget::Configuration>&)
{
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Invalidate(const LayoutDeviceIntRect& aRect)
{
    nsWindow *top = mParent;
    while (top && top->mParent) {
        top = top->mParent;
    }
    const nsTArray<nsWindow*>& windows = mScreen->GetTopWindows();
    if (top != windows[0] && this != windows[0]) {
        return NS_OK;
    }

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
        // Called before primary display's EGLSurface creation.
        return mScreen->GetNativeWindow();
    case NS_NATIVE_OPENGL_CONTEXT:
        return mScreen->GetGLContext().take();
    case NS_RAW_NATIVE_IME_CONTEXT: {
        void* pseudoIMEContext = GetPseudoIMEContext();
        if (pseudoIMEContext) {
            return pseudoIMEContext;
        }
        // There is only one IME context on Gonk.
        return NS_ONLY_ONE_NATIVE_IME_CONTEXT;
    }
    }

    return nullptr;
}

void
nsWindow::SetNativeData(uint32_t aDataType, uintptr_t aVal)
{
    switch (aDataType) {
    case NS_NATIVE_OPENGL_CONTEXT:
        GLContext* context = reinterpret_cast<GLContext*>(aVal);
        if (!context) {
            mScreen->SetEGLInfo(EGL_NO_DISPLAY,
                                EGL_NO_SURFACE,
                                nullptr);
            return;
        }
        mScreen->SetEGLInfo(GLContextEGL::Cast(context)->GetEGLDisplay(),
                            GLContextEGL::Cast(context)->GetEGLSurface(),
                            context);
        return;
    }
}

NS_IMETHODIMP
nsWindow::DispatchEvent(WidgetGUIEvent* aEvent, nsEventStatus& aStatus)
{
    if (mWidgetListener) {
      aStatus = mWidgetListener->HandleEvent(aEvent, mUseAttachedEvents);
    }
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
        nsIntRect virtualBounds;
        mScreen->GetRect(&virtualBounds.x, &virtualBounds.y,
                         &virtualBounds.width, &virtualBounds.height);
        Resize(virtualBounds.x, virtualBounds.y,
               virtualBounds.width, virtualBounds.height,
               /*repaint*/true);
    }

    if (nsIWidgetListener* listener = GetWidgetListener()) {
      listener->FullscreenChanged(aFullScreen);
    }
    return NS_OK;
}

already_AddRefed<DrawTarget>
nsWindow::StartRemoteDrawing()
{
    RefPtr<DrawTarget> buffer = mScreen->StartRemoteDrawing();
    return buffer.forget();
}

void
nsWindow::EndRemoteDrawing()
{
    mScreen->EndRemoteDrawing();
}

float
nsWindow::GetDPI()
{
    return mScreen->GetDpi();
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
    if (dpi < 280.0) {
        return 1.5; // hdpi devices.
    }
    // xhdpi devices and beyond.
    return floor(dpi / 150.0 + 0.5);
}

LayerManager *
nsWindow::GetLayerManager(PLayerTransactionChild* aShadowManager,
                          LayersBackend aBackendHint,
                          LayerManagerPersistence aPersistence)
{
    if (mLayerManager) {
        // This layer manager might be used for painting outside of DoDraw(), so we need
        // to set the correct rotation on it.
        if (mLayerManager->GetBackendType() == LayersBackend::LAYERS_CLIENT) {
            ClientLayerManager* manager =
                static_cast<ClientLayerManager*>(mLayerManager.get());
            uint32_t rotation = mScreen->EffectiveScreenRotation();
            manager->SetDefaultTargetConfiguration(mozilla::layers::BufferMode::BUFFER_NONE,
                                                   ScreenRotation(rotation));
        }
        return mLayerManager;
    }

    const nsTArray<nsWindow*>& windows = mScreen->GetTopWindows();
    nsWindow *topWindow = windows[0];

    if (!topWindow) {
        LOGW(" -- no topwindow\n");
        return nullptr;
    }

    CreateCompositor();
    if (RefPtr<CompositorBridgeParent> bridge = GetCompositorBridgeParent()) {
        mScreen->SetCompositorBridgeParent(bridge);
        if (mScreen->IsPrimaryScreen()) {
            mComposer2D->SetCompositorBridgeParent(bridge);
        }
    }
    MOZ_ASSERT(mLayerManager);
    return mLayerManager;
}

void
nsWindow::DestroyCompositor()
{
    if (RefPtr<CompositorBridgeParent> bridge = GetCompositorBridgeParent()) {
        mScreen->SetCompositorBridgeParent(nullptr);
        if (mScreen->IsPrimaryScreen()) {
            // Unset CompositorBridgeParent
            mComposer2D->SetCompositorBridgeParent(nullptr);
        }
    }
    nsBaseWidget::DestroyCompositor();
}

void
nsWindow::BringToTop()
{
    const nsTArray<nsWindow*>& windows = mScreen->GetTopWindows();
    if (!windows.IsEmpty()) {
        if (nsIWidgetListener* listener = windows[0]->GetWidgetListener()) {
            listener->WindowDeactivated();
        }
    }

    mScreen->BringToTop(this);

    if (mWidgetListener) {
        mWidgetListener->WindowActivated();
    }

    Invalidate(mBounds);
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

LayoutDeviceIntRect
nsWindow::GetNaturalBounds()
{
    return mScreen->GetNaturalBounds();
}

nsScreenGonk*
nsWindow::GetScreen()
{
    return mScreen;
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
    if (mScreen->GetDisplayType() == GonkDisplay::DISPLAY_VIRTUAL) {
        return nullptr;
    }

    return mComposer2D;
}

CompositorBridgeParent*
nsWindow::GetCompositorBridgeParent() const
{
    return mCompositorSession ? mCompositorSession->GetInProcessBridge() : nullptr;
}
