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
#include "mozilla/StaticPrefs_android.h"
#include "mozilla/StaticPrefs_ui.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/Unused.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/WheelHandlingHelper.h"  // for WheelDeltaAdjustmentStrategy

#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "mozilla/a11y/SessionAccessibility.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/RenderTrace.h"
#include "mozilla/widget/AndroidVsync.h"
#include <algorithm>

using mozilla::Unused;
using mozilla::dom::ContentChild;
using mozilla::dom::ContentParent;
using mozilla::gfx::DataSourceSurface;
using mozilla::gfx::IntSize;
using mozilla::gfx::Matrix;
using mozilla::gfx::SurfaceFormat;

#include "nsWindow.h"

#include "AndroidGraphics.h"
#include "JavaExceptions.h"

#include "nsIWidgetListener.h"
#include "nsIWindowWatcher.h"
#include "nsIAppWindow.h"

#include "nsAppShell.h"
#include "nsFocusManager.h"
#include "nsUserIdleService.h"
#include "nsLayoutUtils.h"
#include "nsNetUtil.h"
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
#include "AndroidView.h"
#include "GeckoEditableSupport.h"
#include "GeckoViewSupport.h"
#include "KeyEvent.h"
#include "MotionEvent.h"
#include "mozilla/java/EventDispatcherWrappers.h"
#include "mozilla/java/GeckoAppShellWrappers.h"
#include "mozilla/java/GeckoEditableChildWrappers.h"
#include "mozilla/java/GeckoResultWrappers.h"
#include "mozilla/java/GeckoSessionNatives.h"
#include "mozilla/java/GeckoSystemStateListenerWrappers.h"
#include "mozilla/java/PanZoomControllerNatives.h"
#include "mozilla/java/SessionAccessibilityWrappers.h"
#include "ScreenHelperAndroid.h"
#include "TouchResampler.h"

#include "mozilla/ProfilerLabels.h"
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

static const double kTouchResampleVsyncAdjustMs = 5.0;

static const int32_t INPUT_RESULT_UNHANDLED =
    java::PanZoomController::INPUT_RESULT_UNHANDLED;
static const int32_t INPUT_RESULT_HANDLED =
    java::PanZoomController::INPUT_RESULT_HANDLED;
static const int32_t INPUT_RESULT_HANDLED_CONTENT =
    java::PanZoomController::INPUT_RESULT_HANDLED_CONTENT;
static const int32_t INPUT_RESULT_IGNORED =
    java::PanZoomController::INPUT_RESULT_IGNORED;

static const nsCString::size_type MAX_TOPLEVEL_DATA_URI_LEN = 2 * 1024 * 1024;

namespace {
template <class Instance, class Impl>
std::enable_if_t<jni::detail::NativePtrPicker<Impl>::value ==
                     jni::detail::NativePtrType::REFPTR,
                 void>
CallAttachNative(Instance aInstance, Impl* aImpl) {
  Impl::AttachNative(aInstance, RefPtr<Impl>(aImpl).get());
}

template <class Instance, class Impl>
std::enable_if_t<jni::detail::NativePtrPicker<Impl>::value ==
                     jni::detail::NativePtrType::OWNING,
                 void>
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

namespace mozilla {
namespace widget {

using WindowPtr = jni::NativeWeakPtr<GeckoViewSupport>;

/**
 * PanZoomController handles its native calls on the UI thread, so make
 * it separate from GeckoViewSupport.
 */
class NPZCSupport final
    : public java::PanZoomController::NativeProvider::Natives<NPZCSupport>,
      public AndroidVsync::Observer {
  WindowPtr mWindow;
  java::PanZoomController::NativeProvider::WeakRef mNPZC;

  // Stores the returnResult of each pending motion event between
  // HandleMotionEvent and FinishHandlingMotionEvent.
  std::queue<std::pair<uint64_t, java::GeckoResult::GlobalRef>>
      mPendingMotionEventReturnResults;

  RefPtr<AndroidVsync> mAndroidVsync;
  TouchResampler mTouchResampler;
  int mPreviousButtons = 0;
  bool mListeningToVsync = false;

  // Only true if mAndroidVsync is non-null and the resampling pref is set.
  bool mTouchResamplingEnabled = false;

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
      const auto npzcSupportWeak = GetNative(
          java::PanZoomController::NativeProvider::LocalRef(env, mNPZC));
      if (!npzcSupportWeak) {
        // We already shut down.
        env->ExceptionClear();
        return;
      }

      auto acc = npzcSupportWeak->Access();
      if (!acc) {
        // We already shut down.
        env->ExceptionClear();
        return;
      }

      auto win = acc->mWindow.Access();
      if (!win) {
        // We already shut down.
        env->ExceptionClear();
        return;
      }

      nsWindow* const window = win->GetNsWindow();
      if (!window) {
        // We already shut down.
        env->ExceptionClear();
        return;
      }

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

  NPZCSupport(WindowPtr aWindow,
              const java::PanZoomController::NativeProvider::LocalRef& aNPZC)
      : mWindow(aWindow), mNPZC(aNPZC) {
#if defined(DEBUG)
    auto win(mWindow.Access());
    MOZ_ASSERT(!!win);
#endif  // defined(DEBUG)

    // Use vsync for touch resampling on API level 19 and above.
    // See gfxAndroidPlatform::CreateHardwareVsyncSource() for comparison.
    if (jni::GetAPIVersion() >= 19) {
      mAndroidVsync = AndroidVsync::GetInstance();
    }
  }

  ~NPZCSupport() {
    if (mListeningToVsync) {
      MOZ_RELEASE_ASSERT(mAndroidVsync);
      mAndroidVsync->UnregisterObserver(this, AndroidVsync::INPUT);
      mListeningToVsync = false;
    }
  }

  using Base::AttachNative;
  using Base::DisposeNative;

  void OnWeakNonIntrusiveDetach(already_AddRefed<Runnable> aDisposer) {
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
    // 1) happens through OnWeakNonIntrusiveDetach, which first notifies the UI
    // thread through Destroy; Destroy then calls DisposeNative, which
    // finally disposes the native instance back on the Gecko thread. Using
    // Destroy to indirectly call DisposeNative here also solves 5), by
    // making everything go through the UI thread, avoiding contention.
    //
    // 2) and 3) are solved by clearing mWindow, which signals to the
    // pending event that we had shut down. In that case the event bails
    // and does not touch mWindow.
    //
    // 4) happens through DisposeNative directly.
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

      uiThread->Dispatch(
          NS_NewRunnableFunction("NPZCSupport::OnWeakNonIntrusiveDetach",
                                 [npzc, disposer = std::move(disposer)] {
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

    if (auto window = mWindow.Access()) {
      nsWindow* gkWindow = window->GetNsWindow();
      if (gkWindow) {
        controller = gkWindow->mAPZC;
      }
    }

    if (controller) {
      controller->SetLongTapEnabled(aIsLongpressEnabled);
    }
  }

  int32_t HandleScrollEvent(int64_t aTime, int32_t aMetaState, float aX,
                            float aY, float aHScroll, float aVScroll) {
    MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

    RefPtr<IAPZCTreeManager> controller;

    if (auto window = mWindow.Access()) {
      nsWindow* gkWindow = window->GetNsWindow();
      if (gkWindow) {
        controller = gkWindow->mAPZC;
      }
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
        aTime, nsWindow::GetEventTimeStamp(aTime),
        nsWindow::GetModifiers(aMetaState), ScrollWheelInput::SCROLLMODE_SMOOTH,
        ScrollWheelInput::SCROLLDELTA_PIXEL, origin, aHScroll, aVScroll, false,
        // XXX Do we need to support auto-dir scrolling
        // for Android widgets with a wheel device?
        // Currently, I just leave it unimplemented. If
        // we need to implement it, what's the extra work
        // to do?
        WheelDeltaAdjustmentStrategy::eNone);

    APZEventResult result = controller->InputBridge()->ReceiveInputEvent(input);
    if (result.GetStatus() == nsEventStatus_eConsumeNoDefault) {
      return INPUT_RESULT_IGNORED;
    }

    PostInputEvent([input, result](nsWindow* window) {
      WidgetWheelEvent wheelEvent = input.ToWidgetEvent(window);
      window->ProcessUntransformedAPZEvent(&wheelEvent, result);
    });

    switch (result.GetStatus()) {
      case nsEventStatus_eIgnore:
        return INPUT_RESULT_UNHANDLED;
      case nsEventStatus_eConsumeDoDefault:
        return result.GetHandledResult()->IsHandledByRoot()
                   ? INPUT_RESULT_HANDLED
                   : INPUT_RESULT_HANDLED_CONTENT;
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
        result = MouseInput::PRIMARY_BUTTON;
        break;
      case java::sdk::MotionEvent::BUTTON_SECONDARY:
        result = MouseInput::SECONDARY_BUTTON;
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
      result |= MouseButtonsFlag::ePrimaryFlag;
    }
    if (buttons & java::sdk::MotionEvent::BUTTON_SECONDARY) {
      result |= MouseButtonsFlag::eSecondaryFlag;
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

  static int32_t ConvertAPZHandledPlace(APZHandledPlace aHandledPlace) {
    switch (aHandledPlace) {
      case APZHandledPlace::Unhandled:
        return INPUT_RESULT_UNHANDLED;
      case APZHandledPlace::HandledByRoot:
        return INPUT_RESULT_HANDLED;
      case APZHandledPlace::HandledByContent:
        return INPUT_RESULT_HANDLED_CONTENT;
      case APZHandledPlace::Invalid:
        MOZ_ASSERT_UNREACHABLE("The handled result should NOT be Invalid");
        return INPUT_RESULT_UNHANDLED;
    }
    MOZ_ASSERT_UNREACHABLE("Unknown handled result");
    return INPUT_RESULT_UNHANDLED;
  }

  static int32_t ConvertSideBits(SideBits aSideBits) {
    int32_t ret = java::PanZoomController::SCROLLABLE_FLAG_NONE;
    if (aSideBits & SideBits::eTop) {
      ret |= java::PanZoomController::SCROLLABLE_FLAG_TOP;
    }
    if (aSideBits & SideBits::eRight) {
      ret |= java::PanZoomController::SCROLLABLE_FLAG_RIGHT;
    }
    if (aSideBits & SideBits::eBottom) {
      ret |= java::PanZoomController::SCROLLABLE_FLAG_BOTTOM;
    }
    if (aSideBits & SideBits::eLeft) {
      ret |= java::PanZoomController::SCROLLABLE_FLAG_LEFT;
    }
    return ret;
  }

  static int32_t ConvertScrollDirections(
      layers::ScrollDirections aScrollDirections) {
    int32_t ret = java::PanZoomController::OVERSCROLL_FLAG_NONE;
    if (aScrollDirections.contains(layers::HorizontalScrollDirection)) {
      ret |= java::PanZoomController::OVERSCROLL_FLAG_HORIZONTAL;
    }
    if (aScrollDirections.contains(layers::VerticalScrollDirection)) {
      ret |= java::PanZoomController::OVERSCROLL_FLAG_VERTICAL;
    }
    return ret;
  }

  static java::PanZoomController::InputResultDetail::LocalRef
  ConvertAPZHandledResult(const APZHandledResult& aHandledResult) {
    return java::PanZoomController::InputResultDetail::New(
        ConvertAPZHandledPlace(aHandledResult.mPlace),
        ConvertSideBits(aHandledResult.mScrollableDirections),
        ConvertScrollDirections(aHandledResult.mOverscrollDirections));
  }

 public:
  int32_t HandleMouseEvent(int32_t aAction, int64_t aTime, int32_t aMetaState,
                           float aX, float aY, int buttons) {
    MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

    RefPtr<IAPZCTreeManager> controller;

    if (auto window = mWindow.Access()) {
      nsWindow* gkWindow = window->GetNsWindow();
      if (gkWindow) {
        controller = gkWindow->mAPZC;
      }
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

    MouseInput input(
        mouseType, buttonType, MouseEvent_Binding::MOZ_SOURCE_MOUSE,
        ConvertButtons(buttons), origin, aTime,
        nsWindow::GetEventTimeStamp(aTime), nsWindow::GetModifiers(aMetaState));

    APZEventResult result = controller->InputBridge()->ReceiveInputEvent(input);
    if (result.GetStatus() == nsEventStatus_eConsumeNoDefault) {
      return INPUT_RESULT_IGNORED;
    }

    PostInputEvent([input, result](nsWindow* window) {
      WidgetMouseEvent mouseEvent = input.ToWidgetEvent(window);
      window->ProcessUntransformedAPZEvent(&mouseEvent, result);
    });

    switch (result.GetStatus()) {
      case nsEventStatus_eIgnore:
        return INPUT_RESULT_UNHANDLED;
      case nsEventStatus_eConsumeDoDefault:
        return result.GetHandledResult()->IsHandledByRoot()
                   ? INPUT_RESULT_HANDLED
                   : INPUT_RESULT_HANDLED_CONTENT;
      default:
        MOZ_ASSERT_UNREACHABLE("Unexpected nsEventStatus");
        return INPUT_RESULT_UNHANDLED;
    }
  }

  // Convert MotionEvent touch radius and orientation into the format required
  // by w3c touchevents.
  // toolMajor and toolMinor span a rectangle that's oriented as per
  // aOrientation, centered around the touch point.
  static std::pair<float, ScreenSize> ConvertOrientationAndRadius(
      float aOrientation, float aToolMajor, float aToolMinor) {
    float angle = aOrientation * 180.0f / M_PI;
    // w3c touchevents spec does not allow orientations == 90
    // this shifts it to -90, which will be shifted to zero below
    if (angle >= 90.0) {
      angle -= 180.0f;
    }

    // w3c touchevent radii are given with an orientation between 0 and
    // 90. The radii are found by removing the orientation and
    // measuring the x and y radii of the resulting ellipse. For
    // Android orientations >= 0 and < 90, use the y radius as the
    // major radius, and x as the minor radius. However, for an
    // orientation < 0, we have to shift the orientation by adding 90,
    // and reverse which radius is major and minor.
    ScreenSize radius;
    if (angle < 0.0f) {
      angle += 90.0f;
      radius =
          ScreenSize(int32_t(aToolMajor / 2.0f), int32_t(aToolMinor / 2.0f));
    } else {
      radius =
          ScreenSize(int32_t(aToolMinor / 2.0f), int32_t(aToolMajor / 2.0f));
    }

    return std::make_pair(angle, radius);
  }

  void HandleMotionEvent(
      const java::PanZoomController::NativeProvider::LocalRef& aInstance,
      jni::Object::Param aEventData, float aScreenX, float aScreenY,
      jni::Object::Param aResult) {
    MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

    auto returnResult = java::GeckoResult::Ref::From(aResult);
    auto eventData =
        java::PanZoomController::MotionEventData::Ref::From(aEventData);
    nsTArray<int32_t> pointerId(eventData->PointerId()->GetElements());
    size_t pointerCount = pointerId.Length();
    MultiTouchInput::MultiTouchType type;
    size_t startIndex = 0;
    size_t endIndex = pointerCount;

    switch (eventData->Action()) {
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
        startIndex = eventData->ActionIndex();
        endIndex = startIndex + 1;
        break;
      case java::sdk::MotionEvent::ACTION_OUTSIDE:
      case java::sdk::MotionEvent::ACTION_CANCEL:
        type = MultiTouchInput::MULTITOUCH_CANCEL;
        break;
      default:
        if (returnResult) {
          returnResult->Complete(
              java::sdk::Integer::ValueOf(INPUT_RESULT_UNHANDLED));
        }
        return;
    }

    MultiTouchInput input(type, eventData->Time(),
                          nsWindow::GetEventTimeStamp(eventData->Time()), 0);
    input.modifiers = nsWindow::GetModifiers(eventData->MetaState());
    input.mTouches.SetCapacity(endIndex - startIndex);
    input.mScreenOffset =
        ExternalIntPoint(int32_t(floorf(aScreenX)), int32_t(floorf(aScreenY)));

    size_t historySize = eventData->HistorySize();
    nsTArray<int64_t> historicalTime(
        eventData->HistoricalTime()->GetElements());
    MOZ_RELEASE_ASSERT(historicalTime.Length() == historySize);

    // Each of these is |historySize| sets of |pointerCount| values.
    size_t historicalDataCount = historySize * pointerCount;
    nsTArray<float> historicalX(eventData->HistoricalX()->GetElements());
    nsTArray<float> historicalY(eventData->HistoricalY()->GetElements());
    nsTArray<float> historicalOrientation(
        eventData->HistoricalOrientation()->GetElements());
    nsTArray<float> historicalPressure(
        eventData->HistoricalPressure()->GetElements());
    nsTArray<float> historicalToolMajor(
        eventData->HistoricalToolMajor()->GetElements());
    nsTArray<float> historicalToolMinor(
        eventData->HistoricalToolMinor()->GetElements());

    MOZ_RELEASE_ASSERT(historicalX.Length() == historicalDataCount);
    MOZ_RELEASE_ASSERT(historicalY.Length() == historicalDataCount);
    MOZ_RELEASE_ASSERT(historicalOrientation.Length() == historicalDataCount);
    MOZ_RELEASE_ASSERT(historicalPressure.Length() == historicalDataCount);
    MOZ_RELEASE_ASSERT(historicalToolMajor.Length() == historicalDataCount);
    MOZ_RELEASE_ASSERT(historicalToolMinor.Length() == historicalDataCount);

    // Each of these is |pointerCount| values.
    nsTArray<float> x(eventData->X()->GetElements());
    nsTArray<float> y(eventData->Y()->GetElements());
    nsTArray<float> orientation(eventData->Orientation()->GetElements());
    nsTArray<float> pressure(eventData->Pressure()->GetElements());
    nsTArray<float> toolMajor(eventData->ToolMajor()->GetElements());
    nsTArray<float> toolMinor(eventData->ToolMinor()->GetElements());

    MOZ_ASSERT(x.Length() == pointerCount);
    MOZ_ASSERT(y.Length() == pointerCount);
    MOZ_ASSERT(orientation.Length() == pointerCount);
    MOZ_ASSERT(pressure.Length() == pointerCount);
    MOZ_ASSERT(toolMajor.Length() == pointerCount);
    MOZ_ASSERT(toolMinor.Length() == pointerCount);

    for (size_t i = startIndex; i < endIndex; i++) {
      float orien;
      ScreenSize radius;
      std::tie(orien, radius) = ConvertOrientationAndRadius(
          orientation[i], toolMajor[i], toolMinor[i]);

      ScreenIntPoint point(int32_t(floorf(x[i])), int32_t(floorf(y[i])));
      SingleTouchData singleTouchData(pointerId[i], point, radius, orien,
                                      pressure[i]);

      for (size_t historyIndex = 0; historyIndex < historySize;
           historyIndex++) {
        size_t historicalI = historyIndex * pointerCount + i;
        float historicalAngle;
        ScreenSize historicalRadius;
        std::tie(historicalAngle, historicalRadius) =
            ConvertOrientationAndRadius(historicalOrientation[historicalI],
                                        historicalToolMajor[historicalI],
                                        historicalToolMinor[historicalI]);
        ScreenIntPoint historicalPoint(
            int32_t(floorf(historicalX[historicalI])),
            int32_t(floorf(historicalY[historicalI])));
        singleTouchData.mHistoricalData.AppendElement(
            SingleTouchData::HistoricalTouchData{
                nsWindow::GetEventTimeStamp(historicalTime[historyIndex]),
                historicalPoint,
                {},  // mLocalScreenPoint will be computed later by APZ
                historicalRadius,
                historicalAngle,
                historicalPressure[historicalI]});
      }

      input.mTouches.AppendElement(singleTouchData);
    }

    if (mAndroidVsync &&
        eventData->Action() == java::sdk::MotionEvent::ACTION_DOWN) {
      // Query pref value at the beginning of a touch gesture so that we don't
      // leave events stuck in the resampler after a pref flip.
      mTouchResamplingEnabled = StaticPrefs::android_touch_resampling_enabled();
    }

    if (!mTouchResamplingEnabled) {
      FinishHandlingMotionEvent(std::move(input),
                                java::GeckoResult::LocalRef(returnResult));
      return;
    }

    uint64_t eventId = mTouchResampler.ProcessEvent(std::move(input));
    mPendingMotionEventReturnResults.push(
        {eventId, java::GeckoResult::GlobalRef(returnResult)});

    RegisterOrUnregisterForVsync(mTouchResampler.InTouchingState());
    ConsumeMotionEventsFromResampler();
  }

  void RegisterOrUnregisterForVsync(bool aNeedVsync) {
    MOZ_RELEASE_ASSERT(mAndroidVsync);
    if (aNeedVsync && !mListeningToVsync) {
      mAndroidVsync->RegisterObserver(this, AndroidVsync::INPUT);
    } else if (!aNeedVsync && mListeningToVsync) {
      mAndroidVsync->UnregisterObserver(this, AndroidVsync::INPUT);
    }
    mListeningToVsync = aNeedVsync;
  }

  void OnVsync(const TimeStamp& aTimeStamp) override {
    mTouchResampler.NotifyFrame(aTimeStamp - TimeDuration::FromMilliseconds(
                                                 kTouchResampleVsyncAdjustMs));
    ConsumeMotionEventsFromResampler();
  }

  void ConsumeMotionEventsFromResampler() {
    auto outgoing = mTouchResampler.ConsumeOutgoingEvents();
    while (!outgoing.empty()) {
      auto outgoingEvent = std::move(outgoing.front());
      outgoing.pop();
      java::GeckoResult::GlobalRef returnResult;
      if (outgoingEvent.mEventId) {
        // Look up the GeckoResult for this event.
        // The outgoing events from the resampler are in the same order as the
        // original events, and no event IDs are skipped.
        MOZ_RELEASE_ASSERT(!mPendingMotionEventReturnResults.empty());
        auto pair = mPendingMotionEventReturnResults.front();
        mPendingMotionEventReturnResults.pop();
        MOZ_RELEASE_ASSERT(pair.first == *outgoingEvent.mEventId);
        returnResult = pair.second;
      }
      FinishHandlingMotionEvent(std::move(outgoingEvent.mEvent),
                                java::GeckoResult::LocalRef(returnResult));
    }
  }

  void FinishHandlingMotionEvent(MultiTouchInput&& aInput,
                                 java::GeckoResult::LocalRef&& aReturnResult) {
    RefPtr<IAPZCTreeManager> controller;

    if (auto window = mWindow.Access()) {
      nsWindow* gkWindow = window->GetNsWindow();
      if (gkWindow) {
        controller = gkWindow->mAPZC;
      }
    }

    if (!controller) {
      if (aReturnResult) {
        aReturnResult->Complete(java::PanZoomController::InputResultDetail::New(
            INPUT_RESULT_UNHANDLED,
            java::PanZoomController::SCROLLABLE_FLAG_NONE,
            java::PanZoomController::OVERSCROLL_FLAG_NONE));
      }
      return;
    }

    APZEventResult result =
        controller->InputBridge()->ReceiveInputEvent(aInput);
    if (result.GetStatus() == nsEventStatus_eConsumeNoDefault) {
      if (aReturnResult) {
        aReturnResult->Complete(java::PanZoomController::InputResultDetail::New(
            INPUT_RESULT_IGNORED, java::PanZoomController::SCROLLABLE_FLAG_NONE,
            java::PanZoomController::OVERSCROLL_FLAG_NONE));
      }
      return;
    }

    // Dispatch APZ input event on Gecko thread.
    PostInputEvent([aInput, result](nsWindow* window) {
      WidgetTouchEvent touchEvent = aInput.ToWidgetTouchEvent(window);
      window->ProcessUntransformedAPZEvent(&touchEvent, result);
      window->DispatchHitTest(touchEvent);
    });

    if (!aReturnResult) {
      // We don't care how APZ handled the event so we're done here.
      return;
    }

    if (result.GetHandledResult() != Nothing()) {
      // We know conclusively that the root APZ handled this or not and
      // don't need to do any more work.
      switch (result.GetStatus()) {
        case nsEventStatus_eIgnore:
          aReturnResult->Complete(
              java::PanZoomController::InputResultDetail::New(
                  INPUT_RESULT_UNHANDLED,
                  java::PanZoomController::SCROLLABLE_FLAG_NONE,
                  java::PanZoomController::OVERSCROLL_FLAG_NONE));
          break;
        case nsEventStatus_eConsumeDoDefault:
          aReturnResult->Complete(
              ConvertAPZHandledResult(result.GetHandledResult().value()));
          break;
        default:
          MOZ_ASSERT_UNREACHABLE("Unexpected nsEventStatus");
          aReturnResult->Complete(
              java::PanZoomController::InputResultDetail::New(
                  INPUT_RESULT_UNHANDLED,
                  java::PanZoomController::SCROLLABLE_FLAG_NONE,
                  java::PanZoomController::OVERSCROLL_FLAG_NONE));
          break;
      }
      return;
    }

    // Wait to see if APZ handled the event or not...
    controller->AddInputBlockCallback(
        result.mInputBlockId,
        [aReturnResult = java::GeckoResult::GlobalRef(aReturnResult)](
            uint64_t aInputBlockId, const APZHandledResult& aHandledResult) {
          aReturnResult->Complete(ConvertAPZHandledResult(aHandledResult));
        });
  }
};

NS_IMPL_ISUPPORTS(AndroidView, nsIAndroidEventDispatcher, nsIAndroidView)

nsresult AndroidView::GetInitData(JSContext* aCx, JS::MutableHandleValue aOut) {
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
class LayerViewSupport final
    : public GeckoSession::Compositor::Natives<LayerViewSupport> {
  WindowPtr mWindow;
  GeckoSession::Compositor::WeakRef mCompositor;
  Atomic<bool, ReleaseAcquire> mCompositorPaused;
  jni::Object::GlobalRef mSurface;

  struct CaptureRequest {
    explicit CaptureRequest() : mResult(nullptr) {}
    explicit CaptureRequest(java::GeckoResult::GlobalRef aResult,
                            java::sdk::Bitmap::GlobalRef aBitmap,
                            const ScreenRect& aSource,
                            const IntSize& aOutputSize)
        : mResult(aResult),
          mBitmap(aBitmap),
          mSource(aSource),
          mOutputSize(aOutputSize) {}

    // where to send the pixels
    java::GeckoResult::GlobalRef mResult;

    // where to store the pixels
    java::sdk::Bitmap::GlobalRef mBitmap;

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

  LayerViewSupport(WindowPtr aWindow,
                   const GeckoSession::Compositor::LocalRef& aInstance)
      : mWindow(aWindow), mCompositor(aInstance), mCompositorPaused(true) {
#if defined(DEBUG)
    auto win(mWindow.Access());
    MOZ_ASSERT(!!win);
#endif  // defined(DEBUG)
  }

  ~LayerViewSupport() {}

  using Base::AttachNative;
  using Base::DisposeNative;

  void OnWeakNonIntrusiveDetach(already_AddRefed<Runnable> aDisposer) {
    RefPtr<Runnable> disposer = aDisposer;
    if (RefPtr<nsThread> uiThread = GetAndroidUiThread()) {
      GeckoSession::Compositor::GlobalRef compositor(mCompositor);
      if (!compositor) {
        return;
      }

      uiThread->Dispatch(NS_NewRunnableFunction(
          "LayerViewSupport::OnWeakNonIntrusiveDetach",
          [compositor, disposer = std::move(disposer),
           results = &mCapturePixelsResults, window = mWindow]() mutable {
            if (auto accWindow = window.Access()) {
              while (!results->empty()) {
                auto aResult =
                    java::GeckoResult::LocalRef(results->front().mResult);
                if (aResult) {
                  aResult->CompleteExceptionally(
                      java::sdk::IllegalStateException::New(
                          "The compositor has detached from the session")
                          .Cast<jni::Throwable>());
                }
                results->pop();
              }
              compositor->OnCompositorDetached();
            }

            disposer->Run();
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
    if (auto window = mWindow.Access()) {
      nsWindow* gkWindow = window->GetNsWindow();
      if (gkWindow) {
        child = gkWindow->GetUiCompositorControllerChild();
      }
    }
    return child.forget();
  }

  already_AddRefed<DataSourceSurface> FlipScreenPixels(
      Shmem& aMem, const ScreenIntSize& aInSize, const ScreenRect& aInRegion,
      const IntSize& aOutSize) {
    RefPtr<gfx::DataSourceSurface> image =
        gfx::Factory::CreateWrappingDataSourceSurface(
            aMem.get<uint8_t>(),
            StrideForFormatAndWidth(SurfaceFormat::B8G8R8A8, aInSize.width),
            IntSize(aInSize.width, aInSize.height), SurfaceFormat::B8G8R8A8);
    RefPtr<gfx::DrawTarget> drawTarget =
        gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
            aOutSize, SurfaceFormat::B8G8R8A8);
    if (!drawTarget) {
      return nullptr;
    }

    drawTarget->SetTransform(Matrix::Scaling(1.0, -1.0) *
                             Matrix::Translation(0, aOutSize.height));

    gfx::Rect srcRect(aInRegion.x,
                      (aInSize.height - aInRegion.height) - aInRegion.y,
                      aInRegion.width, aInRegion.height);
    gfx::Rect destRect(0, 0, aOutSize.width, aOutSize.height);
    drawTarget->DrawSurface(image, destRect, srcRect);

    RefPtr<gfx::SourceSurface> snapshot = drawTarget->Snapshot();
    RefPtr<gfx::DataSourceSurface> data = snapshot->GetDataSurface();
    return data.forget();
  }

  /**
   * Compositor methods
   */
 public:
  void AttachNPZC(jni::Object::Param aNPZC) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aNPZC);

    auto locked(mWindow.Access());
    if (!locked) {
      return;  // Already shut down.
    }

    nsWindow* gkWindow = locked->GetNsWindow();

    // We can have this situation if we get two GeckoViewSupport::Transfer()
    // called before the first AttachNPZC() gets here. Just detach the current
    // instance since that's what happens in GeckoViewSupport::Transfer() as
    // well.
    gkWindow->mNPZCSupport.Detach();

    auto npzc = java::PanZoomController::NativeProvider::LocalRef(
        jni::GetGeckoThreadEnv(),
        java::PanZoomController::NativeProvider::Ref::From(aNPZC));
    gkWindow->mNPZCSupport =
        jni::NativeWeakPtrHolder<NPZCSupport>::Attach(npzc, mWindow, npzc);

    DispatchToUiThread(
        "LayerViewSupport::AttachNPZC",
        [npzc = java::PanZoomController::NativeProvider::GlobalRef(npzc)] {
          npzc->SetAttached(true);
        });
  }

  void OnBoundsChanged(int32_t aLeft, int32_t aTop, int32_t aWidth,
                       int32_t aHeight) {
    MOZ_ASSERT(NS_IsMainThread());
    auto acc = mWindow.Access();
    if (!acc) {
      return;  // Already shut down.
    }

    nsWindow* gkWindow = acc->GetNsWindow();
    if (!gkWindow) {
      return;
    }

    gkWindow->Resize(aLeft, aTop, aWidth, aHeight, /* repaint */ false);
  }

  void SetDynamicToolbarMaxHeight(int32_t aHeight) {
    MOZ_ASSERT(NS_IsMainThread());
    auto acc = mWindow.Access();
    if (!acc) {
      return;  // Already shut down.
    }

    nsWindow* gkWindow = acc->GetNsWindow();
    if (!gkWindow) {
      return;
    }

    gkWindow->UpdateDynamicToolbarMaxHeight(ScreenIntCoord(aHeight));
  }

  void SyncPauseCompositor() {
    MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

    if (RefPtr<UiCompositorControllerChild> child =
            GetUiCompositorControllerChild()) {
      mCompositorPaused = true;
      child->Pause();
    }

    if (auto lock{mWindow.Access()}) {
      while (!mCapturePixelsResults.empty()) {
        auto result =
            java::GeckoResult::LocalRef(mCapturePixelsResults.front().mResult);
        if (result) {
          result->CompleteExceptionally(
              java::sdk::IllegalStateException::New(
                  "The compositor has detached from the session")
                  .Cast<jni::Throwable>());
        }
        mCapturePixelsResults.pop();
      }
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
        const auto lvsHolder =
            GetNative(GeckoSession::Compositor::LocalRef(env, mCompositor));

        if (!lvsHolder) {
          env->ExceptionClear();
          return;  // Already shut down.
        }

        auto lvs(lvsHolder->Access());
        if (!lvs) {
          env->ExceptionClear();
          return;  // Already shut down.
        }

        auto win = lvs->mWindow.Access();
        if (!win) {
          env->ExceptionClear();
          return;  // Already shut down.
        }

        // When we get here, the compositor has already been told to
        // resume. This means it's now safe for layer updates to occur.
        // Since we might have prevented one or more draw events from
        // occurring while the compositor was paused, we need to
        // schedule a draw event now.
        if (!lvs->mCompositorPaused) {
          nsWindow* const gkWindow = win->GetNsWindow();
          if (gkWindow) {
            gkWindow->RedrawAll();
          }
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
    if (auto acc{mWindow.Access()}) {
      nsWindow* gkWindow = acc->GetNsWindow();
      if (gkWindow) {
        gkWindow->UpdateDynamicToolbarOffset(ScreenIntCoord(aOffset));
      }
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

  void RequestScreenPixels(jni::Object::Param aResult,
                           jni::Object::Param aTarget, int32_t aXOffset,
                           int32_t aYOffset, int32_t aSrcWidth,
                           int32_t aSrcHeight, int32_t aOutWidth,
                           int32_t aOutHeight) {
    MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

    int size = 0;
    if (auto window = mWindow.Access()) {
      mCapturePixelsResults.push(CaptureRequest(
          java::GeckoResult::GlobalRef(java::GeckoResult::LocalRef(aResult)),
          java::sdk::Bitmap::GlobalRef(java::sdk::Bitmap::LocalRef(aTarget)),
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

  void RecvScreenPixels(Shmem&& aMem, const ScreenIntSize& aSize,
                        bool aNeedsYFlip) {
    MOZ_ASSERT(AndroidBridge::IsJavaUiThread());
    CaptureRequest request;
    java::GeckoResult::LocalRef result = nullptr;
    java::sdk::Bitmap::LocalRef bitmap = nullptr;
    if (auto window = mWindow.Access()) {
      // The result might have been already rejected if the compositor was
      // detached from the session
      if (!mCapturePixelsResults.empty()) {
        request = mCapturePixelsResults.front();
        result = java::GeckoResult::LocalRef(request.mResult);
        bitmap = java::sdk::Bitmap::LocalRef(request.mBitmap);
        mCapturePixelsResults.pop();
      }
    }

    if (result) {
      if (bitmap) {
        RefPtr<DataSourceSurface> surf;
        if (aNeedsYFlip) {
          surf = FlipScreenPixels(aMem, aSize, request.mSource,
                                  request.mOutputSize);
        } else {
          surf = gfx::Factory::CreateWrappingDataSourceSurface(
              aMem.get<uint8_t>(),
              StrideForFormatAndWidth(SurfaceFormat::B8G8R8A8, aSize.width),
              IntSize(aSize.width, aSize.height), SurfaceFormat::B8G8R8A8);
        }
        if (surf) {
          DataSourceSurface::ScopedMap smap(surf, DataSourceSurface::READ);
          auto pixels = mozilla::jni::ByteBuffer::New(
              reinterpret_cast<int8_t*>(smap.GetData()),
              smap.GetStride() * request.mOutputSize.height);
          bitmap->CopyPixelsFromBuffer(pixels);
          result->Complete(bitmap);
        } else {
          result->CompleteExceptionally(
              java::sdk::IllegalStateException::New(
                  "Failed to create flipped snapshot surface (probably out of "
                  "memory)")
                  .Cast<jni::Throwable>());
        }
      } else {
        result->CompleteExceptionally(java::sdk::IllegalArgumentException::New(
                                          "No target bitmap argument provided")
                                          .Cast<jni::Throwable>());
      }
    }

    // Pixels have been copied, so Dealloc Shmem
    if (RefPtr<UiCompositorControllerChild> child =
            GetUiCompositorControllerChild()) {
      child->DeallocPixelBuffer(aMem);

      if (auto window = mWindow.Access()) {
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

  void OnSafeAreaInsetsChanged(int32_t aTop, int32_t aRight, int32_t aBottom,
                               int32_t aLeft) {
    MOZ_ASSERT(NS_IsMainThread());
    auto win(mWindow.Access());
    if (!win) {
      return;  // Already shut down.
    }

    nsWindow* gkWindow = win->GetNsWindow();
    if (!gkWindow) {
      return;
    }

    ScreenIntMargin safeAreaInsets(aTop, aRight, aBottom, aLeft);
    gkWindow->UpdateSafeAreaInsets(safeAreaInsets);
  }
};

GeckoViewSupport::~GeckoViewSupport() {
  if (mWindow) {
    mWindow->DetachNatives();
  }
}

/* static */
void GeckoViewSupport::Open(
    const jni::Class::LocalRef& aCls, GeckoSession::Window::Param aWindow,
    jni::Object::Param aQueue, jni::Object::Param aCompositor,
    jni::Object::Param aDispatcher, jni::Object::Param aSessionAccessibility,
    jni::Object::Param aInitData, jni::String::Param aId,
    jni::String::Param aChromeURI, int32_t aScreenId, bool aPrivateMode) {
  MOZ_ASSERT(NS_IsMainThread());

  AUTO_PROFILER_LABEL("mozilla::widget::GeckoViewSupport::Open", OTHER);

  nsCOMPtr<nsIWindowWatcher> ww = do_GetService(NS_WINDOWWATCHER_CONTRACTID);
  MOZ_RELEASE_ASSERT(ww);

  nsAutoCString url;
  if (aChromeURI) {
    url = aChromeURI->ToCString();
  } else {
    nsresult rv = Preferences::GetCString("toolkit.defaultChromeURI", url);
    if (NS_FAILED(rv)) {
      url = "chrome://geckoview/content/geckoview.xhtml"_ns;
    }
  }

  // Prepare an nsIAndroidView to pass as argument to the window.
  RefPtr<AndroidView> androidView = new AndroidView();
  androidView->mEventDispatcher->Attach(
      java::EventDispatcher::Ref::From(aDispatcher), nullptr);
  androidView->mInitData = java::GeckoBundle::Ref::From(aInitData);

  nsAutoCString chromeFlags("chrome,dialog=0,remote,resizable,scrollbars");
  if (aPrivateMode) {
    chromeFlags += ",private";
  }
  nsCOMPtr<mozIDOMWindowProxy> domWindow;
  ww->OpenWindow(nullptr, url, nsDependentCString(aId->ToCString().get()),
                 chromeFlags, androidView, getter_AddRefs(domWindow));
  MOZ_RELEASE_ASSERT(domWindow);

  nsCOMPtr<nsPIDOMWindowOuter> pdomWindow = nsPIDOMWindowOuter::From(domWindow);
  const RefPtr<nsWindow> window = nsWindow::From(pdomWindow);
  MOZ_ASSERT(window);
  window->SetScreenId(aScreenId);

  // Attach a new GeckoView support object to the new window.
  GeckoSession::Window::LocalRef sessionWindow(aCls.Env(), aWindow);
  auto weakGeckoViewSupport =
      jni::NativeWeakPtrHolder<GeckoViewSupport>::Attach(
          sessionWindow, window, sessionWindow, pdomWindow);

  window->mGeckoViewSupport = weakGeckoViewSupport;
  window->mAndroidView = androidView;

  // Attach other session support objects.
  {  // Scope for gvsAccess
    auto gvsAccess = weakGeckoViewSupport.Access();
    MOZ_ASSERT(gvsAccess);

    gvsAccess->Transfer(sessionWindow, aQueue, aCompositor, aDispatcher,
                        aSessionAccessibility, aInitData);
  }

  if (window->mWidgetListener) {
    nsCOMPtr<nsIAppWindow> appWindow(window->mWidgetListener->GetAppWindow());
    if (appWindow) {
      // Our window is not intrinsically sized, so tell AppWindow to
      // not set a size for us.
      appWindow->SetIntrinsicallySized(false);
    }
  }
}

void GeckoViewSupport::Close() {
  if (mWindow) {
    if (mWindow->mAndroidView) {
      mWindow->mAndroidView->mEventDispatcher->Detach();
    }
    mWindow = nullptr;
  }

  if (!mDOMWindow) {
    return;
  }

  mDOMWindow->ForceClose();
  mDOMWindow = nullptr;
  mGeckoViewWindow = nullptr;
}

void GeckoViewSupport::Transfer(const GeckoSession::Window::LocalRef& inst,
                                jni::Object::Param aQueue,
                                jni::Object::Param aCompositor,
                                jni::Object::Param aDispatcher,
                                jni::Object::Param aSessionAccessibility,
                                jni::Object::Param aInitData) {
  mWindow->mNPZCSupport.Detach();

  auto compositor = GeckoSession::Compositor::LocalRef(
      inst.Env(), GeckoSession::Compositor::Ref::From(aCompositor));

  bool attachLvs;
  {  // Scope for lvsAccess
    auto lvsAccess{mWindow->mLayerViewSupport.Access()};
    // If we do not yet have mLayerViewSupport, or if the compositor has
    // changed, then we must attach a new one.
    attachLvs = !lvsAccess || lvsAccess->GetJavaCompositor() != compositor;
  }

  if (attachLvs) {
    mWindow->mLayerViewSupport =
        jni::NativeWeakPtrHolder<LayerViewSupport>::Attach(
            compositor, mWindow->mGeckoViewSupport, compositor);
  }

  MOZ_ASSERT(mWindow->mAndroidView);
  mWindow->mAndroidView->mEventDispatcher->Attach(
      java::EventDispatcher::Ref::From(aDispatcher), mDOMWindow);

  mWindow->mSessionAccessibility.Detach();
  if (aSessionAccessibility) {
    AttachAccessibility(inst, aSessionAccessibility);
  }

  if (mIsReady) {
    // We're in a transfer; update init-data and notify JS code.
    mWindow->mAndroidView->mInitData = java::GeckoBundle::Ref::From(aInitData);
    OnReady(aQueue);
    mWindow->mAndroidView->mEventDispatcher->Dispatch(
        u"GeckoView:UpdateInitData");
  }

  DispatchToUiThread("GeckoViewSupport::Transfer",
                     [compositor = GeckoSession::Compositor::GlobalRef(
                          compositor)] { compositor->OnCompositorAttached(); });
}

void GeckoViewSupport::AttachEditable(
    const GeckoSession::Window::LocalRef& inst,
    jni::Object::Param aEditableParent) {
  if (auto win{mWindow->mEditableSupport.Access()}) {
    win->TransferParent(aEditableParent);
  } else {
    auto editableChild = java::GeckoEditableChild::New(aEditableParent,
                                                       /* default */ true);
    mWindow->mEditableSupport =
        jni::NativeWeakPtrHolder<GeckoEditableSupport>::Attach(
            editableChild, mWindow->mGeckoViewSupport, editableChild);
  }

  mWindow->mEditableParent = aEditableParent;
}

void GeckoViewSupport::AttachAccessibility(
    const GeckoSession::Window::LocalRef& inst,
    jni::Object::Param aSessionAccessibility) {
  java::SessionAccessibility::NativeProvider::LocalRef sessionAccessibility(
      inst.Env());
  sessionAccessibility = java::SessionAccessibility::NativeProvider::Ref::From(
      aSessionAccessibility);

  mWindow->mSessionAccessibility =
      jni::NativeWeakPtrHolder<a11y::SessionAccessibility>::Attach(
          sessionAccessibility, mWindow->mGeckoViewSupport,
          sessionAccessibility);
}

auto GeckoViewSupport::OnLoadRequest(mozilla::jni::String::Param aUri,
                                     int32_t aWindowType, int32_t aFlags,
                                     mozilla::jni::String::Param aTriggeringUri,
                                     bool aHasUserGesture,
                                     bool aIsTopLevel) const
    -> java::GeckoResult::LocalRef {
  GeckoSession::Window::LocalRef window(mGeckoViewWindow);
  if (!window) {
    return nullptr;
  }
  return window->OnLoadRequest(aUri, aWindowType, aFlags, aTriggeringUri,
                               aHasUserGesture, aIsTopLevel);
}

void GeckoViewSupport::OnReady(jni::Object::Param aQueue) {
  GeckoSession::Window::LocalRef window(mGeckoViewWindow);
  if (!window) {
    return;
  }
  window->OnReady(aQueue);
  mIsReady = true;
}

void GeckoViewSupport::PassExternalResponse(
    java::WebResponse::Param aResponse) {
  GeckoSession::Window::LocalRef window(mGeckoViewWindow);
  if (!window) {
    return;
  }

  auto response = java::WebResponse::GlobalRef(aResponse);

  DispatchToUiThread("GeckoViewSupport::PassExternalResponse",
                     [window = java::GeckoSession::Window::GlobalRef(window),
                      response] { window->PassExternalWebResponse(response); });
}

}  // namespace widget
}  // namespace mozilla

void nsWindow::InitNatives() {
  jni::InitConversionStatics();
  mozilla::widget::GeckoViewSupport::Base::Init();
  mozilla::widget::LayerViewSupport::Init();
  mozilla::widget::NPZCSupport::Init();

  mozilla::widget::GeckoEditableSupport::Init();
  a11y::SessionAccessibility::Init();
}

void nsWindow::DetachNatives() {
  MOZ_ASSERT(NS_IsMainThread());
  mEditableSupport.Detach();
  mNPZCSupport.Detach();
  mLayerViewSupport.Detach();
  mSessionAccessibility.Detach();
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

  // Disassociate our native object from GeckoView.
  mGeckoViewSupport.Detach();

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

mozilla::widget::EventDispatcher* nsWindow::GetEventDispatcher() const {
  if (mAndroidView) {
    return mAndroidView->mEventDispatcher;
  }
  return nullptr;
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
  auto acc(mGeckoViewSupport.Access());
  if (!acc) {
    return;
  }

  acc->OnReady();
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

RefPtr<MozPromise<bool, bool, false>> nsWindow::OnLoadRequest(
    nsIURI* aUri, int32_t aWindowType, int32_t aFlags,
    nsIPrincipal* aTriggeringPrincipal, bool aHasUserGesture,
    bool aIsTopLevel) {
  auto geckoViewSupport(mGeckoViewSupport.Access());
  if (!geckoViewSupport) {
    return MozPromise<bool, bool, false>::CreateAndResolve(false, __func__);
  }

  nsAutoCString spec, triggeringSpec;
  if (aUri) {
    aUri->GetDisplaySpec(spec);
    if (aIsTopLevel && mozilla::net::SchemeIsData(aUri) &&
        spec.Length() > MAX_TOPLEVEL_DATA_URI_LEN) {
      return MozPromise<bool, bool, false>::CreateAndResolve(false, __func__);
    }
  }

  bool isNullPrincipal = false;
  if (aTriggeringPrincipal) {
    aTriggeringPrincipal->GetIsNullPrincipal(&isNullPrincipal);

    if (!isNullPrincipal) {
      nsCOMPtr<nsIURI> triggeringUri;
      BasePrincipal::Cast(aTriggeringPrincipal)
          ->GetURI(getter_AddRefs(triggeringUri));
      if (triggeringUri) {
        triggeringUri->GetDisplaySpec(triggeringSpec);
      }
    }
  }

  auto geckoResult = geckoViewSupport->OnLoadRequest(
      spec.get(), aWindowType, aFlags,
      isNullPrincipal ? nullptr : triggeringSpec.get(), aHasUserGesture,
      aIsTopLevel);
  return geckoResult
             ? MozPromise<bool, bool, false>::FromGeckoResult(geckoResult)
             : nullptr;
}

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

  nsIWidgetListener* listener = GetWidgetListener();
  if (listener) {
    listener->FullscreenWillChange(aFullScreen);
  }

  mIsFullScreen = aFullScreen;
  mAndroidView->mEventDispatcher->Dispatch(
      aFullScreen ? u"GeckoView:FullScreenEnter" : u"GeckoView:FullScreenExit");

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
  if (::mozilla::jni::NativeWeakPtr<LayerViewSupport>::Accessor lvs{
          mLayerViewSupport.Access()}) {
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
  if (::mozilla::jni::NativeWeakPtr<LayerViewSupport>::Accessor lvs{
          mLayerViewSupport.Access()}) {
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
      if (::mozilla::jni::NativeWeakPtr<LayerViewSupport>::Accessor lvs{
              mLayerViewSupport.Access()}) {
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
    hittest.mInputSource = MouseEvent_Binding::MOZ_SOURCE_TOUCH;
    nsEventStatus status;
    DispatchEvent(&hittest, status);
  }
}

void nsWindow::PassExternalResponse(java::WebResponse::Param aResponse) {
  auto acc(mGeckoViewSupport.Access());
  if (!acc) {
    return;
  }

  acc->PassExternalResponse(aResponse);
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

void nsWindow::UserActivity() {
  if (!mIdleService) {
    mIdleService = do_GetService("@mozilla.org/widget/useridleservice;1");
  }

  if (mIdleService) {
    mIdleService->ResetIdleTimeOut(0);
  }

  if (FindTopLevel() != nsWindow::TopWindow()) {
    BringToFront();
  }
}

RefPtr<mozilla::a11y::SessionAccessibility>
nsWindow::GetSessionAccessibility() {
  auto acc(mSessionAccessibility.Access());
  if (!acc) {
    return nullptr;
  }

  return acc.AsRefPtr();
}

TextEventDispatcherListener* nsWindow::GetNativeTextEventDispatcherListener() {
  nsWindow* top = FindTopLevel();
  MOZ_ASSERT(top);

  auto acc(top->mEditableSupport.Access());
  if (!acc) {
    // Non-GeckoView windows don't support IME operations.
    return nullptr;
  }

  nsCOMPtr<TextEventDispatcherListener> ptr;
  if (NS_FAILED(acc->QueryInterface(NS_GET_IID(TextEventDispatcherListener),
                                    getter_AddRefs(ptr)))) {
    return nullptr;
  }

  return ptr.get();
}

void nsWindow::SetInputContext(const InputContext& aContext,
                               const InputContextAction& aAction) {
  nsWindow* top = FindTopLevel();
  MOZ_ASSERT(top);

  auto acc(top->mEditableSupport.Access());
  if (!acc) {
    // Non-GeckoView windows don't support IME operations.
    return;
  }

  // We are using an IME event later to notify Java, and the IME event
  // will be processed by the top window. Therefore, to ensure the
  // IME event uses the correct mInputContext, we need to let the top
  // window process SetInputContext
  acc->SetInputContext(aContext, aAction);
}

InputContext nsWindow::GetInputContext() {
  nsWindow* top = FindTopLevel();
  MOZ_ASSERT(top);

  auto acc(top->mEditableSupport.Access());
  if (!acc) {
    // Non-GeckoView windows don't support IME operations.
    return InputContext();
  }

  // We let the top window process SetInputContext,
  // so we should let it process GetInputContext as well.
  return acc->GetInputContext();
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

  MOZ_ASSERT(mNPZCSupport.IsAttached());
  auto npzcSup(mNPZCSupport.Access());
  MOZ_ASSERT(!!npzcSup);

  const auto& npzc = npzcSup->GetJavaNPZC();
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

nsresult nsWindow::SynthesizeNativeMouseEvent(
    LayoutDeviceIntPoint aPoint, NativeMouseMessage aNativeMessage,
    MouseButton aButton, nsIWidget::Modifiers aModifierFlags,
    nsIObserver* aObserver) {
  mozilla::widget::AutoObserverNotifier notifier(aObserver, "mouseevent");

  MOZ_ASSERT(mNPZCSupport.IsAttached());
  auto npzcSup(mNPZCSupport.Access());
  MOZ_ASSERT(!!npzcSup);

  const auto& npzc = npzcSup->GetJavaNPZC();
  const auto& bounds = FindTopLevel()->mBounds;
  aPoint.x -= bounds.x;
  aPoint.y -= bounds.y;

  int32_t nativeMessage;
  switch (aNativeMessage) {
    case NativeMouseMessage::ButtonDown:
      nativeMessage = java::sdk::MotionEvent::ACTION_POINTER_DOWN;
      break;
    case NativeMouseMessage::ButtonUp:
      nativeMessage = java::sdk::MotionEvent::ACTION_POINTER_UP;
      break;
    case NativeMouseMessage::Move:
      nativeMessage = java::sdk::MotionEvent::ACTION_HOVER_MOVE;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Non supported mouse event on Android");
      return NS_ERROR_INVALID_ARG;
  }
  int32_t button = 0;
  if (aNativeMessage != NativeMouseMessage::ButtonUp) {
    switch (aButton) {
      case MouseButton::ePrimary:
        button = java::sdk::MotionEvent::BUTTON_PRIMARY;
        break;
      case MouseButton::eMiddle:
        button = java::sdk::MotionEvent::BUTTON_TERTIARY;
        break;
      case MouseButton::eSecondary:
        button = java::sdk::MotionEvent::BUTTON_SECONDARY;
        break;
      case MouseButton::eX1:
        button = java::sdk::MotionEvent::BUTTON_BACK;
        break;
      case MouseButton::eX2:
        button = java::sdk::MotionEvent::BUTTON_FORWARD;
        break;
      default:
        if (aNativeMessage == NativeMouseMessage::ButtonDown) {
          MOZ_ASSERT_UNREACHABLE("Non supported mouse button type on Android");
          return NS_ERROR_INVALID_ARG;
        }
        break;
    }
  }

  // TODO (bug 1693237): Handle aModifierFlags.
  DispatchToUiThread(
      "nsWindow::SynthesizeNativeMouseEvent",
      [npzc = java::PanZoomController::NativeProvider::GlobalRef(npzc),
       nativeMessage, aPoint, button] {
        npzc->SynthesizeNativeMouseEvent(nativeMessage, aPoint.x, aPoint.y,
                                         button);
      });
  return NS_OK;
}

nsresult nsWindow::SynthesizeNativeMouseMove(LayoutDeviceIntPoint aPoint,
                                             nsIObserver* aObserver) {
  return SynthesizeNativeMouseEvent(
      aPoint, NativeMouseMessage::Move, MouseButton::eNotPressed,
      nsIWidget::Modifiers::NO_MODIFIERS, aObserver);
}

bool nsWindow::WidgetPaintsBackground() {
  return StaticPrefs::android_widget_paints_background();
}

bool nsWindow::NeedsPaint() {
  auto lvs(mLayerViewSupport.Access());
  if (!lvs || lvs->CompositorPaused() || !GetLayerManager(nullptr)) {
    return false;
  }

  return nsIWidget::NeedsPaint();
}

void nsWindow::ConfigureAPZControllerThread() {
  nsCOMPtr<nsISerialEventTarget> thread = mozilla::GetAndroidUiThread();
  APZThreadUtils::SetControllerThread(thread);
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
  if (::mozilla::jni::NativeWeakPtr<LayerViewSupport>::Accessor lvs{
          mLayerViewSupport.Access()}) {
    lvs->RecvToolbarAnimatorMessage(aMessage);
  }
}

void nsWindow::UpdateRootFrameMetrics(const ScreenPoint& aScrollOffset,
                                      const CSSToScreenScale& aZoom) {
  MOZ_ASSERT(AndroidBridge::IsJavaUiThread());
  if (::mozilla::jni::NativeWeakPtr<LayerViewSupport>::Accessor lvs{
          mLayerViewSupport.Access()}) {
    const auto& compositor = lvs->GetJavaCompositor();
    mContentDocumentDisplayed = true;
    compositor->UpdateRootFrameMetrics(aScrollOffset.x, aScrollOffset.y,
                                       aZoom.scale);
  }
}

void nsWindow::RecvScreenPixels(Shmem&& aMem, const ScreenIntSize& aSize,
                                bool aNeedsYFlip) {
  MOZ_ASSERT(AndroidBridge::IsJavaUiThread());
  if (::mozilla::jni::NativeWeakPtr<LayerViewSupport>::Accessor lvs{
          mLayerViewSupport.Access()}) {
    lvs->RecvScreenPixels(std::move(aMem), aSize, aNeedsYFlip);
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
