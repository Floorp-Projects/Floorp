/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "gfxPlatform.h"
#include "nsRefreshDriver.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/layers/APZChild.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/NativeKeyBindingsType.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEventDispatcher.h"
#include "mozilla/TextEvents.h"
#include "mozilla/Unused.h"
#include "PuppetWidget.h"
#include "nsContentUtils.h"
#include "nsIWidgetListener.h"
#include "imgIContainer.h"
#include "nsView.h"
#include "nsXPLookAndFeel.h"
#include "nsPrintfCString.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::widget;

static void InvalidateRegion(nsIWidget* aWidget,
                             const LayoutDeviceIntRegion& aRegion) {
  for (auto iter = aRegion.RectIter(); !iter.Done(); iter.Next()) {
    aWidget->Invalidate(iter.Get());
  }
}

/*static*/
already_AddRefed<nsIWidget> nsIWidget::CreatePuppetWidget(
    BrowserChild* aBrowserChild) {
  MOZ_ASSERT(!aBrowserChild || nsIWidget::UsePuppetWidgets(),
             "PuppetWidgets not allowed in this configuration");

  nsCOMPtr<nsIWidget> widget = new PuppetWidget(aBrowserChild);
  return widget.forget();
}

namespace mozilla {
namespace widget {

static bool IsPopup(const widget::InitData* aInitData) {
  return aInitData && aInitData->mWindowType == WindowType::Popup;
}

static bool MightNeedIMEFocus(const widget::InitData* aInitData) {
  // In the puppet-widget world, popup widgets are just dummies and
  // shouldn't try to mess with IME state.
#ifdef MOZ_CROSS_PROCESS_IME
  return !IsPopup(aInitData);
#else
  return false;
#endif
}

NS_IMPL_ISUPPORTS_INHERITED(PuppetWidget, nsBaseWidget,
                            TextEventDispatcherListener)

PuppetWidget::PuppetWidget(BrowserChild* aBrowserChild)
    : mBrowserChild(aBrowserChild),
      mMemoryPressureObserver(nullptr),
      mEnabled(false),
      mVisible(false),
      mSizeMode(nsSizeMode_Normal),
      mNeedIMEStateInit(false),
      mIgnoreCompositionEvents(false) {
  // Setting 'Unknown' means "not yet cached".
  mInputContext.mIMEState.mEnabled = IMEEnabled::Unknown;
}

PuppetWidget::~PuppetWidget() { Destroy(); }

void PuppetWidget::InfallibleCreate(nsIWidget* aParent,
                                    nsNativeWidget aNativeParent,
                                    const LayoutDeviceIntRect& aRect,
                                    widget::InitData* aInitData) {
  MOZ_ASSERT(!aNativeParent, "got a non-Puppet native parent");

  BaseCreate(nullptr, aInitData);

  mBounds = aRect;
  mEnabled = true;
  mVisible = true;

  mDrawTarget = gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
      IntSize(1, 1), SurfaceFormat::B8G8R8A8);

  mNeedIMEStateInit = MightNeedIMEFocus(aInitData);

  PuppetWidget* parent = static_cast<PuppetWidget*>(aParent);
  if (parent) {
    parent->SetChild(this);
    mWindowRenderer = parent->GetWindowRenderer();
  } else {
    Resize(mBounds.X(), mBounds.Y(), mBounds.Width(), mBounds.Height(), false);
  }
  mMemoryPressureObserver = MemoryPressureObserver::Create(this);
}

nsresult PuppetWidget::Create(nsIWidget* aParent, nsNativeWidget aNativeParent,
                              const LayoutDeviceIntRect& aRect,
                              widget::InitData* aInitData) {
  InfallibleCreate(aParent, aNativeParent, aRect, aInitData);
  return NS_OK;
}

void PuppetWidget::InitIMEState() {
  MOZ_ASSERT(mBrowserChild);
  if (mNeedIMEStateInit) {
    mContentCache.Clear();
    mBrowserChild->SendUpdateContentCache(mContentCache);
    mIMENotificationRequestsOfParent = IMENotificationRequests();
    mNeedIMEStateInit = false;
  }
}

already_AddRefed<nsIWidget> PuppetWidget::CreateChild(
    const LayoutDeviceIntRect& aRect, widget::InitData* aInitData,
    bool aForceUseIWidgetParent) {
  bool isPopup = IsPopup(aInitData);
  nsCOMPtr<nsIWidget> widget = nsIWidget::CreatePuppetWidget(mBrowserChild);
  return ((widget && NS_SUCCEEDED(widget->Create(isPopup ? nullptr : this,
                                                 nullptr, aRect, aInitData)))
              ? widget.forget()
              : nullptr);
}

void PuppetWidget::Destroy() {
  if (mOnDestroyCalled) {
    return;
  }
  mOnDestroyCalled = true;

  Base::OnDestroy();
  Base::Destroy();
  if (mMemoryPressureObserver) {
    mMemoryPressureObserver->Unregister();
    mMemoryPressureObserver = nullptr;
  }
  mChild = nullptr;
  if (mWindowRenderer) {
    mWindowRenderer->Destroy();
  }
  mWindowRenderer = nullptr;
  mBrowserChild = nullptr;
}

void PuppetWidget::Show(bool aState) {
  NS_ASSERTION(mEnabled,
               "does it make sense to Show()/Hide() a disabled widget?");

  bool wasVisible = mVisible;
  mVisible = aState;

  if (mChild) {
    mChild->mVisible = aState;
  }

  if (!wasVisible && mVisible) {
    // The previously attached widget listener is handy if
    // we're transitioning from page to page without dropping
    // layers (since we'll continue to show the old layers
    // associated with that old widget listener). If the
    // PuppetWidget was hidden, those layers are dropped,
    // so the previously attached widget listener is really
    // of no use anymore (and is actually actively harmful - see
    // bug 1323586).
    mPreviouslyAttachedWidgetListener = nullptr;
    Resize(mBounds.Width(), mBounds.Height(), false);
    Invalidate(mBounds);
  }
}

void PuppetWidget::Resize(double aWidth, double aHeight, bool aRepaint) {
  LayoutDeviceIntRect oldBounds = mBounds;
  mBounds.SizeTo(
      LayoutDeviceIntSize(NSToIntRound(aWidth), NSToIntRound(aHeight)));

  if (mChild) {
    mChild->Resize(aWidth, aHeight, aRepaint);
    return;
  }

  // XXX: roc says that |aRepaint| dictates whether or not to
  // invalidate the expanded area
  if (oldBounds.Size() < mBounds.Size() && aRepaint) {
    LayoutDeviceIntRegion dirty(mBounds);
    dirty.Sub(dirty, oldBounds);
    InvalidateRegion(this, dirty);
  }

  // call WindowResized() on both the current listener, and possibly
  // also the previous one if we're in a state where we're drawing that one
  // because the current one is paint suppressed
  if (!oldBounds.IsEqualEdges(mBounds) && mAttachedWidgetListener) {
    if (GetCurrentWidgetListener() &&
        GetCurrentWidgetListener() != mAttachedWidgetListener) {
      GetCurrentWidgetListener()->WindowResized(this, mBounds.Width(),
                                                mBounds.Height());
    }
    mAttachedWidgetListener->WindowResized(this, mBounds.Width(),
                                           mBounds.Height());
  }
}

void PuppetWidget::SetFocus(Raise aRaise, CallerType aCallerType) {
  if (aRaise == Raise::Yes && mBrowserChild) {
    mBrowserChild->SendRequestFocus(true, aCallerType);
  }
}

void PuppetWidget::Invalidate(const LayoutDeviceIntRect& aRect) {
#ifdef DEBUG
  debug_DumpInvalidate(stderr, this, &aRect, "PuppetWidget", 0);
#endif

  if (mChild) {
    mChild->Invalidate(aRect);
    return;
  }

  if (mBrowserChild && !aRect.IsEmpty() && !mWidgetPaintTask.IsPending()) {
    mWidgetPaintTask = new WidgetPaintTask(this);
    nsCOMPtr<nsIRunnable> event(mWidgetPaintTask.get());
    SchedulerGroup::Dispatch(event.forget());
  }
}

mozilla::LayoutDeviceToLayoutDeviceMatrix4x4
PuppetWidget::WidgetToTopLevelWidgetTransform() {
  if (!GetOwningBrowserChild()) {
    NS_WARNING("PuppetWidget without Tab does not have transform information.");
    return mozilla::LayoutDeviceToLayoutDeviceMatrix4x4();
  }
  return GetOwningBrowserChild()->GetChildToParentConversionMatrix();
}

void PuppetWidget::InitEvent(WidgetGUIEvent& aEvent,
                             LayoutDeviceIntPoint* aPoint) {
  if (nullptr == aPoint) {
    aEvent.mRefPoint = LayoutDeviceIntPoint(0, 0);
  } else {
    // use the point override if provided
    aEvent.mRefPoint = *aPoint;
  }
}

nsresult PuppetWidget::DispatchEvent(WidgetGUIEvent* aEvent,
                                     nsEventStatus& aStatus) {
#ifdef DEBUG
  debug_DumpEvent(stdout, aEvent->mWidget, aEvent, "PuppetWidget", 0);
#endif

  MOZ_ASSERT(!mChild || mChild->mWindowType == WindowType::Popup,
             "Unexpected event dispatch!");

  MOZ_ASSERT(!aEvent->AsKeyboardEvent() ||
                 aEvent->mFlags.mIsSynthesizedForTests ||
                 aEvent->AsKeyboardEvent()->AreAllEditCommandsInitialized(),
             "Non-sysnthesized keyboard events should have edit commands for "
             "all types "
             "before dispatched");

  if (aEvent->mClass == eCompositionEventClass) {
    // If we've already requested to commit/cancel the latest composition,
    // TextComposition for the old composition has been destroyed.  Then,
    // the DOM tree needs to listen to next eCompositionStart and its
    // following events.  So, until we meet new eCompositionStart, let's
    // discard all unnecessary composition events here.
    if (mIgnoreCompositionEvents) {
      if (aEvent->mMessage != eCompositionStart) {
        aStatus = nsEventStatus_eIgnore;
        return NS_OK;
      }
      // Now, we receive new eCompositionStart.  Let's restart to handle
      // composition in this process.
      mIgnoreCompositionEvents = false;
    }
    // Store the latest native IME context of parent process's widget or
    // TextEventDispatcher if it's in this process.
    WidgetCompositionEvent* compositionEvent = aEvent->AsCompositionEvent();
#ifdef DEBUG
    if (mNativeIMEContext.IsValid() &&
        mNativeIMEContext != compositionEvent->mNativeIMEContext) {
      RefPtr<TextComposition> composition =
          IMEStateManager::GetTextCompositionFor(this);
      MOZ_ASSERT(
          !composition,
          "When there is composition caused by old native IME context, "
          "composition events caused by different native IME context are not "
          "allowed");
    }
#endif  // #ifdef DEBUG
    mNativeIMEContext = compositionEvent->mNativeIMEContext;
    mContentCache.OnCompositionEvent(*compositionEvent);
  }

  // If the event is a composition event or a keyboard event, it should be
  // dispatched with TextEventDispatcher if we could do that with current
  // design.  However, we cannot do that without big changes and the behavior
  // is not so complicated for now.  Therefore, we should just notify it
  // of dispatching events and TextEventDispatcher should emulate the state
  // with events here.
  if (aEvent->mClass == eCompositionEventClass ||
      aEvent->mClass == eKeyboardEventClass) {
    TextEventDispatcher* dispatcher = GetTextEventDispatcher();
    // However, if the event is being dispatched by the text event dispatcher
    // or, there is native text event dispatcher listener, that means that
    // native text input event handler is in this process like on Android,
    // and the event is not synthesized for tests, the event is coming from
    // the TextEventDispatcher.  In these cases, we shouldn't notify
    // TextEventDispatcher of dispatching the event.
    if (!dispatcher->IsDispatchingEvent() &&
        !(mNativeTextEventDispatcherListener &&
          !aEvent->mFlags.mIsSynthesizedForTests)) {
      DebugOnly<nsresult> rv =
          dispatcher->BeginInputTransactionFor(aEvent, this);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "The text event dispatcher should always succeed to start input "
          "transaction for the event");
    }
  }

  aStatus = nsEventStatus_eIgnore;

  if (GetCurrentWidgetListener()) {
    aStatus =
        GetCurrentWidgetListener()->HandleEvent(aEvent, mUseAttachedEvents);
  }

  return NS_OK;
}

nsIWidget::ContentAndAPZEventStatus PuppetWidget::DispatchInputEvent(
    WidgetInputEvent* aEvent) {
  ContentAndAPZEventStatus status;
  if (!AsyncPanZoomEnabled()) {
    DispatchEvent(aEvent, status.mContentStatus);
    return status;
  }

  if (!mBrowserChild) {
    return status;
  }

  switch (aEvent->mClass) {
    case eWheelEventClass:
      Unused << mBrowserChild->SendDispatchWheelEvent(*aEvent->AsWheelEvent());
      break;
    case eMouseEventClass:
      Unused << mBrowserChild->SendDispatchMouseEvent(*aEvent->AsMouseEvent());
      break;
    case eKeyboardEventClass:
      Unused << mBrowserChild->SendDispatchKeyboardEvent(
          *aEvent->AsKeyboardEvent());
      break;
    case eTouchEventClass:
      Unused << mBrowserChild->SendDispatchTouchEvent(*aEvent->AsTouchEvent());
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("unsupported event type");
  }

  return status;
}

nsresult PuppetWidget::SynthesizeNativeKeyEvent(
    int32_t aNativeKeyboardLayout, int32_t aNativeKeyCode,
    uint32_t aModifierFlags, const nsAString& aCharacters,
    const nsAString& aUnmodifiedCharacters, nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "keyevent");
  if (!mBrowserChild) {
    return NS_ERROR_FAILURE;
  }
  mBrowserChild->SendSynthesizeNativeKeyEvent(
      aNativeKeyboardLayout, aNativeKeyCode, aModifierFlags, aCharacters,
      aUnmodifiedCharacters, notifier.SaveObserver());
  return NS_OK;
}

nsresult PuppetWidget::SynthesizeNativeMouseEvent(
    mozilla::LayoutDeviceIntPoint aPoint, NativeMouseMessage aNativeMessage,
    MouseButton aButton, nsIWidget::Modifiers aModifierFlags,
    nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "mouseevent");
  if (!mBrowserChild) {
    return NS_ERROR_FAILURE;
  }
  mBrowserChild->SendSynthesizeNativeMouseEvent(
      aPoint, static_cast<uint32_t>(aNativeMessage),
      static_cast<int16_t>(aButton), static_cast<uint32_t>(aModifierFlags),
      notifier.SaveObserver());
  return NS_OK;
}

nsresult PuppetWidget::SynthesizeNativeMouseMove(
    mozilla::LayoutDeviceIntPoint aPoint, nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "mousemove");
  if (!mBrowserChild) {
    return NS_ERROR_FAILURE;
  }
  mBrowserChild->SendSynthesizeNativeMouseMove(aPoint, notifier.SaveObserver());
  return NS_OK;
}

nsresult PuppetWidget::SynthesizeNativeMouseScrollEvent(
    mozilla::LayoutDeviceIntPoint aPoint, uint32_t aNativeMessage,
    double aDeltaX, double aDeltaY, double aDeltaZ, uint32_t aModifierFlags,
    uint32_t aAdditionalFlags, nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "mousescrollevent");
  if (!mBrowserChild) {
    return NS_ERROR_FAILURE;
  }
  mBrowserChild->SendSynthesizeNativeMouseScrollEvent(
      aPoint, aNativeMessage, aDeltaX, aDeltaY, aDeltaZ, aModifierFlags,
      aAdditionalFlags, notifier.SaveObserver());
  return NS_OK;
}

nsresult PuppetWidget::SynthesizeNativeTouchPoint(
    uint32_t aPointerId, TouchPointerState aPointerState,
    LayoutDeviceIntPoint aPoint, double aPointerPressure,
    uint32_t aPointerOrientation, nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "touchpoint");
  if (!mBrowserChild) {
    return NS_ERROR_FAILURE;
  }
  mBrowserChild->SendSynthesizeNativeTouchPoint(
      aPointerId, aPointerState, aPoint, aPointerPressure, aPointerOrientation,
      notifier.SaveObserver());
  return NS_OK;
}

nsresult PuppetWidget::SynthesizeNativeTouchPadPinch(
    TouchpadGesturePhase aEventPhase, float aScale, LayoutDeviceIntPoint aPoint,
    int32_t aModifierFlags) {
  if (!mBrowserChild) {
    return NS_ERROR_FAILURE;
  }
  mBrowserChild->SendSynthesizeNativeTouchPadPinch(aEventPhase, aScale, aPoint,
                                                   aModifierFlags);
  return NS_OK;
}

nsresult PuppetWidget::SynthesizeNativeTouchTap(LayoutDeviceIntPoint aPoint,
                                                bool aLongTap,
                                                nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "touchtap");
  if (!mBrowserChild) {
    return NS_ERROR_FAILURE;
  }
  mBrowserChild->SendSynthesizeNativeTouchTap(aPoint, aLongTap,
                                              notifier.SaveObserver());
  return NS_OK;
}

nsresult PuppetWidget::ClearNativeTouchSequence(nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "cleartouch");
  if (!mBrowserChild) {
    return NS_ERROR_FAILURE;
  }
  mBrowserChild->SendClearNativeTouchSequence(notifier.SaveObserver());
  return NS_OK;
}

nsresult PuppetWidget::SynthesizeNativePenInput(
    uint32_t aPointerId, TouchPointerState aPointerState,
    LayoutDeviceIntPoint aPoint, double aPressure, uint32_t aRotation,
    int32_t aTiltX, int32_t aTiltY, int32_t aButton, nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "peninput");
  if (!mBrowserChild) {
    return NS_ERROR_FAILURE;
  }
  mBrowserChild->SendSynthesizeNativePenInput(
      aPointerId, aPointerState, aPoint, aPressure, aRotation, aTiltX, aTiltY,
      aButton, notifier.SaveObserver());
  return NS_OK;
}

nsresult PuppetWidget::SynthesizeNativeTouchpadDoubleTap(
    LayoutDeviceIntPoint aPoint, uint32_t aModifierFlags) {
  if (!mBrowserChild) {
    return NS_ERROR_FAILURE;
  }
  mBrowserChild->SendSynthesizeNativeTouchpadDoubleTap(aPoint, aModifierFlags);
  return NS_OK;
}

nsresult PuppetWidget::SynthesizeNativeTouchpadPan(
    TouchpadGesturePhase aEventPhase, LayoutDeviceIntPoint aPoint,
    double aDeltaX, double aDeltaY, int32_t aModifierFlags,
    nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "touchpadpanevent");
  if (!mBrowserChild) {
    return NS_ERROR_FAILURE;
  }
  mBrowserChild->SendSynthesizeNativeTouchpadPan(aEventPhase, aPoint, aDeltaX,
                                                 aDeltaY, aModifierFlags,
                                                 notifier.SaveObserver());
  return NS_OK;
}

void PuppetWidget::LockNativePointer() {
  if (!mBrowserChild) {
    return;
  }
  mBrowserChild->SendLockNativePointer();
}

void PuppetWidget::UnlockNativePointer() {
  if (!mBrowserChild) {
    return;
  }
  mBrowserChild->SendUnlockNativePointer();
}

void PuppetWidget::SetConfirmedTargetAPZC(
    uint64_t aInputBlockId,
    const nsTArray<ScrollableLayerGuid>& aTargets) const {
  if (mBrowserChild) {
    mBrowserChild->SetTargetAPZC(aInputBlockId, aTargets);
  }
}

void PuppetWidget::UpdateZoomConstraints(
    const uint32_t& aPresShellId, const ScrollableLayerGuid::ViewID& aViewId,
    const Maybe<ZoomConstraints>& aConstraints) {
  if (mBrowserChild) {
    mBrowserChild->DoUpdateZoomConstraints(aPresShellId, aViewId, aConstraints);
  }
}

bool PuppetWidget::AsyncPanZoomEnabled() const {
  return mBrowserChild && mBrowserChild->AsyncPanZoomEnabled();
}

bool PuppetWidget::GetEditCommands(NativeKeyBindingsType aType,
                                   const WidgetKeyboardEvent& aEvent,
                                   nsTArray<CommandInt>& aCommands) {
  MOZ_ASSERT(!aEvent.mFlags.mIsSynthesizedForTests);
  // Validate the arguments.
  if (NS_WARN_IF(!nsIWidget::GetEditCommands(aType, aEvent, aCommands))) {
    return false;
  }
  if (NS_WARN_IF(!mBrowserChild)) {
    return false;
  }
  mBrowserChild->RequestEditCommands(aType, aEvent, aCommands);
  return true;
}

WindowRenderer* PuppetWidget::GetWindowRenderer() {
  if (!mWindowRenderer) {
    if (XRE_IsParentProcess()) {
      // On the parent process there is no CompositorBridgeChild which confuses
      // some layers code, so we use basic layers instead. Note that we create
      mWindowRenderer = new FallbackRenderer;
      return mWindowRenderer;
    }

    // If we know for sure that the parent side of this BrowserChild is not
    // connected to the compositor, we don't want to use a "remote" layer
    // manager like WebRender or Client. Instead we use a Basic one which
    // can do drawing in this process.
    MOZ_ASSERT(!mBrowserChild ||
               mBrowserChild->IsLayersConnected() != Some(true));
    mWindowRenderer = CreateFallbackRenderer();
  }

  return mWindowRenderer;
}

bool PuppetWidget::CreateRemoteLayerManager(
    const std::function<bool(WebRenderLayerManager*)>& aInitializeFunc) {
  RefPtr<WebRenderLayerManager> lm = new WebRenderLayerManager(this);
  MOZ_ASSERT(mBrowserChild);

  if (!aInitializeFunc(lm)) {
    return false;
  }

  // Force the old LM to self destruct, otherwise if the reference dangles we
  // could fail to revoke the most recent transaction. We only want to replace
  // it if we successfully create its successor because a partially initialized
  // layer manager is worse than a fully initialized but shutdown layer manager.
  DestroyLayerManager();
  mWindowRenderer = std::move(lm);
  return true;
}

nsresult PuppetWidget::RequestIMEToCommitComposition(bool aCancel) {
  if (!mBrowserChild) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(!Destroyed());

  // There must not be composition which is caused by the PuppetWidget instance.
  if (NS_WARN_IF(!mNativeIMEContext.IsValid())) {
    return NS_OK;
  }

  // We've already requested to commit/cancel composition.
  if (NS_WARN_IF(mIgnoreCompositionEvents)) {
#ifdef DEBUG
    RefPtr<TextComposition> composition =
        IMEStateManager::GetTextCompositionFor(this);
    MOZ_ASSERT(!composition);
#endif  // #ifdef DEBUG
    return NS_OK;
  }

  RefPtr<TextComposition> composition =
      IMEStateManager::GetTextCompositionFor(this);
  // This method shouldn't be called when there is no text composition instance.
  if (NS_WARN_IF(!composition)) {
    return NS_OK;
  }

  MOZ_DIAGNOSTIC_ASSERT(
      composition->IsRequestingCommitOrCancelComposition(),
      "Requesting commit or cancel composition should be requested via "
      "TextComposition instance");

  bool isCommitted = false;
  nsAutoString committedString;
  if (NS_WARN_IF(!mBrowserChild->SendRequestIMEToCommitComposition(
          aCancel, composition->Id(), &isCommitted, &committedString))) {
    return NS_ERROR_FAILURE;
  }

  // If the composition wasn't committed synchronously, we need to wait async
  // composition events for destroying the TextComposition instance.
  if (!isCommitted) {
    return NS_OK;
  }

  // Dispatch eCompositionCommit event.
  WidgetCompositionEvent compositionCommitEvent(true, eCompositionCommit, this);
  InitEvent(compositionCommitEvent, nullptr);
  compositionCommitEvent.mData = committedString;
  nsEventStatus status = nsEventStatus_eIgnore;
  DispatchEvent(&compositionCommitEvent, status);

#ifdef DEBUG
  RefPtr<TextComposition> currentComposition =
      IMEStateManager::GetTextCompositionFor(this);
  MOZ_ASSERT(!currentComposition);
#endif  // #ifdef DEBUG

  // Ignore the following composition events until we receive new
  // eCompositionStart event.
  mIgnoreCompositionEvents = true;

  Unused << mBrowserChild->SendOnEventNeedingAckHandled(
      eCompositionCommitRequestHandled, composition->Id());

  // NOTE: PuppetWidget might be destroyed already.
  return NS_OK;
}

// When this widget caches input context and currently managed by
// IMEStateManager, the cache is valid.
bool PuppetWidget::HaveValidInputContextCache() const {
  return (mInputContext.mIMEState.mEnabled != IMEEnabled::Unknown &&
          IMEStateManager::GetWidgetForActiveInputContext() == this);
}

nsRefreshDriver* PuppetWidget::GetTopLevelRefreshDriver() const {
  if (!mBrowserChild) {
    return nullptr;
  }

  if (PresShell* presShell = mBrowserChild->GetTopLevelPresShell()) {
    return presShell->GetRefreshDriver();
  }

  return nullptr;
}

void PuppetWidget::SetInputContext(const InputContext& aContext,
                                   const InputContextAction& aAction) {
  mInputContext = aContext;
  // Any widget instances cannot cache IME open state because IME open state
  // can be changed by user but native IME may not notify us of changing the
  // open state on some platforms.
  mInputContext.mIMEState.mOpen = IMEState::OPEN_STATE_NOT_SUPPORTED;
  if (!mBrowserChild) {
    return;
  }
  mBrowserChild->SendSetInputContext(aContext, aAction);
}

InputContext PuppetWidget::GetInputContext() {
  // XXX Currently, we don't support retrieving IME open state from child
  //     process.

  // If the cache of input context is valid, we can avoid to use synchronous
  // IPC.
  if (HaveValidInputContextCache()) {
    return mInputContext;
  }

  NS_WARNING("PuppetWidget::GetInputContext() needs to retrieve it with IPC");

  // Don't cache InputContext here because this process isn't managing IME
  // state of the chrome widget.  So, we cannot modify mInputContext when
  // chrome widget is set to new context.
  InputContext context;
  if (mBrowserChild) {
    mBrowserChild->SendGetInputContext(&context.mIMEState);
  }
  return context;
}

NativeIMEContext PuppetWidget::GetNativeIMEContext() {
  return mNativeIMEContext;
}

nsresult PuppetWidget::NotifyIMEOfFocusChange(
    const IMENotification& aIMENotification) {
  MOZ_ASSERT(IMEStateManager::CanSendNotificationToWidget());

  if (!mBrowserChild) {
    return NS_ERROR_FAILURE;
  }

  bool gotFocus = aIMENotification.mMessage == NOTIFY_IME_OF_FOCUS;
  if (gotFocus) {
    // When IME gets focus, we should initialize all information of the
    // content, however, it may fail to get it because the editor may have
    // already been blurred.
    if (NS_WARN_IF(!mContentCache.CacheAll(this, &aIMENotification))) {
      return NS_ERROR_FAILURE;
    }
  } else {
    // When IME loses focus, we don't need to store anything.
    mContentCache.Clear();
  }

  mIMENotificationRequestsOfParent =
      IMENotificationRequests(IMENotificationRequests::NOTIFY_ALL);
  RefPtr<PuppetWidget> self = this;
  mBrowserChild->SendNotifyIMEFocus(mContentCache, aIMENotification)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [self](IMENotificationRequests&& aRequests) {
            self->mIMENotificationRequestsOfParent = aRequests;
            if (TextEventDispatcher* dispatcher =
                    self->GetTextEventDispatcher()) {
              dispatcher->OnWidgetChangeIMENotificationRequests(self);
            }
          },
          [self](mozilla::ipc::ResponseRejectReason&& aReason) {
            NS_WARNING("SendNotifyIMEFocus got rejected.");
          });

  return NS_OK;
}

nsresult PuppetWidget::NotifyIMEOfCompositionUpdate(
    const IMENotification& aIMENotification) {
  MOZ_ASSERT(IMEStateManager::CanSendNotificationToWidget());

  if (NS_WARN_IF(!mBrowserChild)) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(
          !mContentCache.CacheCaretAndTextRects(this, &aIMENotification))) {
    return NS_ERROR_FAILURE;
  }
  mBrowserChild->SendNotifyIMECompositionUpdate(mContentCache,
                                                aIMENotification);
  return NS_OK;
}

nsresult PuppetWidget::NotifyIMEOfTextChange(
    const IMENotification& aIMENotification) {
  MOZ_ASSERT(IMEStateManager::CanSendNotificationToWidget());
  MOZ_ASSERT(aIMENotification.mMessage == NOTIFY_IME_OF_TEXT_CHANGE,
             "Passed wrong notification");

  if (!mBrowserChild) {
    return NS_ERROR_FAILURE;
  }

  // FYI: text change notification is the first notification after
  //      a user operation changes the content.  So, we need to modify
  //      the cache as far as possible here.

  if (NS_WARN_IF(!mContentCache.CacheText(this, &aIMENotification))) {
    return NS_ERROR_FAILURE;
  }

  // BrowserParent doesn't this this to cache.  we don't send the notification
  // if parent process doesn't request NOTIFY_TEXT_CHANGE.
  if (mIMENotificationRequestsOfParent.WantTextChange()) {
    mBrowserChild->SendNotifyIMETextChange(mContentCache, aIMENotification);
  } else {
    mBrowserChild->SendUpdateContentCache(mContentCache);
  }
  return NS_OK;
}

nsresult PuppetWidget::NotifyIMEOfSelectionChange(
    const IMENotification& aIMENotification) {
  MOZ_ASSERT(IMEStateManager::CanSendNotificationToWidget());
  MOZ_ASSERT(aIMENotification.mMessage == NOTIFY_IME_OF_SELECTION_CHANGE,
             "Passed wrong notification");
  if (!mBrowserChild) {
    return NS_ERROR_FAILURE;
  }

  // Note that selection change must be notified after text change if it occurs.
  // Therefore, we don't need to query text content again here.
  if (MOZ_UNLIKELY(!mContentCache.SetSelection(
          this, aIMENotification.mSelectionChangeData))) {
    // If there is no text cache yet, caching text will cache selection too.
    // Therefore, in the case, we don't need to notify IME of selection change
    // right now.
    return NS_OK;
  }

  mBrowserChild->SendNotifyIMESelection(mContentCache, aIMENotification);

  return NS_OK;
}

nsresult PuppetWidget::NotifyIMEOfMouseButtonEvent(
    const IMENotification& aIMENotification) {
  MOZ_ASSERT(IMEStateManager::CanSendNotificationToWidget());
  if (!mBrowserChild) {
    return NS_ERROR_FAILURE;
  }

  bool consumedByIME = false;
  if (!mBrowserChild->SendNotifyIMEMouseButtonEvent(aIMENotification,
                                                    &consumedByIME)) {
    return NS_ERROR_FAILURE;
  }

  return consumedByIME ? NS_SUCCESS_EVENT_CONSUMED : NS_OK;
}

nsresult PuppetWidget::NotifyIMEOfPositionChange(
    const IMENotification& aIMENotification) {
  MOZ_ASSERT(IMEStateManager::CanSendNotificationToWidget());
  if (NS_WARN_IF(!mBrowserChild)) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!mContentCache.CacheEditorRect(this, &aIMENotification))) {
    return NS_ERROR_FAILURE;
  }
  if (NS_WARN_IF(
          !mContentCache.CacheCaretAndTextRects(this, &aIMENotification))) {
    return NS_ERROR_FAILURE;
  }
  if (mIMENotificationRequestsOfParent.WantPositionChanged()) {
    mBrowserChild->SendNotifyIMEPositionChange(mContentCache, aIMENotification);
  } else {
    mBrowserChild->SendUpdateContentCache(mContentCache);
  }
  return NS_OK;
}

struct CursorSurface {
  UniquePtr<char[]> mData;
  IntSize mSize;
};

void PuppetWidget::SetCursor(const Cursor& aCursor) {
  if (!mBrowserChild) {
    return;
  }

  const bool force = mUpdateCursor;
  if (!force && mCursor == aCursor) {
    return;
  }

  bool hasCustomCursor = false;
  Maybe<mozilla::ipc::BigBuffer> customCursorData;
  size_t length = 0;
  IntSize customCursorSize;
  int32_t stride = 0;
  auto format = SurfaceFormat::B8G8R8A8;
  ImageResolution resolution = aCursor.mResolution;
  if (aCursor.IsCustom()) {
    int32_t width = 0, height = 0;
    aCursor.mContainer->GetWidth(&width);
    aCursor.mContainer->GetHeight(&height);
    const int32_t flags =
        imgIContainer::FLAG_SYNC_DECODE | imgIContainer::FLAG_ASYNC_NOTIFY;
    RefPtr<SourceSurface> surface;
    if (width && height &&
        aCursor.mContainer->GetType() == imgIContainer::TYPE_VECTOR) {
      // For vector images, scale to device pixels.
      resolution.ScaleBy(GetDefaultScale().scale);
      resolution.ApplyInverseTo(width, height);
      surface = aCursor.mContainer->GetFrameAtSize(
          {width, height}, imgIContainer::FRAME_CURRENT, flags);
    } else {
      // NOTE(emilio): We get the frame at the full size, ignoring resolution,
      // because we're going to rasterize it, and we'd effectively lose the
      // extra pixels if we rasterized to CustomCursorSize.
      surface =
          aCursor.mContainer->GetFrame(imgIContainer::FRAME_CURRENT, flags);
    }
    if (surface) {
      if (RefPtr<DataSourceSurface> dataSurface = surface->GetDataSurface()) {
        hasCustomCursor = true;
        customCursorData =
            nsContentUtils::GetSurfaceData(*dataSurface, &length, &stride);
        customCursorSize = dataSurface->GetSize();
        format = dataSurface->GetFormat();
      }
    }
  }

  if (!mBrowserChild->SendSetCursor(
          aCursor.mDefaultCursor, hasCustomCursor, std::move(customCursorData),
          customCursorSize.width, customCursorSize.height, resolution.mX,
          resolution.mY, stride, format, aCursor.mHotspotX, aCursor.mHotspotY,
          force)) {
    return;
  }
  mCursor = aCursor;
  mUpdateCursor = false;
}

void PuppetWidget::SetChild(PuppetWidget* aChild) {
  MOZ_ASSERT(this != aChild, "can't parent a widget to itself");
  MOZ_ASSERT(!aChild->mChild,
             "fake widget 'hierarchy' only expected to have one level");

  mChild = aChild;
}

NS_IMETHODIMP
PuppetWidget::WidgetPaintTask::Run() {
  if (mWidget) {
    mWidget->Paint();
  }
  return NS_OK;
}

void PuppetWidget::Paint() {
  if (!GetCurrentWidgetListener()) return;

  mWidgetPaintTask.Revoke();

  RefPtr<PuppetWidget> strongThis(this);

  GetCurrentWidgetListener()->WillPaintWindow(this);

  if (GetCurrentWidgetListener()) {
    GetCurrentWidgetListener()->DidPaintWindow();
  }
}

void PuppetWidget::PaintNowIfNeeded() {
  if (IsVisible() && mWidgetPaintTask.IsPending()) {
    Paint();
  }
}

void PuppetWidget::OnMemoryPressure(layers::MemoryPressureReason aWhy) {
  if (aWhy != MemoryPressureReason::LOW_MEMORY_ONGOING && !mVisible &&
      mWindowRenderer && mWindowRenderer->AsWebRender() &&
      XRE_IsContentProcess()) {
    mWindowRenderer->AsWebRender()->ClearCachedResources();
  }
}

bool PuppetWidget::NeedsPaint() {
  // e10s popups are handled by the parent process, so never should be painted
  // here
  return mVisible;
}

LayoutDeviceIntPoint PuppetWidget::GetChromeOffset() {
  if (!GetOwningBrowserChild()) {
    NS_WARNING("PuppetWidget without Tab does not have chrome information.");
    return LayoutDeviceIntPoint();
  }
  return GetOwningBrowserChild()->GetChromeOffset();
}

LayoutDeviceIntPoint PuppetWidget::WidgetToScreenOffset() {
  return GetWindowPosition() + WidgetToTopLevelWidgetOffset();
}

LayoutDeviceIntPoint PuppetWidget::GetWindowPosition() {
  if (!GetOwningBrowserChild()) {
    return LayoutDeviceIntPoint();
  }

  int32_t winX, winY, winW, winH;
  NS_ENSURE_SUCCESS(GetOwningBrowserChild()->GetDimensions(
                        DimensionKind::Outer, &winX, &winY, &winW, &winH),
                    LayoutDeviceIntPoint());
  return LayoutDeviceIntPoint(winX, winY) +
         GetOwningBrowserChild()->GetClientOffset();
}

LayoutDeviceIntRect PuppetWidget::GetScreenBounds() {
  return LayoutDeviceIntRect(WidgetToScreenOffset(), mBounds.Size());
}

uint32_t PuppetWidget::GetMaxTouchPoints() const {
  return mBrowserChild ? mBrowserChild->MaxTouchPoints() : 0;
}

void PuppetWidget::StartAsyncScrollbarDrag(
    const AsyncDragMetrics& aDragMetrics) {
  mBrowserChild->StartScrollbarDrag(aDragMetrics);
}

ScreenIntMargin PuppetWidget::GetSafeAreaInsets() const {
  return mSafeAreaInsets;
}

void PuppetWidget::UpdateSafeAreaInsets(
    const ScreenIntMargin& aSafeAreaInsets) {
  mSafeAreaInsets = aSafeAreaInsets;
}

nsIWidgetListener* PuppetWidget::GetCurrentWidgetListener() {
  if (!mPreviouslyAttachedWidgetListener || !mAttachedWidgetListener) {
    return mAttachedWidgetListener;
  }

  if (mAttachedWidgetListener->GetView()->IsPrimaryFramePaintSuppressed()) {
    return mPreviouslyAttachedWidgetListener;
  }

  return mAttachedWidgetListener;
}

void PuppetWidget::ZoomToRect(const uint32_t& aPresShellId,
                              const ScrollableLayerGuid::ViewID& aViewId,
                              const CSSRect& aRect, const uint32_t& aFlags) {
  if (!mBrowserChild) {
    return;
  }

  mBrowserChild->ZoomToRect(aPresShellId, aViewId, aRect, aFlags);
}

void PuppetWidget::LookUpDictionary(
    const nsAString& aText, const nsTArray<mozilla::FontRange>& aFontRangeArray,
    const bool aIsVertical, const LayoutDeviceIntPoint& aPoint) {
  if (!mBrowserChild) {
    return;
  }

  mBrowserChild->SendLookUpDictionary(aText, aFontRangeArray, aIsVertical,
                                      aPoint);
}

bool PuppetWidget::HasPendingInputEvent() {
  if (!mBrowserChild) {
    return false;
  }

  bool ret = false;

  mBrowserChild->GetIPCChannel()->PeekMessages(
      [&ret](const IPC::Message& aMsg) -> bool {
        if (nsContentUtils::IsMessageInputEvent(aMsg)) {
          ret = true;
          return false;  // Stop peeking.
        }
        return true;
      });

  return ret;
}

// TextEventDispatcherListener

NS_IMETHODIMP
PuppetWidget::NotifyIME(TextEventDispatcher* aTextEventDispatcher,
                        const IMENotification& aIMENotification) {
  MOZ_ASSERT(aTextEventDispatcher == mTextEventDispatcher);

  // If there is different text event dispatcher listener for handling
  // text event dispatcher, that means that native keyboard events and
  // IME events are handled in this process.  Therefore, we don't need
  // to send any requests and notifications to the parent process.
  if (mNativeTextEventDispatcherListener) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  switch (aIMENotification.mMessage) {
    case REQUEST_TO_COMMIT_COMPOSITION:
      return RequestIMEToCommitComposition(false);
    case REQUEST_TO_CANCEL_COMPOSITION:
      return RequestIMEToCommitComposition(true);
    case NOTIFY_IME_OF_FOCUS:
    case NOTIFY_IME_OF_BLUR:
      return NotifyIMEOfFocusChange(aIMENotification);
    case NOTIFY_IME_OF_SELECTION_CHANGE:
      return NotifyIMEOfSelectionChange(aIMENotification);
    case NOTIFY_IME_OF_TEXT_CHANGE:
      return NotifyIMEOfTextChange(aIMENotification);
    case NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED:
      return NotifyIMEOfCompositionUpdate(aIMENotification);
    case NOTIFY_IME_OF_MOUSE_BUTTON_EVENT:
      return NotifyIMEOfMouseButtonEvent(aIMENotification);
    case NOTIFY_IME_OF_POSITION_CHANGE:
      return NotifyIMEOfPositionChange(aIMENotification);
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
}

NS_IMETHODIMP_(IMENotificationRequests)
PuppetWidget::GetIMENotificationRequests() {
  return IMENotificationRequests(
      mIMENotificationRequestsOfParent.mWantUpdates |
      IMENotificationRequests::NOTIFY_TEXT_CHANGE |
      IMENotificationRequests::NOTIFY_POSITION_CHANGE);
}

NS_IMETHODIMP_(void)
PuppetWidget::OnRemovedFrom(TextEventDispatcher* aTextEventDispatcher) {
  MOZ_ASSERT(aTextEventDispatcher == mTextEventDispatcher);
}

NS_IMETHODIMP_(void)
PuppetWidget::WillDispatchKeyboardEvent(
    TextEventDispatcher* aTextEventDispatcher,
    WidgetKeyboardEvent& aKeyboardEvent, uint32_t aIndexOfKeypress,
    void* aData) {
  MOZ_ASSERT(aTextEventDispatcher == mTextEventDispatcher);
}

nsresult PuppetWidget::SetSystemFont(const nsCString& aFontName) {
  if (!mBrowserChild) {
    return NS_ERROR_FAILURE;
  }

  mBrowserChild->SendSetSystemFont(aFontName);
  return NS_OK;
}

nsresult PuppetWidget::GetSystemFont(nsCString& aFontName) {
  if (!mBrowserChild) {
    return NS_ERROR_FAILURE;
  }
  mBrowserChild->SendGetSystemFont(&aFontName);
  return NS_OK;
}

}  // namespace widget
}  // namespace mozilla
