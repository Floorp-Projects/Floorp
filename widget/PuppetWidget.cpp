/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "ClientLayerManager.h"
#include "gfxPlatform.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/TabGroup.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/Hal.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/layers/APZChild.h"
#include "mozilla/layers/PLayerTransactionChild.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEventDispatcher.h"
#include "mozilla/TextEvents.h"
#include "mozilla/Unused.h"
#include "BasicLayers.h"
#include "PuppetWidget.h"
#include "nsContentUtils.h"
#include "nsIWidgetListener.h"
#include "imgIContainer.h"
#include "nsView.h"
#include "nsPrintfCString.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::hal;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::widget;

static void
InvalidateRegion(nsIWidget* aWidget, const LayoutDeviceIntRegion& aRegion)
{
  for (auto iter = aRegion.RectIter(); !iter.Done(); iter.Next()) {
    aWidget->Invalidate(iter.Get());
  }
}

/*static*/ already_AddRefed<nsIWidget>
nsIWidget::CreatePuppetWidget(TabChild* aTabChild)
{
  MOZ_ASSERT(!aTabChild || nsIWidget::UsePuppetWidgets(),
             "PuppetWidgets not allowed in this configuration");

  nsCOMPtr<nsIWidget> widget = new PuppetWidget(aTabChild);
  return widget.forget();
}

namespace mozilla {
namespace widget {

static bool
IsPopup(const nsWidgetInitData* aInitData)
{
  return aInitData && aInitData->mWindowType == eWindowType_popup;
}

static bool
MightNeedIMEFocus(const nsWidgetInitData* aInitData)
{
  // In the puppet-widget world, popup widgets are just dummies and
  // shouldn't try to mess with IME state.
#ifdef MOZ_CROSS_PROCESS_IME
  return !IsPopup(aInitData);
#else
  return false;
#endif
}

// Arbitrary, fungible.
const size_t PuppetWidget::kMaxDimension = 4000;

static bool gRemoteDesktopBehaviorEnabled = false;
static bool gRemoteDesktopBehaviorInitialized = false;

NS_IMPL_ISUPPORTS_INHERITED(PuppetWidget, nsBaseWidget
                                        , TextEventDispatcherListener)

PuppetWidget::PuppetWidget(TabChild* aTabChild)
  : mTabChild(aTabChild)
  , mMemoryPressureObserver(nullptr)
  , mDPI(-1)
  , mRounding(1)
  , mDefaultScale(-1)
  , mCursorHotspotX(0)
  , mCursorHotspotY(0)
  , mIgnoreCompositionEvents(false)
{
  // Setting 'Unknown' means "not yet cached".
  mInputContext.mIMEState.mEnabled = IMEState::UNKNOWN;

  if (!gRemoteDesktopBehaviorInitialized) {
    Preferences::AddBoolVarCache(&gRemoteDesktopBehaviorEnabled, "browser.tabs.remote.desktopbehavior", false);
    gRemoteDesktopBehaviorInitialized = true;
  }
}

PuppetWidget::~PuppetWidget()
{
  Destroy();
}

void
PuppetWidget::InfallibleCreate(nsIWidget* aParent,
                               nsNativeWidget aNativeParent,
                               const LayoutDeviceIntRect& aRect,
                               nsWidgetInitData* aInitData)
{
  MOZ_ASSERT(!aNativeParent, "got a non-Puppet native parent");

  BaseCreate(nullptr, aInitData);

  mBounds = aRect;
  mEnabled = true;
  mVisible = true;

  mDrawTarget = gfxPlatform::GetPlatform()->
    CreateOffscreenContentDrawTarget(IntSize(1, 1), SurfaceFormat::B8G8R8A8);

  mNeedIMEStateInit = MightNeedIMEFocus(aInitData);

  PuppetWidget* parent = static_cast<PuppetWidget*>(aParent);
  if (parent) {
    parent->SetChild(this);
    mLayerManager = parent->GetLayerManager();
  }
  else {
    Resize(mBounds.X(), mBounds.Y(), mBounds.Width(), mBounds.Height(), false);
  }
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    mMemoryPressureObserver = new MemoryPressureObserver(this);
    obs->AddObserver(mMemoryPressureObserver, "memory-pressure", false);
  }
}

nsresult
PuppetWidget::Create(nsIWidget* aParent,
                     nsNativeWidget aNativeParent,
                     const LayoutDeviceIntRect& aRect,
                     nsWidgetInitData* aInitData)
{
  InfallibleCreate(aParent, aNativeParent, aRect, aInitData);
  return NS_OK;
}

void
PuppetWidget::InitIMEState()
{
  MOZ_ASSERT(mTabChild);
  if (mNeedIMEStateInit) {
    mContentCache.Clear();
    mTabChild->SendUpdateContentCache(mContentCache);
    mIMENotificationRequestsOfParent = IMENotificationRequests();
    mNeedIMEStateInit = false;
  }
}

already_AddRefed<nsIWidget>
PuppetWidget::CreateChild(const LayoutDeviceIntRect& aRect,
                          nsWidgetInitData* aInitData,
                          bool aForceUseIWidgetParent)
{
  bool isPopup = IsPopup(aInitData);
  nsCOMPtr<nsIWidget> widget = nsIWidget::CreatePuppetWidget(mTabChild);
  return ((widget &&
           NS_SUCCEEDED(widget->Create(isPopup ? nullptr: this, nullptr, aRect,
                                       aInitData))) ?
          widget.forget() : nullptr);
}

void
PuppetWidget::Destroy()
{
  if (mOnDestroyCalled) {
    return;
  }
  mOnDestroyCalled = true;

  Base::OnDestroy();
  Base::Destroy();
  mPaintTask.Revoke();
  if (mMemoryPressureObserver) {
    mMemoryPressureObserver->Remove();
  }
  mMemoryPressureObserver = nullptr;
  mChild = nullptr;
  if (mLayerManager) {
    mLayerManager->Destroy();
  }
  mLayerManager = nullptr;
  mTabChild = nullptr;
}

void
PuppetWidget::Show(bool aState)
{
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

void
PuppetWidget::Resize(double aWidth,
                     double aHeight,
                     bool   aRepaint)
{
  LayoutDeviceIntRect oldBounds = mBounds;
  mBounds.SizeTo(LayoutDeviceIntSize(NSToIntRound(aWidth),
                                     NSToIntRound(aHeight)));

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
      GetCurrentWidgetListener()->WindowResized(this, mBounds.Width(), mBounds.Height());
    }
    mAttachedWidgetListener->WindowResized(this, mBounds.Width(), mBounds.Height());
  }
}

nsresult
PuppetWidget::ConfigureChildren(const nsTArray<Configuration>& aConfigurations)
{
  for (uint32_t i = 0; i < aConfigurations.Length(); ++i) {
    const Configuration& configuration = aConfigurations[i];
    PuppetWidget* w = static_cast<PuppetWidget*>(configuration.mChild.get());
    NS_ASSERTION(w->GetParent() == this,
                 "Configured widget is not a child");
    w->SetWindowClipRegion(configuration.mClipRegion, true);
    LayoutDeviceIntRect bounds = w->GetBounds();
    if (bounds.Size() != configuration.mBounds.Size()) {
      w->Resize(configuration.mBounds.X(), configuration.mBounds.Y(),
                configuration.mBounds.Width(), configuration.mBounds.Height(),
                true);
    } else if (bounds.TopLeft() != configuration.mBounds.TopLeft()) {
      w->Move(configuration.mBounds.X(), configuration.mBounds.Y());
    }
    w->SetWindowClipRegion(configuration.mClipRegion, false);
  }
  return NS_OK;
}

nsresult
PuppetWidget::SetFocus(bool aRaise)
{
  if (aRaise && mTabChild) {
    mTabChild->SendRequestFocus(true);
  }

  return NS_OK;
}

void
PuppetWidget::Invalidate(const LayoutDeviceIntRect& aRect)
{
#ifdef DEBUG
  debug_DumpInvalidate(stderr, this, &aRect, "PuppetWidget", 0);
#endif

  if (mChild) {
    mChild->Invalidate(aRect);
    return;
  }

  mDirtyRegion.Or(mDirtyRegion, aRect);

  if (mTabChild && !mDirtyRegion.IsEmpty() && !mPaintTask.IsPending()) {
    mPaintTask = new PaintTask(this);
    nsCOMPtr<nsIRunnable> event(mPaintTask.get());
    mTabChild->TabGroup()->Dispatch(TaskCategory::Other, event.forget());
    return;
  }
}

void
PuppetWidget::InitEvent(WidgetGUIEvent& aEvent, LayoutDeviceIntPoint* aPoint)
{
  if (nullptr == aPoint) {
    aEvent.mRefPoint = LayoutDeviceIntPoint(0, 0);
  } else {
    // use the point override if provided
    aEvent.mRefPoint = *aPoint;
  }
  aEvent.mTime = PR_Now() / 1000;
}

nsresult
PuppetWidget::DispatchEvent(WidgetGUIEvent* aEvent, nsEventStatus& aStatus)
{
#ifdef DEBUG
  debug_DumpEvent(stdout, aEvent->mWidget, aEvent, "PuppetWidget", 0);
#endif

  MOZ_ASSERT(!mChild || mChild->mWindowType == eWindowType_popup,
             "Unexpected event dispatch!");

  MOZ_ASSERT(!aEvent->AsKeyboardEvent() ||
             aEvent->mFlags.mIsSynthesizedForTests ||
             aEvent->AsKeyboardEvent()->AreAllEditCommandsInitialized(),
    "Non-sysnthesized keyboard events should have edit commands for all types "
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
      MOZ_ASSERT(!composition,
        "When there is composition caused by old native IME context, "
        "composition events caused by different native IME context are not "
        "allowed");
    }
#endif // #ifdef DEBUG
    mNativeIMEContext = compositionEvent->mNativeIMEContext;
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
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
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

nsEventStatus
PuppetWidget::DispatchInputEvent(WidgetInputEvent* aEvent)
{
  if (!AsyncPanZoomEnabled()) {
    nsEventStatus status = nsEventStatus_eIgnore;
    DispatchEvent(aEvent, status);
    return status;
  }

  if (!mTabChild) {
    return nsEventStatus_eIgnore;
  }

  switch (aEvent->mClass) {
    case eWheelEventClass:
      Unused <<
        mTabChild->SendDispatchWheelEvent(*aEvent->AsWheelEvent());
      break;
    case eMouseEventClass:
      Unused <<
        mTabChild->SendDispatchMouseEvent(*aEvent->AsMouseEvent());
      break;
    case eKeyboardEventClass:
      Unused <<
        mTabChild->SendDispatchKeyboardEvent(*aEvent->AsKeyboardEvent());
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("unsupported event type");
  }

  return nsEventStatus_eIgnore;
}

nsresult
PuppetWidget::SynthesizeNativeKeyEvent(int32_t aNativeKeyboardLayout,
                                       int32_t aNativeKeyCode,
                                       uint32_t aModifierFlags,
                                       const nsAString& aCharacters,
                                       const nsAString& aUnmodifiedCharacters,
                                       nsIObserver* aObserver)
{
  AutoObserverNotifier notifier(aObserver, "keyevent");
  if (!mTabChild) {
    return NS_ERROR_FAILURE;
  }
  mTabChild->SendSynthesizeNativeKeyEvent(aNativeKeyboardLayout, aNativeKeyCode,
                                          aModifierFlags, nsString(aCharacters),
                                          nsString(aUnmodifiedCharacters),
                                          notifier.SaveObserver());
  return NS_OK;
}

nsresult
PuppetWidget::SynthesizeNativeMouseEvent(mozilla::LayoutDeviceIntPoint aPoint,
                                         uint32_t aNativeMessage,
                                         uint32_t aModifierFlags,
                                         nsIObserver* aObserver)
{
  AutoObserverNotifier notifier(aObserver, "mouseevent");
  if (!mTabChild) {
    return NS_ERROR_FAILURE;
  }
  mTabChild->SendSynthesizeNativeMouseEvent(aPoint, aNativeMessage,
                                            aModifierFlags,
                                            notifier.SaveObserver());
  return NS_OK;
}

nsresult
PuppetWidget::SynthesizeNativeMouseMove(mozilla::LayoutDeviceIntPoint aPoint,
                                        nsIObserver* aObserver)
{
  AutoObserverNotifier notifier(aObserver, "mousemove");
  if (!mTabChild) {
    return NS_ERROR_FAILURE;
  }
  mTabChild->SendSynthesizeNativeMouseMove(aPoint, notifier.SaveObserver());
  return NS_OK;
}

nsresult
PuppetWidget::SynthesizeNativeMouseScrollEvent(
                mozilla::LayoutDeviceIntPoint aPoint,
                uint32_t aNativeMessage,
                double aDeltaX,
                double aDeltaY,
                double aDeltaZ,
                uint32_t aModifierFlags,
                uint32_t aAdditionalFlags,
                nsIObserver* aObserver)
{
  AutoObserverNotifier notifier(aObserver, "mousescrollevent");
  if (!mTabChild) {
    return NS_ERROR_FAILURE;
  }
  mTabChild->SendSynthesizeNativeMouseScrollEvent(aPoint, aNativeMessage,
                                                  aDeltaX, aDeltaY, aDeltaZ,
                                                  aModifierFlags,
                                                  aAdditionalFlags,
                                                  notifier.SaveObserver());
  return NS_OK;
}

nsresult
PuppetWidget::SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                         TouchPointerState aPointerState,
                                         LayoutDeviceIntPoint aPoint,
                                         double aPointerPressure,
                                         uint32_t aPointerOrientation,
                                         nsIObserver* aObserver)
{
  AutoObserverNotifier notifier(aObserver, "touchpoint");
  if (!mTabChild) {
    return NS_ERROR_FAILURE;
  }
  mTabChild->SendSynthesizeNativeTouchPoint(aPointerId, aPointerState,
                                            aPoint, aPointerPressure,
                                            aPointerOrientation,
                                            notifier.SaveObserver());
  return NS_OK;
}

nsresult
PuppetWidget::SynthesizeNativeTouchTap(LayoutDeviceIntPoint aPoint,
                                       bool aLongTap,
                                       nsIObserver* aObserver)
{
  AutoObserverNotifier notifier(aObserver, "touchtap");
  if (!mTabChild) {
    return NS_ERROR_FAILURE;
  }
  mTabChild->SendSynthesizeNativeTouchTap(aPoint, aLongTap,
                                          notifier.SaveObserver());
  return NS_OK;
}

nsresult
PuppetWidget::ClearNativeTouchSequence(nsIObserver* aObserver)
{
  AutoObserverNotifier notifier(aObserver, "cleartouch");
  if (!mTabChild) {
    return NS_ERROR_FAILURE;
  }
  mTabChild->SendClearNativeTouchSequence(notifier.SaveObserver());
  return NS_OK;
}

void
PuppetWidget::SetConfirmedTargetAPZC(
                uint64_t aInputBlockId,
                const nsTArray<ScrollableLayerGuid>& aTargets) const
{
  if (mTabChild) {
    mTabChild->SetTargetAPZC(aInputBlockId, aTargets);
  }
}

void
PuppetWidget::UpdateZoomConstraints(const uint32_t& aPresShellId,
                                    const FrameMetrics::ViewID& aViewId,
                                    const Maybe<ZoomConstraints>& aConstraints)
{
  if (mTabChild) {
    mTabChild->DoUpdateZoomConstraints(aPresShellId, aViewId, aConstraints);
  }
}

bool
PuppetWidget::AsyncPanZoomEnabled() const
{
  return mTabChild && mTabChild->AsyncPanZoomEnabled();
}

void
PuppetWidget::GetEditCommands(NativeKeyBindingsType aType,
                              const WidgetKeyboardEvent& aEvent,
                              nsTArray<CommandInt>& aCommands)
{
  // Validate the arguments.
  nsIWidget::GetEditCommands(aType, aEvent, aCommands);

  mTabChild->RequestEditCommands(aType, aEvent, aCommands);
}

LayerManager*
PuppetWidget::GetLayerManager(PLayerTransactionChild* aShadowManager,
                              LayersBackend aBackendHint,
                              LayerManagerPersistence aPersistence)
{
  if (!mLayerManager) {
    if (XRE_IsParentProcess()) {
      // On the parent process there is no CompositorBridgeChild which confuses
      // some layers code, so we use basic layers instead. Note that we create
      // a non-retaining layer manager since we don't care about performance.
      mLayerManager = new BasicLayerManager(BasicLayerManager::BLM_OFFSCREEN);
      return mLayerManager;
    }

    // If we know for sure that the parent side of this TabChild is not
    // connected to the compositor, we don't want to use a "remote" layer
    // manager like WebRender or Client. Instead we use a Basic one which
    // can do drawing in this process.
    MOZ_ASSERT(!mTabChild || mTabChild->IsLayersConnected() != Some(true));
    mLayerManager = new BasicLayerManager(this);
  }

  return mLayerManager;
}

bool
PuppetWidget::CreateRemoteLayerManager(const std::function<bool(LayerManager*)>& aInitializeFunc)
{
  RefPtr<LayerManager> lm;
  MOZ_ASSERT(mTabChild);
  if (mTabChild->GetCompositorOptions().UseWebRender()) {
    lm = new WebRenderLayerManager(this);
  } else {
    lm = new ClientLayerManager(this);
  }

  if (!aInitializeFunc(lm)) {
    return false;
  }

  // Force the old LM to self destruct, otherwise if the reference dangles we
  // could fail to revoke the most recent transaction. We only want to replace
  // it if we successfully create its successor because a partially initialized
  // layer manager is worse than a fully initialized but shutdown layer manager.
  DestroyLayerManager();
  mLayerManager = lm.forget();
  return true;
}

nsresult
PuppetWidget::RequestIMEToCommitComposition(bool aCancel)
{
  if (!mTabChild) {
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
#endif // #ifdef DEBUG
    return NS_OK;
  }

  RefPtr<TextComposition> composition =
    IMEStateManager::GetTextCompositionFor(this);
  // This method shouldn't be called when there is no text composition instance.
  if (NS_WARN_IF(!composition)) {
    return NS_OK;
  }

  MOZ_DIAGNOSTIC_ASSERT(composition->IsRequestingCommitOrCancelComposition(),
    "Requesting commit or cancel composition should be requested via "
    "TextComposition instance");

  bool isCommitted = false;
  nsAutoString committedString;
  if (NS_WARN_IF(!mTabChild->SendRequestIMEToCommitComposition(
                               aCancel, &isCommitted, &committedString))) {
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
#endif // #ifdef DEBUG

  // Ignore the following composition events until we receive new
  // eCompositionStart event.
  mIgnoreCompositionEvents = true;

  Unused <<
    mTabChild->SendOnEventNeedingAckHandled(eCompositionCommitRequestHandled);

  // NOTE: PuppetWidget might be destroyed already.
  return NS_OK;
}

nsresult
PuppetWidget::StartPluginIME(const mozilla::WidgetKeyboardEvent& aKeyboardEvent,
                             int32_t aPanelX, int32_t aPanelY,
                             nsString& aCommitted)
{
  if (!mTabChild ||
      !mTabChild->SendStartPluginIME(aKeyboardEvent, aPanelX,
                                     aPanelY, &aCommitted)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

void
PuppetWidget::SetPluginFocused(bool& aFocused)
{
  if (mTabChild) {
    mTabChild->SendSetPluginFocused(aFocused);
  }
}

void
PuppetWidget::DefaultProcOfPluginEvent(const WidgetPluginEvent& aEvent)
{
  if (!mTabChild) {
    return;
  }
  mTabChild->SendDefaultProcOfPluginEvent(aEvent);
}

void
PuppetWidget::SetInputContext(const InputContext& aContext,
                              const InputContextAction& aAction)
{
  mInputContext = aContext;
  // Any widget instances cannot cache IME open state because IME open state
  // can be changed by user but native IME may not notify us of changing the
  // open state on some platforms.
  mInputContext.mIMEState.mOpen = IMEState::OPEN_STATE_NOT_SUPPORTED;
  if (!mTabChild) {
    return;
  }
  mTabChild->SendSetInputContext(
    static_cast<int32_t>(aContext.mIMEState.mEnabled),
    static_cast<int32_t>(aContext.mIMEState.mOpen),
    aContext.mHTMLInputType,
    aContext.mHTMLInputInputmode,
    aContext.mActionHint,
    aContext.mInPrivateBrowsing,
    static_cast<int32_t>(aAction.mCause),
    static_cast<int32_t>(aAction.mFocusChange));
}

InputContext
PuppetWidget::GetInputContext()
{
  // XXX Currently, we don't support retrieving IME open state from child
  //     process.

  // When this widget caches input context and currently managed by
  // IMEStateManager, the cache is valid.  Only in this case, we can
  // avoid to use synchronous IPC.
  if (mInputContext.mIMEState.mEnabled != IMEState::UNKNOWN &&
      IMEStateManager::GetWidgetForActiveInputContext() == this) {
    return mInputContext;
  }

  NS_WARNING("PuppetWidget::GetInputContext() needs to retrieve it with IPC");

  // Don't cache InputContext here because this process isn't managing IME
  // state of the chrome widget.  So, we cannot modify mInputContext when
  // chrome widget is set to new context.
  InputContext context;
  if (mTabChild) {
    int32_t enabled, open;
    mTabChild->SendGetInputContext(&enabled, &open);
    context.mIMEState.mEnabled = static_cast<IMEState::Enabled>(enabled);
    context.mIMEState.mOpen = static_cast<IMEState::Open>(open);
  }
  return context;
}

NativeIMEContext
PuppetWidget::GetNativeIMEContext()
{
  return mNativeIMEContext;
}

nsresult
PuppetWidget::NotifyIMEOfFocusChange(const IMENotification& aIMENotification)
{
  if (!mTabChild) {
    return NS_ERROR_FAILURE;
  }

  bool gotFocus = aIMENotification.mMessage == NOTIFY_IME_OF_FOCUS;
  if (gotFocus) {
    if (mInputContext.mIMEState.mEnabled != IMEState::PLUGIN) {
      // When IME gets focus, we should initalize all information of the
      // content.
      if (NS_WARN_IF(!mContentCache.CacheAll(this, &aIMENotification))) {
        return NS_ERROR_FAILURE;
      }
    } else {
      // However, if a plugin has focus, only the editor rect information is
      // available.
      if (NS_WARN_IF(!mContentCache.CacheEditorRect(this, &aIMENotification))) {
        return NS_ERROR_FAILURE;
      }
    }
  } else {
    // When IME loses focus, we don't need to store anything.
    mContentCache.Clear();
  }

  mIMENotificationRequestsOfParent =
  IMENotificationRequests(IMENotificationRequests::NOTIFY_ALL);
  RefPtr<PuppetWidget> self = this;
  mTabChild->SendNotifyIMEFocus(mContentCache, aIMENotification)->Then(
    mTabChild->TabGroup()->EventTargetFor(TaskCategory::UI),
    __func__,
    [self] (IMENotificationRequests aRequests) {
      self->mIMENotificationRequestsOfParent = aRequests;
    },
    [self] (mozilla::ipc::ResponseRejectReason aReason) {
      NS_WARNING("SendNotifyIMEFocus got rejected.");
    });

  return NS_OK;
}

nsresult
PuppetWidget::NotifyIMEOfCompositionUpdate(
                const IMENotification& aIMENotification)
{
  if (NS_WARN_IF(!mTabChild)) {
    return NS_ERROR_FAILURE;
  }

  if (mInputContext.mIMEState.mEnabled != IMEState::PLUGIN &&
      NS_WARN_IF(!mContentCache.CacheSelection(this, &aIMENotification))) {
    return NS_ERROR_FAILURE;
  }
  mTabChild->SendNotifyIMECompositionUpdate(mContentCache, aIMENotification);
  return NS_OK;
}

nsresult
PuppetWidget::NotifyIMEOfTextChange(const IMENotification& aIMENotification)
{
  MOZ_ASSERT(aIMENotification.mMessage == NOTIFY_IME_OF_TEXT_CHANGE,
             "Passed wrong notification");
  if (!mTabChild) {
    return NS_ERROR_FAILURE;
  }

  // While a plugin has focus, text change notification shouldn't be available.
  if (NS_WARN_IF(mInputContext.mIMEState.mEnabled == IMEState::PLUGIN)) {
    return NS_ERROR_FAILURE;
  }

  // FYI: text change notification is the first notification after
  //      a user operation changes the content.  So, we need to modify
  //      the cache as far as possible here.

  if (NS_WARN_IF(!mContentCache.CacheText(this, &aIMENotification))) {
    return NS_ERROR_FAILURE;
  }

  // TabParent doesn't this this to cache.  we don't send the notification
  // if parent process doesn't request NOTIFY_TEXT_CHANGE.
  if (mIMENotificationRequestsOfParent.WantTextChange()) {
    mTabChild->SendNotifyIMETextChange(mContentCache, aIMENotification);
  } else {
    mTabChild->SendUpdateContentCache(mContentCache);
  }
  return NS_OK;
}

nsresult
PuppetWidget::NotifyIMEOfSelectionChange(
                const IMENotification& aIMENotification)
{
  MOZ_ASSERT(aIMENotification.mMessage == NOTIFY_IME_OF_SELECTION_CHANGE,
             "Passed wrong notification");
  if (!mTabChild) {
    return NS_ERROR_FAILURE;
  }

  // While a plugin has focus, selection change notification shouldn't be
  // available.
  if (NS_WARN_IF(mInputContext.mIMEState.mEnabled == IMEState::PLUGIN)) {
    return NS_ERROR_FAILURE;
  }

  // Note that selection change must be notified after text change if it occurs.
  // Therefore, we don't need to query text content again here.
  mContentCache.SetSelection(
    this,
    aIMENotification.mSelectionChangeData.mOffset,
    aIMENotification.mSelectionChangeData.Length(),
    aIMENotification.mSelectionChangeData.mReversed,
    aIMENotification.mSelectionChangeData.GetWritingMode());

  mTabChild->SendNotifyIMESelection(mContentCache, aIMENotification);

  return NS_OK;
}

nsresult
PuppetWidget::NotifyIMEOfMouseButtonEvent(
                const IMENotification& aIMENotification)
{
  if (!mTabChild) {
    return NS_ERROR_FAILURE;
  }

  // While a plugin has focus, mouse button event notification shouldn't be
  // available.
  if (NS_WARN_IF(mInputContext.mIMEState.mEnabled == IMEState::PLUGIN)) {
    return NS_ERROR_FAILURE;
  }


  bool consumedByIME = false;
  if (!mTabChild->SendNotifyIMEMouseButtonEvent(aIMENotification,
                                                &consumedByIME)) {
    return NS_ERROR_FAILURE;
  }

  return consumedByIME ? NS_SUCCESS_EVENT_CONSUMED : NS_OK;
}

nsresult
PuppetWidget::NotifyIMEOfPositionChange(const IMENotification& aIMENotification)
{
  if (NS_WARN_IF(!mTabChild)) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!mContentCache.CacheEditorRect(this, &aIMENotification))) {
    return NS_ERROR_FAILURE;
  }
  // While a plugin has focus, selection range isn't available.  So, we don't
  // need to cache it at that time.
  if (mInputContext.mIMEState.mEnabled != IMEState::PLUGIN &&
      NS_WARN_IF(!mContentCache.CacheSelection(this, &aIMENotification))) {
    return NS_ERROR_FAILURE;
  }
  if (mIMENotificationRequestsOfParent.WantPositionChanged()) {
    mTabChild->SendNotifyIMEPositionChange(mContentCache, aIMENotification);
  } else {
    mTabChild->SendUpdateContentCache(mContentCache);
  }
  return NS_OK;
}

void
PuppetWidget::SetCursor(nsCursor aCursor)
{
  // Don't cache on windows, Windowless flash breaks this via async cursor updates.
#if !defined(XP_WIN)
  if (mCursor == aCursor && !mCustomCursor && !mUpdateCursor) {
    return;
  }
#endif

  mCustomCursor = nullptr;

  if (mTabChild &&
      !mTabChild->SendSetCursor(aCursor, mUpdateCursor)) {
    return;
  }

  mCursor = aCursor;
  mUpdateCursor = false;
}

nsresult
PuppetWidget::SetCursor(imgIContainer* aCursor,
                        uint32_t aHotspotX, uint32_t aHotspotY)
{
  if (!aCursor || !mTabChild) {
    return NS_OK;
  }

#if !defined(XP_WIN)
  if (mCustomCursor == aCursor &&
      mCursorHotspotX == aHotspotX &&
      mCursorHotspotY == aHotspotY &&
      !mUpdateCursor) {
    return NS_OK;
  }
#endif

  RefPtr<mozilla::gfx::SourceSurface> surface =
    aCursor->GetFrame(imgIContainer::FRAME_CURRENT,
                      imgIContainer::FLAG_SYNC_DECODE);
  if (!surface) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<mozilla::gfx::DataSourceSurface> dataSurface =
    surface->GetDataSurface();
  if (!dataSurface) {
    return NS_ERROR_FAILURE;
  }

  size_t length;
  int32_t stride;
  mozilla::UniquePtr<char[]> surfaceData =
    nsContentUtils::GetSurfaceData(WrapNotNull(dataSurface), &length, &stride);

  nsDependentCString cursorData(surfaceData.get(), length);
  mozilla::gfx::IntSize size = dataSurface->GetSize();
  if (!mTabChild->SendSetCustomCursor(cursorData, size.width, size.height, stride,
                                      static_cast<uint8_t>(dataSurface->GetFormat()),
                                      aHotspotX, aHotspotY, mUpdateCursor)) {
    return NS_ERROR_FAILURE;
  }

  mCursor = eCursorInvalid;
  mCustomCursor = aCursor;
  mCursorHotspotX = aHotspotX;
  mCursorHotspotY = aHotspotY;
  mUpdateCursor = false;

  return NS_OK;
}

void
PuppetWidget::ClearCachedCursor()
{
  nsBaseWidget::ClearCachedCursor();
  mCustomCursor = nullptr;
}

nsresult
PuppetWidget::Paint()
{
  MOZ_ASSERT(!mDirtyRegion.IsEmpty(), "paint event logic messed up");

  if (!GetCurrentWidgetListener())
    return NS_OK;

  LayoutDeviceIntRegion region = mDirtyRegion;

  // reset repaint tracking
  mDirtyRegion.SetEmpty();
  mPaintTask.Revoke();

  RefPtr<PuppetWidget> strongThis(this);

  GetCurrentWidgetListener()->WillPaintWindow(this);

  if (GetCurrentWidgetListener()) {
#ifdef DEBUG
    debug_DumpPaintEvent(stderr, this, region.ToUnknownRegion(),
                         "PuppetWidget", 0);
#endif

    if (mLayerManager->GetBackendType() == mozilla::layers::LayersBackend::LAYERS_CLIENT ||
        mLayerManager->GetBackendType() == mozilla::layers::LayersBackend::LAYERS_WR ||
        (mozilla::layers::LayersBackend::LAYERS_BASIC == mLayerManager->GetBackendType() &&
         mTabChild && mTabChild->IsLayersConnected().isSome())) {
      // Do nothing, the compositor will handle drawing
      if (mTabChild) {
        mTabChild->NotifyPainted();
      }
    } else if (mozilla::layers::LayersBackend::LAYERS_BASIC == mLayerManager->GetBackendType()) {
      RefPtr<gfxContext> ctx = gfxContext::CreateOrNull(mDrawTarget);
      if (!ctx) {
        gfxDevCrash(LogReason::InvalidContext) << "PuppetWidget context problem " << gfx::hexa(mDrawTarget);
        return NS_ERROR_FAILURE;
      }
      ctx->Rectangle(gfxRect(0,0,0,0));
      ctx->Clip();
      AutoLayerManagerSetup setupLayerManager(this, ctx,
                                              BufferMode::BUFFER_NONE);
      GetCurrentWidgetListener()->PaintWindow(this, region);
      if (mTabChild) {
        mTabChild->NotifyPainted();
      }
    }
  }

  if (GetCurrentWidgetListener()) {
    GetCurrentWidgetListener()->DidPaintWindow();
  }

  return NS_OK;
}

void
PuppetWidget::SetChild(PuppetWidget* aChild)
{
  MOZ_ASSERT(this != aChild, "can't parent a widget to itself");
  MOZ_ASSERT(!aChild->mChild,
             "fake widget 'hierarchy' only expected to have one level");

  mChild = aChild;
}

NS_IMETHODIMP
PuppetWidget::PaintTask::Run()
{
  if (mWidget) {
    mWidget->Paint();
  }
  return NS_OK;
}

void
PuppetWidget::PaintNowIfNeeded()
{
  if (IsVisible() && mPaintTask.IsPending()) {
    Paint();
  }
}

NS_IMPL_ISUPPORTS(PuppetWidget::MemoryPressureObserver, nsIObserver)

NS_IMETHODIMP
PuppetWidget::MemoryPressureObserver::Observe(nsISupports* aSubject,
                                              const char* aTopic,
                                              const char16_t* aData)
{
  if (!mWidget) {
    return NS_OK;
  }

  if (strcmp("memory-pressure", aTopic) == 0 &&
      !NS_LITERAL_STRING("lowering-priority").Equals(aData)) {
    if (!mWidget->mVisible && mWidget->mLayerManager &&
        XRE_IsContentProcess()) {
      mWidget->mLayerManager->ClearCachedResources();
    }
  }
  return NS_OK;
}

void
PuppetWidget::MemoryPressureObserver::Remove()
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, "memory-pressure");
  }
  mWidget = nullptr;
}

bool
PuppetWidget::NeedsPaint()
{
  // e10s popups are handled by the parent process, so never should be painted here
  if (XRE_IsContentProcess() &&
      gRemoteDesktopBehaviorEnabled &&
      mWindowType == eWindowType_popup) {
    NS_WARNING("Trying to paint an e10s popup in the child process!");
    return false;
  }

  return mVisible;
}

float
PuppetWidget::GetDPI()
{
  return mDPI;
}

double
PuppetWidget::GetDefaultScaleInternal()
{
  return mDefaultScale;
}

int32_t
PuppetWidget::RoundsWidgetCoordinatesTo()
{
  return mRounding;
}

void*
PuppetWidget::GetNativeData(uint32_t aDataType)
{
  switch (aDataType) {
  case NS_NATIVE_SHAREABLE_WINDOW: {
    // NOTE: We can not have a tab child in some situations, such as when we're
    // rendering to a fake widget for thumbnails.
    if (!mTabChild) {
      NS_WARNING("Need TabChild to get the nativeWindow from!");
    }
    mozilla::WindowsHandle nativeData = 0;
    if (mTabChild) {
      nativeData = mTabChild->WidgetNativeData();
    }
    return (void*)nativeData;
  }
  case NS_NATIVE_WINDOW:
  case NS_NATIVE_WIDGET:
  case NS_NATIVE_DISPLAY:
    // These types are ignored (see bug 1183828, bug 1240891).
    break;
  case NS_RAW_NATIVE_IME_CONTEXT:
    MOZ_CRASH("You need to call GetNativeIMEContext() instead");
  case NS_NATIVE_PLUGIN_PORT:
  case NS_NATIVE_GRAPHIC:
  case NS_NATIVE_SHELLWIDGET:
  default:
    NS_WARNING("nsWindow::GetNativeData called with bad value");
    break;
  }
  return nullptr;
}

#if defined(XP_WIN)
void
PuppetWidget::SetNativeData(uint32_t aDataType, uintptr_t aVal)
{
  switch (aDataType) {
  case NS_NATIVE_CHILD_OF_SHAREABLE_WINDOW:
    MOZ_ASSERT(mTabChild, "Need TabChild to send the message.");
    if (mTabChild) {
      mTabChild->SendSetNativeChildOfShareableWindow(aVal);
    }
    break;
  default:
    NS_WARNING("SetNativeData called with unsupported data type.");
  }
}
#endif

LayoutDeviceIntPoint
PuppetWidget::GetChromeOffset()
{
  if (!GetOwningTabChild()) {
    NS_WARNING("PuppetWidget without Tab does not have chrome information.");
    return LayoutDeviceIntPoint();
  }
  return GetOwningTabChild()->GetChromeOffset();
}

LayoutDeviceIntPoint
PuppetWidget::GetWindowPosition()
{
  if (!GetOwningTabChild()) {
    return LayoutDeviceIntPoint();
  }

  int32_t winX, winY, winW, winH;
  NS_ENSURE_SUCCESS(GetOwningTabChild()->GetDimensions(0, &winX, &winY, &winW, &winH), LayoutDeviceIntPoint());
  return LayoutDeviceIntPoint(winX, winY) + GetOwningTabChild()->GetClientOffset();
}

LayoutDeviceIntRect
PuppetWidget::GetScreenBounds()
{
  return LayoutDeviceIntRect(WidgetToScreenOffset(), mBounds.Size());
}

uint32_t PuppetWidget::GetMaxTouchPoints() const
{
  uint32_t maxTouchPoints = 0;
  if (mTabChild) {
    mTabChild->GetMaxTouchPoints(&maxTouchPoints);
  }
  return maxTouchPoints;
}

void
PuppetWidget::StartAsyncScrollbarDrag(const AsyncDragMetrics& aDragMetrics)
{
  mTabChild->StartScrollbarDrag(aDragMetrics);
}

PuppetScreen::PuppetScreen(void *nativeScreen)
{
}

PuppetScreen::~PuppetScreen()
{
}

static ScreenConfiguration
ScreenConfig()
{
  ScreenConfiguration config;
  hal::GetCurrentScreenConfiguration(&config);
  return config;
}

nsIntSize
PuppetWidget::GetScreenDimensions()
{
  nsIntRect r = ScreenConfig().rect();
  return nsIntSize(r.Width(), r.Height());
}

NS_IMETHODIMP
PuppetScreen::GetRect(int32_t *outLeft,  int32_t *outTop,
                      int32_t *outWidth, int32_t *outHeight)
{
  nsIntRect r = ScreenConfig().rect();
  r.GetRect(outLeft, outTop, outWidth, outHeight);
  return NS_OK;
}

NS_IMETHODIMP
PuppetScreen::GetAvailRect(int32_t *outLeft,  int32_t *outTop,
                           int32_t *outWidth, int32_t *outHeight)
{
  return GetRect(outLeft, outTop, outWidth, outHeight);
}

NS_IMETHODIMP
PuppetScreen::GetPixelDepth(int32_t *aPixelDepth)
{
  *aPixelDepth = ScreenConfig().pixelDepth();
  return NS_OK;
}

NS_IMETHODIMP
PuppetScreen::GetColorDepth(int32_t *aColorDepth)
{
  *aColorDepth = ScreenConfig().colorDepth();
  return NS_OK;
}

NS_IMPL_ISUPPORTS(PuppetScreenManager, nsIScreenManager)

PuppetScreenManager::PuppetScreenManager()
{
    mOneScreen = new PuppetScreen(nullptr);
}

PuppetScreenManager::~PuppetScreenManager()
{
}

NS_IMETHODIMP
PuppetScreenManager::GetPrimaryScreen(nsIScreen** outScreen)
{
  NS_IF_ADDREF(*outScreen = mOneScreen.get());
  return NS_OK;
}

NS_IMETHODIMP
PuppetScreenManager::ScreenForRect(int32_t inLeft,
                                   int32_t inTop,
                                   int32_t inWidth,
                                   int32_t inHeight,
                                   nsIScreen** outScreen)
{
  return GetPrimaryScreen(outScreen);
}

nsIWidgetListener*
PuppetWidget::GetCurrentWidgetListener()
{
  if (!mPreviouslyAttachedWidgetListener ||
      !mAttachedWidgetListener) {
    return mAttachedWidgetListener;
  }

  if (mAttachedWidgetListener->GetView()->IsPrimaryFramePaintSuppressed()) {
    return mPreviouslyAttachedWidgetListener;
  }

  return mAttachedWidgetListener;
}

void
PuppetWidget::SetCandidateWindowForPlugin(
                const CandidateWindowPosition& aPosition)
{
  if (!mTabChild) {
    return;
  }

  mTabChild->SendSetCandidateWindowForPlugin(aPosition);
}

void
PuppetWidget::ZoomToRect(const uint32_t& aPresShellId,
                         const FrameMetrics::ViewID& aViewId,
                         const CSSRect& aRect,
                         const uint32_t& aFlags)
{
  if (!mTabChild) {
    return;
  }

  mTabChild->ZoomToRect(aPresShellId, aViewId, aRect, aFlags);
}

void
PuppetWidget::LookUpDictionary(
                const nsAString& aText,
                const nsTArray<mozilla::FontRange>& aFontRangeArray,
                const bool aIsVertical,
                const LayoutDeviceIntPoint& aPoint)
{
  if (!mTabChild) {
    return;
  }

  mTabChild->SendLookUpDictionary(nsString(aText), aFontRangeArray, aIsVertical, aPoint);
}

bool
PuppetWidget::HasPendingInputEvent()
{
  if (!mTabChild) {
    return false;
  }

  bool ret = false;

  mTabChild->GetIPCChannel()->PeekMessages(
    [&ret](const IPC::Message& aMsg) -> bool {
      if (nsContentUtils::IsMessageInputEvent(aMsg)) {
        ret = true;
        return false; // Stop peeking.
      }
      return true;
    }
  );

  return ret;
}

void
PuppetWidget::HandledWindowedPluginKeyEvent(
                const NativeEventData& aKeyEventData,
                bool aIsConsumed)
{
  if (NS_WARN_IF(mKeyEventInPluginCallbacks.IsEmpty())) {
    return;
  }
  nsCOMPtr<nsIKeyEventInPluginCallback> callback =
    mKeyEventInPluginCallbacks[0];
  MOZ_ASSERT(callback);
  mKeyEventInPluginCallbacks.RemoveElementAt(0);
  callback->HandledWindowedPluginKeyEvent(aKeyEventData, aIsConsumed);
}

nsresult
PuppetWidget::OnWindowedPluginKeyEvent(const NativeEventData& aKeyEventData,
                                       nsIKeyEventInPluginCallback* aCallback)
{
  if (NS_WARN_IF(!mTabChild)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (NS_WARN_IF(!mTabChild->SendOnWindowedPluginKeyEvent(aKeyEventData))) {
    return NS_ERROR_FAILURE;
  }
  mKeyEventInPluginCallbacks.AppendElement(aCallback);
  return NS_SUCCESS_EVENT_HANDLED_ASYNCHRONOUSLY;
}

// TextEventDispatcherListener

NS_IMETHODIMP
PuppetWidget::NotifyIME(TextEventDispatcher* aTextEventDispatcher,
                        const IMENotification& aIMENotification)
{
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

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP_(IMENotificationRequests)
PuppetWidget::GetIMENotificationRequests()
{
  if (mInputContext.mIMEState.mEnabled == IMEState::PLUGIN) {
    // If a plugin has focus, we cannot receive text nor selection change
    // in the plugin.  Therefore, PuppetWidget needs to receive only position
    // change event for updating the editor rect cache.
    return IMENotificationRequests(
             mIMENotificationRequestsOfParent.mWantUpdates |
             IMENotificationRequests::NOTIFY_POSITION_CHANGE);
  }
  return IMENotificationRequests(
           mIMENotificationRequestsOfParent.mWantUpdates |
           IMENotificationRequests::NOTIFY_TEXT_CHANGE |
           IMENotificationRequests::NOTIFY_POSITION_CHANGE);
}

NS_IMETHODIMP_(void)
PuppetWidget::OnRemovedFrom(TextEventDispatcher* aTextEventDispatcher)
{
  MOZ_ASSERT(aTextEventDispatcher == mTextEventDispatcher);
}

NS_IMETHODIMP_(void)
PuppetWidget::WillDispatchKeyboardEvent(
                TextEventDispatcher* aTextEventDispatcher,
                WidgetKeyboardEvent& aKeyboardEvent,
                uint32_t aIndexOfKeypress,
                void* aData)
{
  MOZ_ASSERT(aTextEventDispatcher == mTextEventDispatcher);
}

} // namespace widget
} // namespace mozilla
