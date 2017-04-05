/* -*- Mode: c++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set sw=4 ts=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <math.h>
#include <unistd.h>

#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/WeakPtr.h"

#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/Unused.h"
#include "mozilla/Preferences.h"
#include "mozilla/layers/RenderTrace.h"
#include <algorithm>

using mozilla::dom::ContentParent;
using mozilla::dom::ContentChild;
using mozilla::Unused;

#include "nsWindow.h"

#include "nsIBaseWindow.h"
#include "nsIBrowserDOMWindow.h"
#include "nsIDOMChromeWindow.h"
#include "nsIObserverService.h"
#include "nsISupportsPrimitives.h"
#include "nsIWidgetListener.h"
#include "nsIWindowWatcher.h"
#include "nsIXULWindow.h"

#include "nsAppShell.h"
#include "nsFocusManager.h"
#include "nsIdleService.h"
#include "nsLayoutUtils.h"
#include "nsViewManager.h"

#include "WidgetUtils.h"

#include "nsIDOMSimpleGestureEvent.h"

#include "nsGkAtoms.h"
#include "nsWidgetsCID.h"
#include "nsGfxCIID.h"

#include "gfxContext.h"

#include "Layers.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/AsyncCompositionManager.h"
#include "mozilla/layers/APZEventState.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layers/IAPZCTreeManager.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "ScopedGLHelpers.h"
#include "mozilla/layers/CompositorOGL.h"
#include "AndroidContentController.h"

#include "nsTArray.h"

#include "AndroidBridge.h"
#include "AndroidBridgeUtilities.h"
#include "AndroidUiThread.h"
#include "android_npapi.h"
#include "FennecJNINatives.h"
#include "GeneratedJNINatives.h"
#include "GeckoEditableSupport.h"
#include "KeyEvent.h"
#include "MotionEvent.h"

#include "imgIEncoder.h"

#include "nsString.h"
#include "GeckoProfiler.h" // For PROFILER_LABEL
#include "nsIXULRuntime.h"
#include "nsPrintfCString.h"

#include "mozilla/ipc/Shmem.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;
using namespace mozilla::java;
using namespace mozilla::widget;
using namespace mozilla::ipc;

NS_IMPL_ISUPPORTS_INHERITED0(nsWindow, nsBaseWidget)

#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/CompositorSession.h"
#include "mozilla/layers/LayerTransactionParent.h"
#include "mozilla/layers/UiCompositorControllerChild.h"
#include "mozilla/Services.h"
#include "nsThreadUtils.h"

// All the toplevel windows that have been created; these are in
// stacking order, so the window at gTopLevelWindows[0] is the topmost
// one.
static nsTArray<nsWindow*> gTopLevelWindows;

static bool sFailedToCreateGLContext = false;

// Multitouch swipe thresholds in inches
static const double SWIPE_MAX_PINCH_DELTA_INCHES = 0.4;
static const double SWIPE_MIN_DISTANCE_INCHES = 0.6;

template<typename Lambda, bool IsStatic, typename InstanceType, class Impl>
class nsWindow::WindowEvent : public nsAppShell::LambdaEvent<Lambda>
{
    typedef nsAppShell::Event Event;
    typedef nsAppShell::LambdaEvent<Lambda> Base;

    bool IsStaleCall()
    {
        if (IsStatic) {
            // Static calls are never stale.
            return false;
        }

        JNIEnv* const env = mozilla::jni::GetEnvForThread();

        const auto natives = reinterpret_cast<mozilla::WeakPtr<Impl>*>(
                jni::GetNativeHandle(env, mInstance.Get()));
        MOZ_CATCH_JNI_EXCEPTION(env);

        // The call is stale if the nsWindow has been destroyed on the
        // Gecko side, but the Java object is still attached to it through
        // a weak pointer. Stale calls should be discarded. Note that it's
        // an error if natives is nullptr here; we return false but the
        // native call will throw an error.
        return natives && !natives->get();
    }

    const InstanceType mInstance;
    const Event::Type mEventType;

public:
    WindowEvent(Lambda&& aLambda,
                InstanceType&& aInstance,
                Event::Type aEventType = Event::Type::kGeneralActivity)
        : Base(mozilla::Move(aLambda))
        , mInstance(mozilla::Move(aInstance))
        , mEventType(aEventType)
    {}

    WindowEvent(Lambda&& aLambda,
                Event::Type aEventType = Event::Type::kGeneralActivity)
        : Base(mozilla::Move(aLambda))
        , mInstance(Base::lambda.GetThisArg())
        , mEventType(aEventType)
    {}

    void Run() override
    {
        if (!IsStaleCall()) {
            return Base::Run();
        }
    }

    Event::Type ActivityType() const override
    {
        return mEventType;
    }
};

namespace {
    template<class Instance, class Impl> typename EnableIf<
        jni::detail::NativePtrPicker<Impl>::value ==
        jni::detail::REFPTR, void>::Type
    CallAttachNative(Instance aInstance, Impl* aImpl)
    {
        Impl::AttachNative(aInstance, RefPtr<Impl>(aImpl).get());
    }

    template<class Instance, class Impl> typename EnableIf<
        jni::detail::NativePtrPicker<Impl>::value ==
        jni::detail::OWNING, void>::Type
    CallAttachNative(Instance aInstance, Impl* aImpl)
    {
        Impl::AttachNative(aInstance, UniquePtr<Impl>(aImpl));
    }
} // namespace

template<class Impl>
template<class Instance, typename... Args> void
nsWindow::NativePtr<Impl>::Attach(Instance aInstance, nsWindow* aWindow,
                                  Args&&... aArgs)
{
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!mPtr && !mImpl);

    Impl* const impl = new Impl(
            this, aWindow, mozilla::Forward<Args>(aArgs)...);
    mImpl = impl;

    // CallAttachNative transfers ownership of impl.
    CallAttachNative<Instance, Impl>(aInstance, impl);
}

template<class Impl> void
nsWindow::NativePtr<Impl>::Detach()
{
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mPtr && mImpl);

    mImpl->OnDetach();
    {
        Locked implLock(*this);
        mImpl = nullptr;
    }

    typename WindowPtr<Impl>::Locked lock(*mPtr);
    mPtr->mWindow = nullptr;
    mPtr->mPtr = nullptr;
    mPtr = nullptr;
}

template<class Impl>
class nsWindow::NativePtr<Impl>::Locked final : private MutexAutoLock
{
    Impl* const mImpl;

public:
    Locked(NativePtr<Impl>& aPtr)
        : MutexAutoLock(aPtr.mImplLock)
        , mImpl(aPtr.mImpl)
    {}

    operator Impl*() const { return mImpl; }
    Impl* operator->() const { return mImpl; }
};

class nsWindow::GeckoViewSupport final
    : public GeckoView::Window::Natives<GeckoViewSupport>
    , public SupportsWeakPtr<GeckoViewSupport>
{
    nsWindow& window;
    GeckoView::Window::GlobalRef mGeckoViewWindow;

public:
    typedef GeckoView::Window::Natives<GeckoViewSupport> Base;
    typedef SupportsWeakPtr<GeckoViewSupport> SupportsWeakPtr;

    MOZ_DECLARE_WEAKREFERENCE_TYPENAME(GeckoViewSupport);

    template<typename Functor>
    static void OnNativeCall(Functor&& aCall)
    {
        if (aCall.IsTarget(&Open) && NS_IsMainThread()) {
            // Gecko state probably just switched to PROFILE_READY, and the
            // event loop is not running yet. Skip the event loop here so we
            // can get a head start on opening our window.
            return aCall();
        }

        nsAppShell::PostEvent(mozilla::MakeUnique<WindowEvent<Functor>>(
                mozilla::Move(aCall)));
    }

    GeckoViewSupport(nsWindow* aWindow,
                     const GeckoView::Window::LocalRef& aInstance)
        : window(*aWindow)
        , mGeckoViewWindow(aInstance)
    {
        Base::AttachNative(aInstance, static_cast<SupportsWeakPtr*>(this));
    }

    ~GeckoViewSupport();

    using Base::DisposeNative;

    /**
     * GeckoView methods
     */
private:
    nsCOMPtr<nsPIDOMWindowOuter> mDOMWindow;

public:
    // Create and attach a window.
    static void Open(const jni::Class::LocalRef& aCls,
                     GeckoView::Window::Param aWindow,
                     GeckoView::Param aView, jni::Object::Param aCompositor,
                     jni::Object::Param aDispatcher,
                     jni::String::Param aChromeURI,
                     jni::Object::Param aSettings,
                     int32_t aScreenId,
                     bool aPrivateMode);

    // Close and destroy the nsWindow.
    void Close();

    // Reattach this nsWindow to a new GeckoView.
    void Reattach(const GeckoView::Window::LocalRef& inst,
                  GeckoView::Param aView, jni::Object::Param aCompositor,
                  jni::Object::Param aDispatcher);

    void LoadUri(jni::String::Param aUri, int32_t aFlags);

    void EnableEventDispatcher();
};

/**
 * NativePanZoomController handles its native calls on the UI thread, so make
 * it separate from GeckoViewSupport.
 */
class nsWindow::NPZCSupport final
    : public NativePanZoomController::Natives<NPZCSupport>
{
    using LockedWindowPtr = WindowPtr<NPZCSupport>::Locked;

    WindowPtr<NPZCSupport> mWindow;
    NativePanZoomController::GlobalRef mNPZC;
    int mPreviousButtons;

    template<typename Lambda>
    class InputEvent final : public nsAppShell::Event
    {
        NativePanZoomController::GlobalRef mNPZC;
        Lambda mLambda;

    public:
        InputEvent(const NPZCSupport* aNPZCSupport, Lambda&& aLambda)
            : mNPZC(aNPZCSupport->mNPZC)
            , mLambda(mozilla::Move(aLambda))
        {}

        void Run() override
        {
            MOZ_ASSERT(NS_IsMainThread());

            JNIEnv* const env = jni::GetGeckoThreadEnv();
            NPZCSupport* npzcSupport = GetNative(
                    NativePanZoomController::LocalRef(env, mNPZC));

            if (!npzcSupport || !npzcSupport->mWindow) {
                // We already shut down.
                env->ExceptionClear();
                return;
            }

            nsWindow* const window = npzcSupport->mWindow;
            window->UserActivity();
            return mLambda(window);
        }

        nsAppShell::Event::Type ActivityType() const override
        {
            return nsAppShell::Event::Type::kUIActivity;
        }
    };

    template<typename Lambda>
    void PostInputEvent(Lambda&& aLambda)
    {
        nsAppShell::PostEvent(MakeUnique<InputEvent<Lambda>>(
                this, mozilla::Move(aLambda)));
    }

public:
    typedef NativePanZoomController::Natives<NPZCSupport> Base;

    NPZCSupport(NativePtr<NPZCSupport>* aPtr, nsWindow* aWindow,
                const NativePanZoomController::LocalRef& aNPZC)
        : mWindow(aPtr, aWindow)
        , mNPZC(aNPZC)
        , mPreviousButtons(0)
    {}

    ~NPZCSupport()
    {}

    using Base::AttachNative;
    using Base::DisposeNative;

    void OnDetach()
    {
        // There are several considerations when shutting down NPZC. 1) The
        // Gecko thread may destroy NPZC at any time when nsWindow closes. 2)
        // There may be pending events on the Gecko thread when NPZC is
        // destroyed. 3) mWindow may not be available when the pending event
        // runs. 4) The UI thread may destroy NPZC at any time when GeckoView
        // is destroyed. 5) The UI thread may destroy NPZC at the same time as
        // Gecko thread trying to destroy NPZC. 6) There may be pending calls
        // on the UI thread when NPZC is destroyed. 7) mWindow may have been
        // cleared on the Gecko thread when the pending call happens on the UI
        // thread.
        //
        // 1) happens through OnDetach, which first notifies the UI
        // thread through Destroy; Destroy then calls DisposeNative, which
        // finally disposes the native instance back on the Gecko thread. Using
        // Destroy to indirectly call DisposeNative here also solves 5), by
        // making everything go through the UI thread, avoiding contention.
        //
        // 2) and 3) are solved by clearing mWindow, which signals to the
        // pending event that we had shut down. In that case the event bails
        // and does not touch mWindow.
        //
        // 4) happens through DisposeNative directly. OnDetach is not
        // called.
        //
        // 6) is solved by keeping a destroyed flag in the Java NPZC instance,
        // and only make a pending call if the destroyed flag is not set.
        //
        // 7) is solved by taking a lock whenever mWindow is modified on the
        // Gecko thread or accessed on the UI thread. That way, we don't
        // release mWindow until the UI thread is done using it, thus avoiding
        // the race condition.

        typedef NativePanZoomController::GlobalRef NPZCRef;
        auto callDestroy = [] (const NPZCRef& npzc) {
            npzc->Destroy();
        };

        NativePanZoomController::GlobalRef npzc = mNPZC;
        AndroidBridge::Bridge()->PostTaskToUiThread(NewRunnableFunction(
                static_cast<void(*)(const NPZCRef&)>(callDestroy),
                mozilla::Move(npzc)), 0);
    }

public:
    void AdjustScrollForSurfaceShift(float aX, float aY)
    {
        MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

        RefPtr<IAPZCTreeManager> controller;

        if (LockedWindowPtr window{mWindow}) {
            controller = window->mAPZC;
        }

        if (controller) {
            controller->AdjustScrollForSurfaceShift(
                ScreenPoint(aX, aY));
        }
    }

    void SetIsLongpressEnabled(bool aIsLongpressEnabled)
    {
        MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

        RefPtr<IAPZCTreeManager> controller;

        if (LockedWindowPtr window{mWindow}) {
            controller = window->mAPZC;
        }

        if (controller) {
            controller->SetLongTapEnabled(aIsLongpressEnabled);
        }
    }

    bool HandleScrollEvent(int64_t aTime, int32_t aMetaState,
                           float aX, float aY,
                           float aHScroll, float aVScroll)
    {
        MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

        RefPtr<IAPZCTreeManager> controller;

        if (LockedWindowPtr window{mWindow}) {
            controller = window->mAPZC;
        }

        if (!controller) {
            return false;
        }

        ScreenPoint origin = ScreenPoint(aX, aY);

        ScrollWheelInput input(aTime, GetEventTimeStamp(aTime), GetModifiers(aMetaState),
                               ScrollWheelInput::SCROLLMODE_SMOOTH,
                               ScrollWheelInput::SCROLLDELTA_PIXEL,
                               origin,
                               aHScroll, aVScroll,
                               false);

        ScrollableLayerGuid guid;
        uint64_t blockId;
        nsEventStatus status = controller->ReceiveInputEvent(input, &guid, &blockId);

        if (status == nsEventStatus_eConsumeNoDefault) {
            return true;
        }

        PostInputEvent([input, guid, blockId, status] (nsWindow* window) {
            WidgetWheelEvent wheelEvent = input.ToWidgetWheelEvent(window);
            window->ProcessUntransformedAPZEvent(&wheelEvent, guid,
                                                 blockId, status);
        });

        return true;
    }

private:
    static MouseInput::ButtonType GetButtonType(int button)
    {
        MouseInput::ButtonType result = MouseInput::NONE;

        switch (button) {
            case java::sdk::MotionEvent::BUTTON_PRIMARY:
                result = MouseInput::LEFT_BUTTON;
                break;
            case java::sdk::MotionEvent::BUTTON_SECONDARY:
                result = MouseInput::RIGHT_BUTTON;
                break;
            case java::sdk::MotionEvent::BUTTON_TERTIARY:
                result = MouseInput::MIDDLE_BUTTON;
                break;
            default:
                break;
        }

        return result;
    }

    static int16_t ConvertButtons(int buttons) {
        int16_t result = 0;

        if (buttons & java::sdk::MotionEvent::BUTTON_PRIMARY) {
            result |= WidgetMouseEventBase::eLeftButtonFlag;
        }
        if (buttons & java::sdk::MotionEvent::BUTTON_SECONDARY) {
            result |= WidgetMouseEventBase::eRightButtonFlag;
        }
        if (buttons & java::sdk::MotionEvent::BUTTON_TERTIARY) {
            result |= WidgetMouseEventBase::eMiddleButtonFlag;
        }
        if (buttons & java::sdk::MotionEvent::BUTTON_BACK) {
            result |= WidgetMouseEventBase::e4thButtonFlag;
        }
        if (buttons & java::sdk::MotionEvent::BUTTON_FORWARD) {
            result |= WidgetMouseEventBase::e5thButtonFlag;
        }

        return result;
    }

public:
    bool HandleMouseEvent(int32_t aAction, int64_t aTime, int32_t aMetaState,
                          float aX, float aY, int buttons)
    {
        MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

        RefPtr<IAPZCTreeManager> controller;

        if (LockedWindowPtr window{mWindow}) {
            controller = window->mAPZC;
        }

        if (!controller) {
            return false;
        }

        MouseInput::MouseType mouseType = MouseInput::MOUSE_NONE;
        MouseInput::ButtonType buttonType = MouseInput::NONE;
        switch (aAction) {
            case AndroidMotionEvent::ACTION_DOWN:
                mouseType = MouseInput::MOUSE_DOWN;
                buttonType = GetButtonType(buttons ^ mPreviousButtons);
                mPreviousButtons = buttons;
                break;
            case AndroidMotionEvent::ACTION_UP:
                mouseType = MouseInput::MOUSE_UP;
                buttonType = GetButtonType(buttons ^ mPreviousButtons);
                mPreviousButtons = buttons;
                break;
            case AndroidMotionEvent::ACTION_MOVE:
                mouseType = MouseInput::MOUSE_MOVE;
                break;
            case AndroidMotionEvent::ACTION_HOVER_MOVE:
                mouseType = MouseInput::MOUSE_MOVE;
                break;
            case AndroidMotionEvent::ACTION_HOVER_ENTER:
                mouseType = MouseInput::MOUSE_WIDGET_ENTER;
                break;
            case AndroidMotionEvent::ACTION_HOVER_EXIT:
                mouseType = MouseInput::MOUSE_WIDGET_EXIT;
                break;
            default:
                break;
        }

        if (mouseType == MouseInput::MOUSE_NONE) {
            return false;
        }

        ScreenPoint origin = ScreenPoint(aX, aY);

        MouseInput input(mouseType, buttonType, nsIDOMMouseEvent::MOZ_SOURCE_MOUSE, ConvertButtons(buttons), origin, aTime, GetEventTimeStamp(aTime), GetModifiers(aMetaState));

        ScrollableLayerGuid guid;
        uint64_t blockId;
        nsEventStatus status = controller->ReceiveInputEvent(input, &guid, &blockId);

        if (status == nsEventStatus_eConsumeNoDefault) {
            return true;
        }

        PostInputEvent([input, guid, blockId, status] (nsWindow* window) {
            WidgetMouseEvent mouseEvent = input.ToWidgetMouseEvent(window);
            window->ProcessUntransformedAPZEvent(&mouseEvent, guid,
                                                 blockId, status);
        });

        return true;
    }

    bool HandleMotionEvent(const NativePanZoomController::LocalRef& aInstance,
                           int32_t aAction, int32_t aActionIndex,
                           int64_t aTime, int32_t aMetaState,
                           jni::IntArray::Param aPointerId,
                           jni::FloatArray::Param aX,
                           jni::FloatArray::Param aY,
                           jni::FloatArray::Param aOrientation,
                           jni::FloatArray::Param aPressure,
                           jni::FloatArray::Param aToolMajor,
                           jni::FloatArray::Param aToolMinor)
    {
        MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

        RefPtr<IAPZCTreeManager> controller;

        if (LockedWindowPtr window{mWindow}) {
            controller = window->mAPZC;
        }

        if (!controller) {
            return false;
        }

        nsTArray<int32_t> pointerId(aPointerId->GetElements());
        MultiTouchInput::MultiTouchType type;
        size_t startIndex = 0;
        size_t endIndex = pointerId.Length();

        switch (aAction) {
            case sdk::MotionEvent::ACTION_DOWN:
            case sdk::MotionEvent::ACTION_POINTER_DOWN:
                type = MultiTouchInput::MULTITOUCH_START;
                break;
            case sdk::MotionEvent::ACTION_MOVE:
                type = MultiTouchInput::MULTITOUCH_MOVE;
                break;
            case sdk::MotionEvent::ACTION_UP:
            case sdk::MotionEvent::ACTION_POINTER_UP:
                // for pointer-up events we only want the data from
                // the one pointer that went up
                type = MultiTouchInput::MULTITOUCH_END;
                startIndex = aActionIndex;
                endIndex = aActionIndex + 1;
                break;
            case sdk::MotionEvent::ACTION_OUTSIDE:
            case sdk::MotionEvent::ACTION_CANCEL:
                type = MultiTouchInput::MULTITOUCH_CANCEL;
                break;
            default:
                return false;
        }

        MultiTouchInput input(type, aTime, GetEventTimeStamp(aTime), 0);
        input.modifiers = GetModifiers(aMetaState);
        input.mTouches.SetCapacity(endIndex - startIndex);

        nsTArray<float> x(aX->GetElements());
        nsTArray<float> y(aY->GetElements());
        nsTArray<float> orientation(aOrientation->GetElements());
        nsTArray<float> pressure(aPressure->GetElements());
        nsTArray<float> toolMajor(aToolMajor->GetElements());
        nsTArray<float> toolMinor(aToolMinor->GetElements());

        MOZ_ASSERT(pointerId.Length() == x.Length());
        MOZ_ASSERT(pointerId.Length() == y.Length());
        MOZ_ASSERT(pointerId.Length() == orientation.Length());
        MOZ_ASSERT(pointerId.Length() == pressure.Length());
        MOZ_ASSERT(pointerId.Length() == toolMajor.Length());
        MOZ_ASSERT(pointerId.Length() == toolMinor.Length());

        for (size_t i = startIndex; i < endIndex; i++) {

            float orien = orientation[i] * 180.0f / M_PI;
            // w3c touchevents spec does not allow orientations == 90
            // this shifts it to -90, which will be shifted to zero below
            if (orien >= 90.0) {
                orien -= 180.0f;
            }

            nsIntPoint point = nsIntPoint(int32_t(floorf(x[i])),
                                          int32_t(floorf(y[i])));

            // w3c touchevent radii are given with an orientation between 0 and
            // 90. The radii are found by removing the orientation and
            // measuring the x and y radii of the resulting ellipse. For
            // Android orientations >= 0 and < 90, use the y radius as the
            // major radius, and x as the minor radius. However, for an
            // orientation < 0, we have to shift the orientation by adding 90,
            // and reverse which radius is major and minor.
            gfx::Size radius;
            if (orien < 0.0f) {
                orien += 90.0f;
                radius = gfx::Size(int32_t(toolMajor[i] / 2.0f),
                                   int32_t(toolMinor[i] / 2.0f));
            } else {
                radius = gfx::Size(int32_t(toolMinor[i] / 2.0f),
                                   int32_t(toolMajor[i] / 2.0f));
            }

            input.mTouches.AppendElement(SingleTouchData(
                    pointerId[i], ScreenIntPoint::FromUnknownPoint(point),
                    ScreenSize::FromUnknownSize(radius), orien, pressure[i]));
        }

        ScrollableLayerGuid guid;
        uint64_t blockId;
        nsEventStatus status =
            controller->ReceiveInputEvent(input, &guid, &blockId);

        if (status == nsEventStatus_eConsumeNoDefault) {
            return true;
        }

        // Dispatch APZ input event on Gecko thread.
        PostInputEvent([input, guid, blockId, status] (nsWindow* window) {
            WidgetTouchEvent touchEvent = input.ToWidgetTouchEvent(window);
            window->ProcessUntransformedAPZEvent(&touchEvent, guid,
                                                 blockId, status);
            window->DispatchHitTest(touchEvent);
        });
        return true;
    }

    void UpdateOverscrollVelocity(const float x, const float y)
    {
        mNPZC->UpdateOverscrollVelocity(x, y);
    }

    void UpdateOverscrollOffset(const float x, const float y)
    {
        mNPZC->UpdateOverscrollOffset(x, y);
    }

    void SetSelectionDragState(const bool aState)
    {
        mNPZC->OnSelectionDragState(aState);
    }
};

template<> const char
nsWindow::NativePtr<nsWindow::NPZCSupport>::sName[] = "NPZCSupport";

NS_IMPL_ISUPPORTS(nsWindow::AndroidView,
                  nsIAndroidEventDispatcher,
                  nsIAndroidView)


nsresult
nsWindow::AndroidView::GetSettings(JSContext* aCx, JS::MutableHandleValue aOut)
{
    if (!mSettings) {
        aOut.setNull();
        return NS_OK;
    }

    // Lock to prevent races with UI thread.
    auto lock = mSettings.Lock();
    return widget::EventDispatcher::UnboxBundle(aCx, mSettings, aOut);
}

/**
 * Compositor has some unique requirements for its native calls, so make it
 * separate from GeckoViewSupport.
 */
class nsWindow::LayerViewSupport final
    : public LayerView::Compositor::Natives<LayerViewSupport>
{
    using LockedWindowPtr = WindowPtr<LayerViewSupport>::Locked;

    WindowPtr<LayerViewSupport> mWindow;
    LayerView::Compositor::GlobalRef mCompositor;
    GeckoLayerClient::GlobalRef mLayerClient;
    Atomic<bool, ReleaseAcquire> mCompositorPaused;
    jni::Object::GlobalRef mSurface;

    // In order to use Event::HasSameTypeAs in PostTo(), we cannot make
    // LayerViewEvent a template because each template instantiation is
    // a different type. So implement LayerViewEvent as a ProxyEvent.
    class LayerViewEvent final : public nsAppShell::ProxyEvent
    {
        using Event = nsAppShell::Event;

    public:
        static UniquePtr<Event> MakeEvent(UniquePtr<Event>&& event)
        {
            return MakeUnique<LayerViewEvent>(mozilla::Move(event));
        }

        LayerViewEvent(UniquePtr<Event>&& event)
            : nsAppShell::ProxyEvent(mozilla::Move(event))
        {}

        void PostTo(LinkedList<Event>& queue) override
        {
            // Give priority to compositor events, but keep in order with
            // existing compositor events.
            nsAppShell::Event* event = queue.getFirst();
            while (event && event->HasSameTypeAs(this)) {
                event = event->getNext();
            }
            if (event) {
                event->setPrevious(this);
            } else {
                queue.insertBack(this);
            }
        }
    };

public:
    typedef LayerView::Compositor::Natives<LayerViewSupport> Base;

    template<class Functor>
    static void OnNativeCall(Functor&& aCall)
    {
        if (aCall.IsTarget(&LayerViewSupport::CreateCompositor)) {
            // This call is blocking.
            nsAppShell::SyncRunEvent(nsAppShell::LambdaEvent<Functor>(
                    mozilla::Move(aCall)), &LayerViewEvent::MakeEvent);
            return;
        }

        MOZ_CRASH("Unexpected call");
    }

    static LayerViewSupport*
    FromNative(const LayerView::Compositor::LocalRef& instance)
    {
        return GetNative(instance);
    }

    LayerViewSupport(NativePtr<LayerViewSupport>* aPtr, nsWindow* aWindow,
                     const LayerView::Compositor::LocalRef& aInstance)
        : mWindow(aPtr, aWindow)
        , mCompositor(aInstance)
        , mCompositorPaused(true)
    {}

    ~LayerViewSupport()
    {}

    using Base::AttachNative;
    using Base::DisposeNative;

    void OnDetach()
    {
        mCompositor->Destroy();
    }

    const GeckoLayerClient::Ref& GetLayerClient() const
    {
        return mLayerClient;
    }

    bool CompositorPaused() const
    {
        return mCompositorPaused;
    }

    jni::Object::Param GetSurface()
    {
        return mSurface;
    }

private:
    void OnResumedCompositor()
    {
        MOZ_ASSERT(NS_IsMainThread());

        // When we receive this, the compositor has already been told to
        // resume. (It turns out that waiting till we reach here to tell
        // the compositor to resume takes too long, resulting in a black
        // flash.) This means it's now safe for layer updates to occur.
        // Since we might have prevented one or more draw events from
        // occurring while the compositor was paused, we need to schedule
        // a draw event now.
        if (!mCompositorPaused) {
            mWindow->RedrawAll();
        }
    }

    /**
     * Compositor methods
     */
public:
    void AttachToJava(jni::Object::Param aClient, jni::Object::Param aNPZC)
    {
        MOZ_ASSERT(NS_IsMainThread());
        if (!mWindow) {
            return; // Already shut down.
        }

        mLayerClient = GeckoLayerClient::Ref::From(aClient);

        MOZ_ASSERT(aNPZC);
        auto npzc = NativePanZoomController::LocalRef(
                jni::GetGeckoThreadEnv(),
                NativePanZoomController::Ref::From(aNPZC));
        mWindow->mNPZCSupport.Attach(npzc, mWindow, npzc);

        mLayerClient->OnGeckoReady();

        // Set the first-paint flag so that we (re-)link any new Java objects
        // to Gecko, co-ordinate viewports, etc.
        if (RefPtr<CompositorBridgeChild> bridge = mWindow->GetCompositorBridgeChild()) {
            bridge->SendForceIsFirstPaint();
        }
    }

    void OnSizeChanged(int32_t aWindowWidth, int32_t aWindowHeight,
                       int32_t aScreenWidth, int32_t aScreenHeight)
    {
        MOZ_ASSERT(NS_IsMainThread());
        if (!mWindow) {
            return; // Already shut down.
        }

        if (aWindowWidth != mWindow->mBounds.width ||
            aWindowHeight != mWindow->mBounds.height) {

            mWindow->Resize(aWindowWidth, aWindowHeight, /* repaint */ false);
        }
    }

    void CreateCompositor(int32_t aWidth, int32_t aHeight,
                          jni::Object::Param aSurface)
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(mWindow);

        mSurface = aSurface;
        mWindow->CreateLayerManager(aWidth, aHeight);

        mCompositorPaused = false;
        OnResumedCompositor();
    }

    void SyncPauseCompositor()
    {
        MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

        if (RefPtr<UiCompositorControllerChild> child = GetUiCompositorControllerChild()) {
          mCompositorPaused = true;
          child->Pause();
        }
    }

    void SyncResumeCompositor()
    {
        MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

        if (RefPtr<UiCompositorControllerChild> child = GetUiCompositorControllerChild()) {
          mCompositorPaused = false;
          child->Resume();
        }
    }

    void SyncResumeResizeCompositor(const LayerView::Compositor::LocalRef& aObj,
                                    int32_t aWidth, int32_t aHeight,
                                    jni::Object::Param aSurface)
    {
        MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

        mSurface = aSurface;

        if (RefPtr<UiCompositorControllerChild> child = GetUiCompositorControllerChild()) {
            child->ResumeAndResize(aWidth, aHeight);
        }

        mCompositorPaused = false;

        class OnResumedEvent : public nsAppShell::Event
        {
            LayerView::Compositor::GlobalRef mCompositor;

        public:
            OnResumedEvent(LayerView::Compositor::GlobalRef&& aCompositor)
                : mCompositor(mozilla::Move(aCompositor))
            {}

            void Run() override
            {
                MOZ_ASSERT(NS_IsMainThread());

                JNIEnv* const env = jni::GetGeckoThreadEnv();
                LayerViewSupport* const lvs = GetNative(
                        LayerView::Compositor::LocalRef(env, mCompositor));
                MOZ_CATCH_JNI_EXCEPTION(env);

                lvs->OnResumedCompositor();
            }
        };

        nsAppShell::PostEvent(MakeUnique<LayerViewEvent>(
                MakeUnique<OnResumedEvent>(aObj)));
    }

    void SyncInvalidateAndScheduleComposite()
    {
        RefPtr<UiCompositorControllerChild> child = GetUiCompositorControllerChild();
        if (!child) {
            return;
        }

        if (!AndroidBridge::IsJavaUiThread()) {
            RefPtr<nsThread> uiThread = GetAndroidUiThread();
            if (uiThread) {
                uiThread->Dispatch(NewRunnableMethod(child,
                                                     &UiCompositorControllerChild::InvalidateAndRender),
                                   nsIThread::DISPATCH_NORMAL);
            }
            return;
        }

        child->InvalidateAndRender();
    }

    void SetMaxToolbarHeight(int32_t aHeight)
    {
        MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

        if (RefPtr<UiCompositorControllerChild> child = GetUiCompositorControllerChild()) {
          child->SetMaxToolbarHeight(aHeight);
        }
    }

    void SetPinned(bool aPinned, int32_t aReason)
    {
        RefPtr<UiCompositorControllerChild> child = GetUiCompositorControllerChild();
        if (!child) {
          return;
        }

        if (!AndroidBridge::IsJavaUiThread()) {
            RefPtr<nsThread> uiThread = GetAndroidUiThread();
            if (uiThread) {
                uiThread->Dispatch(NewRunnableMethod<bool, int32_t>(
                                       child, &UiCompositorControllerChild::SetPinned, aPinned, aReason),
                                   nsIThread::DISPATCH_NORMAL);
            }
            return;
        }

        child->SetPinned(aPinned, aReason);
    }


    void SendToolbarAnimatorMessage(int32_t aMessage)
    {
        RefPtr<UiCompositorControllerChild> child = GetUiCompositorControllerChild();

        if (!child) {
          return;
        }

        if (!AndroidBridge::IsJavaUiThread()) {
            RefPtr<nsThread> uiThread = GetAndroidUiThread();
            if (uiThread) {
                uiThread->Dispatch(NewRunnableMethod<int32_t>(
                                       child, &UiCompositorControllerChild::ToolbarAnimatorMessageFromUI, aMessage),
                                   nsIThread::DISPATCH_NORMAL);
            }
            return;
        }

        child->ToolbarAnimatorMessageFromUI(aMessage);
    }

    void RecvToolbarAnimatorMessage(int32_t aMessage)
    {
        mCompositor->RecvToolbarAnimatorMessage(aMessage);
    }

    void SetDefaultClearColor(int32_t aColor)
    {
        MOZ_ASSERT(AndroidBridge::IsJavaUiThread());
        if (RefPtr<UiCompositorControllerChild> child = GetUiCompositorControllerChild()) {
            child->SetDefaultClearColor((uint32_t)aColor);
        }
    }

    void RequestScreenPixels()
    {
        MOZ_ASSERT(AndroidBridge::IsJavaUiThread());
        if (RefPtr<UiCompositorControllerChild> child = GetUiCompositorControllerChild()) {
            child->RequestScreenPixels();
        }
    }

    void RecvScreenPixels(Shmem&& aMem, const ScreenIntSize& aSize)
    {
        MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

        auto pixels = mozilla::jni::IntArray::New(aMem.get<int>(), aMem.Size<int>());
        mCompositor->RecvScreenPixels(aSize.width, aSize.height, pixels);

        // Pixels have been copied, so Dealloc Shmem
        if (RefPtr<UiCompositorControllerChild> child = GetUiCompositorControllerChild()) {
            child->DeallocPixelBuffer(aMem);
        }
    }

    void EnableLayerUpdateNotifications(bool aEnable)
    {
        MOZ_ASSERT(AndroidBridge::IsJavaUiThread());
        if (RefPtr<UiCompositorControllerChild> child = GetUiCompositorControllerChild()) {
            child->EnableLayerUpdateNotifications(aEnable);
        }
    }

    void SendToolbarPixelsToCompositor(int32_t aWidth, int32_t aHeight, jni::IntArray::Param aPixels)
    {
        MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

        if (RefPtr<UiCompositorControllerChild> child = GetUiCompositorControllerChild()) {
           Shmem mem;
           child->AllocPixelBuffer(aPixels->Length() * sizeof(int32_t), &mem);
           aPixels->CopyTo(mem.get<int32_t>(), mem.Size<int32_t>());
           if (!child->ToolbarPixelsToCompositor(mem, ScreenIntSize(aWidth, aHeight))) {
               child->DeallocPixelBuffer(mem);
           }
        }
    }

    already_AddRefed<UiCompositorControllerChild> GetUiCompositorControllerChild()
    {
        RefPtr<UiCompositorControllerChild> child;
        if (LockedWindowPtr window{mWindow}) {
            child = window->GetUiCompositorControllerChild();
        }
        MOZ_ASSERT(child);
        return child.forget();
    }
};

template<> const char
nsWindow::NativePtr<nsWindow::LayerViewSupport>::sName[] = "LayerViewSupport";

/* PresentationMediaPlayerManager native calls access inner nsWindow functionality so PMPMSupport is a child class of nsWindow */
class nsWindow::PMPMSupport final
    : public PresentationMediaPlayerManager::Natives<PMPMSupport>
{
    PMPMSupport() = delete;

    static LayerViewSupport* GetLayerViewSupport(jni::Object::Param aView)
    {
        const auto& layerView = LayerView::Ref::From(aView);

        LayerView::Compositor::LocalRef compositor = layerView->GetCompositor();
        if (!layerView->CompositorCreated() || !compositor) {
            return nullptr;
        }

        LayerViewSupport* const lvs = LayerViewSupport::FromNative(compositor);
        if (!lvs) {
            // There is a pending exception whenever FromNative returns nullptr.
            compositor.Env()->ExceptionClear();
        }
        return lvs;
    }

public:
    static ANativeWindow* sWindow;
    static EGLSurface sSurface;

    static void InvalidateAndScheduleComposite(jni::Object::Param aView)
    {
        LayerViewSupport* const lvs = GetLayerViewSupport(aView);
        if (lvs) {
            lvs->SyncInvalidateAndScheduleComposite();
        }
    }

    static void AddPresentationSurface(const jni::Class::LocalRef& aCls,
                                       jni::Object::Param aView,
                                       jni::Object::Param aSurface)
    {
        RemovePresentationSurface();

        LayerViewSupport* const lvs = GetLayerViewSupport(aView);
        if (!lvs) {
            return;
        }

        ANativeWindow* const window = ANativeWindow_fromSurface(
                aCls.Env(), aSurface.Get());
        if (!window) {
            return;
        }

        sWindow = window;

        const bool wasAlreadyPaused = lvs->CompositorPaused();
        if (!wasAlreadyPaused) {
            lvs->SyncPauseCompositor();
        }

        if (sSurface) {
            // Destroy the EGL surface! The compositor is paused so it should
            // be okay to destroy the surface here.
            mozilla::gl::GLContextProvider::DestroyEGLSurface(sSurface);
            sSurface = nullptr;
        }

        if (!wasAlreadyPaused) {
            lvs->SyncResumeCompositor();
        }

        lvs->SyncInvalidateAndScheduleComposite();
    }

    static void RemovePresentationSurface()
    {
        if (sWindow) {
            ANativeWindow_release(sWindow);
            sWindow = nullptr;
        }
    }
};

ANativeWindow* nsWindow::PMPMSupport::sWindow;
EGLSurface nsWindow::PMPMSupport::sSurface;


nsWindow::GeckoViewSupport::~GeckoViewSupport()
{
    // Disassociate our GeckoEditable instance with our native object.
    MOZ_ASSERT(window.mEditableSupport && window.mEditable);
    window.mEditableSupport.Detach();
    window.mEditable->OnViewChange(nullptr);
    window.mEditable = nullptr;

    if (window.mNPZCSupport) {
        window.mNPZCSupport.Detach();
    }

    if (window.mLayerViewSupport) {
        window.mLayerViewSupport.Detach();
    }
}

/* static */ void
nsWindow::GeckoViewSupport::Open(const jni::Class::LocalRef& aCls,
                                 GeckoView::Window::Param aWindow,
                                 GeckoView::Param aView,
                                 jni::Object::Param aCompositor,
                                 jni::Object::Param aDispatcher,
                                 jni::String::Param aChromeURI,
                                 jni::Object::Param aSettings,
                                 int32_t aScreenId,
                                 bool aPrivateMode)
{
    MOZ_ASSERT(NS_IsMainThread());

    PROFILER_LABEL("nsWindow", "GeckoViewSupport::Open",
                   js::ProfileEntry::Category::OTHER);

    nsCOMPtr<nsIWindowWatcher> ww = do_GetService(NS_WINDOWWATCHER_CONTRACTID);
    MOZ_RELEASE_ASSERT(ww);

    nsAdoptingCString url;
    if (aChromeURI) {
        url = aChromeURI->ToCString();
    } else {
        url = Preferences::GetCString("toolkit.defaultChromeURI");
        if (!url) {
            url = NS_LITERAL_CSTRING("chrome://browser/content/browser.xul");
        }
    }

    RefPtr<AndroidView> androidView = new AndroidView();
    androidView->mEventDispatcher->Attach(
            java::EventDispatcher::Ref::From(aDispatcher), nullptr);
    if (aSettings) {
        androidView->mSettings = java::GeckoBundle::Ref::From(aSettings);
    }

    nsAutoCString chromeFlags("chrome,dialog=0,resizable,scrollbars");
    if (aPrivateMode) {
        chromeFlags += ",private";
    }
    nsCOMPtr<mozIDOMWindowProxy> domWindow;
    ww->OpenWindow(nullptr, url, nullptr, chromeFlags.get(),
                   androidView, getter_AddRefs(domWindow));
    MOZ_RELEASE_ASSERT(domWindow);

    nsCOMPtr<nsPIDOMWindowOuter> pdomWindow =
            nsPIDOMWindowOuter::From(domWindow);
    nsCOMPtr<nsIWidget> widget = WidgetUtils::DOMWindowToWidget(pdomWindow);
    MOZ_ASSERT(widget);

    const auto window = static_cast<nsWindow*>(widget.get());
    window->SetScreenId(aScreenId);

    // Attach a new GeckoView support object to the new window.
    window->mGeckoViewSupport = mozilla::MakeUnique<GeckoViewSupport>(
        window, (GeckoView::Window::LocalRef(aCls.Env(), aWindow)));

    window->mGeckoViewSupport->mDOMWindow = pdomWindow;

    // Attach a new GeckoEditable support object to the new window.
    auto editable = GeckoEditable::New(aView);
    auto editableChild = GeckoEditableChild::New(editable);
    editable->SetDefaultEditableChild(editableChild);
    window->mEditable = editable;
    window->mEditableSupport.Attach(editableChild, window, editableChild);

    // Attach the Compositor to the new window.
    auto compositor = LayerView::Compositor::LocalRef(
            aCls.Env(), LayerView::Compositor::Ref::From(aCompositor));
    window->mLayerViewSupport.Attach(compositor, window, compositor);

    // Attach again using the new window.
    androidView->mEventDispatcher->Attach(
            java::EventDispatcher::Ref::From(aDispatcher), pdomWindow);
    window->mAndroidView = androidView;

    if (window->mWidgetListener) {
        nsCOMPtr<nsIXULWindow> xulWindow(
                window->mWidgetListener->GetXULWindow());
        if (xulWindow) {
            // Our window is not intrinsically sized, so tell nsXULWindow to
            // not set a size for us.
            xulWindow->SetIntrinsicallySized(false);
        }
    }
}

void
nsWindow::GeckoViewSupport::Close()
{
    if (window.mAndroidView) {
        window.mAndroidView->mEventDispatcher->Detach();
    }

    if (!mDOMWindow) {
        return;
    }

    mDOMWindow->ForceClose();
    mDOMWindow = nullptr;
    mGeckoViewWindow = nullptr;
}

void
nsWindow::GeckoViewSupport::Reattach(const GeckoView::Window::LocalRef& inst,
                                     GeckoView::Param aView,
                                     jni::Object::Param aCompositor,
                                     jni::Object::Param aDispatcher)
{
    // Associate our previous GeckoEditable with the new GeckoView.
    MOZ_ASSERT(window.mEditable);
    window.mEditable->OnViewChange(aView);

    // mNPZCSupport might have already been detached through the Java side calling
    // NativePanZoomController.destroy().
    if (window.mNPZCSupport) {
        window.mNPZCSupport.Detach();
    }

    MOZ_ASSERT(window.mLayerViewSupport);
    window.mLayerViewSupport.Detach();

    auto compositor = LayerView::Compositor::LocalRef(
            inst.Env(), LayerView::Compositor::Ref::From(aCompositor));
    window.mLayerViewSupport.Attach(compositor, &window, compositor);
    compositor->Reattach();

    MOZ_ASSERT(window.mAndroidView);
    window.mAndroidView->mEventDispatcher->Attach(
            java::EventDispatcher::Ref::From(aDispatcher), mDOMWindow);

    mGeckoViewWindow->OnReattach(aView);
}

void
nsWindow::GeckoViewSupport::LoadUri(jni::String::Param aUri, int32_t aFlags)
{
    if (!mDOMWindow) {
        return;
    }

    nsCOMPtr<nsIURI> uri = nsAppShell::ResolveURI(aUri->ToCString());
    if (NS_WARN_IF(!uri)) {
        return;
    }

    nsCOMPtr<nsIDOMChromeWindow> chromeWin = do_QueryInterface(mDOMWindow);
    nsCOMPtr<nsIBrowserDOMWindow> browserWin;

    if (NS_WARN_IF(!chromeWin) || NS_WARN_IF(NS_FAILED(
            chromeWin->GetBrowserDOMWindow(getter_AddRefs(browserWin))))) {
        return;
    }

    const int flags = aFlags == GeckoView::LOAD_NEW_TAB ?
                        nsIBrowserDOMWindow::OPEN_NEWTAB :
                      aFlags == GeckoView::LOAD_SWITCH_TAB ?
                        nsIBrowserDOMWindow::OPEN_SWITCHTAB :
                        nsIBrowserDOMWindow::OPEN_CURRENTWINDOW;
    nsCOMPtr<mozIDOMWindowProxy> newWin;

    if (NS_FAILED(browserWin->OpenURI(
            uri, nullptr, flags, nsIBrowserDOMWindow::OPEN_EXTERNAL,
            getter_AddRefs(newWin)))) {
        NS_WARNING("Failed to open URI");
    }
}

void
nsWindow::InitNatives()
{
    nsWindow::GeckoViewSupport::Base::Init();
    nsWindow::LayerViewSupport::Init();
    nsWindow::NPZCSupport::Init();
    if (jni::IsFennec()) {
        nsWindow::PMPMSupport::Init();
    }
}

nsWindow*
nsWindow::TopWindow()
{
    if (!gTopLevelWindows.IsEmpty())
        return gTopLevelWindows[0];
    return nullptr;
}

void
nsWindow::LogWindow(nsWindow *win, int index, int indent)
{
#if defined(DEBUG) || defined(FORCE_ALOG)
    char spaces[] = "                    ";
    spaces[indent < 20 ? indent : 20] = 0;
    ALOG("%s [% 2d] 0x%08x [parent 0x%08x] [% 3d,% 3dx% 3d,% 3d] vis %d type %d",
         spaces, index, (intptr_t)win, (intptr_t)win->mParent,
         win->mBounds.x, win->mBounds.y,
         win->mBounds.width, win->mBounds.height,
         win->mIsVisible, win->mWindowType);
#endif
}

void
nsWindow::DumpWindows()
{
    DumpWindows(gTopLevelWindows);
}

void
nsWindow::DumpWindows(const nsTArray<nsWindow*>& wins, int indent)
{
    for (uint32_t i = 0; i < wins.Length(); ++i) {
        nsWindow *w = wins[i];
        LogWindow(w, i, indent);
        DumpWindows(w->mChildren, indent+1);
    }
}

nsWindow::nsWindow() :
    mScreenId(0), // Use 0 (primary screen) as the default value.
    mIsVisible(false),
    mParent(nullptr),
    mAwaitingFullScreen(false),
    mIsFullScreen(false)
{
}

nsWindow::~nsWindow()
{
    gTopLevelWindows.RemoveElement(this);
    ALOG("nsWindow %p destructor", (void*)this);
}

bool
nsWindow::IsTopLevel()
{
    return mWindowType == eWindowType_toplevel ||
        mWindowType == eWindowType_dialog ||
        mWindowType == eWindowType_invisible;
}

nsresult
nsWindow::Create(nsIWidget* aParent,
                 nsNativeWidget aNativeParent,
                 const LayoutDeviceIntRect& aRect,
                 nsWidgetInitData* aInitData)
{
    ALOG("nsWindow[%p]::Create %p [%d %d %d %d]", (void*)this, (void*)aParent,
         aRect.x, aRect.y, aRect.width, aRect.height);

    nsWindow *parent = (nsWindow*) aParent;
    if (aNativeParent) {
        if (parent) {
            ALOG("Ignoring native parent on Android window [%p], "
                 "since parent was specified (%p %p)", (void*)this,
                 (void*)aNativeParent, (void*)aParent);
        } else {
            parent = (nsWindow*) aNativeParent;
        }
    }

    mBounds = aRect;

    BaseCreate(nullptr, aInitData);

    NS_ASSERTION(IsTopLevel() || parent,
                 "non-top-level window doesn't have a parent!");

    if (IsTopLevel()) {
        gTopLevelWindows.AppendElement(this);

    } else if (parent) {
        parent->mChildren.AppendElement(this);
        mParent = parent;
    }

#ifdef DEBUG_ANDROID_WIDGET
    DumpWindows();
#endif

    return NS_OK;
}

void
nsWindow::Destroy()
{
    nsBaseWidget::mOnDestroyCalled = true;

    if (mGeckoViewSupport) {
        // Disassociate our native object with GeckoView.
        mGeckoViewSupport = nullptr;
    }

    // Stuff below may release the last ref to this
    nsCOMPtr<nsIWidget> kungFuDeathGrip(this);

    while (mChildren.Length()) {
        // why do we still have children?
        ALOG("### Warning: Destroying window %p and reparenting child %p to null!", (void*)this, (void*)mChildren[0]);
        mChildren[0]->SetParent(nullptr);
    }

    nsBaseWidget::Destroy();

    if (IsTopLevel())
        gTopLevelWindows.RemoveElement(this);

    SetParent(nullptr);

    nsBaseWidget::OnDestroy();

#ifdef DEBUG_ANDROID_WIDGET
    DumpWindows();
#endif
}

nsresult
nsWindow::ConfigureChildren(const nsTArray<nsIWidget::Configuration>& config)
{
    for (uint32_t i = 0; i < config.Length(); ++i) {
        nsWindow *childWin = (nsWindow*) config[i].mChild.get();
        childWin->Resize(config[i].mBounds.x,
                         config[i].mBounds.y,
                         config[i].mBounds.width,
                         config[i].mBounds.height,
                         false);
    }

    return NS_OK;
}

void
nsWindow::RedrawAll()
{
    if (mAttachedWidgetListener) {
        mAttachedWidgetListener->RequestRepaint();
    } else if (mWidgetListener) {
        mWidgetListener->RequestRepaint();
    }
}


RefPtr<UiCompositorControllerChild>
nsWindow::GetUiCompositorControllerChild()
{
    return mCompositorSession ? mCompositorSession->GetUiCompositorControllerChild() : nullptr;
}

int64_t
nsWindow::GetRootLayerId() const
{
    return mCompositorSession ? mCompositorSession->RootLayerTreeId() : 0;
}

void
nsWindow::EnableEventDispatcher()
{
    if (!mGeckoViewSupport) {
        return;
    }
    mGeckoViewSupport->EnableEventDispatcher();
}

void
nsWindow::SetParent(nsIWidget *aNewParent)
{
    if ((nsIWidget*)mParent == aNewParent)
        return;

    // If we had a parent before, remove ourselves from its list of
    // children.
    if (mParent)
        mParent->mChildren.RemoveElement(this);

    mParent = (nsWindow*)aNewParent;

    if (mParent)
        mParent->mChildren.AppendElement(this);

    // if we are now in the toplevel window's hierarchy, schedule a redraw
    if (FindTopLevel() == nsWindow::TopWindow())
        RedrawAll();
}

nsIWidget*
nsWindow::GetParent()
{
    return mParent;
}

float
nsWindow::GetDPI()
{
    if (AndroidBridge::Bridge())
        return AndroidBridge::Bridge()->GetDPI();
    return 160.0f;
}

double
nsWindow::GetDefaultScaleInternal()
{

    nsCOMPtr<nsIScreen> screen = GetWidgetScreen();
    MOZ_ASSERT(screen);
    RefPtr<nsScreenAndroid> screenAndroid = (nsScreenAndroid*) screen.get();
    return screenAndroid->GetDensity();
}

void
nsWindow::Show(bool aState)
{
    ALOG("nsWindow[%p]::Show %d", (void*)this, aState);

    if (mWindowType == eWindowType_invisible) {
        ALOG("trying to show invisible window! ignoring..");
        return;
    }

    if (aState == mIsVisible)
        return;

    mIsVisible = aState;

    if (IsTopLevel()) {
        // XXX should we bring this to the front when it's shown,
        // if it's a toplevel widget?

        // XXX we should synthesize a eMouseExitFromWidget (for old top
        // window)/eMouseEnterIntoWidget (for new top window) since we need
        // to pretend that the top window always has focus.  Not sure
        // if Show() is the right place to do this, though.

        if (aState) {
            // It just became visible, so bring it to the front.
            BringToFront();

        } else if (nsWindow::TopWindow() == this) {
            // find the next visible window to show
            unsigned int i;
            for (i = 1; i < gTopLevelWindows.Length(); i++) {
                nsWindow *win = gTopLevelWindows[i];
                if (!win->mIsVisible)
                    continue;

                win->BringToFront();
                break;
            }
        }
    } else if (FindTopLevel() == nsWindow::TopWindow()) {
        RedrawAll();
    }

#ifdef DEBUG_ANDROID_WIDGET
    DumpWindows();
#endif
}

bool
nsWindow::IsVisible() const
{
    return mIsVisible;
}

void
nsWindow::ConstrainPosition(bool aAllowSlop,
                            int32_t *aX,
                            int32_t *aY)
{
    ALOG("nsWindow[%p]::ConstrainPosition %d [%d %d]", (void*)this, aAllowSlop, *aX, *aY);

    // constrain toplevel windows; children we don't care about
    if (IsTopLevel()) {
        *aX = 0;
        *aY = 0;
    }
}

void
nsWindow::Move(double aX,
               double aY)
{
    if (IsTopLevel())
        return;

    Resize(aX,
           aY,
           mBounds.width,
           mBounds.height,
           true);
}

void
nsWindow::Resize(double aWidth,
                 double aHeight,
                 bool aRepaint)
{
    Resize(mBounds.x,
           mBounds.y,
           aWidth,
           aHeight,
           aRepaint);
}

void
nsWindow::Resize(double aX,
                 double aY,
                 double aWidth,
                 double aHeight,
                 bool aRepaint)
{
    ALOG("nsWindow[%p]::Resize [%f %f %f %f] (repaint %d)", (void*)this, aX, aY, aWidth, aHeight, aRepaint);

    bool needSizeDispatch = aWidth != mBounds.width || aHeight != mBounds.height;

    mBounds.x = NSToIntRound(aX);
    mBounds.y = NSToIntRound(aY);
    mBounds.width = NSToIntRound(aWidth);
    mBounds.height = NSToIntRound(aHeight);

    if (needSizeDispatch) {
        OnSizeChanged(gfx::IntSize::Truncate(aWidth, aHeight));
    }

    // Should we skip honoring aRepaint here?
    if (aRepaint && FindTopLevel() == nsWindow::TopWindow())
        RedrawAll();

    nsIWidgetListener* listener = GetWidgetListener();
    if (mAwaitingFullScreen && listener) {
        listener->FullscreenChanged(mIsFullScreen);
        mAwaitingFullScreen = false;
    }
}

void
nsWindow::SetZIndex(int32_t aZIndex)
{
    ALOG("nsWindow[%p]::SetZIndex %d ignored", (void*)this, aZIndex);
}

void
nsWindow::SetSizeMode(nsSizeMode aMode)
{
    switch (aMode) {
        case nsSizeMode_Minimized:
            GeckoAppShell::MoveTaskToBack();
            break;
        case nsSizeMode_Fullscreen:
            MakeFullScreen(true);
            break;
        default:
            break;
    }
}

void
nsWindow::Enable(bool aState)
{
    ALOG("nsWindow[%p]::Enable %d ignored", (void*)this, aState);
}

bool
nsWindow::IsEnabled() const
{
    return true;
}

void
nsWindow::Invalidate(const LayoutDeviceIntRect& aRect)
{
}

nsWindow*
nsWindow::FindTopLevel()
{
    nsWindow *toplevel = this;
    while (toplevel) {
        if (toplevel->IsTopLevel())
            return toplevel;

        toplevel = toplevel->mParent;
    }

    ALOG("nsWindow::FindTopLevel(): couldn't find a toplevel or dialog window in this [%p] widget's hierarchy!", (void*)this);
    return this;
}

nsresult
nsWindow::SetFocus(bool aRaise)
{
    nsWindow *top = FindTopLevel();
    top->BringToFront();

    return NS_OK;
}

void
nsWindow::BringToFront()
{
    // If the window to be raised is the same as the currently raised one,
    // do nothing. We need to check the focus manager as well, as the first
    // window that is created will be first in the window list but won't yet
    // be focused.
    nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
    nsCOMPtr<mozIDOMWindowProxy> existingTopWindow;
    fm->GetActiveWindow(getter_AddRefs(existingTopWindow));
    if (existingTopWindow && FindTopLevel() == nsWindow::TopWindow())
        return;

    if (!IsTopLevel()) {
        FindTopLevel()->BringToFront();
        return;
    }

    RefPtr<nsWindow> kungFuDeathGrip(this);

    nsWindow *oldTop = nullptr;
    if (!gTopLevelWindows.IsEmpty()) {
        oldTop = gTopLevelWindows[0];
    }

    gTopLevelWindows.RemoveElement(this);
    gTopLevelWindows.InsertElementAt(0, this);

    if (oldTop) {
      nsIWidgetListener* listener = oldTop->GetWidgetListener();
      if (listener) {
          listener->WindowDeactivated();
      }
    }

    if (mWidgetListener) {
        mWidgetListener->WindowActivated();
    }

    RedrawAll();
}

LayoutDeviceIntRect
nsWindow::GetScreenBounds()
{
    return LayoutDeviceIntRect(WidgetToScreenOffset(), mBounds.Size());
}

LayoutDeviceIntPoint
nsWindow::WidgetToScreenOffset()
{
    LayoutDeviceIntPoint p(0, 0);
    nsWindow *w = this;

    while (w && !w->IsTopLevel()) {
        p.x += w->mBounds.x;
        p.y += w->mBounds.y;

        w = w->mParent;
    }

    return p;
}

nsresult
nsWindow::DispatchEvent(WidgetGUIEvent* aEvent,
                        nsEventStatus& aStatus)
{
    aStatus = DispatchEvent(aEvent);
    return NS_OK;
}

nsEventStatus
nsWindow::DispatchEvent(WidgetGUIEvent* aEvent)
{
    if (mAttachedWidgetListener) {
        return mAttachedWidgetListener->HandleEvent(aEvent, mUseAttachedEvents);
    } else if (mWidgetListener) {
        return mWidgetListener->HandleEvent(aEvent, mUseAttachedEvents);
    }
    return nsEventStatus_eIgnore;
}

nsresult
nsWindow::MakeFullScreen(bool aFullScreen, nsIScreen*)
{
    mIsFullScreen = aFullScreen;
    mAwaitingFullScreen = true;
    GeckoAppShell::SetFullScreen(aFullScreen);
    return NS_OK;
}

mozilla::layers::LayerManager*
nsWindow::GetLayerManager(PLayerTransactionChild*, LayersBackend, LayerManagerPersistence)
{
    if (mLayerManager) {
        return mLayerManager;
    }
    return nullptr;
}

void
nsWindow::CreateLayerManager(int aCompositorWidth, int aCompositorHeight)
{
    if (mLayerManager) {
        return;
    }

    nsWindow *topLevelWindow = FindTopLevel();
    if (!topLevelWindow || topLevelWindow->mWindowType == eWindowType_invisible) {
        // don't create a layer manager for an invisible top-level window
        return;
    }

    // Ensure that gfxPlatform is initialized first.
    gfxPlatform::GetPlatform();

    if (ShouldUseOffMainThreadCompositing()) {
        CreateCompositor(aCompositorWidth, aCompositorHeight);
        if (mLayerManager) {
            return;
        }

        // If we get here, then off main thread compositing failed to initialize.
        sFailedToCreateGLContext = true;
    }

    if (!ComputeShouldAccelerate() || sFailedToCreateGLContext) {
        printf_stderr(" -- creating basic, not accelerated\n");
        mLayerManager = CreateBasicLayerManager();
    }
}

void
nsWindow::OnSizeChanged(const gfx::IntSize& aSize)
{
    ALOG("nsWindow: %p OnSizeChanged [%d %d]", (void*)this, aSize.width, aSize.height);

    mBounds.width = aSize.width;
    mBounds.height = aSize.height;

    if (mWidgetListener) {
        mWidgetListener->WindowResized(this, aSize.width, aSize.height);
    }

    if (mAttachedWidgetListener) {
        mAttachedWidgetListener->WindowResized(this, aSize.width, aSize.height);
    }
}

void
nsWindow::InitEvent(WidgetGUIEvent& event, LayoutDeviceIntPoint* aPoint)
{
    if (aPoint) {
        event.mRefPoint = *aPoint;
    } else {
        event.mRefPoint = LayoutDeviceIntPoint(0, 0);
    }

    event.mTime = PR_Now() / 1000;
}

void
nsWindow::UpdateOverscrollVelocity(const float aX, const float aY)
{
    if (NativePtr<NPZCSupport>::Locked npzcs{mNPZCSupport}) {
        npzcs->UpdateOverscrollVelocity(aX, aY);
    }
}

void
nsWindow::UpdateOverscrollOffset(const float aX, const float aY)
{
    if (NativePtr<NPZCSupport>::Locked npzcs{mNPZCSupport}) {
        npzcs->UpdateOverscrollOffset(aX, aY);
    }
}

void
nsWindow::SetSelectionDragState(bool aState)
{
    if (NativePtr<NPZCSupport>::Locked npzcs{mNPZCSupport}) {
        npzcs->SetSelectionDragState(aState);
    }
}

void *
nsWindow::GetNativeData(uint32_t aDataType)
{
    switch (aDataType) {
        // used by GLContextProviderEGL, nullptr is EGL_DEFAULT_DISPLAY
        case NS_NATIVE_DISPLAY:
            return nullptr;

        case NS_NATIVE_WIDGET:
            return (void *) this;

        case NS_RAW_NATIVE_IME_CONTEXT: {
            void* pseudoIMEContext = GetPseudoIMEContext();
            if (pseudoIMEContext) {
                return pseudoIMEContext;
            }
            // We assume that there is only one context per process on Android
            return NS_ONLY_ONE_NATIVE_IME_CONTEXT;
        }

        case NS_JAVA_SURFACE:
            if (NativePtr<LayerViewSupport>::Locked lvs{mLayerViewSupport}) {
                return lvs->GetSurface().Get();
            }
            return nullptr;

        case NS_PRESENTATION_WINDOW:
            return PMPMSupport::sWindow;

        case NS_PRESENTATION_SURFACE:
            return PMPMSupport::sSurface;
    }

    return nullptr;
}

void
nsWindow::SetNativeData(uint32_t aDataType, uintptr_t aVal)
{
    switch (aDataType) {
        case NS_PRESENTATION_SURFACE:
            PMPMSupport::sSurface = reinterpret_cast<EGLSurface>(aVal);
            break;
    }
}

void
nsWindow::DispatchHitTest(const WidgetTouchEvent& aEvent)
{
    if (aEvent.mMessage == eTouchStart && aEvent.mTouches.Length() == 1) {
        // Since touch events don't get retargeted by PositionedEventTargeting.cpp
        // code on Fennec, we dispatch a dummy mouse event that *does* get
        // retargeted. The Fennec browser.js code can use this to activate the
        // highlight element in case the this touchstart is the start of a tap.
        WidgetMouseEvent hittest(true, eMouseHitTest, this,
                                 WidgetMouseEvent::eReal);
        hittest.mRefPoint = aEvent.mTouches[0]->mRefPoint;
        hittest.mIgnoreRootScrollFrame = true;
        hittest.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
        nsEventStatus status;
        DispatchEvent(&hittest, status);

        if (mAPZEventState && hittest.hitCluster) {
            mAPZEventState->ProcessClusterHit();
        }
    }
}

mozilla::Modifiers
nsWindow::GetModifiers(int32_t metaState)
{
    using mozilla::java::sdk::KeyEvent;
    return (metaState & KeyEvent::META_ALT_MASK ? MODIFIER_ALT : 0)
        | (metaState & KeyEvent::META_SHIFT_MASK ? MODIFIER_SHIFT : 0)
        | (metaState & KeyEvent::META_CTRL_MASK ? MODIFIER_CONTROL : 0)
        | (metaState & KeyEvent::META_META_MASK ? MODIFIER_META : 0)
        | (metaState & KeyEvent::META_FUNCTION_ON ? MODIFIER_FN : 0)
        | (metaState & KeyEvent::META_CAPS_LOCK_ON ? MODIFIER_CAPSLOCK : 0)
        | (metaState & KeyEvent::META_NUM_LOCK_ON ? MODIFIER_NUMLOCK : 0)
        | (metaState & KeyEvent::META_SCROLL_LOCK_ON ? MODIFIER_SCROLLLOCK : 0);
}

TimeStamp
nsWindow::GetEventTimeStamp(int64_t aEventTime)
{
    // Android's event time is SystemClock.uptimeMillis that is counted in ms
    // since OS was booted.
    // (https://developer.android.com/reference/android/os/SystemClock.html)
    // and this SystemClock.uptimeMillis uses SYSTEM_TIME_MONOTONIC.
    // Our posix implemententaion of TimeStamp::Now uses SYSTEM_TIME_MONOTONIC
    //  too. Due to same implementation, we can use this via FromSystemTime.
    int64_t tick =
        BaseTimeDurationPlatformUtils::TicksFromMilliseconds(aEventTime);
    return TimeStamp::FromSystemTime(tick);
}

void
nsWindow::GeckoViewSupport::EnableEventDispatcher()
{
    if (!mGeckoViewWindow) {
        return;
    }
    mGeckoViewWindow->SetState(GeckoView::State::READY());
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

TextEventDispatcherListener*
nsWindow::GetNativeTextEventDispatcherListener()
{
    nsWindow* top = FindTopLevel();
    MOZ_ASSERT(top);

    if (!top->mEditableSupport) {
        // Non-GeckoView windows don't support IME operations.
        return nullptr;
    }
    return top->mEditableSupport;
}

void
nsWindow::SetInputContext(const InputContext& aContext,
                          const InputContextAction& aAction)
{
    nsWindow* top = FindTopLevel();
    MOZ_ASSERT(top);

    if (!top->mEditableSupport) {
        // Non-GeckoView windows don't support IME operations.
        return;
    }

    // We are using an IME event later to notify Java, and the IME event
    // will be processed by the top window. Therefore, to ensure the
    // IME event uses the correct mInputContext, we need to let the top
    // window process SetInputContext
    top->mEditableSupport->SetInputContext(aContext, aAction);
}

InputContext
nsWindow::GetInputContext()
{
    nsWindow* top = FindTopLevel();
    MOZ_ASSERT(top);

    if (!top->mEditableSupport) {
        // Non-GeckoView windows don't support IME operations.
        return InputContext();
    }

    // We let the top window process SetInputContext,
    // so we should let it process GetInputContext as well.
    return top->mEditableSupport->GetInputContext();
}

nsresult
nsWindow::SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                     TouchPointerState aPointerState,
                                     LayoutDeviceIntPoint aPoint,
                                     double aPointerPressure,
                                     uint32_t aPointerOrientation,
                                     nsIObserver* aObserver)
{
    mozilla::widget::AutoObserverNotifier notifier(aObserver, "touchpoint");

    int eventType;
    switch (aPointerState) {
    case TOUCH_CONTACT:
        // This could be a ACTION_DOWN or ACTION_MOVE depending on the
        // existing state; it is mapped to the right thing in Java.
        eventType = sdk::MotionEvent::ACTION_POINTER_DOWN;
        break;
    case TOUCH_REMOVE:
        // This could be turned into a ACTION_UP in Java
        eventType = sdk::MotionEvent::ACTION_POINTER_UP;
        break;
    case TOUCH_CANCEL:
        eventType = sdk::MotionEvent::ACTION_CANCEL;
        break;
    case TOUCH_HOVER:   // not supported for now
    default:
        return NS_ERROR_UNEXPECTED;
    }

    MOZ_ASSERT(mLayerViewSupport);
    GeckoLayerClient::LocalRef client = mLayerViewSupport->GetLayerClient();
    client->SynthesizeNativeTouchPoint(aPointerId, eventType,
        aPoint.x, aPoint.y, aPointerPressure, aPointerOrientation);

    return NS_OK;
}

nsresult
nsWindow::SynthesizeNativeMouseEvent(LayoutDeviceIntPoint aPoint,
                                     uint32_t aNativeMessage,
                                     uint32_t aModifierFlags,
                                     nsIObserver* aObserver)
{
    mozilla::widget::AutoObserverNotifier notifier(aObserver, "mouseevent");

    MOZ_ASSERT(mLayerViewSupport);
    GeckoLayerClient::LocalRef client = mLayerViewSupport->GetLayerClient();
    client->SynthesizeNativeMouseEvent(aNativeMessage, aPoint.x, aPoint.y);

    return NS_OK;
}

nsresult
nsWindow::SynthesizeNativeMouseMove(LayoutDeviceIntPoint aPoint,
                                    nsIObserver* aObserver)
{
    mozilla::widget::AutoObserverNotifier notifier(aObserver, "mouseevent");

    MOZ_ASSERT(mLayerViewSupport);
    GeckoLayerClient::LocalRef client = mLayerViewSupport->GetLayerClient();
    client->SynthesizeNativeMouseEvent(sdk::MotionEvent::ACTION_HOVER_MOVE, aPoint.x, aPoint.y);

    return NS_OK;
}

bool
nsWindow::WidgetPaintsBackground()
{
    static bool sWidgetPaintsBackground = true;
    static bool sWidgetPaintsBackgroundPrefCached = false;

    if (!sWidgetPaintsBackgroundPrefCached) {
        sWidgetPaintsBackgroundPrefCached = true;
        mozilla::Preferences::AddBoolVarCache(&sWidgetPaintsBackground,
                                              "android.widget_paints_background",
                                              true);
    }

    return sWidgetPaintsBackground;
}

bool
nsWindow::NeedsPaint()
{
    if (!mLayerViewSupport || mLayerViewSupport->CompositorPaused() ||
            // FindTopLevel() != nsWindow::TopWindow() ||
            !GetLayerManager(nullptr)) {
        return false;
    }
    return nsIWidget::NeedsPaint();
}

void
nsWindow::ConfigureAPZControllerThread()
{
    APZThreadUtils::SetControllerThread(mozilla::GetAndroidUiThreadMessageLoop());
}

already_AddRefed<GeckoContentController>
nsWindow::CreateRootContentController()
{
    RefPtr<GeckoContentController> controller = new AndroidContentController(this, mAPZEventState, mAPZC);
    return controller.forget();
}

uint32_t
nsWindow::GetMaxTouchPoints() const
{
    return GeckoAppShell::GetMaxTouchPoints();
}

void
nsWindow::UpdateZoomConstraints(const uint32_t& aPresShellId,
                                const FrameMetrics::ViewID& aViewId,
                                const mozilla::Maybe<ZoomConstraints>& aConstraints)
{
    nsBaseWidget::UpdateZoomConstraints(aPresShellId, aViewId, aConstraints);
}

CompositorBridgeChild*
nsWindow::GetCompositorBridgeChild() const
{
    return mCompositorSession ? mCompositorSession->GetCompositorBridgeChild() : nullptr;
}

already_AddRefed<nsIScreen>
nsWindow::GetWidgetScreen()
{
    nsCOMPtr<nsIScreenManager> screenMgr =
        do_GetService("@mozilla.org/gfx/screenmanager;1");
    MOZ_ASSERT(screenMgr, "Failed to get nsIScreenManager");

    RefPtr<nsScreenManagerAndroid> screenMgrAndroid =
        (nsScreenManagerAndroid*) screenMgr.get();
    return screenMgrAndroid->ScreenForId(mScreenId);
}

jni::DependentRef<java::GeckoLayerClient>
nsWindow::GetLayerClient()
{
    if (NativePtr<LayerViewSupport>::Locked lvs{mLayerViewSupport}) {
        return lvs->GetLayerClient().Get();
    }
    return nullptr;
}
void
nsWindow::RecvToolbarAnimatorMessageFromCompositor(int32_t aMessage)
{
    MOZ_ASSERT(AndroidBridge::IsJavaUiThread());
    if (NativePtr<LayerViewSupport>::Locked lvs{mLayerViewSupport}) {
        lvs->RecvToolbarAnimatorMessage(aMessage);
    }
}

void
nsWindow::UpdateRootFrameMetrics(const ScreenPoint& aScrollOffset, const CSSToScreenScale& aZoom, const CSSRect& aPage)
{

  MOZ_ASSERT(AndroidBridge::IsJavaUiThread());
  if (NativePtr<LayerViewSupport>::Locked lvs{mLayerViewSupport}) {
    GeckoLayerClient::LocalRef client = lvs->GetLayerClient();
    client->UpdateRootFrameMetrics(aScrollOffset.x, aScrollOffset.y, aZoom.scale,
                                   aPage.x, aPage.y,
                                   aPage.XMost(), aPage.YMost());
  }
}

void
nsWindow::RecvScreenPixels(Shmem&& aMem, const ScreenIntSize& aSize)
{
  MOZ_ASSERT(AndroidBridge::IsJavaUiThread());
  if (NativePtr<LayerViewSupport>::Locked lvs{mLayerViewSupport}) {
    lvs->RecvScreenPixels(mozilla::Move(aMem), aSize);
  }
}

