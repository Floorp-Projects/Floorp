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
 *		John C. Griggs <jcgriggs@sympatico.ca>
 *      	Wes Morgan <wmorga13@calvin.edu> 
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

#include "nsWidget.h"
#include "nsIDeviceContext.h"
#include "nsGfxCIID.h"
#include "nsIComponentManager.h"
#include "nsIMenuRollup.h"
#include "nsFontMetricsQT.h"
#include "nsReadableUtils.h"

#include <qapplication.h>

static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);
static NS_DEFINE_IID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);

static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);

nsCOMPtr<nsIRollupListener> nsWidget::gRollupListener;
nsWeakPtr                   nsWidget::gRollupWidget;
PRBool                      nsWidget::gRollupConsumeRollupEvent = PR_FALSE;

//JCG #define DBG_JCG 1

#ifdef DBG_JCG
PRUint32                    gWidgetCount = 0;
PRUint32                    gWidgetID = 0;
#endif

nsWidget::nsWidget()
{
  // get the proper color from the look and feel code
  nsILookAndFeel *lookAndFeel;
  if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, 
                                                  nsnull, 
                                                  kILookAndFeelIID, 
                                                  (void**)&lookAndFeel)) {
    lookAndFeel->GetColor(nsILookAndFeel::eColor_WindowBackground, 
                          mBackground);
  }
  NS_IF_RELEASE(lookAndFeel);
  mWidget          = nsnull;
  mParent          = nsnull;
  mPreferredWidth  = 0;
  mPreferredHeight = 0;
  mBounds.x        = 0;
  mBounds.y        = 0;
  mBounds.width    = 0;
  mBounds.height   = 0;
  mListenForResizes = PR_FALSE;
  mIsToplevel      = PR_FALSE;
  mUpdateArea      = do_CreateInstance(kRegionCID);
  if (mUpdateArea) {
    mUpdateArea->Init();
    mUpdateArea->SetTo(0,0,0,0);
  }
#ifdef DBG_JCG
  gWidgetCount++;
  mWidgetID = gWidgetID++;
  printf("JCG: nsWidget CTOR (%p). ID: %d, Count: %d\n",this,mWidgetID,gWidgetCount);
#endif
}

nsWidget::~nsWidget()
{
  Destroy();
  if (mWidget) {
    delete mWidget;
    mWidget = nsnull;
  }
#ifdef DBG_JCG
  gWidgetCount--;
  if (mIsToplevel) {
    printf("JCG: nsWidget DTOR (%p, ID: %d) of toplevel: %d widgets still exist.\n",
           this,mWidgetID,gWidgetCount);
  }
  else {
    printf("JCG: nsWidget DTOR (%p). ID: %d, Count: %d\n",this,mWidgetID,gWidgetCount);
  }
#endif
}

NS_IMPL_ISUPPORTS_INHERITED2(nsWidget,nsBaseWidget,nsIKBStateControl,nsISupportsWeakReference)

const char *nsWidget::GetName() 
{ 
  return ((mWidget != nsnull) ? mWidget->Name() : ""); 
}

NS_IMETHODIMP nsWidget::WidgetToScreen(const nsRect& aOldRect,nsRect& aNewRect)
{
  aNewRect.width = aOldRect.width;
  aNewRect.height = aOldRect.height;

  if (mWidget) {
    PRInt32 X,Y;

    mWidget->OffsetXYToGlobal(&X,&Y);
    aNewRect.x = aOldRect.x + X;
    aNewRect.y = aOldRect.y + Y;
  }
  return NS_OK;
}

NS_METHOD nsWidget::ScreenToWidget(const nsRect& aOldRect,nsRect& aNewRect)
{
  if (mWidget) {
    PRInt32 X,Y;

    mWidget->OffsetXYFromGlobal(&X,&Y);
    aNewRect.x = aOldRect.x + X;
    aNewRect.y = aOldRect.y + Y;
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
// Close this nsWidget
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWidget::Destroy(void)
{
  if (mIsDestroying)
    return NS_OK;

  mIsDestroying = PR_TRUE;
  nsBaseWidget::Destroy();
  if (mWidget) {
    mWidget->Destroy();
  }
  if (PR_FALSE == mOnDestroyCalled)
    OnDestroy();

  mEventCallback = nsnull;
  return NS_OK;
}

// make sure that we clean up here
void nsWidget::OnDestroy()
{
  // release references to children, device context, toolkit + app shell
  mOnDestroyCalled = PR_TRUE;
  nsBaseWidget::OnDestroy();

  // dispatch the event
  // dispatching of the event may cause the reference count to drop to 0
  // and result in this object being destroyed. To avoid that, add a
  // reference and then release it after dispatching the event
  nsCOMPtr<nsIWidget> kungFuDeathGrip = this; 
  DispatchStandardEvent(NS_DESTROY);
}

//-------------------------------------------------------------------------
// Get this nsWidget parent
//-------------------------------------------------------------------------
nsIWidget *nsWidget::GetParent(void)
{
  nsIWidget *ret;

  ret = mParent;
  NS_IF_ADDREF(ret);
  return ret;
}

//-------------------------------------------------------------------------
// Hide or show this component
//-------------------------------------------------------------------------
NS_METHOD nsWidget::Show(PRBool bState)
{
  if (!mWidget) {
    return NS_OK; // Will be null during printing
  }
  if (bState) {
    mWidget->Show();
  }
  else {
    mWidget->Hide();
  }
  return NS_OK;
}

NS_IMETHODIMP nsWidget::CaptureRollupEvents(nsIRollupListener *aListener, 
                                            PRBool aDoCapture, 
                                            PRBool aConsumeRollupEvent)
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::SetModal(PRBool aModal)
{
  if (mWidget)
    mWidget->SetModal(aModal);
  else
    return NS_ERROR_FAILURE;
  return NS_OK;
}

NS_METHOD nsWidget::IsVisible(PRBool &aState)
{
  if (mWidget) {
    aState = mWidget->IsVisible();
  }
  else {
    aState = PR_FALSE;
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
// Constrain a potential move so that it remains onscreen
//-------------------------------------------------------------------------
NS_METHOD nsWidget::ConstrainPosition(PRInt32 *aX, PRInt32 *aY)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
// Move this component
//-------------------------------------------------------------------------
NS_METHOD nsWidget::Move(PRInt32 aX, PRInt32 aY)
{
  if (aX == mBounds.x && aY == mBounds.y) {
    return NS_OK;
  }
  mBounds.x = aX;
  mBounds.y = aY;
  if (mWidget) {
    mWidget->Move(aX, aY);
  }
  return NS_OK;
}

NS_METHOD nsWidget::Resize(PRInt32 aWidth,PRInt32 aHeight,PRBool aRepaint)
{
  if (mWidget && (mWidget->Width() != aWidth
                  || mWidget->Height() != aHeight)) {
    mBounds.width  = aWidth;
    mBounds.height = aHeight;
     
    mWidget->Resize(aWidth,aHeight);

    if (mListenForResizes) {
      nsSizeEvent sevent;
      nsRect rect(0,0,aWidth,aHeight);

      sevent.message = NS_SIZE;
      sevent.widget = this;
      sevent.eventStructType = NS_SIZE_EVENT;
      sevent.windowSize = &rect;
      sevent.point.x = 0;
      sevent.point.y = 0;
      sevent.mWinWidth = aWidth;
      sevent.mWinHeight = aHeight;
      sevent.time =  PR_IntervalNow();
      NS_ADDREF_THIS();
      OnResize(sevent);
      NS_RELEASE_THIS();
    } 
    if (aRepaint) {
      if (mWidget->IsVisible()) {
        mWidget->Repaint(false);
      }
    }
  }
  return NS_OK;
}

NS_METHOD nsWidget::Resize(PRInt32 aX,PRInt32 aY, 
                           PRInt32 aWidth,PRInt32 aHeight, 
                           PRBool aRepaint)
{
  Move(aX,aY);
  Resize(aWidth,aHeight,aRepaint);
  return NS_OK;
}

//-------------------------------------------------------------------------
// Send a resize message to the listener
//-------------------------------------------------------------------------
PRBool nsWidget::OnResize(nsSizeEvent event)
{
  mBounds.width = event.mWinWidth;
  mBounds.height = event.mWinHeight;

  return DispatchWindowEvent(&event);
}

PRBool nsWidget::OnResize(nsRect &aRect)
{
  nsSizeEvent event;
  PRBool ret;

  InitEvent(event, NS_SIZE);
  event.eventStructType = NS_SIZE_EVENT;

  nsRect foo(0,0,aRect.width,aRect.height);
  event.windowSize = &foo;

  event.point.x = 0;
  event.point.y = 0;
  event.mWinWidth = aRect.width;
  event.mWinHeight = aRect.height;
 
  NS_ADDREF_THIS();
  ret = OnResize(event);
  NS_RELEASE_THIS();

  return ret;
}

PRBool nsWidget::OnMove(PRInt32 aX, PRInt32 aY)
{
  nsGUIEvent event;

  mBounds.x = aX;
  mBounds.y = aY; 
  InitEvent(event,NS_MOVE);
  event.point.x = aX;
  event.point.y = aY;
  event.eventStructType = NS_GUI_EVENT;
  PRBool result = DispatchWindowEvent(&event);
  return result;
}

//-------------------------------------------------------------------------
// Enable/disable this component
//-------------------------------------------------------------------------
NS_METHOD nsWidget::Enable(PRBool bState)
{
  if (mWidget) {
    mWidget->Enable(bState);
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
// Give the focus to this component
//-------------------------------------------------------------------------
NS_METHOD nsWidget::SetFocus(PRBool aRaise)
{
  if (mWidget) {
    mWidget->SetFocus();
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
// Get this component font
//-------------------------------------------------------------------------
nsIFontMetrics *nsWidget::GetFont(void)
{
  NS_NOTYETIMPLEMENTED("nsWidget::GetFont");
  return nsnull;
}

//-------------------------------------------------------------------------
// Set this component font
//-------------------------------------------------------------------------
NS_METHOD nsWidget::SetFont(const nsFont &aFont)
{
  nsCOMPtr<nsIFontMetrics> fontMetrics;
  mContext->GetMetricsFor(aFont,*getter_AddRefs(fontMetrics));

  if (!fontMetrics)
    return NS_ERROR_FAILURE;

  nsFontHandle  fontHandle;
  fontMetrics->GetFontHandle(fontHandle);

  if (fontHandle && mWidget) {
    nsFontQT *qtFontHandle = (nsFontQT*)fontHandle;

    mWidget->SetFont(qtFontHandle);
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
// Set the background color
//-------------------------------------------------------------------------
NS_METHOD nsWidget::SetBackgroundColor(const nscolor &aColor)
{
  nsBaseWidget::SetBackgroundColor(aColor);

  if (mWidget) {
    mWidget->SetBackgroundColor(aColor);
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
// Set this component cursor
//-------------------------------------------------------------------------
NS_METHOD nsWidget::SetCursor(nsCursor aCursor)
{
  if (mWidget) {
    if (mCursor != aCursor) {
      mWidget->SetCursor(aCursor);
      mCursor = aCursor;
    }
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_METHOD nsWidget::Invalidate(PRBool aIsSynchronous)
{
  if (!mWidget) 
    return NS_OK; // mWidget will be null during printing. 

  if (aIsSynchronous) {
    mWidget->Repaint(false);
    mUpdateArea->SetTo(0,0,0,0);
  } 
  else {
    mWidget->Update();
    mUpdateArea->SetTo(0,0,mBounds.width,mBounds.height);
  }
  return NS_OK;
}

NS_METHOD nsWidget::Invalidate(const nsRect &aRect,PRBool aIsSynchronous)
{
  if (mWidget == nsnull) {
    return NS_OK;  // mWidget is null during printing
  }
  mUpdateArea->Union(aRect.x,aRect.y,aRect.width,aRect.height);
  if (aIsSynchronous) {
    mWidget->Repaint(aRect.x,aRect.y,aRect.width,aRect.height,false);
  } 
  else {
    mWidget->Update(aRect.x,aRect.y,aRect.width,aRect.height);
  }
  return NS_OK;
}

NS_IMETHODIMP nsWidget::InvalidateRegion(const nsIRegion *aRegion,
                                         PRBool aIsSynchronous)
{
  nsRegionRectSet *regionRectSet = nsnull;
 
  if (!mWidget)
    return NS_OK;
 
  mUpdateArea->Union(*aRegion);
 
  if (NS_FAILED(mUpdateArea->GetRects(&regionRectSet)))
    return NS_ERROR_FAILURE;

  PRUint32 len;
  PRUint32 i;
 
  len = regionRectSet->mRectsLen;
 
  for (i = 0; i < len; ++i) {
    nsRegionRect *r = &(regionRectSet->mRects[i]);
 
    if (aIsSynchronous)
      mWidget->Repaint(r->x,r->y,r->width,r->height,false);
    else
      mWidget->Update(r->x,r->y,r->width,r->height);
  }
  // drop the const.. whats the right thing to do here?
  ((nsIRegion*)aRegion)->FreeRects(regionRectSet);
 
  return NS_OK;
}

NS_METHOD nsWidget::Update(void)
{
  if (mIsDestroying)
    return NS_ERROR_FAILURE;

  return InvalidateRegion(mUpdateArea, PR_TRUE);
}

//-------------------------------------------------------------------------
// Return some native data according to aDataType
//-------------------------------------------------------------------------
void *nsWidget::GetNativeData(PRUint32 aDataType)
{
  switch(aDataType) {
    case NS_NATIVE_WINDOW:
      if (mWidget)
        return mWidget->GetNativeWindow();
      break;

    case NS_NATIVE_DISPLAY:
      if (mWidget)
        return mWidget->X11Display();
      break;

    case NS_NATIVE_WIDGET:
      if (mWidget)
        return mWidget->GetNativeWidget();
      break;

    case NS_NATIVE_PLUGIN_PORT:
      if (mWidget)
        return mWidget->WinID();
      break;

    default:
      break;
  }
  return nsnull;
}

//-------------------------------------------------------------------------
// Set the colormap of the window
//-------------------------------------------------------------------------
NS_METHOD nsWidget::SetColorMap(nsColorMap *aColorMap)
{
  return NS_OK;
}

//Stub for nsWindow functionality
NS_METHOD nsWidget::Scroll(PRInt32 aDx,PRInt32 aDy,nsRect *aClipRect)
{
  return NS_ERROR_FAILURE;
}

NS_METHOD nsWidget::BeginResizingChildren(void)
{
  return NS_OK;
}

NS_METHOD nsWidget::EndResizingChildren(void)
{
  return NS_OK;
}

NS_METHOD nsWidget::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
  aWidth  = mPreferredWidth;
  aHeight = mPreferredHeight;
  return (mPreferredWidth != 0 && mPreferredHeight != 0) ? NS_OK
                                                         : NS_ERROR_FAILURE;
}

NS_METHOD nsWidget::SetPreferredSize(PRInt32 aWidth,PRInt32 aHeight)
{
  mPreferredWidth  = aWidth;
  mPreferredHeight = aHeight;
  return NS_OK;
}

NS_METHOD nsWidget::SetTitle(const nsString &aTitle)
{
  if (mWidget) {
    const char *title = ToNewCString(aTitle);

    mWidget->SetTitle(title);
    delete [] title;
  }
  return NS_OK;
}

nsresult nsWidget::CreateWidget(nsIWidget *aParent,
                                const nsRect &aRect,
                                EVENT_CALLBACK aHandleEventFunction,
                                nsIDeviceContext *aContext,
                                nsIAppShell *aAppShell,
                                nsIToolkit *aToolkit,
                                nsWidgetInitData *aInitData,
                                nsNativeWidget aNativeParent)
{
  QWidget *parentWidget = nsnull;
  nsIWidget *baseParent;

  baseParent = aInitData
                && (aInitData->mWindowType == eWindowType_dialog
                    || aInitData->mWindowType == eWindowType_toplevel)
                ? nsnull : aParent;

  BaseCreate(baseParent,aRect,aHandleEventFunction,aContext,
             aAppShell,aToolkit,aInitData);

  mParent = aParent;
  if (aNativeParent) {
    parentWidget = (QWidget*)aNativeParent;
    mListenForResizes = PR_TRUE;
  } 
  else if (aParent) {
    parentWidget = (QWidget*)aParent->GetNativeData(NS_NATIVE_WIDGET);
  } 
  mBounds = aRect;

  CreateNative(parentWidget);
  Resize(aRect.width,aRect.height,PR_FALSE);
  mWidget->Polish();
  if (mIsToplevel) {
    /* We have to Spin the Qt Event loop to make top level windows */
    /* come up with the correct size, but it creates problems for  */
    /* menus, etc. */
    qApp->processEvents(1);
  }
  DispatchStandardEvent(NS_CREATE);
  return NS_OK;
}

//-------------------------------------------------------------------------
// create with nsIWidget parent
//-------------------------------------------------------------------------
NS_METHOD nsWidget::Create(nsIWidget *aParent,
                           const nsRect &aRect,
                           EVENT_CALLBACK aHandleEventFunction,
                           nsIDeviceContext *aContext,
                           nsIAppShell *aAppShell,
                           nsIToolkit *aToolkit,
                           nsWidgetInitData *aInitData)
{
  return(CreateWidget(aParent,aRect,aHandleEventFunction,
                      aContext,aAppShell,aToolkit,aInitData,
                      nsnull));
}

//-------------------------------------------------------------------------
// create with a native parent
//-------------------------------------------------------------------------
NS_METHOD nsWidget::Create(nsNativeWidget aParent,
                           const nsRect &aRect,
                           EVENT_CALLBACK aHandleEventFunction,
                           nsIDeviceContext *aContext,
                           nsIAppShell *aAppShell,
                           nsIToolkit *aToolkit,
                           nsWidgetInitData *aInitData)
{
  return(CreateWidget(nsnull,aRect,aHandleEventFunction,
                      aContext,aAppShell,aToolkit,aInitData,
                      aParent));
}

//-------------------------------------------------------------------------
// Initialize all the Callbacks
//-------------------------------------------------------------------------
void nsWidget::InitCallbacks(char *aName)
{
}

void nsWidget::ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY)
{
}

void nsWidget::InitEvent(nsGUIEvent& event, 
                         PRUint32 aEventType, 
                         nsPoint* aPoint)
{
  event.widget = this;

  if (aPoint == nsnull) {     
    event.point.x = 0;
    event.point.y = 0;
  }    
  else {
    // use the point override if provided
    event.point.x = aPoint->x;
    event.point.y = aPoint->y;
  }
  event.time =  PR_IntervalNow();
  event.message = aEventType;
}

PRBool nsWidget::ConvertStatus(nsEventStatus aStatus)
{
  switch(aStatus) {
    case nsEventStatus_eIgnore:
      return(PR_FALSE);

    case nsEventStatus_eConsumeNoDefault:
      return(PR_TRUE);

    case nsEventStatus_eConsumeDoDefault:
      return(PR_FALSE);

    default:
      NS_ASSERTION(0,"Illegal nsEventStatus enumeration value");
      break;
  }
  return(PR_FALSE);
}

PRBool nsWidget::DispatchWindowEvent(nsGUIEvent* event)
{
  nsEventStatus status;
  DispatchEvent(event,status);
  return ConvertStatus(status);
}

//-------------------------------------------------------------------------
// Dispatch standard event
//-------------------------------------------------------------------------
PRBool nsWidget::DispatchStandardEvent(PRUint32 aMsg)
{
  nsGUIEvent event;
  event.eventStructType = NS_GUI_EVENT;
  InitEvent(event,aMsg);

  PRBool result = DispatchWindowEvent(&event);
  return result;
}

//-------------------------------------------------------------------------
// Invokes callback and  ProcessEvent method on Event Listener object
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWidget::DispatchEvent(nsGUIEvent *event,
                                      nsEventStatus &aStatus)
{
  NS_IF_ADDREF(event->widget);
  if (nsnull != mMenuListener) {
    if (NS_MENU_EVENT == event->eventStructType)
      aStatus = mMenuListener->MenuSelected(NS_STATIC_CAST(nsMenuEvent&,
                                                           *event));
  }
  aStatus = nsEventStatus_eIgnore;
  if (nsnull != mEventCallback) {
    aStatus = (*mEventCallback)(event);
  }
  // Dispatch to event listener if event was not consumed
  if (aStatus != nsEventStatus_eIgnore && nsnull != mEventListener) {
    aStatus = mEventListener->ProcessEvent(*event);
  }
  NS_IF_RELEASE(event->widget);
  return NS_OK;
}

PRBool nsWidget::DispatchMouseScrollEvent(nsMouseScrollEvent& aEvent)
{
  if (nsnull != mEventCallback) {
    return DispatchWindowEvent(&aEvent);
  }
  return PR_FALSE;
}

//-------------------------------------------------------------------------
// Deal with all sorts of mouse event
//-------------------------------------------------------------------------
PRBool nsWidget::DispatchMouseEvent(nsMouseEvent& aEvent)
{
  PRBool result = PR_FALSE;

  if (nsnull == mWidget
      || (nsnull == mEventCallback && nsnull == mMouseListener)) {
    return result;
  }
  if (aEvent.message == NS_MOUSE_LEFT_BUTTON_DOWN
      || aEvent.message == NS_MOUSE_RIGHT_BUTTON_DOWN
       || aEvent.message == NS_MOUSE_MIDDLE_BUTTON_DOWN) {
    if (mWidget->HandlePopup((void*)aEvent.nativeMsg))
      return result;
  }
  // call the event callback
  if (nsnull != mEventCallback) {
    result = DispatchWindowEvent(&aEvent);

    return result;
  }
  if (nsnull != mMouseListener) {
    switch (aEvent.message) {
      case NS_MOUSE_LEFT_BUTTON_DOWN:
      case NS_MOUSE_MIDDLE_BUTTON_DOWN:
      case NS_MOUSE_RIGHT_BUTTON_DOWN:
        result = ConvertStatus(mMouseListener->MousePressed(aEvent));
        break;

      case NS_MOUSE_LEFT_BUTTON_UP:
      case NS_MOUSE_MIDDLE_BUTTON_UP:
      case NS_MOUSE_RIGHT_BUTTON_UP:
        result = ConvertStatus(mMouseListener->MouseReleased(aEvent));
        result = ConvertStatus(mMouseListener->MouseClicked(aEvent));
        break;

      default:
        break;
    } // switch
  }
  return result;
}

// HandlePopup
// Deal with rollup of popups (xpmenus, etc)
PRBool nsWidget::HandlePopup(PRInt32 inMouseX,PRInt32 inMouseY)
{
  PRBool retVal = PR_FALSE;
  nsCOMPtr<nsIWidget> rollupWidget = do_QueryReferent(gRollupWidget);
  if (rollupWidget && gRollupListener) {
    void *currentPopup = rollupWidget->GetNativeData(NS_NATIVE_WIDGET);
    if (currentPopup
        && !NS_IsMouseInWindow(currentPopup,inMouseX,inMouseY)) {
      PRBool rollup = PR_TRUE;
      // if we're dealing with menus, we probably have submenus and we don't
      // want to rollup if the clickis in a parent menu of the current submenu
      nsCOMPtr<nsIMenuRollup> menuRollup(do_QueryInterface(gRollupListener));
      if (menuRollup) {
        nsCOMPtr<nsISupportsArray> widgetChain;
        menuRollup->GetSubmenuWidgetChain(getter_AddRefs(widgetChain));
        if (widgetChain) {
          PRUint32 count = 0;
          widgetChain->Count(&count);
          for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsISupports> genericWidget;
            widgetChain->GetElementAt(i,getter_AddRefs(genericWidget));
            nsCOMPtr<nsIWidget> widget(do_QueryInterface(genericWidget));
            if (widget) {
              void *currWindow = widget->GetNativeData(NS_NATIVE_WIDGET);
              if (currWindow
                  && NS_IsMouseInWindow(currWindow,inMouseX,inMouseY)) {
                rollup = PR_FALSE;
                break;
              }
            }
          } // foreach parent menu widget
        }
      } // if rollup listener knows about menus
      // if we've determined that we should still rollup, do it.
      if (rollup) {
        gRollupListener->Rollup();
        retVal = PR_TRUE;
      }
    }
  } 
  else {
    gRollupWidget = nsnull;
    gRollupListener = nsnull;
  }
  return retVal;
} // HandlePopup

//-------------------------------------------------------------------------
// Base implementation of CreateNative.
//-------------------------------------------------------------------------
NS_METHOD nsWidget::CreateNative(QWidget *parentWindow)
{
  if (!mWidget)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

NS_IMETHODIMP nsWidget::ResetInputState()
{
  return NS_OK;  
}

NS_IMETHODIMP nsWidget::PasswordFieldInit()
{
  return NS_OK;  
}
