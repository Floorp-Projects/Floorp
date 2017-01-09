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
#include "mozilla/Hal.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/layers/APZChild.h"
#include "mozilla/layers/PLayerTransactionChild.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEvents.h"
#include "mozilla/Unused.h"
#include "PuppetWidget.h"
#include "nsContentUtils.h"
#include "nsIWidgetListener.h"
#include "imgIContainer.h"
#include "nsView.h"

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

NS_IMPL_ISUPPORTS_INHERITED0(PuppetWidget, nsBaseWidget)

PuppetWidget::PuppetWidget(TabChild* aTabChild)
  : mTabChild(aTabChild)
  , mMemoryPressureObserver(nullptr)
  , mDPI(-1)
  , mRounding(-1)
  , mDefaultScale(-1)
  , mCursorHotspotX(0)
  , mCursorHotspotY(0)
  , mNativeKeyCommandsValid(false)
{
  mSingleLineCommands.SetCapacity(4);
  mMultiLineCommands.SetCapacity(4);
  mRichTextCommands.SetCapacity(4);

  // Setting 'Unknown' means "not yet cached".
  mInputContext.mIMEState.mEnabled = IMEState::UNKNOWN;
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
    Resize(mBounds.x, mBounds.y, mBounds.width, mBounds.height, false);
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
    mIMEPreferenceOfParent = nsIMEUpdatePreference();
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
    Resize(mBounds.width, mBounds.height, false);
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
      GetCurrentWidgetListener()->WindowResized(this, mBounds.width, mBounds.height);
    }
    mAttachedWidgetListener->WindowResized(this, mBounds.width, mBounds.height);
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
      w->Resize(configuration.mBounds.x, configuration.mBounds.y,
                configuration.mBounds.width, configuration.mBounds.height,
                true);
    } else if (bounds.TopLeft() != configuration.mBounds.TopLeft()) {
      w->Move(configuration.mBounds.x, configuration.mBounds.y);
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

  if (!mDirtyRegion.IsEmpty() && !mPaintTask.IsPending()) {
    mPaintTask = new PaintTask(this);
    NS_DispatchToCurrentThread(mPaintTask.get());
    return;
  }
}

void
PuppetWidget::InitEvent(WidgetGUIEvent& event, LayoutDeviceIntPoint* aPoint)
{
  if (nullptr == aPoint) {
    event.mRefPoint = LayoutDeviceIntPoint(0, 0);
  } else {
    // use the point override if provided
    event.mRefPoint = *aPoint;
  }
  event.mTime = PR_Now() / 1000;
}

nsresult
PuppetWidget::DispatchEvent(WidgetGUIEvent* event, nsEventStatus& aStatus)
{
#ifdef DEBUG
  debug_DumpEvent(stdout, event->mWidget, event, "PuppetWidget", 0);
#endif

  MOZ_ASSERT(!mChild || mChild->mWindowType == eWindowType_popup,
             "Unexpected event dispatch!");

  AutoCacheNativeKeyCommands autoCache(this);
  if ((event->mFlags.mIsSynthesizedForTests ||
       event->mFlags.mIsSuppressedOrDelayed) && !mNativeKeyCommandsValid) {
    WidgetKeyboardEvent* keyEvent = event->AsKeyboardEvent();
    if (keyEvent) {
      mTabChild->RequestNativeKeyBindings(&autoCache, keyEvent);
    }
  }

  if (event->mClass == eCompositionEventClass) {
    // Store the latest native IME context of parent process's widget or
    // TextEventDispatcher if it's in this process.
    WidgetCompositionEvent* compositionEvent = event->AsCompositionEvent();
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

  aStatus = nsEventStatus_eIgnore;

  if (GetCurrentWidgetListener()) {
    aStatus = GetCurrentWidgetListener()->HandleEvent(event, mUseAttachedEvents);
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
    aModifierFlags, nsString(aCharacters), nsString(aUnmodifiedCharacters),
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
    aModifierFlags, notifier.SaveObserver());
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
PuppetWidget::SynthesizeNativeMouseScrollEvent(mozilla::LayoutDeviceIntPoint aPoint,
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
    aDeltaX, aDeltaY, aDeltaZ, aModifierFlags, aAdditionalFlags,
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
    aPoint, aPointerPressure, aPointerOrientation,
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
PuppetWidget::SetConfirmedTargetAPZC(uint64_t aInputBlockId,
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

bool
PuppetWidget::ExecuteNativeKeyBinding(NativeKeyBindingsType aType,
                                      const mozilla::WidgetKeyboardEvent& aEvent,
                                      DoCommandCallback aCallback,
                                      void* aCallbackData)
{
  // B2G doesn't have native key bindings.
#ifdef MOZ_WIDGET_GONK
  return false;
#else // #ifdef MOZ_WIDGET_GONK
  AutoCacheNativeKeyCommands autoCache(this);
  if (!aEvent.mWidget && !mNativeKeyCommandsValid) {
    MOZ_ASSERT(!aEvent.mFlags.mIsSynthesizedForTests);
    // Abort if untrusted to avoid leaking system settings
    if (NS_WARN_IF(!aEvent.IsTrusted())) {
      return false;
    }
    mTabChild->RequestNativeKeyBindings(&autoCache, &aEvent);
  }

  MOZ_ASSERT(mNativeKeyCommandsValid);

  const nsTArray<mozilla::CommandInt>* commands = nullptr;
  switch (aType) {
    case nsIWidget::NativeKeyBindingsForSingleLineEditor:
      commands = &mSingleLineCommands;
      break;
    case nsIWidget::NativeKeyBindingsForMultiLineEditor:
      commands = &mMultiLineCommands;
      break;
    case nsIWidget::NativeKeyBindingsForRichTextEditor:
      commands = &mRichTextCommands;
      break;
    default:
      MOZ_CRASH("Invalid type");
      break;
  }

  if (commands->IsEmpty()) {
    return false;
  }

  for (uint32_t i = 0; i < commands->Length(); i++) {
    aCallback(static_cast<mozilla::Command>((*commands)[i]), aCallbackData);
  }
  return true;
#endif
}

LayerManager*
PuppetWidget::GetLayerManager(PLayerTransactionChild* aShadowManager,
                              LayersBackend aBackendHint,
                              LayerManagerPersistence aPersistence)
{
  if (!mLayerManager) {
    mLayerManager = new ClientLayerManager(this);
  }
  ShadowLayerForwarder* lf = mLayerManager->AsShadowForwarder();
  if (lf && !lf->HasShadowManager() && aShadowManager) {
    lf->SetShadowManager(aShadowManager);
  }
  return mLayerManager;
}

LayerManager*
PuppetWidget::RecreateLayerManager(PLayerTransactionChild* aShadowManager)
{
  mLayerManager = new ClientLayerManager(this);
  if (ShadowLayerForwarder* lf = mLayerManager->AsShadowForwarder()) {
    lf->SetShadowManager(aShadowManager);
  }
  return mLayerManager;
}

nsresult
PuppetWidget::RequestIMEToCommitComposition(bool aCancel)
{
#ifdef MOZ_CROSS_PROCESS_IME
  if (!mTabChild) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(!Destroyed());

  // There must not be composition which is caused by the PuppetWidget instance.
  if (NS_WARN_IF(!mNativeIMEContext.IsValid())) {
    return NS_OK;
  }

  RefPtr<TextComposition> composition =
    IMEStateManager::GetTextCompositionFor(this);
  // This method shouldn't be called when there is no text composition instance.
  if (NS_WARN_IF(!composition)) {
    return NS_OK;
  }

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

  // NOTE: PuppetWidget might be destroyed already.

#endif // #ifdef MOZ_CROSS_PROCESS_IME

  return NS_OK;
}

nsresult
PuppetWidget::NotifyIMEInternal(const IMENotification& aIMENotification)
{
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

#ifndef MOZ_CROSS_PROCESS_IME
  return;
#endif

  if (!mTabChild) {
    return;
  }
  mTabChild->SendSetInputContext(
    static_cast<int32_t>(aContext.mIMEState.mEnabled),
    static_cast<int32_t>(aContext.mIMEState.mOpen),
    aContext.mHTMLInputType,
    aContext.mHTMLInputInputmode,
    aContext.mActionHint,
    static_cast<int32_t>(aAction.mCause),
    static_cast<int32_t>(aAction.mFocusChange));
}

InputContext
PuppetWidget::GetInputContext()
{
#ifndef MOZ_CROSS_PROCESS_IME
  return InputContext();
#endif

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
#ifndef MOZ_CROSS_PROCESS_IME
  return NS_OK;
#endif

  if (!mTabChild)
    return NS_ERROR_FAILURE;

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

  mIMEPreferenceOfParent = nsIMEUpdatePreference();
  if (!mTabChild->SendNotifyIMEFocus(mContentCache, aIMENotification,
                                     &mIMEPreferenceOfParent)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
PuppetWidget::NotifyIMEOfCompositionUpdate(
                const IMENotification& aIMENotification)
{
#ifndef MOZ_CROSS_PROCESS_IME
  return NS_OK;
#endif

  NS_ENSURE_TRUE(mTabChild, NS_ERROR_FAILURE);

  if (mInputContext.mIMEState.mEnabled != IMEState::PLUGIN &&
      NS_WARN_IF(!mContentCache.CacheSelection(this, &aIMENotification))) {
    return NS_ERROR_FAILURE;
  }
  mTabChild->SendNotifyIMECompositionUpdate(mContentCache, aIMENotification);
  return NS_OK;
}

nsIMEUpdatePreference
PuppetWidget::GetIMEUpdatePreference()
{
#ifdef MOZ_CROSS_PROCESS_IME
  // e10s requires IME content cache in in the TabParent for handling query
  // content event only with the parent process.  Therefore, this process
  // needs to receive a lot of information from the focused editor to sent
  // the latest content to the parent process.
  if (mInputContext.mIMEState.mEnabled == IMEState::PLUGIN) {
    // But if a plugin has focus, we cannot receive text nor selection change
    // in the plugin.  Therefore, PuppetWidget needs to receive only position
    // change event for updating the editor rect cache.
    return nsIMEUpdatePreference(mIMEPreferenceOfParent.mWantUpdates |
                                 nsIMEUpdatePreference::NOTIFY_POSITION_CHANGE);
  }
  return nsIMEUpdatePreference(mIMEPreferenceOfParent.mWantUpdates |
                               nsIMEUpdatePreference::NOTIFY_TEXT_CHANGE |
                               nsIMEUpdatePreference::NOTIFY_POSITION_CHANGE );
#else
  // B2G doesn't handle IME as widget-level.
  return nsIMEUpdatePreference();
#endif
}

nsresult
PuppetWidget::NotifyIMEOfTextChange(const IMENotification& aIMENotification)
{
  MOZ_ASSERT(aIMENotification.mMessage == NOTIFY_IME_OF_TEXT_CHANGE,
             "Passed wrong notification");

#ifndef MOZ_CROSS_PROCESS_IME
  return NS_OK;
#endif

  if (!mTabChild)
    return NS_ERROR_FAILURE;

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
  if (mIMEPreferenceOfParent.WantTextChange()) {
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

#ifndef MOZ_CROSS_PROCESS_IME
  return NS_OK;
#endif

  if (!mTabChild)
    return NS_ERROR_FAILURE;

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
#ifndef MOZ_CROSS_PROCESS_IME
  return NS_OK;
#endif
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
  if (mIMEPreferenceOfParent.WantPositionChanged()) {
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

  mCursor = nsCursor(-1);
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

    if (mozilla::layers::LayersBackend::LAYERS_CLIENT == mLayerManager->GetBackendType()) {
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
      Preferences::GetBool("browser.tabs.remote.desktopbehavior", false) &&
      mWindowType == eWindowType_popup) {
    NS_WARNING("Trying to paint an e10s popup in the child process!");
    return false;
  }

  return mVisible;
}

float
PuppetWidget::GetDPI()
{
  if (mDPI < 0) {
    if (mTabChild) {
      mTabChild->GetDPI(&mDPI);
    } else {
      mDPI = 96.0;
    }
  }

  return mDPI;
}

double
PuppetWidget::GetDefaultScaleInternal()
{
  if (mDefaultScale < 0) {
    if (mTabChild) {
      mTabChild->GetDefaultScale(&mDefaultScale);
    } else {
      mDefaultScale = 1;
    }
  }

  return mDefaultScale;
}

int32_t
PuppetWidget::RoundsWidgetCoordinatesTo()
{
  if (mRounding < 0) {
    if (mTabChild) {
      mTabChild->GetWidgetRounding(&mRounding);
    } else {
      mRounding = 1;
    }
  }

  return mRounding;
}

void*
PuppetWidget::GetNativeData(uint32_t aDataType)
{
  switch (aDataType) {
  case NS_NATIVE_SHAREABLE_WINDOW: {
    MOZ_ASSERT(mTabChild, "Need TabChild to get the nativeWindow from!");
    mozilla::WindowsHandle nativeData = 0;
    if (mTabChild) {
      mTabChild->SendGetWidgetNativeData(&nativeData);
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

nsIntPoint
PuppetWidget::GetChromeDimensions()
{
  if (!GetOwningTabChild()) {
    NS_WARNING("PuppetWidget without Tab does not have chrome information.");
    return nsIntPoint();
  }
  return GetOwningTabChild()->GetChromeDisplacement().ToUnknownPoint();
}

nsIntPoint
PuppetWidget::GetWindowPosition()
{
  if (!GetOwningTabChild()) {
    return nsIntPoint();
  }

  int32_t winX, winY, winW, winH;
  NS_ENSURE_SUCCESS(GetOwningTabChild()->GetDimensions(0, &winX, &winY, &winW, &winH), nsIntPoint());
  return nsIntPoint(winX, winY) + GetOwningTabChild()->GetClientOffset().ToUnknownPoint();
}

LayoutDeviceIntRect
PuppetWidget::GetScreenBounds()
{
  return LayoutDeviceIntRect(WidgetToScreenOffset(), mBounds.Size());
}

uint32_t PuppetWidget::GetMaxTouchPoints() const
{
  static uint32_t sTouchPoints = 0;
  static bool sIsInitialized = false;
  if (sIsInitialized) {
    return sTouchPoints;
  }
  if (mTabChild) {
    mTabChild->GetMaxTouchPoints(&sTouchPoints);
    sIsInitialized = true;
  }
  return sTouchPoints;
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
  return nsIntSize(r.width, r.height);
}

NS_IMETHODIMP
PuppetScreen::GetId(uint32_t *outId)
{
  *outId = 1;
  return NS_OK;
}

NS_IMETHODIMP
PuppetScreen::GetRect(int32_t *outLeft,  int32_t *outTop,
                      int32_t *outWidth, int32_t *outHeight)
{
  nsIntRect r = ScreenConfig().rect();
  *outLeft = r.x;
  *outTop = r.y;
  *outWidth = r.width;
  *outHeight = r.height;
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

NS_IMETHODIMP
PuppetScreen::GetRotation(uint32_t* aRotation)
{
  NS_WARNING("Attempt to get screen rotation through nsIScreen::GetRotation().  Nothing should know or care this in sandboxed contexts.  If you want *orientation*, use hal.");
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
PuppetScreen::SetRotation(uint32_t aRotation)
{
  NS_WARNING("Attempt to set screen rotation through nsIScreen::GetRotation().  Nothing should know or care this in sandboxed contexts.  If you want *orientation*, use hal.");
  return NS_ERROR_NOT_AVAILABLE;
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
PuppetScreenManager::ScreenForId(uint32_t aId,
                                 nsIScreen** outScreen)
{
  NS_IF_ADDREF(*outScreen = mOneScreen.get());
  return NS_OK;
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

NS_IMETHODIMP
PuppetScreenManager::ScreenForNativeWidget(void* aWidget,
                                           nsIScreen** outScreen)
{
  return GetPrimaryScreen(outScreen);
}

NS_IMETHODIMP
PuppetScreenManager::GetNumberOfScreens(uint32_t* aNumberOfScreens)
{
  *aNumberOfScreens = 1;
  return NS_OK;
}

NS_IMETHODIMP
PuppetScreenManager::GetSystemDefaultScale(float *aDefaultScale)
{
  *aDefaultScale = 1.0f;
  return NS_OK;
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
      if ((aMsg.type() & mozilla::dom::PBrowser::PBrowserStart)
          == mozilla::dom::PBrowser::PBrowserStart) {
        switch (aMsg.type()) {
          case mozilla::dom::PBrowser::Msg_RealMouseMoveEvent__ID:
          case mozilla::dom::PBrowser::Msg_SynthMouseMoveEvent__ID:
          case mozilla::dom::PBrowser::Msg_RealMouseButtonEvent__ID:
          case mozilla::dom::PBrowser::Msg_RealKeyEvent__ID:
          case mozilla::dom::PBrowser::Msg_MouseWheelEvent__ID:
          case mozilla::dom::PBrowser::Msg_RealTouchEvent__ID:
          case mozilla::dom::PBrowser::Msg_RealTouchMoveEvent__ID:
          case mozilla::dom::PBrowser::Msg_RealDragEvent__ID:
          case mozilla::dom::PBrowser::Msg_UpdateDimensions__ID:
          case mozilla::dom::PBrowser::Msg_MouseEvent__ID:
          case mozilla::dom::PBrowser::Msg_KeyEvent__ID:
            ret = true;
            return false;  // Stop peeking.
        }
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

} // namespace widget
} // namespace mozilla
