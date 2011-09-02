/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/dom/PBrowserChild.h"
#include "BasicLayers.h"
#if defined(MOZ_ENABLE_D3D10_LAYER)
# include "LayerManagerD3D10.h"
#endif

#include "gfxPlatform.h"
#include "PuppetWidget.h"

using namespace mozilla::layers;
using namespace mozilla::widget;
using namespace mozilla::dom;

static void
InvalidateRegion(nsIWidget* aWidget, const nsIntRegion& aRegion)
{
  nsIntRegionRectIterator it(aRegion);
  while(const nsIntRect* r = it.Next()) {
    aWidget->Invalidate(*r, PR_FALSE/*async*/);
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
                     nsIAppShell      *aAppShell,
                     nsIToolkit       *aToolkit,
                     nsWidgetInitData *aInitData)
{
  NS_ABORT_IF_FALSE(!aNativeParent, "got a non-Puppet native parent");

  BaseCreate(nsnull, aRect, aHandleEventFunction, aContext,
             aAppShell, aToolkit, aInitData);

  mBounds = aRect;
  mEnabled = PR_TRUE;
  mVisible = PR_TRUE;

  mSurface = gfxPlatform::GetPlatform()
             ->CreateOffscreenSurface(gfxIntSize(1, 1),
                                      gfxASurface::ContentFromFormat(gfxASurface::ImageFormatARGB32));

  mIMEComposing = PR_FALSE;
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
    Resize(mBounds.x, mBounds.y, mBounds.width, mBounds.height, PR_FALSE);
  }

  return NS_OK;
}

already_AddRefed<nsIWidget>
PuppetWidget::CreateChild(const nsIntRect  &aRect,
                          EVENT_CALLBACK   aHandleEventFunction,
                          nsDeviceContext *aContext,
                          nsIAppShell      *aAppShell,
                          nsIToolkit       *aToolkit,
                          nsWidgetInitData *aInitData,
                          PRBool           aForceUseIWidgetParent)
{
  bool isPopup = IsPopup(aInitData);
  nsCOMPtr<nsIWidget> widget = nsIWidget::CreatePuppetWidget(mTabChild);
  return ((widget &&
           NS_SUCCEEDED(widget->Create(isPopup ? nsnull: this, nsnull, aRect,
                                       aHandleEventFunction,
                                       aContext, aAppShell, aToolkit,
                                       aInitData))) ?
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
PuppetWidget::Show(PRBool aState)
{
  NS_ASSERTION(mEnabled,
               "does it make sense to Show()/Hide() a disabled widget?");

  PRBool wasVisible = mVisible;
  mVisible = aState;

  if (!wasVisible && mVisible) {
    Resize(mBounds.width, mBounds.height, PR_FALSE);
  }

  return NS_OK;
}

NS_IMETHODIMP
PuppetWidget::Resize(PRInt32 aWidth,
                     PRInt32 aHeight,
                     PRBool  aRepaint)
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
PuppetWidget::SetFocus(PRBool aRaise)
{
  // XXX/cjones: someone who knows about event handling needs to
  // decide how this should work.
  return NS_OK;
}

NS_IMETHODIMP
PuppetWidget::Invalidate(const nsIntRect& aRect, PRBool aIsSynchronous)
{
#ifdef DEBUG
  debug_DumpInvalidate(stderr, this, &aRect, aIsSynchronous,
                       nsCAutoString("PuppetWidget"), nsnull);
#endif

  if (mChild) {
    return mChild->Invalidate(aRect, aIsSynchronous);
  }

  mDirtyRegion.Or(mDirtyRegion, aRect);

  if (mDirtyRegion.IsEmpty()) {
    return NS_OK;
  } else if (aIsSynchronous) {
    DispatchPaintEvent();
    return NS_OK;
  } else if (!mPaintTask.IsPending()) {
    mPaintTask = new PaintTask(this);
    return NS_DispatchToCurrentThread(mPaintTask.get());
  }

  return NS_OK;
}

NS_IMETHODIMP
PuppetWidget::Update()
{
  if (mChild) {
    return mChild->Update();
  }

  if (mDirtyRegion.IsEmpty()) {
    return NS_OK;
  }
  return DispatchPaintEvent();
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
    mIMEComposing = PR_TRUE;
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
    mIMEComposing = PR_FALSE;
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
PuppetWidget::IMEEndComposition(PRBool aCancel)
{
  nsEventStatus status;
  nsTextEvent textEvent(PR_TRUE, NS_TEXT_TEXT, this);
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

  nsCompositionEvent compEvent(PR_TRUE, NS_COMPOSITION_END, this);
  InitEvent(compEvent, nsnull);
  compEvent.seqno = mIMELastReceivedSeqno;
  DispatchEvent(&compEvent, status);
  return NS_OK;
}

NS_IMETHODIMP
PuppetWidget::ResetInputState()
{
  return IMEEndComposition(PR_FALSE);
}

NS_IMETHODIMP
PuppetWidget::CancelComposition()
{
  return IMEEndComposition(PR_TRUE);
}

NS_IMETHODIMP
PuppetWidget::SetIMEOpenState(PRBool aState)
{
  if (mTabChild &&
      mTabChild->SendSetIMEOpenState(aState))
    return NS_OK;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PuppetWidget::SetInputMode(const IMEContext& aContext)
{
  if (mTabChild &&
      mTabChild->SendSetInputMode(aContext.mStatus, aContext.mHTMLInputType,
                                  aContext.mActionHint, aContext.mReason))
    return NS_OK;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PuppetWidget::GetIMEOpenState(PRBool *aState)
{
  if (mTabChild &&
      mTabChild->SendGetIMEOpenState(aState))
    return NS_OK;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PuppetWidget::GetInputMode(IMEContext& aContext)
{
  if (mTabChild &&
      mTabChild->SendGetIMEEnabled(&aContext.mStatus))
    return NS_OK;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PuppetWidget::OnIMEFocusChange(PRBool aFocus)
{
  if (!mTabChild)
    return NS_ERROR_FAILURE;

  if (aFocus) {
    nsEventStatus status;
    nsQueryContentEvent queryEvent(PR_TRUE, NS_QUERY_TEXT_CONTENT, this);
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
  mIMEPreference.mWantUpdates = PR_FALSE;
  mIMEPreference.mWantHints = PR_FALSE;
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
    nsQueryContentEvent queryEvent(PR_TRUE, NS_QUERY_TEXT_CONTENT, this);
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
    nsQueryContentEvent queryEvent(PR_TRUE, NS_QUERY_SELECTED_TEXT, this);
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
  nsPaintEvent event(PR_TRUE, NS_PAINT, this);
  event.refPoint.x = dirtyRect.x;
  event.refPoint.x = dirtyRect.y;
  event.region = mDirtyRegion;
  event.willSendDidPaint = PR_TRUE;

  // reset repaint tracking
  mDirtyRegion.SetEmpty();
  mPaintTask.Revoke();

  nsEventStatus status;
  {
#ifdef DEBUG
    debug_DumpPaintEvent(stderr, this, &event,
                         nsCAutoString("PuppetWidget"), nsnull);
#endif

    LayerManager* lm = GetLayerManager();
    if (LayerManager::LAYERS_D3D10 == mLayerManager->GetBackendType()) {
      DispatchEvent(&event, status);
    } else {
      nsRefPtr<gfxContext> ctx = new gfxContext(mSurface);
      AutoLayerManagerSetup setupLayerManager(this, ctx,
                                              BasicLayerManager::BUFFER_NONE);
      DispatchEvent(&event, status);  
    }
  }

  nsPaintEvent didPaintEvent(PR_TRUE, NS_DID_PAINT, this);
  DispatchEvent(&didPaintEvent, status);

  return NS_OK;
}

nsresult
PuppetWidget::DispatchResizeEvent()
{
  nsSizeEvent event(PR_TRUE, NS_SIZE, this);

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

}  // namespace widget
}  // namespace mozilla
