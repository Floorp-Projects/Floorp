/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "HeadlessWidget.h"
#include "HeadlessCompositorWidget.h"
#include "Layers.h"
#include "BasicLayers.h"
#include "BasicEvents.h"
#include "MouseEvents.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/TextEvents.h"
#include "mozilla/widget/HeadlessWidgetTypes.h"
#include "mozilla/widget/PlatformWidgetTypes.h"
#include "nsIScreen.h"
#include "HeadlessKeyBindings.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;

using mozilla::LogLevel;

#ifdef MOZ_LOGGING

#  include "mozilla/Logging.h"
static mozilla::LazyLogModule sWidgetLog("Widget");
static mozilla::LazyLogModule sWidgetFocusLog("WidgetFocus");
#  define LOG(args) MOZ_LOG(sWidgetLog, mozilla::LogLevel::Debug, args)
#  define LOGFOCUS(args) \
    MOZ_LOG(sWidgetFocusLog, mozilla::LogLevel::Debug, args)

#else

#  define LOG(args)
#  define LOGFOCUS(args)

#endif /* MOZ_LOGGING */

/*static*/
already_AddRefed<nsIWidget> nsIWidget::CreateHeadlessWidget() {
  nsCOMPtr<nsIWidget> widget = new mozilla::widget::HeadlessWidget();
  return widget.forget();
}

namespace mozilla {
namespace widget {

StaticAutoPtr<nsTArray<HeadlessWidget*>> HeadlessWidget::sActiveWindows;

already_AddRefed<HeadlessWidget> HeadlessWidget::GetActiveWindow() {
  if (!sActiveWindows) {
    return nullptr;
  }
  auto length = sActiveWindows->Length();
  if (length == 0) {
    return nullptr;
  }
  RefPtr<HeadlessWidget> widget = sActiveWindows->ElementAt(length - 1);
  return widget.forget();
}

HeadlessWidget::HeadlessWidget()
    : mEnabled(true),
      mVisible(false),
      mDestroyed(false),
      mTopLevel(nullptr),
      mCompositorWidget(nullptr),
      mLastSizeMode(nsSizeMode_Normal),
      mEffectiveSizeMode(nsSizeMode_Normal),
      mRestoreBounds(0, 0, 0, 0) {
  if (!sActiveWindows) {
    sActiveWindows = new nsTArray<HeadlessWidget*>();
    ClearOnShutdown(&sActiveWindows);
  }
}

HeadlessWidget::~HeadlessWidget() {
  LOG(("HeadlessWidget::~HeadlessWidget() [%p]\n", (void*)this));

  Destroy();
}

void HeadlessWidget::Destroy() {
  if (mDestroyed) {
    return;
  }
  LOG(("HeadlessWidget::Destroy [%p]\n", (void*)this));
  mDestroyed = true;

  if (sActiveWindows) {
    int32_t index = sActiveWindows->IndexOf(this);
    if (index != -1) {
      RefPtr<HeadlessWidget> activeWindow = GetActiveWindow();
      sActiveWindows->RemoveElementAt(index);
      // If this is the currently active widget and there's a previously active
      // widget, activate the previous widget.
      RefPtr<HeadlessWidget> previousActiveWindow = GetActiveWindow();
      if (this == activeWindow && previousActiveWindow &&
          previousActiveWindow->mWidgetListener) {
        previousActiveWindow->mWidgetListener->WindowActivated();
      }
    }
  }

  nsBaseWidget::OnDestroy();

  nsBaseWidget::Destroy();
}

nsresult HeadlessWidget::Create(nsIWidget* aParent,
                                nsNativeWidget aNativeParent,
                                const LayoutDeviceIntRect& aRect,
                                nsWidgetInitData* aInitData) {
  MOZ_ASSERT(!aNativeParent, "No native parents for headless widgets.");

  BaseCreate(nullptr, aInitData);

  mBounds = aRect;
  mRestoreBounds = aRect;

  if (aParent) {
    mTopLevel = aParent->GetTopLevelWidget();
  } else {
    mTopLevel = this;
  }

  return NS_OK;
}

already_AddRefed<nsIWidget> HeadlessWidget::CreateChild(
    const LayoutDeviceIntRect& aRect, nsWidgetInitData* aInitData,
    bool aForceUseIWidgetParent) {
  nsCOMPtr<nsIWidget> widget = nsIWidget::CreateHeadlessWidget();
  if (!widget) {
    return nullptr;
  }
  if (NS_FAILED(widget->Create(this, nullptr, aRect, aInitData))) {
    return nullptr;
  }
  return widget.forget();
}

void HeadlessWidget::GetCompositorWidgetInitData(
    mozilla::widget::CompositorWidgetInitData* aInitData) {
  *aInitData =
      mozilla::widget::HeadlessCompositorWidgetInitData(GetClientSize());
}

nsIWidget* HeadlessWidget::GetTopLevelWidget() { return mTopLevel; }

void HeadlessWidget::RaiseWindow() {
  MOZ_ASSERT(mTopLevel == this || mWindowType == eWindowType_dialog ||
                 mWindowType == eWindowType_sheet,
             "Raising a non-toplevel window.");

  // Do nothing if this is the currently active window.
  RefPtr<HeadlessWidget> activeWindow = GetActiveWindow();
  if (activeWindow == this) {
    return;
  }

  // Raise the window to the top of the stack.
  nsWindowZ placement = nsWindowZTop;
  nsCOMPtr<nsIWidget> actualBelow;
  if (mWidgetListener)
    mWidgetListener->ZLevelChanged(true, &placement, nullptr,
                                   getter_AddRefs(actualBelow));

  // Deactivate the last active window.
  if (activeWindow && activeWindow->mWidgetListener) {
    activeWindow->mWidgetListener->WindowDeactivated();
  }

  // Remove this window if it's already tracked.
  int32_t index = sActiveWindows->IndexOf(this);
  if (index != -1) {
    sActiveWindows->RemoveElementAt(index);
  }

  // Activate this window.
  sActiveWindows->AppendElement(this);
  if (mWidgetListener) mWidgetListener->WindowActivated();
}

void HeadlessWidget::Show(bool aState) {
  mVisible = aState;

  LOG(("HeadlessWidget::Show [%p] state %d\n", (void*)this, aState));

  // Top-level window and dialogs are activated/raised when shown.
  if (aState && (mTopLevel == this || mWindowType == eWindowType_dialog ||
                 mWindowType == eWindowType_sheet)) {
    RaiseWindow();
  }

  ApplySizeModeSideEffects();
}

bool HeadlessWidget::IsVisible() const { return mVisible; }

nsresult HeadlessWidget::SetFocus(bool aRaise) {
  LOGFOCUS(("  SetFocus %d [%p]\n", aRaise, (void*)this));

  // aRaise == true means we request activation of our toplevel window.
  if (aRaise) {
    HeadlessWidget* topLevel = (HeadlessWidget*)GetTopLevelWidget();

    // The toplevel only becomes active if it's currently visible; otherwise, it
    // will be activated anyway when it's shown.
    if (topLevel->IsVisible()) topLevel->RaiseWindow();
  }
  return NS_OK;
}

void HeadlessWidget::Enable(bool aState) { mEnabled = aState; }

bool HeadlessWidget::IsEnabled() const { return mEnabled; }

void HeadlessWidget::Move(double aX, double aY) {
  LOG(("HeadlessWidget::Move [%p] %f %f\n", (void*)this, aX, aY));

  double scale =
      BoundsUseDesktopPixels() ? GetDesktopToDeviceScale().scale : 1.0;
  int32_t x = NSToIntRound(aX * scale);
  int32_t y = NSToIntRound(aY * scale);

  if (mWindowType == eWindowType_toplevel ||
      mWindowType == eWindowType_dialog) {
    SetSizeMode(nsSizeMode_Normal);
  }

  // Since a popup window's x/y coordinates are in relation to
  // the parent, the parent might have moved so we always move a
  // popup window.
  if (mBounds.IsEqualXY(x, y) && mWindowType != eWindowType_popup) {
    return;
  }

  mBounds.MoveTo(x, y);
  NotifyRollupGeometryChange();
}

LayoutDeviceIntPoint HeadlessWidget::WidgetToScreenOffset() {
  return mTopLevel->GetBounds().TopLeft();
}

LayerManager* HeadlessWidget::GetLayerManager(
    PLayerTransactionChild* aShadowManager, LayersBackend aBackendHint,
    LayerManagerPersistence aPersistence) {
  return nsBaseWidget::GetLayerManager(aShadowManager, aBackendHint,
                                       aPersistence);
}

void HeadlessWidget::SetCompositorWidgetDelegate(
    CompositorWidgetDelegate* delegate) {
  if (delegate) {
    mCompositorWidget = delegate->AsHeadlessCompositorWidget();
    MOZ_ASSERT(mCompositorWidget,
               "HeadlessWidget::SetCompositorWidgetDelegate called with a "
               "non-HeadlessCompositorWidget");
  } else {
    mCompositorWidget = nullptr;
  }
}

void HeadlessWidget::Resize(double aWidth, double aHeight, bool aRepaint) {
  int32_t width = NSToIntRound(aWidth);
  int32_t height = NSToIntRound(aHeight);
  ConstrainSize(&width, &height);
  mBounds.SizeTo(LayoutDeviceIntSize(width, height));

  if (mCompositorWidget) {
    mCompositorWidget->NotifyClientSizeChanged(
        LayoutDeviceIntSize(mBounds.Width(), mBounds.Height()));
  }
  if (mWidgetListener) {
    mWidgetListener->WindowResized(this, mBounds.Width(), mBounds.Height());
  }
  if (mAttachedWidgetListener) {
    mAttachedWidgetListener->WindowResized(this, mBounds.Width(),
                                           mBounds.Height());
  }
}

void HeadlessWidget::Resize(double aX, double aY, double aWidth, double aHeight,
                            bool aRepaint) {
  if (!mBounds.IsEqualXY(aX, aY)) {
    NotifyWindowMoved(aX, aY);
  }
  return Resize(aWidth, aHeight, aRepaint);
}

void HeadlessWidget::SetSizeMode(nsSizeMode aMode) {
  LOG(("HeadlessWidget::SetSizeMode [%p] %d\n", (void*)this, aMode));

  if (aMode == mSizeMode) {
    return;
  }

  nsBaseWidget::SetSizeMode(aMode);

  // Normally in real widget backends a window event would be triggered that
  // would cause the window manager to handle resizing the window. In headless
  // the window must manually be resized.
  ApplySizeModeSideEffects();
}

void HeadlessWidget::ApplySizeModeSideEffects() {
  if (!mVisible || mEffectiveSizeMode == mSizeMode) {
    return;
  }

  if (mEffectiveSizeMode == nsSizeMode_Normal) {
    // Store the last normal size bounds so it can be restored when entering
    // normal mode again.
    mRestoreBounds = mBounds;
  }

  switch (mSizeMode) {
    case nsSizeMode_Normal: {
      Resize(mRestoreBounds.X(), mRestoreBounds.Y(), mRestoreBounds.Width(),
             mRestoreBounds.Height(), false);
      break;
    }
    case nsSizeMode_Minimized:
      break;
    case nsSizeMode_Maximized: {
      nsCOMPtr<nsIScreen> screen = GetWidgetScreen();
      if (screen) {
        int32_t left, top, width, height;
        if (NS_SUCCEEDED(
                screen->GetRectDisplayPix(&left, &top, &width, &height))) {
          Resize(0, 0, width, height, true);
        }
      }
      break;
    }
    case nsSizeMode_Fullscreen:
      // This will take care of resizing the window.
      nsBaseWidget::InfallibleMakeFullScreen(true);
      break;
    default:
      break;
  }

  mEffectiveSizeMode = mSizeMode;
}

nsresult HeadlessWidget::MakeFullScreen(bool aFullScreen,
                                        nsIScreen* aTargetScreen) {
  // Directly update the size mode here so a later call SetSizeMode does
  // nothing.
  if (aFullScreen) {
    if (mSizeMode != nsSizeMode_Fullscreen) {
      mLastSizeMode = mSizeMode;
    }
    mSizeMode = nsSizeMode_Fullscreen;
  } else {
    mSizeMode = mLastSizeMode;
  }

  // Notify the listener first so size mode change events are triggered before
  // resize events.
  if (mWidgetListener) {
    mWidgetListener->SizeModeChanged(mSizeMode);
    mWidgetListener->FullscreenChanged(aFullScreen);
  }

  // Real widget backends don't seem to follow a common approach for
  // when and how many resize events are triggered during fullscreen
  // transitions. InfallibleMakeFullScreen will trigger a resize, but it
  // will be ignored if still transitioning to fullscreen, so it must be
  // triggered on the next tick.
  RefPtr<HeadlessWidget> self(this);
  nsCOMPtr<nsIScreen> targetScreen(aTargetScreen);
  NS_DispatchToCurrentThread(NS_NewRunnableFunction(
      "HeadlessWidget::MakeFullScreen",
      [self, targetScreen, aFullScreen]() -> void {
        self->InfallibleMakeFullScreen(aFullScreen, targetScreen);
      }));

  return NS_OK;
}

nsresult HeadlessWidget::AttachNativeKeyEvent(WidgetKeyboardEvent& aEvent) {
  HeadlessKeyBindings& bindings = HeadlessKeyBindings::GetInstance();
  return bindings.AttachNativeKeyEvent(aEvent);
}

void HeadlessWidget::GetEditCommands(NativeKeyBindingsType aType,
                                     const WidgetKeyboardEvent& aEvent,
                                     nsTArray<CommandInt>& aCommands) {
  // Validate the arguments.
  nsIWidget::GetEditCommands(aType, aEvent, aCommands);

  HeadlessKeyBindings& bindings = HeadlessKeyBindings::GetInstance();
  bindings.GetEditCommands(aType, aEvent, aCommands);
}

nsresult HeadlessWidget::DispatchEvent(WidgetGUIEvent* aEvent,
                                       nsEventStatus& aStatus) {
#ifdef DEBUG
  debug_DumpEvent(stdout, aEvent->mWidget, aEvent, "HeadlessWidget", 0);
#endif

  aStatus = nsEventStatus_eIgnore;

  if (mAttachedWidgetListener) {
    aStatus = mAttachedWidgetListener->HandleEvent(aEvent, mUseAttachedEvents);
  } else if (mWidgetListener) {
    aStatus = mWidgetListener->HandleEvent(aEvent, mUseAttachedEvents);
  }

  return NS_OK;
}

nsresult HeadlessWidget::SynthesizeNativeMouseEvent(LayoutDeviceIntPoint aPoint,
                                                    uint32_t aNativeMessage,
                                                    uint32_t aModifierFlags,
                                                    nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "mouseevent");
  EventMessage msg;
  switch (aNativeMessage) {
    case MOZ_HEADLESS_MOUSE_MOVE:
      msg = eMouseMove;
      break;
    case MOZ_HEADLESS_MOUSE_DOWN:
      msg = eMouseDown;
      break;
    case MOZ_HEADLESS_MOUSE_UP:
      msg = eMouseUp;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported synthesized mouse event");
      return NS_ERROR_UNEXPECTED;
  }
  WidgetMouseEvent event(true, msg, this, WidgetMouseEvent::eReal);
  event.mRefPoint = aPoint - WidgetToScreenOffset();
  if (msg == eMouseDown || msg == eMouseUp) {
    event.mButton = MouseButton::eLeft;
  }
  if (msg == eMouseDown) {
    event.mClickCount = 1;
  }
  event.AssignEventTime(WidgetEventTime());
  DispatchInputEvent(&event);
  return NS_OK;
}

nsresult HeadlessWidget::SynthesizeNativeMouseScrollEvent(
    mozilla::LayoutDeviceIntPoint aPoint, uint32_t aNativeMessage,
    double aDeltaX, double aDeltaY, double aDeltaZ, uint32_t aModifierFlags,
    uint32_t aAdditionalFlags, nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "mousescrollevent");
  printf(">>> DEBUG_ME: Synth: aDeltaY=%f\n", aDeltaY);
  // The various platforms seem to handle scrolling deltas differently,
  // but the following seems to emulate it well enough.
  WidgetWheelEvent event(true, eWheel, this);
  event.mDeltaMode = MOZ_HEADLESS_SCROLL_DELTA_MODE;
  event.mIsNoLineOrPageDelta = true;
  event.mDeltaX = -aDeltaX * MOZ_HEADLESS_SCROLL_MULTIPLIER;
  event.mDeltaY = -aDeltaY * MOZ_HEADLESS_SCROLL_MULTIPLIER;
  event.mDeltaZ = -aDeltaZ * MOZ_HEADLESS_SCROLL_MULTIPLIER;
  event.mRefPoint = aPoint - WidgetToScreenOffset();
  event.AssignEventTime(WidgetEventTime());
  DispatchInputEvent(&event);
  return NS_OK;
}

nsresult HeadlessWidget::SynthesizeNativeTouchPoint(
    uint32_t aPointerId, TouchPointerState aPointerState,
    LayoutDeviceIntPoint aPoint, double aPointerPressure,
    uint32_t aPointerOrientation, nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "touchpoint");

  MOZ_ASSERT(NS_IsMainThread());
  if (aPointerState == TOUCH_HOVER) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!mSynthesizedTouchInput) {
    mSynthesizedTouchInput = MakeUnique<MultiTouchInput>();
  }

  LayoutDeviceIntPoint pointInWindow = aPoint - WidgetToScreenOffset();
  MultiTouchInput inputToDispatch = UpdateSynthesizedTouchState(
      mSynthesizedTouchInput.get(), PR_IntervalNow(), TimeStamp::Now(),
      aPointerId, aPointerState, pointInWindow, aPointerPressure,
      aPointerOrientation);
  DispatchTouchInput(inputToDispatch);
  return NS_OK;
}

}  // namespace widget
}  // namespace mozilla
