/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "ClientLayerManager.h"
#include "gfxPlatform.h"
#if defined(MOZ_ENABLE_D3D10_LAYER)
# include "LayerManagerD3D10.h"
#endif
#include "mozilla/dom/TabChild.h"
#include "mozilla/Hal.h"
#include "mozilla/layers/CompositorChild.h"
#include "mozilla/layers/PLayerTransactionChild.h"
#include "mozilla/TextEvents.h"
#include "PuppetWidget.h"
#include "nsIWidgetListener.h"

using namespace mozilla::dom;
using namespace mozilla::hal;
using namespace mozilla::layers;
using namespace mozilla::widget;

static void
InvalidateRegion(nsIWidget* aWidget, const nsIntRegion& aRegion)
{
  nsIntRegionRectIterator it(aRegion);
  while(const nsIntRect* r = it.Next()) {
    aWidget->Invalidate(*r);
  }
}

/*static*/ already_AddRefed<nsIWidget>
nsIWidget::CreatePuppetWidget(TabChild* aTabChild)
{
  NS_ABORT_IF_FALSE(!aTabChild || nsIWidget::UsePuppetWidgets(),
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

NS_IMPL_ISUPPORTS_INHERITED1(PuppetWidget, nsBaseWidget,
                             nsISupportsWeakReference)

PuppetWidget::PuppetWidget(TabChild* aTabChild)
  : mTabChild(aTabChild)
  , mDPI(-1)
  , mDefaultScale(-1)
{
  MOZ_COUNT_CTOR(PuppetWidget);
}

PuppetWidget::~PuppetWidget()
{
  MOZ_COUNT_DTOR(PuppetWidget);
}

NS_IMETHODIMP
PuppetWidget::Create(nsIWidget        *aParent,
                     nsNativeWidget   aNativeParent,
                     const nsIntRect  &aRect,
                     nsDeviceContext *aContext,
                     nsWidgetInitData *aInitData)
{
  NS_ABORT_IF_FALSE(!aNativeParent, "got a non-Puppet native parent");

  BaseCreate(nullptr, aRect, aContext, aInitData);

  mBounds = aRect;
  mEnabled = true;
  mVisible = true;

  mSurface = gfxPlatform::GetPlatform()
             ->CreateOffscreenSurface(gfxIntSize(1, 1),
                                      gfxASurface::ContentFromFormat(gfxImageFormatARGB32));

  mIMEComposing = false;
  mNeedIMEStateInit = MightNeedIMEFocus(aInitData);

  PuppetWidget* parent = static_cast<PuppetWidget*>(aParent);
  if (parent) {
    parent->SetChild(this);
    mLayerManager = parent->GetLayerManager();
  }
  else {
    Resize(mBounds.x, mBounds.y, mBounds.width, mBounds.height, false);
  }

  return NS_OK;
}

void
PuppetWidget::InitIMEState()
{
  MOZ_ASSERT(mTabChild);
  if (mNeedIMEStateInit) {
    uint32_t chromeSeqno;
    mTabChild->SendNotifyIMEFocus(false, &mIMEPreference, &chromeSeqno);
    mIMELastBlurSeqno = mIMELastReceivedSeqno = chromeSeqno;
    mNeedIMEStateInit = false;
  }
}

already_AddRefed<nsIWidget>
PuppetWidget::CreateChild(const nsIntRect  &aRect,
                          nsDeviceContext *aContext,
                          nsWidgetInitData *aInitData,
                          bool             aForceUseIWidgetParent)
{
  bool isPopup = IsPopup(aInitData);
  nsCOMPtr<nsIWidget> widget = nsIWidget::CreatePuppetWidget(mTabChild);
  return ((widget &&
           NS_SUCCEEDED(widget->Create(isPopup ? nullptr: this, nullptr, aRect,
                                       aContext, aInitData))) ?
          widget.forget() : nullptr);
}

NS_IMETHODIMP
PuppetWidget::Destroy()
{
  Base::OnDestroy();
  Base::Destroy();
  mPaintTask.Revoke();
  mChild = nullptr;
  if (mLayerManager) {
    mLayerManager->Destroy();
  }
  mLayerManager = nullptr;
  mTabChild = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
PuppetWidget::Show(bool aState)
{
  NS_ASSERTION(mEnabled,
               "does it make sense to Show()/Hide() a disabled widget?");

  bool wasVisible = mVisible;
  mVisible = aState;

  if (mChild) {
    mChild->mVisible = aState;
  }

  if (!mVisible && mLayerManager) {
    mLayerManager->ClearCachedResources();
  }

  if (!wasVisible && mVisible) {
    Resize(mBounds.width, mBounds.height, false);
    Invalidate(mBounds);
  }

  return NS_OK;
}

NS_IMETHODIMP
PuppetWidget::Resize(double aWidth,
                     double aHeight,
                     bool   aRepaint)
{
  nsIntRect oldBounds = mBounds;
  mBounds.SizeTo(nsIntSize(NSToIntRound(aWidth), NSToIntRound(aHeight)));

  if (mChild) {
    return mChild->Resize(aWidth, aHeight, aRepaint);
  }

  // XXX: roc says that |aRepaint| dictates whether or not to
  // invalidate the expanded area
  if (oldBounds.Size() < mBounds.Size() && aRepaint) {
    nsIntRegion dirty(mBounds);
    dirty.Sub(dirty,  oldBounds);
    InvalidateRegion(this, dirty);
  }

  if (!oldBounds.IsEqualEdges(mBounds) && mAttachedWidgetListener) {
    mAttachedWidgetListener->WindowResized(this, mBounds.width, mBounds.height);
  }

  return NS_OK;
}

NS_IMETHODIMP
PuppetWidget::SetFocus(bool aRaise)
{
  // XXX/cjones: someone who knows about event handling needs to
  // decide how this should work.
  return NS_OK;
}

NS_IMETHODIMP
PuppetWidget::Invalidate(const nsIntRect& aRect)
{
#ifdef DEBUG
  debug_DumpInvalidate(stderr, this, &aRect,
                       nsAutoCString("PuppetWidget"), 0);
#endif

  if (mChild) {
    return mChild->Invalidate(aRect);
  }

  mDirtyRegion.Or(mDirtyRegion, aRect);

  if (!mDirtyRegion.IsEmpty() && !mPaintTask.IsPending()) {
    mPaintTask = new PaintTask(this);
    return NS_DispatchToCurrentThread(mPaintTask.get());
  }

  return NS_OK;
}

void
PuppetWidget::InitEvent(WidgetGUIEvent& event, nsIntPoint* aPoint)
{
  if (nullptr == aPoint) {
    event.refPoint.x = 0;
    event.refPoint.y = 0;
  }
  else {
    // use the point override if provided
    event.refPoint.x = aPoint->x;
    event.refPoint.y = aPoint->y;
  }
  event.time = PR_Now() / 1000;
}

NS_IMETHODIMP
PuppetWidget::DispatchEvent(WidgetGUIEvent* event, nsEventStatus& aStatus)
{
#ifdef DEBUG
  debug_DumpEvent(stdout, event->widget, event,
                  nsAutoCString("PuppetWidget"), 0);
#endif

  NS_ABORT_IF_FALSE(!mChild || mChild->mWindowType == eWindowType_popup,
                    "Unexpected event dispatch!");

  aStatus = nsEventStatus_eIgnore;

  if (event->message == NS_COMPOSITION_START) {
    mIMEComposing = true;
  }
  uint32_t seqno = kLatestSeqno;
  switch (event->eventStructType) {
  case NS_COMPOSITION_EVENT:
    seqno = event->AsCompositionEvent()->mSeqno;
    break;
  case NS_TEXT_EVENT:
    seqno = event->AsTextEvent()->mSeqno;
    break;
  case NS_SELECTION_EVENT:
    seqno = event->AsSelectionEvent()->mSeqno;
    break;
  default:
    break;
  }
  if (seqno != kLatestSeqno) {
    mIMELastReceivedSeqno = seqno;
    if (mIMELastReceivedSeqno < mIMELastBlurSeqno) {
      return NS_OK;
    }
  }

  if (mAttachedWidgetListener) {
    aStatus = mAttachedWidgetListener->HandleEvent(event, mUseAttachedEvents);
  }

  if (event->message == NS_COMPOSITION_END) {
    mIMEComposing = false;
  }

  return NS_OK;
}

LayerManager*
PuppetWidget::GetLayerManager(PLayerTransactionChild* aShadowManager,
                              LayersBackend aBackendHint,
                              LayerManagerPersistence aPersistence,
                              bool* aAllowRetaining)
{
  if (!mLayerManager) {
    // The backend hint is a temporary placeholder until Azure, when
    // all content-process layer managers will be BasicLayerManagers.
#if defined(MOZ_ENABLE_D3D10_LAYER)
    if (mozilla::layers::LAYERS_D3D10 == aBackendHint) {
      nsRefPtr<LayerManagerD3D10> m = new LayerManagerD3D10(this);
      m->AsShadowForwarder()->SetShadowManager(aShadowManager);
      if (m->Initialize()) {
        mLayerManager = m;
      }
    }
#endif
    if (!mLayerManager) {
      mLayerManager = new ClientLayerManager(this);
    }
  }
  ShadowLayerForwarder* lf = mLayerManager->AsShadowForwarder();
  if (!lf->HasShadowManager() && aShadowManager) {
    lf->SetShadowManager(aShadowManager);
  }
  if (aAllowRetaining) {
    *aAllowRetaining = true;
  }
  return mLayerManager;
}

gfxASurface*
PuppetWidget::GetThebesSurface()
{
  return mSurface;
}

nsresult
PuppetWidget::IMEEndComposition(bool aCancel)
{
#ifndef MOZ_CROSS_PROCESS_IME
  return NS_OK;
#endif

  nsEventStatus status;
  WidgetTextEvent textEvent(true, NS_TEXT_TEXT, this);
  InitEvent(textEvent, nullptr);
  textEvent.mSeqno = mIMELastReceivedSeqno;
  // SendEndIMEComposition is always called since ResetInputState
  // should always be called even if we aren't composing something.
  if (!mTabChild ||
      !mTabChild->SendEndIMEComposition(aCancel, &textEvent.theText)) {
    return NS_ERROR_FAILURE;
  }

  if (!mIMEComposing)
    return NS_OK;

  DispatchEvent(&textEvent, status);

  WidgetCompositionEvent compEvent(true, NS_COMPOSITION_END, this);
  InitEvent(compEvent, nullptr);
  compEvent.mSeqno = mIMELastReceivedSeqno;
  DispatchEvent(&compEvent, status);
  return NS_OK;
}

NS_IMETHODIMP
PuppetWidget::NotifyIME(NotificationToIME aNotification)
{
  switch (aNotification) {
    case NOTIFY_IME_OF_CURSOR_POS_CHANGED:
    case REQUEST_TO_COMMIT_COMPOSITION:
      return IMEEndComposition(false);
    case REQUEST_TO_CANCEL_COMPOSITION:
      return IMEEndComposition(true);
    case NOTIFY_IME_OF_FOCUS:
      return NotifyIMEOfFocusChange(true);
    case NOTIFY_IME_OF_BLUR:
      return NotifyIMEOfFocusChange(false);
    case NOTIFY_IME_OF_SELECTION_CHANGE:
      return NotifyIMEOfSelectionChange();
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
}

NS_IMETHODIMP_(void)
PuppetWidget::SetInputContext(const InputContext& aContext,
                              const InputContextAction& aAction)
{
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

NS_IMETHODIMP_(InputContext)
PuppetWidget::GetInputContext()
{
#ifndef MOZ_CROSS_PROCESS_IME
  return InputContext();
#endif

  InputContext context;
  if (mTabChild) {
    int32_t enabled, open;
    intptr_t nativeIMEContext;
    mTabChild->SendGetInputContext(&enabled, &open, &nativeIMEContext);
    context.mIMEState.mEnabled = static_cast<IMEState::Enabled>(enabled);
    context.mIMEState.mOpen = static_cast<IMEState::Open>(open);
    context.mNativeIMEContext = reinterpret_cast<void*>(nativeIMEContext);
  }
  return context;
}

nsresult
PuppetWidget::NotifyIMEOfFocusChange(bool aFocus)
{
#ifndef MOZ_CROSS_PROCESS_IME
  return NS_OK;
#endif

  if (!mTabChild)
    return NS_ERROR_FAILURE;

  if (aFocus) {
    nsEventStatus status;
    WidgetQueryContentEvent queryEvent(true, NS_QUERY_TEXT_CONTENT, this);
    InitEvent(queryEvent, nullptr);
    // Query entire content
    queryEvent.InitForQueryTextContent(0, UINT32_MAX);
    DispatchEvent(&queryEvent, status);

    if (queryEvent.mSucceeded) {
      mTabChild->SendNotifyIMETextHint(queryEvent.mReply.mString);
    }
  } else {
    // Might not have been committed composition yet
    IMEEndComposition(false);
  }

  uint32_t chromeSeqno;
  mIMEPreference.mWantUpdates = nsIMEUpdatePreference::NOTIFY_NOTHING;
  mIMEPreference.mWantHints = false;
  if (!mTabChild->SendNotifyIMEFocus(aFocus, &mIMEPreference, &chromeSeqno))
    return NS_ERROR_FAILURE;

  if (aFocus) {
    if ((mIMEPreference.mWantUpdates &
           nsIMEUpdatePreference::NOTIFY_SELECTION_CHANGE) &&
        mIMEPreference.mWantHints) {
      NotifyIMEOfSelectionChange(); // Update selection
    }
  } else {
    mIMELastBlurSeqno = chromeSeqno;
  }
  return NS_OK;
}

nsIMEUpdatePreference
PuppetWidget::GetIMEUpdatePreference()
{
  return mIMEPreference;
}

NS_IMETHODIMP
PuppetWidget::NotifyIMEOfTextChange(uint32_t aStart,
                                    uint32_t aEnd,
                                    uint32_t aNewEnd)
{
#ifndef MOZ_CROSS_PROCESS_IME
  return NS_OK;
#endif

  if (!mTabChild)
    return NS_ERROR_FAILURE;

  if (mIMEPreference.mWantHints) {
    nsEventStatus status;
    WidgetQueryContentEvent queryEvent(true, NS_QUERY_TEXT_CONTENT, this);
    InitEvent(queryEvent, nullptr);
    queryEvent.InitForQueryTextContent(0, UINT32_MAX);
    DispatchEvent(&queryEvent, status);

    if (queryEvent.mSucceeded) {
      mTabChild->SendNotifyIMETextHint(queryEvent.mReply.mString);
    }
  }
  if (mIMEPreference.mWantUpdates & nsIMEUpdatePreference::NOTIFY_TEXT_CHANGE) {
    mTabChild->SendNotifyIMETextChange(aStart, aEnd, aNewEnd);
  }
  return NS_OK;
}

nsresult
PuppetWidget::NotifyIMEOfSelectionChange()
{
#ifndef MOZ_CROSS_PROCESS_IME
  return NS_OK;
#endif

  if (!mTabChild)
    return NS_ERROR_FAILURE;

  if (mIMEPreference.mWantUpdates &
        nsIMEUpdatePreference::NOTIFY_SELECTION_CHANGE) {
    nsEventStatus status;
    WidgetQueryContentEvent queryEvent(true, NS_QUERY_SELECTED_TEXT, this);
    InitEvent(queryEvent, nullptr);
    DispatchEvent(&queryEvent, status);

    if (queryEvent.mSucceeded) {
      mTabChild->SendNotifyIMESelection(mIMELastReceivedSeqno,
                                        queryEvent.GetSelectionStart(),
                                        queryEvent.GetSelectionEnd());
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
PuppetWidget::SetCursor(nsCursor aCursor)
{
  if (mCursor == aCursor) {
    return NS_OK;
  }

  if (mTabChild && !mTabChild->SendSetCursor(aCursor)) {
    return NS_ERROR_FAILURE;
  }

  mCursor = aCursor;

  return NS_OK;
}

nsresult
PuppetWidget::Paint()
{
  NS_ABORT_IF_FALSE(!mDirtyRegion.IsEmpty(), "paint event logic messed up");

  if (!mAttachedWidgetListener)
    return NS_OK;

  nsIntRegion region = mDirtyRegion;

  // reset repaint tracking
  mDirtyRegion.SetEmpty();
  mPaintTask.Revoke();

  mAttachedWidgetListener->WillPaintWindow(this);

  if (mAttachedWidgetListener) {
#ifdef DEBUG
    debug_DumpPaintEvent(stderr, this, region,
                         nsAutoCString("PuppetWidget"), 0);
#endif

    if (mozilla::layers::LAYERS_D3D10 == mLayerManager->GetBackendType()) {
      mAttachedWidgetListener->PaintWindow(this, region);
    } else if (mozilla::layers::LAYERS_CLIENT == mLayerManager->GetBackendType()) {
      // Do nothing, the compositor will handle drawing
      if (mTabChild) {
        mTabChild->NotifyPainted();
      }
    } else {
      nsRefPtr<gfxContext> ctx = new gfxContext(mSurface);
      ctx->Rectangle(gfxRect(0,0,0,0));
      ctx->Clip();
      AutoLayerManagerSetup setupLayerManager(this, ctx,
                                              BUFFER_NONE);
      mAttachedWidgetListener->PaintWindow(this, region);
      if (mTabChild) {
        mTabChild->NotifyPainted();
      }
    }
  }

  if (mAttachedWidgetListener) {
    mAttachedWidgetListener->DidPaintWindow();
  }

  return NS_OK;
}

void
PuppetWidget::SetChild(PuppetWidget* aChild)
{
  NS_ABORT_IF_FALSE(this != aChild, "can't parent a widget to itself");
  NS_ABORT_IF_FALSE(!aChild->mChild,
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

bool
PuppetWidget::NeedsPaint()
{
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

void*
PuppetWidget::GetNativeData(uint32_t aDataType)
{
  switch (aDataType) {
  case NS_NATIVE_SHAREABLE_WINDOW: {
    NS_ABORT_IF_FALSE(mTabChild, "Need TabChild to get the nativeWindow from!");
    mozilla::WindowsHandle nativeData = 0;
    if (mTabChild) {
      mTabChild->SendGetWidgetNativeData(&nativeData);
    }
    return (void*)nativeData;
  }
  case NS_NATIVE_WINDOW:
  case NS_NATIVE_DISPLAY:
  case NS_NATIVE_PLUGIN_PORT:
  case NS_NATIVE_GRAPHIC:
  case NS_NATIVE_SHELLWIDGET:
  case NS_NATIVE_WIDGET:
    NS_WARNING("nsWindow::GetNativeData not implemented for this type");
    break;
  default:
    NS_WARNING("nsWindow::GetNativeData called with bad value");
    break;
  }
  return nullptr;
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

NS_IMPL_ISUPPORTS1(PuppetScreenManager, nsIScreenManager)

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

}  // namespace widget
}  // namespace mozilla
