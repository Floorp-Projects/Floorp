/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PBrowserChild.h"
#include "BasicLayers.h"
#if defined(MOZ_ENABLE_D3D10_LAYER)
# include "LayerManagerD3D10.h"
#endif

#include "gfxPlatform.h"
#include "mozilla/Hal.h"
#include "PuppetWidget.h"

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
nsIWidget::CreatePuppetWidget(PBrowserChild *aTabChild)
{
  NS_ABORT_IF_FALSE(nsIWidget::UsePuppetWidgets(),
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
  return !IsPopup(aInitData);
}


// Arbitrary, fungible.
const size_t PuppetWidget::kMaxDimension = 4000;

NS_IMPL_ISUPPORTS_INHERITED1(PuppetWidget, nsBaseWidget,
                             nsISupportsWeakReference)

PuppetWidget::PuppetWidget(PBrowserChild *aTabChild)
  : mTabChild(aTabChild)
  , mDPI(-1)
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
                     EVENT_CALLBACK   aHandleEventFunction,
                     nsDeviceContext *aContext,
                     nsWidgetInitData *aInitData)
{
  NS_ABORT_IF_FALSE(!aNativeParent, "got a non-Puppet native parent");

  BaseCreate(nsnull, aRect, aHandleEventFunction, aContext, aInitData);

  mBounds = aRect;
  mEnabled = true;
  mVisible = true;

  mSurface = gfxPlatform::GetPlatform()
             ->CreateOffscreenSurface(gfxIntSize(1, 1),
                                      gfxASurface::ContentFromFormat(gfxASurface::ImageFormatARGB32));

  mIMEComposing = false;
  if (MightNeedIMEFocus(aInitData)) {
    PRUint32 chromeSeqno;
    mTabChild->SendNotifyIMEFocus(false, &mIMEPreference, &chromeSeqno);
    mIMELastBlurSeqno = mIMELastReceivedSeqno = chromeSeqno;
  }

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

already_AddRefed<nsIWidget>
PuppetWidget::CreateChild(const nsIntRect  &aRect,
                          EVENT_CALLBACK   aHandleEventFunction,
                          nsDeviceContext *aContext,
                          nsWidgetInitData *aInitData,
                          bool             aForceUseIWidgetParent)
{
  bool isPopup = IsPopup(aInitData);
  nsCOMPtr<nsIWidget> widget = nsIWidget::CreatePuppetWidget(mTabChild);
  return ((widget &&
           NS_SUCCEEDED(widget->Create(isPopup ? nsnull: this, nsnull, aRect,
                                       aHandleEventFunction,
                                       aContext, aInitData))) ?
          widget.forget() : nsnull);
}

NS_IMETHODIMP
PuppetWidget::Destroy()
{
  Base::OnDestroy();
  Base::Destroy();
  mPaintTask.Revoke();
  mChild = nsnull;
  if (mLayerManager) {
    mLayerManager->Destroy();
  }
  mLayerManager = nsnull;
  mTabChild = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
PuppetWidget::Show(bool aState)
{
  NS_ASSERTION(mEnabled,
               "does it make sense to Show()/Hide() a disabled widget?");

  bool wasVisible = mVisible;
  mVisible = aState;

  if (!wasVisible && mVisible) {
    Resize(mBounds.width, mBounds.height, false);
  }

  return NS_OK;
}

NS_IMETHODIMP
PuppetWidget::Resize(PRInt32 aWidth,
                     PRInt32 aHeight,
                     bool    aRepaint)
{
  nsIntRect oldBounds = mBounds;
  mBounds.SizeTo(nsIntSize(aWidth, aHeight));

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

  if (!oldBounds.IsEqualEdges(mBounds)) {
    DispatchResizeEvent();
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
                       nsCAutoString("PuppetWidget"), nsnull);
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
PuppetWidget::InitEvent(nsGUIEvent& event, nsIntPoint* aPoint)
{
  if (nsnull == aPoint) {
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
PuppetWidget::DispatchEvent(nsGUIEvent* event, nsEventStatus& aStatus)
{
#ifdef DEBUG
  debug_DumpEvent(stdout, event->widget, event,
                  nsCAutoString("PuppetWidget"), nsnull);
#endif

  NS_ABORT_IF_FALSE(!mChild || mChild->mWindowType == eWindowType_popup,
                    "Unexpected event dispatch!");

  aStatus = nsEventStatus_eIgnore;

  NS_ABORT_IF_FALSE(mViewCallback, "No view callback!");

  if (event->message == NS_COMPOSITION_START) {
    mIMEComposing = true;
  }
  switch (event->eventStructType) {
  case NS_COMPOSITION_EVENT:
    mIMELastReceivedSeqno = static_cast<nsCompositionEvent*>(event)->seqno;
    if (mIMELastReceivedSeqno < mIMELastBlurSeqno)
      return NS_OK;
    break;
  case NS_TEXT_EVENT:
    mIMELastReceivedSeqno = static_cast<nsTextEvent*>(event)->seqno;
    if (mIMELastReceivedSeqno < mIMELastBlurSeqno)
      return NS_OK;
    break;
  case NS_SELECTION_EVENT:
    mIMELastReceivedSeqno = static_cast<nsSelectionEvent*>(event)->seqno;
    if (mIMELastReceivedSeqno < mIMELastBlurSeqno)
      return NS_OK;
    break;
  }
  aStatus = (*mViewCallback)(event);

  if (event->message == NS_COMPOSITION_END) {
    mIMEComposing = false;
  }

  return NS_OK;
}

LayerManager*
PuppetWidget::GetLayerManager(PLayersChild* aShadowManager,
                              LayersBackend aBackendHint,
                              LayerManagerPersistence aPersistence,
                              bool* aAllowRetaining)
{
  if (!mLayerManager) {
    // The backend hint is a temporary placeholder until Azure, when
    // all content-process layer managers will be BasicLayerManagers.
#if defined(MOZ_ENABLE_D3D10_LAYER)
    if (LayerManager::LAYERS_D3D10 == aBackendHint) {
      nsRefPtr<LayerManagerD3D10> m = new LayerManagerD3D10(this);
      m->AsShadowForwarder()->SetShadowManager(aShadowManager);
      if (m->Initialize()) {
        mLayerManager = m;
      }
    }
#endif
    if (!mLayerManager) {
      mLayerManager = new BasicShadowLayerManager(this);
      mLayerManager->AsShadowForwarder()->SetShadowManager(aShadowManager);
    }
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
  nsEventStatus status;
  nsTextEvent textEvent(true, NS_TEXT_TEXT, this);
  InitEvent(textEvent, nsnull);
  textEvent.seqno = mIMELastReceivedSeqno;
  // SendEndIMEComposition is always called since ResetInputState
  // should always be called even if we aren't composing something.
  if (!mTabChild ||
      !mTabChild->SendEndIMEComposition(aCancel, &textEvent.theText)) {
    return NS_ERROR_FAILURE;
  }

  if (!mIMEComposing)
    return NS_OK;

  DispatchEvent(&textEvent, status);

  nsCompositionEvent compEvent(true, NS_COMPOSITION_END, this);
  InitEvent(compEvent, nsnull);
  compEvent.seqno = mIMELastReceivedSeqno;
  DispatchEvent(&compEvent, status);
  return NS_OK;
}

NS_IMETHODIMP
PuppetWidget::ResetInputState()
{
  return IMEEndComposition(false);
}

NS_IMETHODIMP
PuppetWidget::CancelComposition()
{
  return IMEEndComposition(true);
}

NS_IMETHODIMP_(void)
PuppetWidget::SetInputContext(const InputContext& aContext,
                              const InputContextAction& aAction)
{
  if (!mTabChild) {
    return;
  }
  mTabChild->SendSetInputContext(
    static_cast<PRInt32>(aContext.mIMEState.mEnabled),
    static_cast<PRInt32>(aContext.mIMEState.mOpen),
    aContext.mHTMLInputType,
    aContext.mActionHint,
    static_cast<PRInt32>(aAction.mCause),
    static_cast<PRInt32>(aAction.mFocusChange));
}

NS_IMETHODIMP_(InputContext)
PuppetWidget::GetInputContext()
{
  InputContext context;
  if (mTabChild) {
    PRInt32 enabled, open;
    mTabChild->SendGetInputContext(&enabled, &open);
    context.mIMEState.mEnabled = static_cast<IMEState::Enabled>(enabled);
    context.mIMEState.mOpen = static_cast<IMEState::Open>(open);
  }
  return context;
}

NS_IMETHODIMP
PuppetWidget::OnIMEFocusChange(bool aFocus)
{
  if (!mTabChild)
    return NS_ERROR_FAILURE;

  if (aFocus) {
    nsEventStatus status;
    nsQueryContentEvent queryEvent(true, NS_QUERY_TEXT_CONTENT, this);
    InitEvent(queryEvent, nsnull);
    // Query entire content
    queryEvent.InitForQueryTextContent(0, PR_UINT32_MAX);
    DispatchEvent(&queryEvent, status);

    if (queryEvent.mSucceeded) {
      mTabChild->SendNotifyIMETextHint(queryEvent.mReply.mString);
    }
  } else {
    // ResetInputState might not have been called yet
    ResetInputState();
  }

  PRUint32 chromeSeqno;
  mIMEPreference.mWantUpdates = false;
  mIMEPreference.mWantHints = false;
  if (!mTabChild->SendNotifyIMEFocus(aFocus, &mIMEPreference, &chromeSeqno))
    return NS_ERROR_FAILURE;

  if (aFocus) {
    if (!mIMEPreference.mWantUpdates && !mIMEPreference.mWantHints)
      // call OnIMEFocusChange on blur but no other updates
      return NS_SUCCESS_IME_NO_UPDATES;
    OnIMESelectionChange(); // Update selection
  } else {
    mIMELastBlurSeqno = chromeSeqno;
  }
  return NS_OK;
}

NS_IMETHODIMP
PuppetWidget::OnIMETextChange(PRUint32 aStart, PRUint32 aEnd, PRUint32 aNewEnd)
{
  if (!mTabChild)
    return NS_ERROR_FAILURE;

  if (mIMEPreference.mWantHints) {
    nsEventStatus status;
    nsQueryContentEvent queryEvent(true, NS_QUERY_TEXT_CONTENT, this);
    InitEvent(queryEvent, nsnull);
    queryEvent.InitForQueryTextContent(0, PR_UINT32_MAX);
    DispatchEvent(&queryEvent, status);

    if (queryEvent.mSucceeded) {
      mTabChild->SendNotifyIMETextHint(queryEvent.mReply.mString);
    }
  }
  if (mIMEPreference.mWantUpdates) {
    mTabChild->SendNotifyIMETextChange(aStart, aEnd, aNewEnd);
  }
  return NS_OK;
}

NS_IMETHODIMP
PuppetWidget::OnIMESelectionChange(void)
{
  if (!mTabChild)
    return NS_ERROR_FAILURE;

  if (mIMEPreference.mWantUpdates) {
    nsEventStatus status;
    nsQueryContentEvent queryEvent(true, NS_QUERY_SELECTED_TEXT, this);
    InitEvent(queryEvent, nsnull);
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
  if (!mTabChild ||
      !mTabChild->SendSetCursor(aCursor)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
PuppetWidget::DispatchPaintEvent()
{
  NS_ABORT_IF_FALSE(!mDirtyRegion.IsEmpty(), "paint event logic messed up");

  nsIntRect dirtyRect = mDirtyRegion.GetBounds();
  nsPaintEvent event(true, NS_PAINT, this);
  event.refPoint.x = dirtyRect.x;
  event.refPoint.y = dirtyRect.y;
  event.region = mDirtyRegion;
  event.willSendDidPaint = true;

  // reset repaint tracking
  mDirtyRegion.SetEmpty();
  mPaintTask.Revoke();

  nsEventStatus status;
  {
#ifdef DEBUG
    debug_DumpPaintEvent(stderr, this, &event,
                         nsCAutoString("PuppetWidget"), nsnull);
#endif

    if (LayerManager::LAYERS_D3D10 == mLayerManager->GetBackendType()) {
      DispatchEvent(&event, status);
    } else {
      nsRefPtr<gfxContext> ctx = new gfxContext(mSurface);
      ctx->Rectangle(gfxRect(0,0,0,0));
      ctx->Clip();
      AutoLayerManagerSetup setupLayerManager(this, ctx,
                                              BasicLayerManager::BUFFER_NONE);
      DispatchEvent(&event, status);  
    }
  }

  nsPaintEvent didPaintEvent(true, NS_DID_PAINT, this);
  DispatchEvent(&didPaintEvent, status);

  return NS_OK;
}

nsresult
PuppetWidget::DispatchResizeEvent()
{
  nsSizeEvent event(true, NS_SIZE, this);

  nsIntRect rect = mBounds;     // copy in case something messes with it
  event.windowSize = &rect;
  event.refPoint.x = rect.x;
  event.refPoint.y = rect.y;
  event.mWinWidth = rect.width;
  event.mWinHeight = rect.height;

  nsEventStatus status;
  return DispatchEvent(&event, status);
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
    mWidget->DispatchPaintEvent();
  }
  return NS_OK;
}

float
PuppetWidget::GetDPI()
{
  if (mDPI < 0) {
    NS_ABORT_IF_FALSE(mTabChild, "Need TabChild to get the DPI from!");
    mTabChild->SendGetDPI(&mDPI);
  }

  return mDPI;
}

void*
PuppetWidget::GetNativeData(PRUint32 aDataType)
{
  switch (aDataType) {
  case NS_NATIVE_SHAREABLE_WINDOW: {
    NS_ABORT_IF_FALSE(mTabChild, "Need TabChild to get the nativeWindow from!");
    mozilla::WindowsHandle nativeData = nsnull;
    mTabChild->SendGetWidgetNativeData(&nativeData);
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
  return nsnull;
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
PuppetScreen::GetRect(PRInt32 *outLeft,  PRInt32 *outTop,
                      PRInt32 *outWidth, PRInt32 *outHeight)
{
  nsIntRect r = ScreenConfig().rect();
  *outLeft = r.x;
  *outTop = r.y;
  *outWidth = r.width;
  *outHeight = r.height;
  return NS_OK;
}

NS_IMETHODIMP
PuppetScreen::GetAvailRect(PRInt32 *outLeft,  PRInt32 *outTop,
                           PRInt32 *outWidth, PRInt32 *outHeight)
{
  return GetRect(outLeft, outTop, outWidth, outHeight);
}


NS_IMETHODIMP
PuppetScreen::GetPixelDepth(PRInt32 *aPixelDepth)
{
  *aPixelDepth = ScreenConfig().pixelDepth();
  return NS_OK;
}

NS_IMETHODIMP
PuppetScreen::GetColorDepth(PRInt32 *aColorDepth)
{
  *aColorDepth = ScreenConfig().colorDepth();
  return NS_OK;
}

NS_IMETHODIMP
PuppetScreen::GetRotation(PRUint32* aRotation)
{
  NS_WARNING("Attempt to get screen rotation through nsIScreen::GetRotation().  Nothing should know or care this in sandboxed contexts.  If you want *orientation*, use hal.");
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
PuppetScreen::SetRotation(PRUint32 aRotation)
{
  NS_WARNING("Attempt to set screen rotation through nsIScreen::GetRotation().  Nothing should know or care this in sandboxed contexts.  If you want *orientation*, use hal.");
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMPL_ISUPPORTS1(PuppetScreenManager, nsIScreenManager)

PuppetScreenManager::PuppetScreenManager()
{
    mOneScreen = new PuppetScreen(nsnull);
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
PuppetScreenManager::ScreenForRect(PRInt32 inLeft,
                                   PRInt32 inTop,
                                   PRInt32 inWidth,
                                   PRInt32 inHeight,
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
PuppetScreenManager::GetNumberOfScreens(PRUint32* aNumberOfScreens)
{
  *aNumberOfScreens = 1;
  return NS_OK;
}

}  // namespace widget
}  // namespace mozilla
