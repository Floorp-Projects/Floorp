/* -*- Mode: c++; c-basic-offset: 2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set sw=2 ts=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <math.h>
#include <queue>
#include <type_traits>
#include <unistd.h>

#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/RWLock.h"
#include "mozilla/StaticPrefs_android.h"
#include "mozilla/StaticPrefs_ui.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/Unused.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/WheelHandlingHelper.h"  // for WheelDeltaAdjustmentStrategy

#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "mozilla/a11y/SessionAccessibility.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/layers/RenderTrace.h"
#include <algorithm>

using mozilla::Unused;
using mozilla::dom::ContentChild;
using mozilla::dom::ContentParent;

#include "nsWindow.h"

#include "AndroidGraphics.h"
#include "JavaExceptions.h"

#include "nsIWidgetListener.h"
#include "nsIWindowWatcher.h"
#include "nsIAppWindow.h"

#include "nsAppShell.h"
#include "nsFocusManager.h"
#include "nsIdleService.h"
#include "nsLayoutUtils.h"
#include "nsViewManager.h"

#include "WidgetUtils.h"
#include "nsContentUtils.h"

#include "nsGfxCIID.h"
#include "nsGkAtoms.h"
#include "nsWidgetsCID.h"

#include "gfxContext.h"

#include "AndroidContentController.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "Layers.h"
#include "ScopedGLHelpers.h"
#include "mozilla/layers/APZEventState.h"
#include "mozilla/layers/APZInputBridge.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layers/AsyncCompositionManager.h"
#include "mozilla/layers/CompositorOGL.h"
#include "mozilla/layers/IAPZCTreeManager.h"
#include "mozilla/layers/LayerManagerComposite.h"

#include "nsTArray.h"

#include "AndroidBridge.h"
#include "AndroidBridgeUtilities.h"
#include "AndroidUiThread.h"
#include "GeckoEditableSupport.h"
#include "GeneratedJNINatives.h"
#include "KeyEvent.h"
#include "MotionEvent.h"
#include "ScreenHelperAndroid.h"

#include "GeckoProfiler.h"  // For AUTO_PROFILER_LABEL
#include "nsPrintfCString.h"
#include "nsString.h"

#include "JavaBuiltins.h"

#include "mozilla/ipc/Shmem.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;
using namespace mozilla::widget;
using namespace mozilla::ipc;

using mozilla::java::GeckoSession;

#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/CompositorSession.h"
#include "mozilla/layers/LayerTransactionParent.h"
#include "mozilla/layers/UiCompositorControllerChild.h"
#include "nsThreadUtils.h"

// All the toplevel windows that have been created; these are in
// stacking order, so the window at gTopLevelWindows[0] is the topmost
// one.
static nsTArray<nsWindow*> gTopLevelWindows;

static bool sFailedToCreateGLContext = false;

// Multitouch swipe thresholds in inches
static const double SWIPE_MAX_PINCH_DELTA_INCHES = 0.4;
static const double SWIPE_MIN_DISTANCE_INCHES = 0.6;

static const int32_t INPUT_RESULT_UNHANDLED =
    java::PanZoomController::INPUT_RESULT_UNHANDLED;
static const int32_t INPUT_RESULT_HANDLED =
    java::PanZoomController::INPUT_RESULT_HANDLED;
static const int32_t INPUT_RESULT_HANDLED_CONTENT =
    java::PanZoomController::INPUT_RESULT_HANDLED_CONTENT;

template <typename Lambda, bool IsStatic, typename InstanceType, class Impl>
class nsWindow::WindowEvent : public Runnable {
  bool IsStaleCall() {
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

  Lambda mLambda;
  const InstanceType mInstance;

 public:
  WindowEvent(Lambda&& aLambda, InstanceType&& aInstance)
      : Runnable("nsWindowEvent"),
        mLambda(std::move(aLambda)),
        mInstance(std::forward<InstanceType>(aInstance)) {}

  explicit WindowEvent(Lambda&& aLambda)
      : Runnable("nsWindowEvent"),
        mLambda(std::move(aLambda)),
        mInstance(mLambda.GetThisArg()) {}

  NS_IMETHOD Run() override {
    if (!IsStaleCall()) {
      mLambda();
    }
    return NS_OK;
  }
};

namespace {
template <class Instance, class Impl>
std::enable_if_t<
    jni::detail::NativePtrPicker<Impl>::value == jni::detail::REFPTR, void>
CallAttachNative(Instance aInstance, Impl* aImpl) {
  Impl::AttachNative(aInstance, RefPtr<Impl>(aImpl).get());
}

template <class Instance, class Impl>
std::enable_if_t<
    jni::detail::NativePtrPicker<Impl>::value == jni::detail::OWNING, void>
CallAttachNative(Instance aInstance, Impl* aImpl) {
  Impl::AttachNative(aInstance, UniquePtr<Impl>(aImpl));
}

template <class Lambda>
bool DispatchToUiThread(const char* aName, Lambda&& aLambda) {
  if (RefPtr<nsThread> uiThread = GetAndroidUiThread()) {
    uiThread->Dispatch(NS_NewRunnableFunction(aName, std::move(aLambda)));
    return true;
  }
  return false;
}
}  // namespace

template <class Impl>
template <class Cls, typename... Args>
void nsWindow::NativePtr<Impl>::Attach(const jni::LocalRef<Cls>& aInstance,
                                       nsWindow* aWindow, Args&&... aArgs) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mPtr && !mImpl);

  Impl* const impl = new Impl(this, aWindow, std::forward<Args>(aArgs)...);
  mImpl = impl;

  // CallAttachNative transfers ownership of impl.
  CallAttachNative<>(aInstance, impl);
}

template <class Impl>
template <class Cls, typename T>
void nsWindow::NativePtr<Impl>::Detach(const jni::Ref<Cls, T>& aInstance) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPtr && mImpl);

  // nsIRunnable that takes care of disposing the native object attached to
  // the Java object in a safe manner.
  class ImplDisposer : public Runnable {
    const typename Cls::GlobalRef mInstance;
    const uintptr_t mOldImpl;

   public:
    explicit ImplDisposer(const typename Cls::LocalRef& aInstance)
        : Runnable("nsWindow::NativePtr::Detach"),
          mInstance(aInstance.Env(), aInstance),
          mOldImpl(aInstance
                       ? jni::GetNativeHandle(aInstance.Env(), aInstance.Get())
                       : 0) {
      MOZ_CATCH_JNI_EXCEPTION(aInstance.Env());
    }

    NS_IMETHOD Run() override {
      if (!mInstance) {
        return NS_OK;
      }

      if (!NS_IsMainThread()) {
        NS_DispatchToMainThread(this);
        return NS_OK;
      }

      typename Cls::LocalRef instance(jni::GetGeckoThreadEnv(), mInstance);
      auto newImpl = jni::GetNativeHandle(instance.Env(), instance.Get());
      MOZ_CATCH_JNI_EXCEPTION(instance.Env());

      if (mOldImpl == newImpl) {
        // Only dispose the object if the native object has not changed.
        Impl::DisposeNative(instance);
      }
      return NS_OK;
    }
  };

  // Objects that use nsWindow::NativePtr are expected to implement a public
  // member function with signature "void OnDetach(
  // already_AddRefed<Runnable> aDisposer)".  This function should perform
  // necessary cleanups for the native/Java objects, as well as mark the Java
  // object as being disposed, so no native methods are called after that
  // point. After this disposal step, the function must call "aDisposer->
  // Run()" to finish disposing the native object. The disposer is
  // thread-safe and may be called on any thread as necessary.
  mImpl->OnDetach(
      do_AddRef(new ImplDisposer({jni::GetGeckoThreadEnv(), aInstance})));

  {
    Locked implLock(*this);
    mImpl = nullptr;
  }

  typename WindowPtr<Impl>::Locked lock(*mPtr);
  mPtr->mWindow = nullptr;
  mPtr->mPtr = nullptr;
  mPtr = nullptr;
}

template <class Impl>
class nsWindow::NativePtr<Impl>::Locked final : private MutexAutoLock {
  Impl* const mImpl;

 public:
  explicit Locked(NativePtr<Impl>& aPtr)
      : MutexAutoLock(aPtr.mImplLock), mImpl(aPtr.mImpl) {}

  operator Impl*() const { return mImpl; }
  Impl* operator->() const { return mImpl; }
};

class nsWindow::GeckoViewSupport final
    : public GeckoSession::Window::Natives<GeckoViewSupport>,
      public SupportsWeakPtr<GeckoViewSupport> {
  nsWindow& window;

  // We hold a WeakRef because we want to allow the
  // GeckoSession.Window to be garbage collected.
  // Callers need to create a LocalRef from this
  // before calling methods.
  GeckoSession::Window::WeakRef mGeckoViewWindow;

 public:
  typedef GeckoSession::Window::Natives<GeckoViewSupport> Base;
  typedef SupportsWeakPtr<GeckoViewSupport> SupportsWeakPtr;

  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(GeckoViewSupport);

  template <typename Functor>
  static void OnNativeCall(Functor&& aCall) {
    NS_DispatchToMainThread(new WindowEvent<Functor>(std::move(aCall)));
  }

  GeckoViewSupport(nsWindow* aWindow,
                   const GeckoSession::Window::LocalRef& aInstance)
      : window(*aWindow), mGeckoViewWindow(aInstance) {
    Base::AttachNative(aInstance, static_cast<SupportsWeakPtr*>(this));
  }

  ~GeckoViewSupport();

  using Base::DisposeNative;

  /**
   * GeckoView methods
   */
 private:
  nsCOMPtr<nsPIDOMWindowOuter> mDOMWindow;
  bool mIsReady{false};

 public:
  // Create and attach a window.
  static void Open(const jni::Class::LocalRef& aCls,
                   GeckoSession::Window::Param aWindow,
                   jni::Object::Param aQueue, jni::Object::Param aCompositor,
                   jni::Object::Param aDispatcher,
                   jni::Object::Param aSessionAccessibility,
                   jni::Object::Param aInitData, jni::String::Param aId,
                   jni::String::Param aChromeURI, int32_t aScreenId,
                   bool aPrivateMode, bool aRemote);

  // Close and destroy the nsWindow.
  void Close();

  // Transfer this nsWindow to new GeckoSession objects.
  void Transfer(const GeckoSession::Window::LocalRef& inst,
                jni::Object::Param aQueue, jni::Object::Param aCompositor,
                jni::Object::Param aDispatcher,
                jni::Object::Param aSessionAccessibility,
                jni::Object::Param aInitData);

  void AttachEditable(const GeckoSession::Window::LocalRef& inst,
                      jni::Object::Param aEditableParent);

  void AttachAccessibility(const GeckoSession::Window::LocalRef& inst,
                           jni::Object::Param aSessionAccessibility);

  void OnReady(jni::Object::Param aQueue = nullptr);
};

/**
 * PanZoomController handles its native calls on the UI thread, so make
 * it separate from GeckoViewSupport.
 */
class nsWindow::NPZCSupport final
    : public java::PanZoomController::NativeProvider::Natives<NPZCSupport> {
  using LockedWindowPtr = WindowPtr<NPZCSupport>::Locked;

  WindowPtr<NPZCSupport> mWindow;
  java::PanZoomController::NativeProvider::WeakRef mNPZC;
  int mPreviousButtons;

  template <typename Lambda>
  class InputEvent final : public nsAppShell::Event {
    java::PanZoomController::NativeProvider::GlobalRef mNPZC;
    Lambda mLambda;

   public:
    InputEvent(const NPZCSupport* aNPZCSupport, Lambda&& aLambda)
        : mNPZC(aNPZCSupport->mNPZC), mLambda(std::move(aLambda)) {}

    void Run() override {
      MOZ_ASSERT(NS_IsMainThread());

      JNIEnv* const env = jni::GetGeckoThreadEnv();
      NPZCSupport* npzcSupport = GetNative(
          java::PanZoomController::NativeProvider::LocalRef(env, mNPZC));

      if (!npzcSupport || !npzcSupport->mWindow) {
        // We already shut down.
        env->ExceptionClear();
        return;
      }

      nsWindow* const window = npzcSupport->mWindow;
      window->UserActivity();
      return mLambda(window);
    }

    bool IsUIEvent() const override { return true; }
  };

  template <typename Lambda>
  void PostInputEvent(Lambda&& aLambda) {
    // Use priority queue for input events.
    nsAppShell::PostEvent(
        MakeUnique<InputEvent<Lambda>>(this, std::move(aLambda)));
  }

 public:
  typedef java::PanZoomController::NativeProvider::Natives<NPZCSupport> Base;

  NPZCSupport(NativePtr<NPZCSupport>* aPtr, nsWindow* aWindow,
              const java::PanZoomController::NativeProvider::LocalRef& aNPZC)
      : mWindow(aPtr, aWindow), mNPZC(aNPZC), mPreviousButtons(0) {
    MOZ_ASSERT(mWindow);
  }

  ~NPZCSupport() {}

  using Base::AttachNative;
  using Base::DisposeNative;

  void OnDetach(already_AddRefed<Runnable> aDisposer) {
    RefPtr<Runnable> disposer = aDisposer;
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

    if (RefPtr<nsThread> uiThread = GetAndroidUiThread()) {
      auto npzc = java::PanZoomController::NativeProvider::GlobalRef(mNPZC);
      if (!npzc) {
        return;
      }

      uiThread->Dispatch(NS_NewRunnableFunction(
          "NPZCSupport::OnDetach", [npzc, disposer = std::move(disposer)] {
            npzc->SetAttached(false);
            disposer->Run();
          }));
    }
  }

  const java::PanZoomController::NativeProvider::Ref& GetJavaNPZC() const {
    return mNPZC;
  }

 public:
  void SetIsLongpressEnabled(bool aIsLongpressEnabled) {
    RefPtr<IAPZCTreeManager> controller;

    if (LockedWindowPtr window{mWindow}) {
      controller = window->mAPZC;
    }

    if (controller) {
      controller->SetLongTapEnabled(aIsLongpressEnabled);
    }
  }

  int32_t HandleScrollEvent(int64_t aTime, int32_t aMetaState, float aX,
                            float aY, float aHScroll, float aVScroll) {
    MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

    RefPtr<IAPZCTreeManager> controller;

    if (LockedWindowPtr window{mWindow}) {
      controller = window->mAPZC;
    }

    if (!controller) {
      return INPUT_RESULT_UNHANDLED;
    }

    ScreenPoint origin = ScreenPoint(aX, aY);

    if (StaticPrefs::ui_scrolling_negate_wheel_scroll()) {
      aHScroll = -aHScroll;
      aVScroll = -aVScroll;
    }

    ScrollWheelInput input(
        aTime, GetEventTimeStamp(aTime), GetModifiers(aMetaState),
        ScrollWheelInput::SCROLLMODE_SMOOTH,
        ScrollWheelInput::SCROLLDELTA_PIXEL, origin, aHScroll, aVScroll, false,
        // XXX Do we need to support auto-dir scrolling
        // for Android widgets with a wheel device?
        // Currently, I just leave it unimplemented. If
        // we need to implement it, what's the extra work
        // to do?
        WheelDeltaAdjustmentStrategy::eNone);

    APZEventResult result = controller->InputBridge()->ReceiveInputEvent(input);

    if (result.mStatus == nsEventStatus_eConsumeNoDefault) {
      return INPUT_RESULT_HANDLED;
    }

    PostInputEvent([input, result](nsWindow* window) {
      WidgetWheelEvent wheelEvent = input.ToWidgetWheelEvent(window);
      window->ProcessUntransformedAPZEvent(&wheelEvent, result);
    });

    if (result.mHitRegionWithApzAwareListeners) {
      return INPUT_RESULT_HANDLED_CONTENT;
    }

    switch (result.mStatus) {
      case nsEventStatus_eIgnore:
        return INPUT_RESULT_UNHANDLED;
      case nsEventStatus_eConsumeDoDefault:
        return INPUT_RESULT_HANDLED;
      default:
        MOZ_ASSERT_UNREACHABLE("Unexpected nsEventStatus");
        return INPUT_RESULT_UNHANDLED;
    }
  }

 private:
  static MouseInput::ButtonType GetButtonType(int button) {
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
      result |= MouseButtonsFlag::eLeftFlag;
    }
    if (buttons & java::sdk::MotionEvent::BUTTON_SECONDARY) {
      result |= MouseButtonsFlag::eRightFlag;
    }
    if (buttons & java::sdk::MotionEvent::BUTTON_TERTIARY) {
      result |= MouseButtonsFlag::eMiddleFlag;
    }
    if (buttons & java::sdk::MotionEvent::BUTTON_BACK) {
      result |= MouseButtonsFlag::e4thFlag;
    }
    if (buttons & java::sdk::MotionEvent::BUTTON_FORWARD) {
      result |= MouseButtonsFlag::e5thFlag;
    }

    return result;
  }

 public:
  int32_t HandleMouseEvent(int32_t aAction, int64_t aTime, int32_t aMetaState,
                           float aX, float aY, int buttons) {
    MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

    RefPtr<IAPZCTreeManager> controller;

    if (LockedWindowPtr window{mWindow}) {
      controller = window->mAPZC;
    }

    if (!controller) {
      return INPUT_RESULT_UNHANDLED;
    }

    MouseInput::MouseType mouseType = MouseInput::MOUSE_NONE;
    MouseInput::ButtonType buttonType = MouseInput::NONE;
    switch (aAction) {
      case java::sdk::MotionEvent::ACTION_DOWN:
        mouseType = MouseInput::MOUSE_DOWN;
        buttonType = GetButtonType(buttons ^ mPreviousButtons);
        mPreviousButtons = buttons;
        break;
      case java::sdk::MotionEvent::ACTION_UP:
        mouseType = MouseInput::MOUSE_UP;
        buttonType = GetButtonType(buttons ^ mPreviousButtons);
        mPreviousButtons = buttons;
        break;
      case java::sdk::MotionEvent::ACTION_MOVE:
        mouseType = MouseInput::MOUSE_MOVE;
        break;
      case java::sdk::MotionEvent::ACTION_HOVER_MOVE:
        mouseType = MouseInput::MOUSE_MOVE;
        break;
      case java::sdk::MotionEvent::ACTION_HOVER_ENTER:
        mouseType = MouseInput::MOUSE_WIDGET_ENTER;
        break;
      case java::sdk::MotionEvent::ACTION_HOVER_EXIT:
        mouseType = MouseInput::MOUSE_WIDGET_EXIT;
        break;
      default:
        break;
    }

    if (mouseType == MouseInput::MOUSE_NONE) {
      return INPUT_RESULT_UNHANDLED;
    }

    ScreenPoint origin = ScreenPoint(aX, aY);

    MouseInput input(mouseType, buttonType,
                     MouseEvent_Binding::MOZ_SOURCE_MOUSE,
                     ConvertButtons(buttons), origin, aTime,
                     GetEventTimeStamp(aTime), GetModifiers(aMetaState));

    APZEventResult result = controller->InputBridge()->ReceiveInputEvent(input);

    if (result.mStatus == nsEventStatus_eConsumeNoDefault) {
      return INPUT_RESULT_HANDLED;
    }

    PostInputEvent([input, result](nsWindow* window) {
      WidgetMouseEvent mouseEvent = input.ToWidgetMouseEvent(window);
      window->ProcessUntransformedAPZEvent(&mouseEvent, result);
    });

    if (result.mHitRegionWithApzAwareListeners) {
      return INPUT_RESULT_HANDLED_CONTENT;
    }

    switch (result.mStatus) {
      case nsEventStatus_eIgnore:
        return INPUT_RESULT_UNHANDLED;
      case nsEventStatus_eConsumeDoDefault:
        return INPUT_RESULT_HANDLED;
      default:
        MOZ_ASSERT_UNREACHABLE("Unexpected nsEventStatus");
        return INPUT_RESULT_UNHANDLED;
    }
  }

  int32_t HandleMotionEvent(
      const java::PanZoomController::NativeProvider::LocalRef& aInstance,
      int32_t aAction, int32_t aActionIndex, int64_t aTime, int32_t aMetaState,
      float aScreenX, float aScreenY, jni::IntArray::Param aPointerId,
      jni::FloatArray::Param aX, jni::FloatArray::Param aY,
      jni::FloatArray::Param aOrientation, jni::FloatArray::Param aPressure,
      jni::FloatArray::Param aToolMajor, jni::FloatArray::Param aToolMinor) {
    MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

    RefPtr<IAPZCTreeManager> controller;

    if (LockedWindowPtr window{mWindow}) {
      controller = window->mAPZC;
    }

    if (!controller) {
      return INPUT_RESULT_UNHANDLED;
    }

    nsTArray<int32_t> pointerId(aPointerId->GetElements());
    MultiTouchInput::MultiTouchType type;
    size_t startIndex = 0;
    size_t endIndex = pointerId.Length();

    switch (aAction) {
      case java::sdk::MotionEvent::ACTION_DOWN:
      case java::sdk::MotionEvent::ACTION_POINTER_DOWN:
        type = MultiTouchInput::MULTITOUCH_START;
        break;
      case java::sdk::MotionEvent::ACTION_MOVE:
        type = MultiTouchInput::MULTITOUCH_MOVE;
        break;
      case java::sdk::MotionEvent::ACTION_UP:
      case java::sdk::MotionEvent::ACTION_POINTER_UP:
        // for pointer-up events we only want the data from
        // the one pointer that went up
        type = MultiTouchInput::MULTITOUCH_END;
        startIndex = aActionIndex;
        endIndex = aActionIndex + 1;
        break;
      case java::sdk::MotionEvent::ACTION_OUTSIDE:
      case java::sdk::MotionEvent::ACTION_CANCEL:
        type = MultiTouchInput::MULTITOUCH_CANCEL;
        break;
      default:
        return INPUT_RESULT_UNHANDLED;
    }

    MultiTouchInput input(type, aTime, GetEventTimeStamp(aTime), 0);
    input.modifiers = GetModifiers(aMetaState);
    input.mTouches.SetCapacity(endIndex - startIndex);
    input.mScreenOffset =
        ExternalIntPoint(int32_t(floorf(aScreenX)), int32_t(floorf(aScreenY)));

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

      nsIntPoint point =
          nsIntPoint(int32_t(floorf(x[i])), int32_t(floorf(y[i])));

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

    APZEventResult result = controller->InputBridge()->ReceiveInputEvent(input);

    if (result.mStatus == nsEventStatus_eConsumeNoDefault) {
      return INPUT_RESULT_HANDLED;
    }

    // Dispatch APZ input event on Gecko thread.
    PostInputEvent([input, result](nsWindow* window) {
      WidgetTouchEvent touchEvent = input.ToWidgetTouchEvent(window);
      window->ProcessUntransformedAPZEvent(&touchEvent, result);
      window->DispatchHitTest(touchEvent);
    });

    if (result.mHitRegionWithApzAwareListeners) {
      return INPUT_RESULT_HANDLED_CONTENT;
    }

    switch (result.mStatus) {
      case nsEventStatus_eIgnore:
        return INPUT_RESULT_UNHANDLED;
      case nsEventStatus_eConsumeDoDefault:
        return INPUT_RESULT_HANDLED;
      default:
        MOZ_ASSERT_UNREACHABLE("Unexpected nsEventStatus");
        return INPUT_RESULT_UNHANDLED;
    }
  }
};

template <>
const char nsWindow::NativePtr<nsWindow::NPZCSupport>::sName[] = "NPZCSupport";

NS_IMPL_ISUPPORTS(nsWindow::AndroidView, nsIAndroidEventDispatcher,
                  nsIAndroidView)

nsresult nsWindow::AndroidView::GetInitData(JSContext* aCx,
                                            JS::MutableHandleValue aOut) {
  if (!mInitData) {
    aOut.setNull();
    return NS_OK;
  }

  return widget::EventDispatcher::UnboxBundle(aCx, mInitData, aOut);
}

/**
 * Compositor has some unique requirements for its native calls, so make it
 * separate from GeckoViewSupport.
 */
class nsWindow::LayerViewSupport final
    : public GeckoSession::Compositor::Natives<LayerViewSupport> {
  using LockedWindowPtr = WindowPtr<LayerViewSupport>::Locked;

  WindowPtr<LayerViewSupport> mWindow;
  GeckoSession::Compositor::WeakRef mCompositor;
  Atomic<bool, ReleaseAcquire> mCompositorPaused;
  jni::Object::GlobalRef mSurface;

  struct CaptureRequest {
    explicit CaptureRequest() : mResult(nullptr) {}
    explicit CaptureRequest(java::GeckoResult::GlobalRef aResult,
                            const ScreenRect& aSource,
                            const IntSize& aOutputSize)
        : mResult(aResult), mSource(aSource), mOutputSize(aOutputSize) {}

    // where to send the pixels
    java::GeckoResult::GlobalRef mResult;

    ScreenRect mSource;

    IntSize mOutputSize;
  };
  std::queue<CaptureRequest> mCapturePixelsResults;

  // In order to use Event::HasSameTypeAs in PostTo(), we cannot make
  // LayerViewEvent a template because each template instantiation is
  // a different type. So implement LayerViewEvent as a ProxyEvent.
  class LayerViewEvent final : public nsAppShell::ProxyEvent {
    using Event = nsAppShell::Event;

   public:
    static UniquePtr<Event> MakeEvent(UniquePtr<Event>&& event) {
      return MakeUnique<LayerViewEvent>(std::move(event));
    }

    explicit LayerViewEvent(UniquePtr<Event>&& event)
        : nsAppShell::ProxyEvent(std::move(event)) {}

    void PostTo(LinkedList<Event>& queue) override {
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
  typedef GeckoSession::Compositor::Natives<LayerViewSupport> Base;

  static LayerViewSupport* FromNative(
      const GeckoSession::Compositor::LocalRef& instance) {
    return GetNative(instance);
  }

  LayerViewSupport(NativePtr<LayerViewSupport>* aPtr, nsWindow* aWindow,
                   const GeckoSession::Compositor::LocalRef& aInstance)
      : mWindow(aPtr, aWindow),
        mCompositor(aInstance),
        mCompositorPaused(true) {
    MOZ_ASSERT(mWindow);
  }

  ~LayerViewSupport() {}

  using Base::AttachNative;
  using Base::DisposeNative;

  void OnDetach(already_AddRefed<Runnable> aDisposer) {
    RefPtr<Runnable> disposer = aDisposer;
    if (RefPtr<nsThread> uiThread = GetAndroidUiThread()) {
      GeckoSession::Compositor::GlobalRef compositor(mCompositor);
      if (!compositor) {
        return;
      }

      uiThread->Dispatch(NS_NewRunnableFunction(
          "LayerViewSupport::OnDetach",
          [compositor, disposer = std::move(disposer),
           results = &mCapturePixelsResults, window = &mWindow] {
            if (LockedWindowPtr lock{*window}) {
              while (!results->empty()) {
                auto aResult =
                    java::GeckoResult::LocalRef(results->front().mResult);
                if (aResult) {
                  aResult->CompleteExceptionally(
                      java::sdk::IllegalStateException::New(
                          "The compositor has detached from the session")
                          .Cast<jni::Throwable>());
                  results->pop();
                }
              }
              compositor->OnCompositorDetached();
              disposer->Run();
            }
          }));
    }
  }

  const GeckoSession::Compositor::Ref& GetJavaCompositor() const {
    return mCompositor;
  }

  bool CompositorPaused() const { return mCompositorPaused; }

  jni::Object::Param GetSurface() { return mSurface; }

 private:
  already_AddRefed<UiCompositorControllerChild>
  GetUiCompositorControllerChild() {
    RefPtr<UiCompositorControllerChild> child;
    if (LockedWindowPtr window{mWindow}) {
      child = window->GetUiCompositorControllerChild();
    }
    return child.forget();
  }

  int8_t* FlipScreenPixels(Shmem& aMem, const ScreenIntSize& aInSize,
                           const ScreenRect& aInRegion,
                           const IntSize& aOutSize) {
    RefPtr<DataSourceSurface> image = gfx::CreateDataSourceSurfaceFromData(
        IntSize(aInSize.width, aInSize.height), SurfaceFormat::B8G8R8A8,
        aMem.get<uint8_t>(),
        StrideForFormatAndWidth(SurfaceFormat::B8G8R8A8, aInSize.width));
    RefPtr<DrawTarget> drawTarget =
        gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
            aOutSize, SurfaceFormat::B8G8R8A8);
    drawTarget->SetTransform(Matrix::Scaling(1.0, -1.0) *
                             Matrix::Translation(0, aOutSize.height));

    gfx::Rect srcRect(aInRegion.x,
                      (aInSize.height - aInRegion.height) - aInRegion.y,
                      aInRegion.width, aInRegion.height);
    gfx::Rect destRect(0, 0, aOutSize.width, aOutSize.height);
    drawTarget->DrawSurface(image, destRect, srcRect);

    RefPtr<SourceSurface> snapshot = drawTarget->Snapshot();
    RefPtr<DataSourceSurface> data = snapshot->GetDataSurface();
    DataSourceSurface::ScopedMap* smap =
        new DataSourceSurface::ScopedMap(data, DataSourceSurface::READ);
    return reinterpret_cast<int8_t*>(smap->GetData());
  }

  /**
   * Compositor methods
   */
 public:
  void AttachNPZC(jni::Object::Param aNPZC) {
    MOZ_ASSERT(NS_IsMainThread());
    if (!mWindow) {
      return;  // Already shut down.
    }

    MOZ_ASSERT(aNPZC);

    // We can have this situation if we get two GeckoViewSupport::Transfer()
    // called before the first AttachNPZC() gets here. Just detach the current
    // instance since that's what happens in GeckoViewSupport::Transfer() as
    // well.
    if (mWindow->mNPZCSupport) {
      mWindow->mNPZCSupport.Detach(mWindow->mNPZCSupport->GetJavaNPZC());
    }

    auto npzc = java::PanZoomController::NativeProvider::LocalRef(
        jni::GetGeckoThreadEnv(),
        java::PanZoomController::NativeProvider::Ref::From(aNPZC));
    mWindow->mNPZCSupport.Attach(npzc, mWindow, npzc);

    DispatchToUiThread(
        "LayerViewSupport::AttachNPZC",
        [npzc = java::PanZoomController::NativeProvider::GlobalRef(npzc)] {
          npzc->SetAttached(true);
        });
  }

  void OnBoundsChanged(int32_t aLeft, int32_t aTop, int32_t aWidth,
                       int32_t aHeight) {
    MOZ_ASSERT(NS_IsMainThread());
    if (!mWindow) {
      return;  // Already shut down.
    }

    mWindow->Resize(aLeft, aTop, aWidth, aHeight, /* repaint */ false);
  }

  void SetDynamicToolbarMaxHeight(int32_t aHeight) {
    MOZ_ASSERT(NS_IsMainThread());
    if (!mWindow) {
      return;  // Already shut down.
    }

    mWindow->UpdateDynamicToolbarMaxHeight(ScreenIntCoord(aHeight));
  }

  void SyncPauseCompositor() {
    MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

    if (RefPtr<UiCompositorControllerChild> child =
            GetUiCompositorControllerChild()) {
      mCompositorPaused = true;
      child->Pause();
    }
  }

  void SyncResumeCompositor() {
    MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

    if (RefPtr<UiCompositorControllerChild> child =
            GetUiCompositorControllerChild()) {
      mCompositorPaused = false;
      child->Resume();
    }
  }

  void SyncResumeResizeCompositor(
      const GeckoSession::Compositor::LocalRef& aObj, int32_t aX, int32_t aY,
      int32_t aWidth, int32_t aHeight, jni::Object::Param aSurface) {
    MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

    mSurface = aSurface;

    if (RefPtr<UiCompositorControllerChild> child =
            GetUiCompositorControllerChild()) {
      child->ResumeAndResize(aX, aY, aWidth, aHeight);
    }

    mCompositorPaused = false;

    class OnResumedEvent : public nsAppShell::Event {
      GeckoSession::Compositor::GlobalRef mCompositor;

     public:
      explicit OnResumedEvent(GeckoSession::Compositor::GlobalRef&& aCompositor)
          : mCompositor(std::move(aCompositor)) {}

      void Run() override {
        MOZ_ASSERT(NS_IsMainThread());

        JNIEnv* const env = jni::GetGeckoThreadEnv();
        LayerViewSupport* const lvs =
            GetNative(GeckoSession::Compositor::LocalRef(env, mCompositor));

        if (!lvs || !lvs->mWindow) {
          env->ExceptionClear();
          return;  // Already shut down.
        }

        // When we get here, the compositor has already been told to
        // resume. This means it's now safe for layer updates to occur.
        // Since we might have prevented one or more draw events from
        // occurring while the compositor was paused, we need to
        // schedule a draw event now.
        if (!lvs->mCompositorPaused) {
          lvs->mWindow->RedrawAll();
        }
      }
    };

    // Use priority queue for timing-sensitive event.
    nsAppShell::PostEvent(
        MakeUnique<LayerViewEvent>(MakeUnique<OnResumedEvent>(aObj)));
  }

  void SyncInvalidateAndScheduleComposite() {
    RefPtr<UiCompositorControllerChild> child =
        GetUiCompositorControllerChild();
    if (!child) {
      return;
    }

    if (AndroidBridge::IsJavaUiThread()) {
      child->InvalidateAndRender();
      return;
    }

    if (RefPtr<nsThread> uiThread = GetAndroidUiThread()) {
      uiThread->Dispatch(NewRunnableMethod<>(
                             "LayerViewSupport::InvalidateAndRender", child,
                             &UiCompositorControllerChild::InvalidateAndRender),
                         nsIThread::DISPATCH_NORMAL);
    }
  }

  void SetMaxToolbarHeight(int32_t aHeight) {
    MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

    if (RefPtr<UiCompositorControllerChild> child =
            GetUiCompositorControllerChild()) {
      child->SetMaxToolbarHeight(aHeight);
    }
  }

  void SetFixedBottomOffset(int32_t aOffset) {
    if (mWindow) {
      mWindow->UpdateDynamicToolbarOffset(ScreenIntCoord(aOffset));
    }

    if (RefPtr<nsThread> uiThread = GetAndroidUiThread()) {
      uiThread->Dispatch(NS_NewRunnableFunction(
          "LayerViewSupport::SetFixedBottomOffset", [this, offset = aOffset] {
            if (RefPtr<UiCompositorControllerChild> child =
                    GetUiCompositorControllerChild()) {
              child->SetFixedBottomOffset(offset);
            }
          }));
    }
  }

  void SetPinned(bool aPinned, int32_t aReason) {
    RefPtr<UiCompositorControllerChild> child =
        GetUiCompositorControllerChild();
    if (!child) {
      return;
    }

    if (AndroidBridge::IsJavaUiThread()) {
      child->SetPinned(aPinned, aReason);
      return;
    }

    if (RefPtr<nsThread> uiThread = GetAndroidUiThread()) {
      uiThread->Dispatch(
          NewRunnableMethod<bool, int32_t>(
              "LayerViewSupport::SetPinned", child,
              &UiCompositorControllerChild::SetPinned, aPinned, aReason),
          nsIThread::DISPATCH_NORMAL);
    }
  }

  void SendToolbarAnimatorMessage(int32_t aMessage) {
    RefPtr<UiCompositorControllerChild> child =
        GetUiCompositorControllerChild();
    if (!child) {
      return;
    }

    if (AndroidBridge::IsJavaUiThread()) {
      child->ToolbarAnimatorMessageFromUI(aMessage);
      return;
    }

    if (RefPtr<nsThread> uiThread = GetAndroidUiThread()) {
      uiThread->Dispatch(
          NewRunnableMethod<int32_t>(
              "LayerViewSupport::ToolbarAnimatorMessageFromUI", child,
              &UiCompositorControllerChild::ToolbarAnimatorMessageFromUI,
              aMessage),
          nsIThread::DISPATCH_NORMAL);
    }
  }

  void RecvToolbarAnimatorMessage(int32_t aMessage) {
    auto compositor = GeckoSession::Compositor::LocalRef(mCompositor);
    if (compositor) {
      compositor->RecvToolbarAnimatorMessage(aMessage);
    }
  }

  void SetDefaultClearColor(int32_t aColor) {
    MOZ_ASSERT(AndroidBridge::IsJavaUiThread());
    if (RefPtr<UiCompositorControllerChild> child =
            GetUiCompositorControllerChild()) {
      child->SetDefaultClearColor((uint32_t)aColor);
    }
  }

  void RequestScreenPixels(jni::Object::Param aResult, int32_t aXOffset,
                           int32_t aYOffset, int32_t aSrcWidth,
                           int32_t aSrcHeight, int32_t aOutWidth,
                           int32_t aOutHeight) {
    MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

    int size = 0;
    if (LockedWindowPtr window{mWindow}) {
      mCapturePixelsResults.push(CaptureRequest(
          java::GeckoResult::GlobalRef(java::GeckoResult::LocalRef(aResult)),
          ScreenRect(aXOffset, aYOffset, aSrcWidth, aSrcHeight),
          IntSize(aOutWidth, aOutHeight)));
      size = mCapturePixelsResults.size();
    }

    if (size == 1) {
      if (RefPtr<UiCompositorControllerChild> child =
              GetUiCompositorControllerChild()) {
        child->RequestScreenPixels();
      }
    }
  }

  void RecvScreenPixels(Shmem&& aMem, const ScreenIntSize& aSize) {
    MOZ_ASSERT(AndroidBridge::IsJavaUiThread());
    CaptureRequest aCaptureRequest;
    java::GeckoResult::LocalRef aResult = nullptr;
    if (LockedWindowPtr window{mWindow}) {
      aCaptureRequest = mCapturePixelsResults.front();
      aResult = java::GeckoResult::LocalRef(aCaptureRequest.mResult);
      if (aResult) {
        mCapturePixelsResults.pop();
      }
    }

    if (aResult) {
      auto pixels = mozilla::jni::ByteBuffer::New(
          FlipScreenPixels(aMem, aSize, aCaptureRequest.mSource,
                           aCaptureRequest.mOutputSize),
          aMem.Size<int8_t>());
      aResult->Complete(pixels);
    }

    // Pixels have been copied, so Dealloc Shmem
    if (RefPtr<UiCompositorControllerChild> child =
            GetUiCompositorControllerChild()) {
      child->DeallocPixelBuffer(aMem);

      if (LockedWindowPtr window{mWindow}) {
        if (!mCapturePixelsResults.empty()) {
          child->RequestScreenPixels();
        }
      }
    }
  }

  void EnableLayerUpdateNotifications(bool aEnable) {
    MOZ_ASSERT(AndroidBridge::IsJavaUiThread());
    if (RefPtr<UiCompositorControllerChild> child =
            GetUiCompositorControllerChild()) {
      child->EnableLayerUpdateNotifications(aEnable);
    }
  }

  void SendToolbarPixelsToCompositor(int32_t aWidth, int32_t aHeight,
                                     jni::IntArray::Param aPixels) {
    MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

    if (RefPtr<UiCompositorControllerChild> child =
            GetUiCompositorControllerChild()) {
      Shmem mem;
      child->AllocPixelBuffer(aPixels->Length() * sizeof(int32_t), &mem);
      aPixels->CopyTo(mem.get<int32_t>(), mem.Size<int32_t>());
      if (!child->ToolbarPixelsToCompositor(mem,
                                            ScreenIntSize(aWidth, aHeight))) {
        child->DeallocPixelBuffer(mem);
      }
    }
  }

  void OnSafeAreaInsetsChanged(int32_t aTop, int32_t aRight, int32_t aBottom,
                               int32_t aLeft) {
    MOZ_ASSERT(NS_IsMainThread());
    if (!mWindow) {
      return;  // Already shut down.
    }
    ScreenIntMargin safeAreaInsets(aTop, aRight, aBottom, aLeft);
    mWindow->UpdateSafeAreaInsets(safeAreaInsets);
  }
};

template <>
const char nsWindow::NativePtr<nsWindow::LayerViewSupport>::sName[] =
    "LayerViewSupport";

nsWindow::GeckoViewSupport::~GeckoViewSupport() {
  // Disassociate our GeckoEditable instance with our native object.
  if (window.mEditableSupport) {
    window.mEditableSupport.Detach(window.mEditableSupport->GetJavaEditable());
    window.mEditableParent = nullptr;
  }

  if (window.mNPZCSupport) {
    window.mNPZCSupport.Detach(window.mNPZCSupport->GetJavaNPZC());
  }

  if (window.mLayerViewSupport) {
    window.mLayerViewSupport.Detach(
        window.mLayerViewSupport->GetJavaCompositor());
  }

  if (window.mSessionAccessibility) {
    window.mSessionAccessibility.Detach(
        window.mSessionAccessibility->GetJavaAccessibility());
  }
}

/* static */
void nsWindow::GeckoViewSupport::Open(
    const jni::Class::LocalRef& aCls, GeckoSession::Window::Param aWindow,
    jni::Object::Param aQueue, jni::Object::Param aCompositor,
    jni::Object::Param aDispatcher, jni::Object::Param aSessionAccessibility,
    jni::Object::Param aInitData, jni::String::Param aId,
    jni::String::Param aChromeURI, int32_t aScreenId, bool aPrivateMode,
    bool aRemote) {
  MOZ_ASSERT(NS_IsMainThread());

  AUTO_PROFILER_LABEL("nsWindow::GeckoViewSupport::Open", OTHER);

  nsCOMPtr<nsIWindowWatcher> ww = do_GetService(NS_WINDOWWATCHER_CONTRACTID);
  MOZ_RELEASE_ASSERT(ww);

  nsAutoCString url;
  if (aChromeURI) {
    url = aChromeURI->ToCString();
  } else {
    nsresult rv = Preferences::GetCString("toolkit.defaultChromeURI", url);
    if (NS_FAILED(rv)) {
      url = NS_LITERAL_CSTRING("chrome://geckoview/content/geckoview.xhtml");
    }
  }

  // Prepare an nsIAndroidView to pass as argument to the window.
  RefPtr<AndroidView> androidView = new AndroidView();
  androidView->mEventDispatcher->Attach(
      java::EventDispatcher::Ref::From(aDispatcher), nullptr);
  androidView->mInitData = java::GeckoBundle::Ref::From(aInitData);

  nsAutoCString chromeFlags("chrome,dialog=0,resizable,scrollbars");
  if (aPrivateMode) {
    chromeFlags += ",private";
  }
  if (aRemote) {
    chromeFlags += ",remote";
  }
  nsCOMPtr<mozIDOMWindowProxy> domWindow;
  ww->OpenWindow(nullptr, url.get(), aId->ToCString().get(), chromeFlags.get(),
                 androidView, getter_AddRefs(domWindow));
  MOZ_RELEASE_ASSERT(domWindow);

  nsCOMPtr<nsPIDOMWindowOuter> pdomWindow = nsPIDOMWindowOuter::From(domWindow);
  const RefPtr<nsWindow> window = nsWindow::From(pdomWindow);
  MOZ_ASSERT(window);
  window->SetScreenId(aScreenId);

  // Attach a new GeckoView support object to the new window.
  GeckoSession::Window::LocalRef sessionWindow(aCls.Env(), aWindow);
  window->mGeckoViewSupport =
      mozilla::MakeUnique<GeckoViewSupport>(window, sessionWindow);
  window->mGeckoViewSupport->mDOMWindow = pdomWindow;
  window->mAndroidView = androidView;

  // Attach other session support objects.
  window->mGeckoViewSupport->Transfer(sessionWindow, aQueue, aCompositor,
                                      aDispatcher, aSessionAccessibility,
                                      aInitData);

  if (window->mWidgetListener) {
    nsCOMPtr<nsIAppWindow> appWindow(window->mWidgetListener->GetAppWindow());
    if (appWindow) {
      // Our window is not intrinsically sized, so tell AppWindow to
      // not set a size for us.
      appWindow->SetIntrinsicallySized(false);
    }
  }
}

void nsWindow::GeckoViewSupport::Close() {
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

void nsWindow::GeckoViewSupport::Transfer(
    const GeckoSession::Window::LocalRef& inst, jni::Object::Param aQueue,
    jni::Object::Param aCompositor, jni::Object::Param aDispatcher,
    jni::Object::Param aSessionAccessibility, jni::Object::Param aInitData) {
  if (window.mNPZCSupport) {
    MOZ_ASSERT(window.mLayerViewSupport);
    window.mNPZCSupport.Detach(window.mNPZCSupport->GetJavaNPZC());
  }

  auto compositor = GeckoSession::Compositor::LocalRef(
      inst.Env(), GeckoSession::Compositor::Ref::From(aCompositor));
  if (window.mLayerViewSupport &&
      window.mLayerViewSupport->GetJavaCompositor() != compositor) {
    window.mLayerViewSupport.Detach(
        window.mLayerViewSupport->GetJavaCompositor());
  }
  if (!window.mLayerViewSupport) {
    window.mLayerViewSupport.Attach(compositor, &window, compositor);
  }

  MOZ_ASSERT(window.mAndroidView);
  window.mAndroidView->mEventDispatcher->Attach(
      java::EventDispatcher::Ref::From(aDispatcher), mDOMWindow);

  if (window.mSessionAccessibility) {
    window.mSessionAccessibility.Detach(
        window.mSessionAccessibility->GetJavaAccessibility());
  }
  if (aSessionAccessibility) {
    AttachAccessibility(inst, aSessionAccessibility);
  }

  if (mIsReady) {
    // We're in a transfer; update init-data and notify JS code.
    window.mAndroidView->mInitData = java::GeckoBundle::Ref::From(aInitData);
    OnReady(aQueue);
    window.mAndroidView->mEventDispatcher->Dispatch(
        u"GeckoView:UpdateInitData");
  }

  DispatchToUiThread("GeckoViewSupport::Transfer",
                     [compositor = GeckoSession::Compositor::GlobalRef(
                          compositor)] { compositor->OnCompositorAttached(); });
}

void nsWindow::GeckoViewSupport::AttachEditable(
    const GeckoSession::Window::LocalRef& inst,
    jni::Object::Param aEditableParent) {
  if (!window.mEditableSupport) {
    auto editableChild = java::GeckoEditableChild::New(aEditableParent,
                                                       /* default */ true);
    window.mEditableSupport.Attach(editableChild, &window, editableChild);
  } else {
    window.mEditableSupport->TransferParent(aEditableParent);
  }

  window.mEditableParent = aEditableParent;
}

void nsWindow::GeckoViewSupport::AttachAccessibility(
    const GeckoSession::Window::LocalRef& inst,
    jni::Object::Param aSessionAccessibility) {
  MOZ_ASSERT(!window.mSessionAccessibility);
  java::SessionAccessibility::NativeProvider::LocalRef sessionAccessibility(
      inst.Env());
  sessionAccessibility = java::SessionAccessibility::NativeProvider::Ref::From(
      aSessionAccessibility);

  if (window.mSessionAccessibility) {
    window.mSessionAccessibility.Detach(
        window.mSessionAccessibility->GetJavaAccessibility());
  }

  window.mSessionAccessibility.Attach(sessionAccessibility, &window,
                                      sessionAccessibility);
}

void nsWindow::InitNatives() {
  jni::InitConversionStatics();
  nsWindow::GeckoViewSupport::Base::Init();
  nsWindow::LayerViewSupport::Init();
  nsWindow::NPZCSupport::Init();

  GeckoEditableSupport::Init();
  a11y::SessionAccessibility::Init();
}

/* static */
already_AddRefed<nsWindow> nsWindow::From(nsPIDOMWindowOuter* aDOMWindow) {
  nsCOMPtr<nsIWidget> widget = WidgetUtils::DOMWindowToWidget(aDOMWindow);
  return From(widget);
}

/* static */
already_AddRefed<nsWindow> nsWindow::From(nsIWidget* aWidget) {
  // `widget` may be one of several different types in the parent
  // process, including the Android nsWindow, PuppetWidget, etc. To
  // ensure that the cast to the Android nsWindow is valid, we check that the
  // widget is a top-level window and that its NS_NATIVE_WIDGET value is
  // non-null, which is not the case for non-native widgets like
  // PuppetWidget.
  if (aWidget && aWidget->WindowType() == nsWindowType::eWindowType_toplevel &&
      aWidget->GetNativeData(NS_NATIVE_WIDGET) == aWidget) {
    RefPtr<nsWindow> window = static_cast<nsWindow*>(aWidget);
    return window.forget();
  }
  return nullptr;
}

nsWindow* nsWindow::TopWindow() {
  if (!gTopLevelWindows.IsEmpty()) return gTopLevelWindows[0];
  return nullptr;
}

void nsWindow::LogWindow(nsWindow* win, int index, int indent) {
#if defined(DEBUG) || defined(FORCE_ALOG)
  char spaces[] = "                    ";
  spaces[indent < 20 ? indent : 20] = 0;
  ALOG("%s [% 2d] 0x%p [parent 0x%p] [% 3d,% 3dx% 3d,% 3d] vis %d type %d",
       spaces, index, win, win->mParent, win->mBounds.x, win->mBounds.y,
       win->mBounds.width, win->mBounds.height, win->mIsVisible,
       win->mWindowType);
#endif
}

void nsWindow::DumpWindows() { DumpWindows(gTopLevelWindows); }

void nsWindow::DumpWindows(const nsTArray<nsWindow*>& wins, int indent) {
  for (uint32_t i = 0; i < wins.Length(); ++i) {
    nsWindow* w = wins[i];
    LogWindow(w, i, indent);
    DumpWindows(w->mChildren, indent + 1);
  }
}

nsWindow::nsWindow()
    : mScreenId(0),  // Use 0 (primary screen) as the default value.
      mIsVisible(false),
      mParent(nullptr),
      mDynamicToolbarMaxHeight(0),
      mIsFullScreen(false),
      mIsDisablingWebRender(false) {}

nsWindow::~nsWindow() {
  gTopLevelWindows.RemoveElement(this);
  ALOG("nsWindow %p destructor", (void*)this);
  // The mCompositorSession should have been cleaned up in nsWindow::Destroy()
  // DestroyLayerManager() will call DestroyCompositor() which will crash if
  // called from nsBaseWidget destructor. See Bug 1392705
  MOZ_ASSERT(!mCompositorSession);
}

bool nsWindow::IsTopLevel() {
  return mWindowType == eWindowType_toplevel ||
         mWindowType == eWindowType_dialog ||
         mWindowType == eWindowType_invisible;
}

nsresult nsWindow::Create(nsIWidget* aParent, nsNativeWidget aNativeParent,
                          const LayoutDeviceIntRect& aRect,
                          nsWidgetInitData* aInitData) {
  ALOG("nsWindow[%p]::Create %p [%d %d %d %d]", (void*)this, (void*)aParent,
       aRect.x, aRect.y, aRect.width, aRect.height);

  nsWindow* parent = (nsWindow*)aParent;
  if (aNativeParent) {
    if (parent) {
      ALOG(
          "Ignoring native parent on Android window [%p], "
          "since parent was specified (%p %p)",
          (void*)this, (void*)aNativeParent, (void*)aParent);
    } else {
      parent = (nsWindow*)aNativeParent;
    }
  }

  // A default size of 1x1 confuses MobileViewportManager, so
  // use 0x0 instead. This is also a little more fitting since
  // we don't yet have a surface yet (and therefore a valid size)
  // and 0x0 is usually recognized as invalid.
  LayoutDeviceIntRect rect = aRect;
  if (aRect.width == 1 && aRect.height == 1) {
    rect.width = 0;
    rect.height = 0;
  }

  mBounds = rect;

  BaseCreate(nullptr, aInitData);

  NS_ASSERTION(IsTopLevel() || parent,
               "non-top-level window doesn't have a parent!");

  if (IsTopLevel()) {
    gTopLevelWindows.AppendElement(this);

  } else if (parent) {
    parent->mChildren.AppendElement(this);
    mParent = parent;
  }

  CreateLayerManager();

#ifdef DEBUG_ANDROID_WIDGET
  DumpWindows();
#endif

  return NS_OK;
}

void nsWindow::Destroy() {
  nsBaseWidget::mOnDestroyCalled = true;

  if (mGeckoViewSupport) {
    // Disassociate our native object with GeckoView.
    mGeckoViewSupport = nullptr;
  }

  // Stuff below may release the last ref to this
  nsCOMPtr<nsIWidget> kungFuDeathGrip(this);

  while (mChildren.Length()) {
    // why do we still have children?
    ALOG("### Warning: Destroying window %p and reparenting child %p to null!",
         (void*)this, (void*)mChildren[0]);
    mChildren[0]->SetParent(nullptr);
  }

  // Ensure the compositor has been shutdown before this nsWindow is potentially
  // deleted
  nsBaseWidget::DestroyCompositor();

  nsBaseWidget::Destroy();

  if (IsTopLevel()) gTopLevelWindows.RemoveElement(this);

  SetParent(nullptr);

  nsBaseWidget::OnDestroy();

#ifdef DEBUG_ANDROID_WIDGET
  DumpWindows();
#endif
}

nsresult nsWindow::ConfigureChildren(
    const nsTArray<nsIWidget::Configuration>& config) {
  for (uint32_t i = 0; i < config.Length(); ++i) {
    nsWindow* childWin = (nsWindow*)config[i].mChild.get();
    childWin->Resize(config[i].mBounds.x, config[i].mBounds.y,
                     config[i].mBounds.width, config[i].mBounds.height, false);
  }

  return NS_OK;
}

void nsWindow::RedrawAll() {
  if (mAttachedWidgetListener) {
    mAttachedWidgetListener->RequestRepaint();
  } else if (mWidgetListener) {
    mWidgetListener->RequestRepaint();
  }
}

RefPtr<UiCompositorControllerChild> nsWindow::GetUiCompositorControllerChild() {
  return mCompositorSession
             ? mCompositorSession->GetUiCompositorControllerChild()
             : nullptr;
}

mozilla::layers::LayersId nsWindow::GetRootLayerId() const {
  return mCompositorSession ? mCompositorSession->RootLayerTreeId()
                            : mozilla::layers::LayersId{0};
}

void nsWindow::OnGeckoViewReady() {
  if (!mGeckoViewSupport) {
    return;
  }
  mGeckoViewSupport->OnReady();
}

void nsWindow::SetParent(nsIWidget* aNewParent) {
  if ((nsIWidget*)mParent == aNewParent) return;

  // If we had a parent before, remove ourselves from its list of
  // children.
  if (mParent) mParent->mChildren.RemoveElement(this);

  mParent = (nsWindow*)aNewParent;

  if (mParent) mParent->mChildren.AppendElement(this);

  // if we are now in the toplevel window's hierarchy, schedule a redraw
  if (FindTopLevel() == nsWindow::TopWindow()) RedrawAll();
}

nsIWidget* nsWindow::GetParent() { return mParent; }

float nsWindow::GetDPI() {
  float dpi = 160.0f;

  nsCOMPtr<nsIScreen> screen = GetWidgetScreen();
  if (screen) {
    screen->GetDpi(&dpi);
  }

  return dpi;
}

double nsWindow::GetDefaultScaleInternal() {
  double scale = 1.0f;

  nsCOMPtr<nsIScreen> screen = GetWidgetScreen();
  if (screen) {
    screen->GetContentsScaleFactor(&scale);
  }

  return scale;
}

void nsWindow::Show(bool aState) {
  ALOG("nsWindow[%p]::Show %d", (void*)this, aState);

  if (mWindowType == eWindowType_invisible) {
    ALOG("trying to show invisible window! ignoring..");
    return;
  }

  if (aState == mIsVisible) return;

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
        nsWindow* win = gTopLevelWindows[i];
        if (!win->mIsVisible) continue;

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

bool nsWindow::IsVisible() const { return mIsVisible; }

void nsWindow::ConstrainPosition(bool aAllowSlop, int32_t* aX, int32_t* aY) {
  ALOG("nsWindow[%p]::ConstrainPosition %d [%d %d]", (void*)this, aAllowSlop,
       *aX, *aY);

  // constrain toplevel windows; children we don't care about
  if (IsTopLevel()) {
    *aX = 0;
    *aY = 0;
  }
}

void nsWindow::Move(double aX, double aY) {
  if (IsTopLevel()) return;

  Resize(aX, aY, mBounds.width, mBounds.height, true);
}

void nsWindow::Resize(double aWidth, double aHeight, bool aRepaint) {
  Resize(mBounds.x, mBounds.y, aWidth, aHeight, aRepaint);
}

void nsWindow::Resize(double aX, double aY, double aWidth, double aHeight,
                      bool aRepaint) {
  ALOG("nsWindow[%p]::Resize [%f %f %f %f] (repaint %d)", (void*)this, aX, aY,
       aWidth, aHeight, aRepaint);

  bool needPositionDispatch = aX != mBounds.x || aY != mBounds.y;
  bool needSizeDispatch = aWidth != mBounds.width || aHeight != mBounds.height;

  mBounds.x = NSToIntRound(aX);
  mBounds.y = NSToIntRound(aY);
  mBounds.width = NSToIntRound(aWidth);
  mBounds.height = NSToIntRound(aHeight);

  if (needSizeDispatch) {
    OnSizeChanged(gfx::IntSize::Truncate(aWidth, aHeight));
  }

  if (needPositionDispatch) {
    NotifyWindowMoved(mBounds.x, mBounds.y);
  }

  // Should we skip honoring aRepaint here?
  if (aRepaint && FindTopLevel() == nsWindow::TopWindow()) RedrawAll();
}

void nsWindow::SetZIndex(int32_t aZIndex) {
  ALOG("nsWindow[%p]::SetZIndex %d ignored", (void*)this, aZIndex);
}

void nsWindow::SetSizeMode(nsSizeMode aMode) {
  if (aMode == mSizeMode) {
    return;
  }

  nsBaseWidget::SetSizeMode(aMode);

  switch (aMode) {
    case nsSizeMode_Minimized:
      java::GeckoAppShell::MoveTaskToBack();
      break;
    case nsSizeMode_Fullscreen:
      MakeFullScreen(true);
      break;
    default:
      break;
  }
}

void nsWindow::Enable(bool aState) {
  ALOG("nsWindow[%p]::Enable %d ignored", (void*)this, aState);
}

bool nsWindow::IsEnabled() const { return true; }

void nsWindow::Invalidate(const LayoutDeviceIntRect& aRect) {}

nsWindow* nsWindow::FindTopLevel() {
  nsWindow* toplevel = this;
  while (toplevel) {
    if (toplevel->IsTopLevel()) return toplevel;

    toplevel = toplevel->mParent;
  }

  ALOG(
      "nsWindow::FindTopLevel(): couldn't find a toplevel or dialog window in "
      "this [%p] widget's hierarchy!",
      (void*)this);
  return this;
}

void nsWindow::SetFocus(Raise, mozilla::dom::CallerType aCallerType) {
  FindTopLevel()->BringToFront();
}

void nsWindow::BringToFront() {
  MOZ_ASSERT(XRE_IsParentProcess());
  // If the window to be raised is the same as the currently raised one,
  // do nothing. We need to check the focus manager as well, as the first
  // window that is created will be first in the window list but won't yet
  // be focused.
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm && fm->GetActiveWindow() && FindTopLevel() == nsWindow::TopWindow()) {
    return;
  }

  if (!IsTopLevel()) {
    FindTopLevel()->BringToFront();
    return;
  }

  RefPtr<nsWindow> kungFuDeathGrip(this);

  nsWindow* oldTop = nullptr;
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

LayoutDeviceIntRect nsWindow::GetScreenBounds() {
  return LayoutDeviceIntRect(WidgetToScreenOffset(), mBounds.Size());
}

LayoutDeviceIntPoint nsWindow::WidgetToScreenOffset() {
  LayoutDeviceIntPoint p(0, 0);

  for (nsWindow* w = this; !!w; w = w->mParent) {
    p.x += w->mBounds.x;
    p.y += w->mBounds.y;

    if (w->IsTopLevel()) {
      break;
    }
  }
  return p;
}

nsresult nsWindow::DispatchEvent(WidgetGUIEvent* aEvent,
                                 nsEventStatus& aStatus) {
  aStatus = DispatchEvent(aEvent);
  return NS_OK;
}

nsEventStatus nsWindow::DispatchEvent(WidgetGUIEvent* aEvent) {
  if (mAttachedWidgetListener) {
    return mAttachedWidgetListener->HandleEvent(aEvent, mUseAttachedEvents);
  } else if (mWidgetListener) {
    return mWidgetListener->HandleEvent(aEvent, mUseAttachedEvents);
  }
  return nsEventStatus_eIgnore;
}

nsresult nsWindow::MakeFullScreen(bool aFullScreen, nsIScreen*) {
  if (!mAndroidView) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mIsFullScreen = aFullScreen;
  mAndroidView->mEventDispatcher->Dispatch(
      aFullScreen ? u"GeckoView:FullScreenEnter" : u"GeckoView:FullScreenExit");

  nsIWidgetListener* listener = GetWidgetListener();
  if (listener) {
    mSizeMode = mIsFullScreen ? nsSizeMode_Fullscreen : nsSizeMode_Normal;
    listener->SizeModeChanged(mSizeMode);
    listener->FullscreenChanged(mIsFullScreen);
  }
  return NS_OK;
}

mozilla::layers::LayerManager* nsWindow::GetLayerManager(
    PLayerTransactionChild*, LayersBackend, LayerManagerPersistence) {
  if (mLayerManager) {
    return mLayerManager;
  }

  if (mIsDisablingWebRender) {
    CreateLayerManager();
    mIsDisablingWebRender = false;
    return mLayerManager;
  }

  return nullptr;
}

void nsWindow::CreateLayerManager() {
  if (mLayerManager) {
    return;
  }

  nsWindow* topLevelWindow = FindTopLevel();
  if (!topLevelWindow || topLevelWindow->mWindowType == eWindowType_invisible) {
    // don't create a layer manager for an invisible top-level window
    return;
  }

  // Ensure that gfxPlatform is initialized first.
  gfxPlatform::GetPlatform();

  if (ShouldUseOffMainThreadCompositing()) {
    LayoutDeviceIntRect rect = GetBounds();
    CreateCompositor(rect.Width(), rect.Height());
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

void nsWindow::NotifyDisablingWebRender() {
  mIsDisablingWebRender = true;
  RedrawAll();
}

void nsWindow::OnSizeChanged(const gfx::IntSize& aSize) {
  ALOG("nsWindow: %p OnSizeChanged [%d %d]", (void*)this, aSize.width,
       aSize.height);

  mBounds.width = aSize.width;
  mBounds.height = aSize.height;

  if (mWidgetListener) {
    mWidgetListener->WindowResized(this, aSize.width, aSize.height);
  }

  if (mAttachedWidgetListener) {
    mAttachedWidgetListener->WindowResized(this, aSize.width, aSize.height);
  }
}

void nsWindow::InitEvent(WidgetGUIEvent& event, LayoutDeviceIntPoint* aPoint) {
  if (aPoint) {
    event.mRefPoint = *aPoint;
  } else {
    event.mRefPoint = LayoutDeviceIntPoint(0, 0);
  }

  event.mTime = PR_Now() / 1000;
}

void nsWindow::UpdateOverscrollVelocity(const float aX, const float aY) {
  if (NativePtr<LayerViewSupport>::Locked lvs{mLayerViewSupport}) {
    const auto& compositor = lvs->GetJavaCompositor();
    if (AndroidBridge::IsJavaUiThread()) {
      compositor->UpdateOverscrollVelocity(aX, aY);
      return;
    }

    DispatchToUiThread(
        "nsWindow::UpdateOverscrollVelocity",
        [compositor = GeckoSession::Compositor::GlobalRef(compositor), aX, aY] {
          compositor->UpdateOverscrollVelocity(aX, aY);
        });
  }
}

void nsWindow::UpdateOverscrollOffset(const float aX, const float aY) {
  if (NativePtr<LayerViewSupport>::Locked lvs{mLayerViewSupport}) {
    const auto& compositor = lvs->GetJavaCompositor();
    if (AndroidBridge::IsJavaUiThread()) {
      compositor->UpdateOverscrollOffset(aX, aY);
      return;
    }

    DispatchToUiThread(
        "nsWindow::UpdateOverscrollOffset",
        [compositor = GeckoSession::Compositor::GlobalRef(compositor), aX, aY] {
          compositor->UpdateOverscrollOffset(aX, aY);
        });
  }
}

void* nsWindow::GetNativeData(uint32_t aDataType) {
  switch (aDataType) {
    // used by GLContextProviderEGL, nullptr is EGL_DEFAULT_DISPLAY
    case NS_NATIVE_DISPLAY:
      return nullptr;

    case NS_NATIVE_WIDGET:
      return (void*)this;

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
  }

  return nullptr;
}

void nsWindow::SetNativeData(uint32_t aDataType, uintptr_t aVal) {
  switch (aDataType) {}
}

void nsWindow::DispatchHitTest(const WidgetTouchEvent& aEvent) {
  if (aEvent.mMessage == eTouchStart && aEvent.mTouches.Length() == 1) {
    // Since touch events don't get retargeted by PositionedEventTargeting.cpp
    // code, we dispatch a dummy mouse event that *does* get retargeted.
    // Front-end code can use this to activate the highlight element in case
    // this touchstart is the start of a tap.
    WidgetMouseEvent hittest(true, eMouseHitTest, this,
                             WidgetMouseEvent::eReal);
    hittest.mRefPoint = aEvent.mTouches[0]->mRefPoint;
    hittest.mIgnoreRootScrollFrame = true;
    hittest.mInputSource = MouseEvent_Binding::MOZ_SOURCE_TOUCH;
    nsEventStatus status;
    DispatchEvent(&hittest, status);
  }
}

mozilla::Modifiers nsWindow::GetModifiers(int32_t metaState) {
  using mozilla::java::sdk::KeyEvent;
  return (metaState & KeyEvent::META_ALT_MASK ? MODIFIER_ALT : 0) |
         (metaState & KeyEvent::META_SHIFT_MASK ? MODIFIER_SHIFT : 0) |
         (metaState & KeyEvent::META_CTRL_MASK ? MODIFIER_CONTROL : 0) |
         (metaState & KeyEvent::META_META_MASK ? MODIFIER_META : 0) |
         (metaState & KeyEvent::META_FUNCTION_ON ? MODIFIER_FN : 0) |
         (metaState & KeyEvent::META_CAPS_LOCK_ON ? MODIFIER_CAPSLOCK : 0) |
         (metaState & KeyEvent::META_NUM_LOCK_ON ? MODIFIER_NUMLOCK : 0) |
         (metaState & KeyEvent::META_SCROLL_LOCK_ON ? MODIFIER_SCROLLLOCK : 0);
}

TimeStamp nsWindow::GetEventTimeStamp(int64_t aEventTime) {
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

void nsWindow::GeckoViewSupport::OnReady(jni::Object::Param aQueue) {
  GeckoSession::Window::LocalRef window(mGeckoViewWindow);
  if (!window) {
    return;
  }
  window->OnReady(aQueue);
  mIsReady = true;
}

void nsWindow::UserActivity() {
  if (!mIdleService) {
    mIdleService = do_GetService("@mozilla.org/widget/idleservice;1");
  }

  if (mIdleService) {
    mIdleService->ResetIdleTimeOut(0);
  }

  if (FindTopLevel() != nsWindow::TopWindow()) {
    BringToFront();
  }
}

TextEventDispatcherListener* nsWindow::GetNativeTextEventDispatcherListener() {
  nsWindow* top = FindTopLevel();
  MOZ_ASSERT(top);

  if (!top->mEditableSupport) {
    // Non-GeckoView windows don't support IME operations.
    return nullptr;
  }
  return top->mEditableSupport;
}

void nsWindow::SetInputContext(const InputContext& aContext,
                               const InputContextAction& aAction) {
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

InputContext nsWindow::GetInputContext() {
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

nsresult nsWindow::SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                              TouchPointerState aPointerState,
                                              LayoutDeviceIntPoint aPoint,
                                              double aPointerPressure,
                                              uint32_t aPointerOrientation,
                                              nsIObserver* aObserver) {
  mozilla::widget::AutoObserverNotifier notifier(aObserver, "touchpoint");

  int eventType;
  switch (aPointerState) {
    case TOUCH_CONTACT:
      // This could be a ACTION_DOWN or ACTION_MOVE depending on the
      // existing state; it is mapped to the right thing in Java.
      eventType = java::sdk::MotionEvent::ACTION_POINTER_DOWN;
      break;
    case TOUCH_REMOVE:
      // This could be turned into a ACTION_UP in Java
      eventType = java::sdk::MotionEvent::ACTION_POINTER_UP;
      break;
    case TOUCH_CANCEL:
      eventType = java::sdk::MotionEvent::ACTION_CANCEL;
      break;
    case TOUCH_HOVER:  // not supported for now
    default:
      return NS_ERROR_UNEXPECTED;
  }

  MOZ_ASSERT(mNPZCSupport);
  const auto& npzc = mNPZCSupport->GetJavaNPZC();
  const auto& bounds = FindTopLevel()->mBounds;
  aPoint.x -= bounds.x;
  aPoint.y -= bounds.y;

  DispatchToUiThread(
      "nsWindow::SynthesizeNativeTouchPoint",
      [npzc = java::PanZoomController::NativeProvider::GlobalRef(npzc),
       aPointerId, eventType, aPoint, aPointerPressure, aPointerOrientation] {
        npzc->SynthesizeNativeTouchPoint(aPointerId, eventType, aPoint.x,
                                         aPoint.y, aPointerPressure,
                                         aPointerOrientation);
      });
  return NS_OK;
}

nsresult nsWindow::SynthesizeNativeMouseEvent(LayoutDeviceIntPoint aPoint,
                                              uint32_t aNativeMessage,
                                              uint32_t aModifierFlags,
                                              nsIObserver* aObserver) {
  mozilla::widget::AutoObserverNotifier notifier(aObserver, "mouseevent");

  MOZ_ASSERT(mNPZCSupport);
  const auto& npzc = mNPZCSupport->GetJavaNPZC();
  const auto& bounds = FindTopLevel()->mBounds;
  aPoint.x -= bounds.x;
  aPoint.y -= bounds.y;

  DispatchToUiThread(
      "nsWindow::SynthesizeNativeMouseEvent",
      [npzc = java::PanZoomController::NativeProvider::GlobalRef(npzc),
       aNativeMessage, aPoint] {
        npzc->SynthesizeNativeMouseEvent(aNativeMessage, aPoint.x, aPoint.y);
      });
  return NS_OK;
}

nsresult nsWindow::SynthesizeNativeMouseMove(LayoutDeviceIntPoint aPoint,
                                             nsIObserver* aObserver) {
  mozilla::widget::AutoObserverNotifier notifier(aObserver, "mouseevent");

  MOZ_ASSERT(mNPZCSupport);
  const auto& npzc = mNPZCSupport->GetJavaNPZC();
  const auto& bounds = FindTopLevel()->mBounds;
  aPoint.x -= bounds.x;
  aPoint.y -= bounds.y;

  DispatchToUiThread(
      "nsWindow::SynthesizeNativeMouseMove",
      [npzc = java::PanZoomController::NativeProvider::GlobalRef(npzc),
       aPoint] {
        npzc->SynthesizeNativeMouseEvent(
            java::sdk::MotionEvent::ACTION_HOVER_MOVE, aPoint.x, aPoint.y);
      });
  return NS_OK;
}

bool nsWindow::WidgetPaintsBackground() {
  return StaticPrefs::android_widget_paints_background();
}

bool nsWindow::NeedsPaint() {
  if (!mLayerViewSupport || mLayerViewSupport->CompositorPaused() ||
      // FindTopLevel() != nsWindow::TopWindow() ||
      !GetLayerManager(nullptr)) {
    return false;
  }
  return nsIWidget::NeedsPaint();
}

void nsWindow::ConfigureAPZControllerThread() {
  APZThreadUtils::SetControllerThread(mozilla::GetAndroidUiThreadMessageLoop());
}

already_AddRefed<GeckoContentController>
nsWindow::CreateRootContentController() {
  RefPtr<GeckoContentController> controller =
      new AndroidContentController(this, mAPZEventState, mAPZC);
  return controller.forget();
}

uint32_t nsWindow::GetMaxTouchPoints() const {
  return java::GeckoAppShell::GetMaxTouchPoints();
}

void nsWindow::UpdateZoomConstraints(
    const uint32_t& aPresShellId, const ScrollableLayerGuid::ViewID& aViewId,
    const mozilla::Maybe<ZoomConstraints>& aConstraints) {
  nsBaseWidget::UpdateZoomConstraints(aPresShellId, aViewId, aConstraints);
}

CompositorBridgeChild* nsWindow::GetCompositorBridgeChild() const {
  return mCompositorSession ? mCompositorSession->GetCompositorBridgeChild()
                            : nullptr;
}

already_AddRefed<nsIScreen> nsWindow::GetWidgetScreen() {
  RefPtr<nsIScreen> screen =
      ScreenHelperAndroid::GetSingleton()->ScreenForId(mScreenId);
  return screen.forget();
}

void nsWindow::SetContentDocumentDisplayed(bool aDisplayed) {
  mContentDocumentDisplayed = aDisplayed;
}

bool nsWindow::IsContentDocumentDisplayed() {
  return mContentDocumentDisplayed;
}

void nsWindow::RecvToolbarAnimatorMessageFromCompositor(int32_t aMessage) {
  MOZ_ASSERT(AndroidBridge::IsJavaUiThread());
  if (NativePtr<LayerViewSupport>::Locked lvs{mLayerViewSupport}) {
    lvs->RecvToolbarAnimatorMessage(aMessage);
  }
}

void nsWindow::UpdateRootFrameMetrics(const ScreenPoint& aScrollOffset,
                                      const CSSToScreenScale& aZoom) {
  MOZ_ASSERT(AndroidBridge::IsJavaUiThread());
  if (NativePtr<LayerViewSupport>::Locked lvs{mLayerViewSupport}) {
    const auto& compositor = lvs->GetJavaCompositor();
    mContentDocumentDisplayed = true;
    compositor->UpdateRootFrameMetrics(aScrollOffset.x, aScrollOffset.y,
                                       aZoom.scale);
  }
}

void nsWindow::RecvScreenPixels(Shmem&& aMem, const ScreenIntSize& aSize) {
  MOZ_ASSERT(AndroidBridge::IsJavaUiThread());
  if (NativePtr<LayerViewSupport>::Locked lvs{mLayerViewSupport}) {
    lvs->RecvScreenPixels(std::move(aMem), aSize);
  }
}

void nsWindow::UpdateDynamicToolbarMaxHeight(ScreenIntCoord aHeight) {
  if (mDynamicToolbarMaxHeight == aHeight) {
    return;
  }

  mDynamicToolbarMaxHeight = aHeight;

  if (mWidgetListener) {
    mWidgetListener->DynamicToolbarMaxHeightChanged(aHeight);
  }

  if (mAttachedWidgetListener) {
    mAttachedWidgetListener->DynamicToolbarMaxHeightChanged(aHeight);
  }
}

void nsWindow::UpdateDynamicToolbarOffset(ScreenIntCoord aOffset) {
  if (mWidgetListener) {
    mWidgetListener->DynamicToolbarOffsetChanged(aOffset);
  }

  if (mAttachedWidgetListener) {
    mAttachedWidgetListener->DynamicToolbarOffsetChanged(aOffset);
  }
}

nsresult nsWindow::SetPrefersReducedMotionOverrideForTest(bool aValue) {
  LookAndFeel::SetPrefersReducedMotionOverrideForTest(aValue);

  // Notify as if the corresponding setting changed.
  java::GeckoSystemStateListener::NotifyPrefersReducedMotionChangedForTest();
  return NS_OK;
}

nsresult nsWindow::ResetPrefersReducedMotionOverrideForTest() {
  LookAndFeel::ResetPrefersReducedMotionOverrideForTest();
  return NS_OK;
}

ScreenIntMargin nsWindow::GetSafeAreaInsets() const { return mSafeAreaInsets; }

void nsWindow::UpdateSafeAreaInsets(const ScreenIntMargin& aSafeAreaInsets) {
  mSafeAreaInsets = aSafeAreaInsets;

  if (mWidgetListener) {
    mWidgetListener->SafeAreaInsetsChanged(aSafeAreaInsets);
  }

  if (mAttachedWidgetListener) {
    mAttachedWidgetListener->SafeAreaInsetsChanged(aSafeAreaInsets);
  }
}

already_AddRefed<nsIWidget> nsIWidget::CreateTopLevelWindow() {
  nsCOMPtr<nsIWidget> window = new nsWindow();
  return window.forget();
}

already_AddRefed<nsIWidget> nsIWidget::CreateChildWindow() {
  nsCOMPtr<nsIWidget> window = new nsWindow();
  return window.forget();
}
