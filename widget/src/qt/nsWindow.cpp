/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *		John C. Griggs <johng@corel.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIServiceManager.h"
#include "nsQWidget.h"
#include "nsWindow.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "nsRect.h"
#include "nsGfxCIID.h"
#include "nsIPref.h"

#include <qapplication.h>

//JCG #define DBG_JCG = 1

#ifdef DBG_JCG
static PRInt32 gWindowCount = 0;
static PRInt32 gWindowID = 0;
static PRInt32 gChildCount = 0;
static PRInt32 gChildID = 0;
#endif

PRBool   nsWindow::mIsGrabbing        = PR_FALSE;
nsWindow *nsWindow::mGrabWindow       = NULL;
nsQBaseWidget *nsWindow::mFocusWidget = NULL;
PRBool nsWindow::mGlobalsInitialized  = PR_FALSE;
PRBool nsWindow::mRaiseWindows        = PR_TRUE;
PRBool nsWindow::mGotActivate         = PR_FALSE;

NS_IMPL_ISUPPORTS_INHERITED0(nsWindow, nsWidget)

//-------------------------------------------------------------------------
// nsWindow constructor
//-------------------------------------------------------------------------
nsWindow::nsWindow() 
{
#ifdef DBG_JCG
  gWindowCount++;
  mWindowID = gWindowID++;
  printf("JCG: nsWindow CTOR. (%p) ID: %d, Count: %d\n",this,mWindowID,gWindowCount);
#endif
  mIsDialog = PR_FALSE;
  mIsPopup = PR_FALSE;
  mBlockFocusEvents = PR_FALSE;
  mWindowType = eWindowType_child;
  mBorderStyle = eBorderStyle_default;
  // initialize globals
  if (!mGlobalsInitialized) {
    mGlobalsInitialized = PR_TRUE;
 
    // check to see if we should set our raise pref
    nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID);
    if (prefs) {
      PRBool val = PR_TRUE;
      nsresult rv;
      rv = prefs->GetBoolPref("mozilla.widget.raise-on-setfocus",
                              &val);
      if (NS_SUCCEEDED(rv))
        mRaiseWindows = val;
    }
  }
}

//-------------------------------------------------------------------------
// nsWindow destructor
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
#ifdef DBG_JCG
  gWindowCount--;
  printf("JCG: nsWindow DTOR. (%p) ID: %d, Count: %d\n",this,mWindowID,gWindowCount);
#endif
  // make sure that we release the grab indicator here
  if (mGrabWindow == this) {
    mIsGrabbing = PR_FALSE;
    mGrabWindow = NULL;
  }
}

//-------------------------------------------------------------------------
PRBool nsWindow::IsChild() const
{
  return PR_FALSE;
}

//-------------------------------------------------------------------------
void nsWindow::ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY)
{
}

//-------------------------------------------------------------------------
// Setup initial tooltip rectangles
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetTooltips(PRUint32 aNumberOfTips,
                                nsRect *aTooltipAreas[])
{
  return NS_OK;
}

//-------------------------------------------------------------------------
// Update all tooltip rectangles
//-------------------------------------------------------------------------
NS_METHOD nsWindow::UpdateTooltips(nsRect* aNewTips[])
{
  return NS_OK;
}

//-------------------------------------------------------------------------
// Remove all tooltip rectangles
//-------------------------------------------------------------------------
NS_METHOD nsWindow::RemoveTooltips()
{
  return NS_OK;
}

NS_METHOD nsWindow::SetFocus(PRBool aRaise)
{
  if (mBlockFocusEvents) {
    return NS_OK;
  }
  PRBool sendActivate = mGotActivate;

  mGotActivate = PR_FALSE;
  if (mWidget) {
    if (!mWidget->IsTopLevelActive()) {
      if (aRaise && mRaiseWindows) {
        mWidget->RaiseTopLevel(); 
      }
      mBlockFocusEvents = PR_TRUE;
      mWidget->SetTopLevelFocus();
      mBlockFocusEvents = PR_FALSE;
      mGotActivate = PR_TRUE;
    }
    else {
      if (mWidget == mFocusWidget) {
        return NS_OK;
      }
      else {
        mFocusWidget = mWidget;
      }
    }
  }
  DispatchFocusInEvent();
  if (sendActivate) {
    mGotActivate = PR_FALSE;
    DispatchActivateEvent();
  }
  return NS_OK;
}

NS_METHOD nsWindow::PreCreateWidget(nsWidgetInitData *aInitData)
{
  if (nsnull != aInitData) {
    SetWindowType(aInitData->mWindowType);
    SetBorderStyle(aInitData->mBorderStyle);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
// Create the native widget
//-------------------------------------------------------------------------
NS_METHOD nsWindow::CreateNative(QWidget *parentWidget)
{
  switch (mWindowType) {
    case eWindowType_toplevel:
#ifdef DBG_JCG
      printf("JCG: Top Level Create: %p\n",this);
#endif
      break;

    case eWindowType_dialog:
#ifdef DBG_JCG
      printf("JCG: Dialog Create: %p\n",this);
#endif
      mIsDialog = PR_TRUE;
      break;

    case eWindowType_popup:
#ifdef DBG_JCG
      printf("JCG: Popup Create: %p\n",this);
#endif
      mIsPopup = PR_TRUE;
      break;

    case eWindowType_child:
#ifdef DBG_JCG
      printf("JCG: Child Create: %p\n",this);
#endif
      break;
  }
  mWidget = new nsQBaseWidget(this);

  if (!mWidget)
    return NS_ERROR_OUT_OF_MEMORY;

  if (!mWidget->CreateNative(parentWidget,QWidget::tr("nsWindow"),
                             NS_GetQWFlags(mBorderStyle,mWindowType))) {
    delete mWidget;
    mWidget = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (!parentWidget && !mIsDialog) {
    // This is a top-level window. I'm not sure what special actions
    // need to be taken here.
    mIsToplevel = PR_TRUE;
    mListenForResizes = PR_TRUE;
  }
  else {
    mIsToplevel = PR_FALSE;
  }
  return nsWidget::CreateNative(parentWidget);
}


//-------------------------------------------------------------------------
// Initialize all the Callbacks
//-------------------------------------------------------------------------
void nsWindow::InitCallbacks(char *aName)
{
}

//-------------------------------------------------------------------------
// Set the colormap of the window
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetColorMap(nsColorMap *aColorMap)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
// Scroll the bits of a window
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Scroll(PRInt32 aDx,PRInt32 aDy,nsRect *aClipRect)
{
  if (mWidget) {
    mWidget->Scroll(aDx,aDy);
  }
  return NS_OK;
}

/* Processes an Expose Event */
PRBool nsWindow::OnPaint(nsPaintEvent &event)
{
  nsresult result;
  PRInt32 x;
  PRInt32 y;
  PRInt32 width;
  PRInt32 height;
    
  // call the event callback
  if (mEventCallback) {
    event.renderingContext = nsnull;
    if (event.rect) {
      x = event.rect->x;
      y = event.rect->y;
      width = event.rect->width;
      height = event.rect->height;
    }
    else {
      x = 0;
      y = 0;
      if (mWidget) {
        width = mWidget->Width();
        height = mWidget->Height();
      }
      else {
        width = 0;
        height = 0;
      }
    }
    static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
    static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);
    if (NS_OK == nsComponentManager::CreateInstance(kRenderingContextCID,
                                                    nsnull,
                                                    kRenderingContextIID,
                                                    (void **)&event.renderingContext)) {
      if (mBounds.width && mBounds.height) {
        event.renderingContext->Init(mContext, this);
        result = DispatchWindowEvent(&event);
        NS_IF_RELEASE(event.renderingContext);
      }
    }
    else {
      result = PR_FALSE;
    }
  }
  return result;
}

NS_METHOD nsWindow::BeginResizingChildren(void)
{
  return NS_OK;
}

NS_METHOD nsWindow::EndResizingChildren(void)
{
  return NS_OK;
}

PRBool nsWindow::OnKey(nsKeyEvent &aEvent)
{
  if (mEventCallback) {
    return DispatchWindowEvent(&aEvent);
  }
  return PR_FALSE;
}

PRBool nsWindow::OnText(nsTextEvent &aEvent)
{
  if (mEventCallback) {
    return DispatchWindowEvent(&aEvent);
  }
  return PR_FALSE;
}

PRBool nsWindow::OnComposition(nsCompositionEvent &aEvent)
{
  if (mEventCallback) {
    return DispatchWindowEvent(&aEvent);
  }
  return PR_FALSE;
}

PRBool nsWindow::DispatchActivateEvent()
{
  nsGUIEvent nsEvent;

  nsEvent.message         = NS_ACTIVATE;
  nsEvent.eventStructType = NS_GUI_EVENT;
  nsEvent.widget          = this;
  nsEvent.time            = 0;
  nsEvent.point.x         = 0;
  nsEvent.point.y         = 0;
 
  return DispatchFocus(nsEvent);
}

PRBool nsWindow::DispatchFocusOutEvent()
{
  nsGUIEvent nsEvent;

  if (mFocusWidget == mWidget)
    mFocusWidget = nsnull;

  nsEvent.message         = NS_LOSTFOCUS;
  nsEvent.eventStructType = NS_GUI_EVENT;
  nsEvent.widget          = this;
  nsEvent.time            = 0;
  nsEvent.point.x         = 0;
  nsEvent.point.y         = 0;
 
  return DispatchFocus(nsEvent);
}

PRBool nsWindow::DispatchFocusInEvent()
{
  if (mBlockFocusEvents) {
    return PR_TRUE;
  }
  PRBool ret;
  nsGUIEvent nsEvent;

  nsEvent.message         = NS_GOTFOCUS;
  nsEvent.eventStructType = NS_GUI_EVENT;
  nsEvent.widget          = this;
  nsEvent.time            = 0;
  nsEvent.point.x         = 0;
  nsEvent.point.y         = 0;
 
  mBlockFocusEvents = PR_TRUE;
  ret = DispatchFocus(nsEvent);
  mBlockFocusEvents = PR_FALSE;
  return ret;
}

PRBool nsWindow::DispatchFocus(nsGUIEvent &aEvent)
{
  if (mEventCallback) {
    return DispatchWindowEvent(&aEvent);
  }
  return PR_FALSE;
}

NS_METHOD nsWindow::GetClientBounds(nsRect &aRect)
{
  return GetBounds(aRect);
}
 
NS_METHOD nsWindow::GetBounds(nsRect &aRect)
{
  nsRect Rct(mWidget->BoundsX(),mWidget->BoundsX(),
             mWidget->Width(),mWidget->Height());
  
  aRect = Rct;
  return NS_OK;
}

NS_IMETHODIMP nsWindow::GetScreenBounds(nsRect &aRect)
{
  nsRect nBounds(0,0,mBounds.width,mBounds.height);

  aRect = nBounds;
  return NS_OK;
}
 
NS_METHOD nsWindow::GetBoundsAppUnits(nsRect &aRect, float aAppUnits)
{
  GetBounds(aRect);

  // Convert to twips
  aRect.x = nscoord((PRFloat64)aRect.x * aAppUnits);
  aRect.y = nscoord((PRFloat64)aRect.y * aAppUnits);
  aRect.width  = nscoord((PRFloat64)aRect.width * aAppUnits);
  aRect.height = nscoord((PRFloat64)aRect.height * aAppUnits);
  return NS_OK;
} 

PRBool nsWindow::OnScroll(nsScrollbarEvent &aEvent, PRUint32 cPos)
{
  return PR_FALSE;
}

NS_METHOD nsWindow::ConstrainPosition(PRInt32 *aX, PRInt32 *aY)
{
  return NS_OK;
}

NS_METHOD nsWindow::Move(PRInt32 aX, PRInt32 aY)
{
  if (mWidget && mParent && mWindowType == eWindowType_popup) {
    nsRect oldrect, newrect;

    mBounds.x = aX;
    mBounds.y = aY;

    oldrect.x = aX;
    oldrect.y = aY;
    mParent->WidgetToScreen(oldrect,newrect);
    mWidget->Move(newrect.x,newrect.y);

    return NS_OK;
  }
  else
    return(nsWidget::Move(aX,aY));
}

NS_IMETHODIMP nsWindow::CaptureRollupEvents(nsIRollupListener *aListener,
                                            PRBool aDoCapture,
                                            PRBool aConsumeRollupEvent)
{ 
  if (aDoCapture) {
    /* Create a pointer region */
    mIsGrabbing = PR_TRUE;
    mGrabWindow = this;

    gRollupConsumeRollupEvent = PR_TRUE;
    gRollupListener = aListener;
    gRollupWidget = getter_AddRefs(NS_GetWeakReference(NS_STATIC_CAST(nsIWidget*,this)));
  }
  else {
    // make sure that the grab window is marked as released
    if (mGrabWindow == this) {
      mGrabWindow = NULL;
    }
    mIsGrabbing = PR_FALSE;

    gRollupListener = nsnull;
    gRollupWidget = nsnull;
  }
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////
ChildWindow::ChildWindow()
{
#ifdef DBG_JCG
  gChildCount++;
  mChildID = gChildID++;
  printf("JCG: nsChildWindow CTOR. (%p) ID: %d, Count: %d\n",this,mChildID,gChildCount);
#endif
}

ChildWindow::~ChildWindow()
{
#ifdef NOISY_DESTROY
  IndentByDepth(stdout);
  printf("ChildWindow::~ChildWindow:%p\n", this);
#endif
#ifdef DBG_JCG
  gChildCount--;
  printf("JCG: nsChildWindow DTOR. (%p) ID: %d, Count: %d\n",this,mChildID,gChildCount);
#endif
}

PRBool ChildWindow::IsChild() const
{
  return PR_TRUE;
}
