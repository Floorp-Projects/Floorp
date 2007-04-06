/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Dean Tessman <dean_tessman@hotmail.com>
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

#include "nsBaseWidget.h"
#include "nsIDeviceContext.h"
#include "nsCOMPtr.h"
#include "nsIMenuListener.h"
#include "nsGfxCIID.h"
#include "nsWidgetsCID.h"
#include "nsIFullScreen.h"
#include "nsServiceManagerUtils.h"
#include "nsIScreenManager.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsISimpleEnumerator.h"

#ifdef DEBUG
#include "nsIServiceManager.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch2.h"
#include "nsIObserver.h"

static void debug_RegisterPrefCallbacks();

#endif

#ifdef NOISY_WIDGET_LEAKS
static PRInt32 gNumWidgets;
#endif

// nsBaseWidget
NS_IMPL_ISUPPORTS1(nsBaseWidget, nsIWidget)


//-------------------------------------------------------------------------
//
// nsBaseWidget constructor
//
//-------------------------------------------------------------------------

nsBaseWidget::nsBaseWidget()
: mClientData(nsnull)
, mEventCallback(nsnull)
, mContext(nsnull)
, mToolkit(nsnull)
, mMouseListener(nsnull)
, mEventListener(nsnull)
, mMenuListener(nsnull)
, mCursor(eCursor_standard)
, mWindowType(eWindowType_child)
, mBorderStyle(eBorderStyle_none)
, mIsShiftDown(PR_FALSE)
, mIsControlDown(PR_FALSE)
, mIsAltDown(PR_FALSE)
, mIsDestroying(PR_FALSE)
, mOnDestroyCalled(PR_FALSE)
, mBounds(0,0,0,0)
, mOriginalBounds(nsnull)
, mZIndex(0)
, mSizeMode(nsSizeMode_Normal)
{
#ifdef NOISY_WIDGET_LEAKS
  gNumWidgets++;
  printf("WIDGETS+ = %d\n", gNumWidgets);
#endif

#ifdef DEBUG
    debug_RegisterPrefCallbacks();
#endif
}


//-------------------------------------------------------------------------
//
// nsBaseWidget destructor
//
//-------------------------------------------------------------------------
nsBaseWidget::~nsBaseWidget()
{
#ifdef NOISY_WIDGET_LEAKS
  gNumWidgets--;
  printf("WIDGETS- = %d\n", gNumWidgets);
#endif

  NS_IF_RELEASE(mMenuListener);
  NS_IF_RELEASE(mToolkit);
  NS_IF_RELEASE(mContext);
  if (mOriginalBounds)
    delete mOriginalBounds;
}


//-------------------------------------------------------------------------
//
// Basic create.
//
//-------------------------------------------------------------------------
void nsBaseWidget::BaseCreate(nsIWidget *aParent,
                              const nsRect &aRect,
                              EVENT_CALLBACK aHandleEventFunction,
                              nsIDeviceContext *aContext,
                              nsIAppShell *aAppShell,
                              nsIToolkit *aToolkit,
                              nsWidgetInitData *aInitData)
{
  if (nsnull == mToolkit) {
    if (nsnull != aToolkit) {
      mToolkit = (nsIToolkit*)aToolkit;
      NS_ADDREF(mToolkit);
    }
    else {
      if (nsnull != aParent) {
        mToolkit = (nsIToolkit*)(aParent->GetToolkit()); // the call AddRef's, we don't have to
      }
      // it's some top level window with no toolkit passed in.
      // Create a default toolkit with the current thread
#if !defined(USE_TLS_FOR_TOOLKIT)
      else {
        static NS_DEFINE_CID(kToolkitCID, NS_TOOLKIT_CID);
        
        nsresult res;
        res = CallCreateInstance(kToolkitCID, &mToolkit);
        NS_ASSERTION(NS_SUCCEEDED(res), "Can not create a toolkit in nsBaseWidget::Create");
        if (mToolkit)
          mToolkit->Init(PR_GetCurrentThread());
      }
#else /* USE_TLS_FOR_TOOLKIT */
      else {
        nsresult rv;

        rv = NS_GetCurrentToolkit(&mToolkit);
      }
#endif /* USE_TLS_FOR_TOOLKIT */
    }
    
  }
  
  // save the event callback function
  mEventCallback = aHandleEventFunction;
  
  // keep a reference to the device context
  if (aContext) {
    mContext = aContext;
    NS_ADDREF(mContext);
  }
  else {
    nsresult  res;
    
    static NS_DEFINE_CID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);
    
    res = CallCreateInstance(kDeviceContextCID, &mContext);

    if (NS_SUCCEEDED(res))
      mContext->Init(nsnull);
  }

  if (nsnull != aInitData) {
    PreCreateWidget(aInitData);
  }

  if (aParent) {
    aParent->AddChild(this);
  }
}

NS_IMETHODIMP nsBaseWidget::CaptureMouse(PRBool aCapture)
{
  return NS_OK;
}

NS_IMETHODIMP nsBaseWidget::Validate()
{
  return NS_OK;
}

NS_IMETHODIMP nsBaseWidget::InvalidateRegion(const nsIRegion *aRegion, PRBool aIsSynchronous)
{
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
//
// Accessor functions to get/set the client data
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsBaseWidget::GetClientData(void*& aClientData)
{
  aClientData = mClientData;
  return NS_OK;
}

NS_IMETHODIMP nsBaseWidget::SetClientData(void* aClientData)
{
  mClientData = aClientData;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Close this nsBaseWidget
//
//-------------------------------------------------------------------------
NS_METHOD nsBaseWidget::Destroy()
{
  // Just in case our parent is the only ref to us
  nsCOMPtr<nsIWidget> kungFuDeathGrip(this);
  
  // disconnect from the parent
  nsIWidget *parent = GetParent();
  if (parent) {
    parent->RemoveChild(this);
  }
  // disconnect listeners.
  NS_IF_RELEASE(mMouseListener);
  NS_IF_RELEASE(mEventListener);
  NS_IF_RELEASE(mMenuListener);

  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set this nsBaseWidget's parent
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseWidget::SetParent(nsIWidget* aNewParent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


//-------------------------------------------------------------------------
//
// Get this nsBaseWidget parent
//
//-------------------------------------------------------------------------
nsIWidget* nsBaseWidget::GetParent(void)
{
  return nsnull;
}

//-------------------------------------------------------------------------
//
// Add a child to the list of children
//
//-------------------------------------------------------------------------
void nsBaseWidget::AddChild(nsIWidget* aChild)
{
  NS_PRECONDITION(!aChild->GetNextSibling() && !aChild->GetPrevSibling(),
                  "aChild not properly removed from its old child list");
  
  if (!mFirstChild) {
    mFirstChild = mLastChild = aChild;
  } else {
    // append to the list
    NS_ASSERTION(mLastChild, "Bogus state");
    NS_ASSERTION(!mLastChild->GetNextSibling(), "Bogus state");
    mLastChild->SetNextSibling(aChild);
    aChild->SetPrevSibling(mLastChild);
    mLastChild = aChild;
  }
}


//-------------------------------------------------------------------------
//
// Remove a child from the list of children
//
//-------------------------------------------------------------------------
void nsBaseWidget::RemoveChild(nsIWidget* aChild)
{
  NS_ASSERTION(aChild->GetParent() == this, "Not one of our kids!");
  
  if (mLastChild == aChild) {
    mLastChild = mLastChild->GetPrevSibling();
  }
  if (mFirstChild == aChild) {
    mFirstChild = mFirstChild->GetNextSibling();
  }

  // Now remove from the list.  Make sure that we pass ownership of the tail
  // of the list correctly before we have aChild let go of it.
  nsIWidget* prev = aChild->GetPrevSibling();
  nsIWidget* next = aChild->GetNextSibling();
  if (prev) {
    prev->SetNextSibling(next);
  }
  if (next) {
    next->SetPrevSibling(prev);
  }
  
  aChild->SetNextSibling(nsnull);
  aChild->SetPrevSibling(nsnull);
}


//-------------------------------------------------------------------------
//
// Sets widget's position within its parent's child list.
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseWidget::SetZIndex(PRInt32 aZIndex)
{
  // Hold a ref to ourselves just in case, since we're going to remove
  // from our parent.
  nsCOMPtr<nsIWidget> kungFuDeathGrip(this);
  
  mZIndex = aZIndex;

  // reorder this child in its parent's list.
  nsBaseWidget* parent = NS_STATIC_CAST(nsBaseWidget*, GetParent());
  if (parent) {
    parent->RemoveChild(this);
    // Scope sib outside the for loop so we can check it afterward
    nsIWidget* sib = parent->GetFirstChild();
    for ( ; sib; sib = sib->GetNextSibling()) {
      PRInt32 childZIndex;
      if (NS_SUCCEEDED(sib->GetZIndex(&childZIndex))) {
        if (aZIndex < childZIndex) {
          // Insert ourselves before sib
          nsIWidget* prev = sib->GetPrevSibling();
          mNextSibling = sib;
          mPrevSibling = prev;
          sib->SetPrevSibling(this);
          if (prev) {
            prev->SetNextSibling(this);
          } else {
            NS_ASSERTION(sib == parent->mFirstChild, "Broken child list");
            // We've taken ownership of sib, so it's safe to have parent let
            // go of it
            parent->mFirstChild = this;
          }
          PlaceBehind(eZPlacementBelow, sib, PR_FALSE);
          break;
        }
      }
    }
    // were we added to the list?
    if (!sib) {
      parent->AddChild(this);
    }
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Gets widget's position within its parent's child list.
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseWidget::GetZIndex(PRInt32* aZIndex)
{
  *aZIndex = mZIndex;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Places widget behind the given widget (platforms must override)
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseWidget::PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                        nsIWidget *aWidget, PRBool aActivate)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Maximize, minimize or restore the window. The BaseWidget implementation
// merely stores the state.
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseWidget::SetSizeMode(PRInt32 aMode) {

  if (aMode == nsSizeMode_Normal || aMode == nsSizeMode_Minimized ||
      aMode == nsSizeMode_Maximized) {

    mSizeMode = (nsSizeMode) aMode;
    return NS_OK;
  }
  return NS_ERROR_ILLEGAL_VALUE;
}

//-------------------------------------------------------------------------
//
// Get the size mode (minimized, maximized, that sort of thing...)
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseWidget::GetSizeMode(PRInt32* aMode) {

  *aMode = mSizeMode;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the foreground color
//
//-------------------------------------------------------------------------
nscolor nsBaseWidget::GetForegroundColor(void)
{
  return mForeground;
}

    
//-------------------------------------------------------------------------
//
// Set the foreground color
//
//-------------------------------------------------------------------------
NS_METHOD nsBaseWidget::SetForegroundColor(const nscolor &aColor)
{
  mForeground = aColor;
  return NS_OK;
}

    
//-------------------------------------------------------------------------
//
// Get the background color
//
//-------------------------------------------------------------------------
nscolor nsBaseWidget::GetBackgroundColor(void)
{
  return mBackground;
}

//-------------------------------------------------------------------------
//
// Set the background color
//
//-------------------------------------------------------------------------
NS_METHOD nsBaseWidget::SetBackgroundColor(const nscolor &aColor)
{
  mBackground = aColor;
  return NS_OK;
}
     
//-------------------------------------------------------------------------
//
// Get this component cursor
//
//-------------------------------------------------------------------------
nsCursor nsBaseWidget::GetCursor()
{
  return mCursor;
}

NS_METHOD nsBaseWidget::SetCursor(nsCursor aCursor)
{
  mCursor = aCursor; 
  return NS_OK;
}

NS_IMETHODIMP nsBaseWidget::SetCursor(imgIContainer* aCursor,
                                      PRUint32 aHotspotX, PRUint32 aHotspotY)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
    
//-------------------------------------------------------------------------
//
// Get the window type for this widget
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseWidget::GetWindowType(nsWindowType& aWindowType)
{
  aWindowType = mWindowType;
  return NS_OK;
}

NS_IMETHODIMP nsBaseWidget::SetWindowType(nsWindowType aWindowType) 
{
  mWindowType = aWindowType;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Window transparency methods
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsBaseWidget::SetWindowTranslucency(PRBool aTranslucent) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsBaseWidget::GetWindowTranslucency(PRBool& aTranslucent) {
  aTranslucent = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsBaseWidget::UpdateTranslucentWindowAlpha(const nsRect& aRect, PRUint8* aAlphas) {
  NS_ASSERTION(PR_FALSE, "Window is not translucent");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-------------------------------------------------------------------------
//
// Hide window borders/decorations for this widget
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseWidget::HideWindowChrome(PRBool aShouldHide)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-------------------------------------------------------------------------
//
// Put the window into full-screen mode
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseWidget::MakeFullScreen(PRBool aFullScreen)
{
  HideWindowChrome(aFullScreen);
  return MakeFullScreenInternal(aFullScreen);
}

nsresult nsBaseWidget::MakeFullScreenInternal(PRBool aFullScreen)
{
  nsCOMPtr<nsIFullScreen> fullScreen = do_GetService("@mozilla.org/browser/fullscreen;1");

  if (aFullScreen) {
    if (!mOriginalBounds)
      mOriginalBounds = new nsRect();
    GetScreenBounds(*mOriginalBounds);

    // Move to top-left corner of screen and size to the screen dimensions
    nsCOMPtr<nsIScreenManager> screenManager;
    screenManager = do_GetService("@mozilla.org/gfx/screenmanager;1"); 
    NS_ASSERTION(screenManager, "Unable to grab screenManager.");
    if (screenManager) {
      nsCOMPtr<nsIScreen> screen;
      screenManager->ScreenForRect(mOriginalBounds->x, mOriginalBounds->y,
                                   mOriginalBounds->width, mOriginalBounds->height,
                                   getter_AddRefs(screen));
      if (screen) {
        PRInt32 left, top, width, height;
        if (NS_SUCCEEDED(screen->GetRect(&left, &top, &width, &height))) {
          SetSizeMode(nsSizeMode_Normal);
          Resize(left, top, width, height, PR_TRUE);
    
          // Hide all of the OS chrome
          if (fullScreen)
            fullScreen->HideAllOSChrome();
        }
      }
    }

  } else if (mOriginalBounds) {
    Resize(mOriginalBounds->x, mOriginalBounds->y, mOriginalBounds->width,
           mOriginalBounds->height, PR_TRUE);

    // Show all of the OS chrome
    if (fullScreen)
      fullScreen->ShowAllOSChrome();
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Create a rendering context from this nsBaseWidget
//
//-------------------------------------------------------------------------
nsIRenderingContext* nsBaseWidget::GetRenderingContext()
{
  nsresult                      rv;
  nsCOMPtr<nsIRenderingContext> renderingCtx;

  if (mOnDestroyCalled)
    return nsnull;

  rv = mContext->CreateRenderingContextInstance(*getter_AddRefs(renderingCtx));
  if (NS_SUCCEEDED(rv)) {
#if defined(MOZ_CAIRO_GFX)
    rv = renderingCtx->Init(mContext, GetThebesSurface());
#else
    rv = renderingCtx->Init(mContext, this);
#endif
    if (NS_SUCCEEDED(rv)) {
      nsIRenderingContext *ret = renderingCtx;
      /* Increment object refcount that the |ret| object is still a valid one
       * after we leave this function... */
      NS_ADDREF(ret);
      return ret;
    }
    else {
      NS_WARNING("GetRenderingContext: nsIRenderingContext::Init() failed.");
    }  
  }
  else {
    NS_WARNING("GetRenderingContext: Cannot create RenderingContext.");
  }  
  
  return nsnull;
}

//-------------------------------------------------------------------------
//
// Return the toolkit this widget was created on
//
//-------------------------------------------------------------------------
nsIToolkit* nsBaseWidget::GetToolkit()
{
  NS_IF_ADDREF(mToolkit);
  return mToolkit;
}


//-------------------------------------------------------------------------
//
// Return the used device context
//
//-------------------------------------------------------------------------
nsIDeviceContext* nsBaseWidget::GetDeviceContext() 
{
  return mContext; 
}

#ifdef MOZ_CAIRO_GFX
//-------------------------------------------------------------------------
//
// Get the thebes surface
//
//-------------------------------------------------------------------------
gfxASurface *nsBaseWidget::GetThebesSurface()
{
  nsIWidget *parent = GetParent();
  if (!parent)
    return nsnull;

  // in theory we should get our parent's surface,
  // clone it, and set a device offset before returning
  return nsnull;
}
#endif

//-------------------------------------------------------------------------
//
// Destroy the window
//
//-------------------------------------------------------------------------
void nsBaseWidget::OnDestroy()
{
  // release references to device context, toolkit, and app shell
  NS_IF_RELEASE(mContext);
  NS_IF_RELEASE(mToolkit);
}

NS_METHOD nsBaseWidget::SetWindowClass(const nsAString& xulWinType)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsBaseWidget::SetBorderStyle(nsBorderStyle aBorderStyle)
{
  mBorderStyle = aBorderStyle;
  return NS_OK;
}


/**
* Processes a mouse pressed event
*
**/
NS_METHOD nsBaseWidget::AddMouseListener(nsIMouseListener * aListener)
{
  NS_PRECONDITION(mMouseListener == nsnull, "Null mouse listener");
  NS_IF_RELEASE(mMouseListener);
  NS_ADDREF(aListener);
  mMouseListener = aListener;
  return NS_OK;
}

/**
* Processes a mouse pressed event
*
**/
NS_METHOD nsBaseWidget::AddEventListener(nsIEventListener * aListener)
{
  NS_PRECONDITION(mEventListener == nsnull, "Null mouse listener");
  NS_IF_RELEASE(mEventListener);
  NS_ADDREF(aListener);
  mEventListener = aListener;
  return NS_OK;
}

/**
* Add a menu listener
* This interface should only be called by the menu services manager
* This will AddRef() the menu listener
* This will Release() a previously set menu listener
*
**/

NS_METHOD nsBaseWidget::AddMenuListener(nsIMenuListener * aListener)
{
  NS_IF_RELEASE(mMenuListener);
  NS_IF_ADDREF(aListener);
  mMenuListener = aListener;
  return NS_OK;
}


/**
* If the implementation of nsWindow supports borders this method MUST be overridden
*
**/
NS_METHOD nsBaseWidget::GetClientBounds(nsRect &aRect)
{
  return GetBounds(aRect);
}

/**
* If the implementation of nsWindow supports borders this method MUST be overridden
*
**/
NS_METHOD nsBaseWidget::GetBounds(nsRect &aRect)
{
  aRect = mBounds;
  return NS_OK;
}

/**
* If the implementation of nsWindow uses a local coordinate system within the window,
* this method must be overridden
*
**/
NS_METHOD nsBaseWidget::GetScreenBounds(nsRect &aRect)
{
  return GetBounds(aRect);
}

/**
* 
*
**/
NS_METHOD nsBaseWidget::SetBounds(const nsRect &aRect)
{
  mBounds = aRect;

  return NS_OK;
}
 


/**
* Calculates the border width and height  
*
**/
NS_METHOD nsBaseWidget::GetBorderSize(PRInt32 &aWidth, PRInt32 &aHeight)
{
  nsRect rectWin;
  nsRect rect;
  GetBounds(rectWin);
  GetClientBounds(rect);

  aWidth  = (rectWin.width - rect.width) / 2;
  aHeight = (rectWin.height - rect.height) / 2;

  return NS_OK;
}

NS_IMETHODIMP nsBaseWidget::ScrollWidgets(PRInt32 aDx, PRInt32 aDy)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsBaseWidget::ScrollRect(nsRect &aRect, PRInt32 aDx, PRInt32 aDy)
{
  return NS_ERROR_FAILURE;
}

NS_METHOD nsBaseWidget::EnableDragDrop(PRBool aEnable)
{
  return NS_OK;
}

NS_METHOD nsBaseWidget::SetModal(PRBool aModal)
{
  return NS_ERROR_FAILURE;
}

// generic xp assumption is that events should be processed
NS_METHOD nsBaseWidget::ModalEventFilter(PRBool aRealEvent, void *aEvent,
                            PRBool *aForWindow)
{
  *aForWindow = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseWidget::GetAttention(PRInt32 aCycleCount) {
    return NS_OK;
}

NS_IMETHODIMP
nsBaseWidget::GetLastInputEventTime(PRUint32& aTime) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBaseWidget::SetIcon(const nsAString&)
{
  return NS_OK;
}

NS_IMETHODIMP
nsBaseWidget::SetAnimatedResize(PRUint16 aAnimation)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBaseWidget::GetAnimatedResize(PRUint16* aAnimation)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/**
 * Modifies aFile to point at an icon file with the given name and suffix.  The
 * suffix may correspond to a file extension with leading '.' if appropriate.
 * Returns true if the icon file exists and can be read.
 */
static PRBool
ResolveIconNameHelper(nsILocalFile *aFile,
                      const nsAString &aIconName,
                      const nsAString &aIconSuffix)
{
  aFile->Append(NS_LITERAL_STRING("icons"));
  aFile->Append(NS_LITERAL_STRING("default"));
  aFile->Append(aIconName + aIconSuffix);

  PRBool readable;
  return NS_SUCCEEDED(aFile->IsReadable(&readable)) && readable;
}

/**
 * Resolve the given icon name into a local file object.  This method is
 * intended to be called by subclasses of nsBaseWidget.  aIconSuffix is a
 * platform specific icon file suffix (e.g., ".ico" under Win32).
 *
 * If no file is found matching the given parameters, then null is returned.
 */
void
nsBaseWidget::ResolveIconName(const nsAString &aIconName,
                              const nsAString &aIconSuffix,
                              nsILocalFile **aResult)
{ 
  *aResult = nsnull;

  nsCOMPtr<nsIProperties> dirSvc = do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
  if (!dirSvc)
    return;

  // first check auxilary chrome directories

  nsCOMPtr<nsISimpleEnumerator> dirs;
  dirSvc->Get(NS_APP_CHROME_DIR_LIST, NS_GET_IID(nsISimpleEnumerator),
              getter_AddRefs(dirs));
  if (dirs) {
    PRBool hasMore;
    while (NS_SUCCEEDED(dirs->HasMoreElements(&hasMore)) && hasMore) {
      nsCOMPtr<nsISupports> element;
      dirs->GetNext(getter_AddRefs(element));
      if (!element)
        continue;
      nsCOMPtr<nsILocalFile> file = do_QueryInterface(element);
      if (!file)
        continue;
      if (ResolveIconNameHelper(file, aIconName, aIconSuffix)) {
        NS_ADDREF(*aResult = file);
        return;
      }
    }
  }

  // then check the main app chrome directory

  nsCOMPtr<nsILocalFile> file;
  dirSvc->Get(NS_APP_CHROME_DIR, NS_GET_IID(nsILocalFile),
              getter_AddRefs(file));
  if (file && ResolveIconNameHelper(file, aIconName, aIconSuffix))
    NS_ADDREF(*aResult = file);
}

#ifdef DEBUG
//////////////////////////////////////////////////////////////
//
// Convert a GUI event message code to a string.
// Makes it a lot easier to debug events.
//
// See gtk/nsWidget.cpp and windows/nsWindow.cpp
// for a DebugPrintEvent() function that uses
// this.
//
//////////////////////////////////////////////////////////////
/* static */ nsAutoString
nsBaseWidget::debug_GuiEventToString(nsGUIEvent * aGuiEvent)
{
  NS_ASSERTION(nsnull != aGuiEvent,"cmon, null gui event.");

  nsAutoString eventName(NS_LITERAL_STRING("UNKNOWN"));

#define _ASSIGN_eventName(_value,_name)\
case _value: eventName.AssignWithConversion(_name) ; break

  switch(aGuiEvent->message)
  {
    _ASSIGN_eventName(NS_BLUR_CONTENT,"NS_BLUR_CONTENT");
    _ASSIGN_eventName(NS_CONTROL_CHANGE,"NS_CONTROL_CHANGE");
    _ASSIGN_eventName(NS_CREATE,"NS_CREATE");
    _ASSIGN_eventName(NS_DESTROY,"NS_DESTROY");
    _ASSIGN_eventName(NS_DRAGDROP_GESTURE,"NS_DND_GESTURE");
    _ASSIGN_eventName(NS_DRAGDROP_DROP,"NS_DND_DROP");
    _ASSIGN_eventName(NS_DRAGDROP_ENTER,"NS_DND_ENTER");
    _ASSIGN_eventName(NS_DRAGDROP_EXIT,"NS_DND_EXIT");
    _ASSIGN_eventName(NS_DRAGDROP_OVER,"NS_DND_OVER");
    _ASSIGN_eventName(NS_FOCUS_CONTENT,"NS_FOCUS_CONTENT");
    _ASSIGN_eventName(NS_FORM_SELECTED,"NS_FORM_SELECTED");
    _ASSIGN_eventName(NS_FORM_CHANGE,"NS_FORM_CHANGE");
    _ASSIGN_eventName(NS_FORM_INPUT,"NS_FORM_INPUT");
    _ASSIGN_eventName(NS_FORM_RESET,"NS_FORM_RESET");
    _ASSIGN_eventName(NS_FORM_SUBMIT,"NS_FORM_SUBMIT");
    _ASSIGN_eventName(NS_GOTFOCUS,"NS_GOTFOCUS");
    _ASSIGN_eventName(NS_IMAGE_ABORT,"NS_IMAGE_ABORT");
    _ASSIGN_eventName(NS_LOAD_ERROR,"NS_LOAD_ERROR");
    _ASSIGN_eventName(NS_KEY_DOWN,"NS_KEY_DOWN");
    _ASSIGN_eventName(NS_KEY_PRESS,"NS_KEY_PRESS");
    _ASSIGN_eventName(NS_KEY_UP,"NS_KEY_UP");
    _ASSIGN_eventName(NS_LOSTFOCUS,"NS_LOSTFOCUS");
    _ASSIGN_eventName(NS_MENU_SELECTED,"NS_MENU_SELECTED");
    _ASSIGN_eventName(NS_MOUSE_ENTER,"NS_MOUSE_ENTER");
    _ASSIGN_eventName(NS_MOUSE_EXIT,"NS_MOUSE_EXIT");
    _ASSIGN_eventName(NS_MOUSE_BUTTON_DOWN,"NS_MOUSE_BUTTON_DOWN");
    _ASSIGN_eventName(NS_MOUSE_BUTTON_UP,"NS_MOUSE_BUTTON_UP");
    _ASSIGN_eventName(NS_MOUSE_CLICK,"NS_MOUSE_CLICK");
    _ASSIGN_eventName(NS_MOUSE_DOUBLECLICK,"NS_MOUSE_DBLCLICK");
    _ASSIGN_eventName(NS_MOUSE_MOVE,"NS_MOUSE_MOVE");
    _ASSIGN_eventName(NS_MOVE,"NS_MOVE");
    _ASSIGN_eventName(NS_LOAD,"NS_LOAD");
    _ASSIGN_eventName(NS_PAGE_UNLOAD,"NS_PAGE_UNLOAD");
    _ASSIGN_eventName(NS_PAINT,"NS_PAINT");
    _ASSIGN_eventName(NS_XUL_BROADCAST, "NS_XUL_BROADCAST");
    _ASSIGN_eventName(NS_XUL_COMMAND_UPDATE, "NS_XUL_COMMAND_UPDATE");
    _ASSIGN_eventName(NS_SCROLLBAR_LINE_NEXT,"NS_SB_LINE_NEXT");
    _ASSIGN_eventName(NS_SCROLLBAR_LINE_PREV,"NS_SB_LINE_PREV");
    _ASSIGN_eventName(NS_SCROLLBAR_PAGE_NEXT,"NS_SB_PAGE_NEXT");
    _ASSIGN_eventName(NS_SCROLLBAR_PAGE_PREV,"NS_SB_PAGE_PREV");
    _ASSIGN_eventName(NS_SCROLLBAR_POS,"NS_SB_POS");
    _ASSIGN_eventName(NS_SIZE,"NS_SIZE");

#undef _ASSIGN_eventName

  default: 
    {
      char buf[32];
      
      sprintf(buf,"UNKNOWN: %d",aGuiEvent->message);
      
      eventName.AssignWithConversion(buf);
    }
    break;
  }
  
  return nsAutoString(eventName);
}
//////////////////////////////////////////////////////////////
//
// Code to deal with paint and event debug prefs.
//
//////////////////////////////////////////////////////////////
struct PrefPair
{
  const char * name;
  PRBool value;
};

static PrefPair debug_PrefValues[] =
{
  { "nglayout.debug.crossing_event_dumping", PR_FALSE },
  { "nglayout.debug.event_dumping", PR_FALSE },
  { "nglayout.debug.invalidate_dumping", PR_FALSE },
  { "nglayout.debug.motion_event_dumping", PR_FALSE },
  { "nglayout.debug.paint_dumping", PR_FALSE },
  { "nglayout.debug.paint_flashing", PR_FALSE }
};

static PRUint32 debug_NumPrefValues = 
  (sizeof(debug_PrefValues) / sizeof(debug_PrefValues[0]));


//////////////////////////////////////////////////////////////
static PRBool debug_GetBoolPref(nsIPrefBranch * aPrefs,const char * aPrefName)
{
  NS_ASSERTION(nsnull != aPrefName,"cmon, pref name is null.");
  NS_ASSERTION(nsnull != aPrefs,"cmon, prefs are null.");

  PRBool value = PR_FALSE;

  if (aPrefs)
  {
    aPrefs->GetBoolPref(aPrefName,&value);
  }

  return value;
}
//////////////////////////////////////////////////////////////
PRBool
nsBaseWidget::debug_GetCachedBoolPref(const char * aPrefName)
{
  NS_ASSERTION(nsnull != aPrefName,"cmon, pref name is null.");

  for (PRUint32 i = 0; i < debug_NumPrefValues; i++)
  {
    if (strcmp(debug_PrefValues[i].name, aPrefName) == 0)
    {
      return debug_PrefValues[i].value;
    }
  }

  return PR_FALSE;
}
//////////////////////////////////////////////////////////////
static void debug_SetCachedBoolPref(const char * aPrefName,PRBool aValue)
{
  NS_ASSERTION(nsnull != aPrefName,"cmon, pref name is null.");

  for (PRUint32 i = 0; i < debug_NumPrefValues; i++)
  {
    if (strcmp(debug_PrefValues[i].name, aPrefName) == 0)
    {
      debug_PrefValues[i].value = aValue;

      return;
    }
  }

  NS_ASSERTION(PR_FALSE, "cmon, this code is not reached dude.");
}

//////////////////////////////////////////////////////////////
class Debug_PrefObserver : public nsIObserver {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS1(Debug_PrefObserver, nsIObserver)

NS_IMETHODIMP
Debug_PrefObserver::Observe(nsISupports* subject, const char* topic,
                            const PRUnichar* data)
{
  nsCOMPtr<nsIPrefBranch> branch(do_QueryInterface(subject));
  NS_ASSERTION(branch, "must implement nsIPrefBranch");

  NS_ConvertUTF16toUTF8 prefName(data);

  PRBool value = PR_FALSE;
  branch->GetBoolPref(prefName.get(), &value);
  debug_SetCachedBoolPref(prefName.get(), value);
  return NS_OK;
}

//////////////////////////////////////////////////////////////
/* static */ void
debug_RegisterPrefCallbacks()
{
  static PRBool once = PR_TRUE;

  if (once)
  {
    once = PR_FALSE;

    nsCOMPtr<nsIPrefBranch2> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
    
    NS_ASSERTION(prefs, "Prefs services is null.");

    if (prefs)
    {
      nsCOMPtr<nsIObserver> obs(new Debug_PrefObserver());
      for (PRUint32 i = 0; i < debug_NumPrefValues; i++)
      {
        // Initialize the pref values
        debug_PrefValues[i].value = 
          debug_GetBoolPref(prefs,debug_PrefValues[i].name);

        if (obs) {
          // Register callbacks for when these change
          prefs->AddObserver(debug_PrefValues[i].name, obs, PR_FALSE);
        }
      }
    }
  }
}
//////////////////////////////////////////////////////////////
static PRInt32
_GetPrintCount()
{
  static PRInt32 sCount = 0;
  
  return ++sCount;
}
//////////////////////////////////////////////////////////////
/* static */ PRBool
nsBaseWidget::debug_WantPaintFlashing()
{
  return debug_GetCachedBoolPref("nglayout.debug.paint_flashing");
}
//////////////////////////////////////////////////////////////
/* static */ void
nsBaseWidget::debug_DumpEvent(FILE *                aFileOut,
                              nsIWidget *           aWidget,
                              nsGUIEvent *          aGuiEvent,
                              const nsCAutoString & aWidgetName,
                              PRInt32               aWindowID)
{
  // NS_PAINT is handled by debug_DumpPaintEvent()
  if (aGuiEvent->message == NS_PAINT)
    return;

  if (aGuiEvent->message == NS_MOUSE_MOVE)
  {
    if (!debug_GetCachedBoolPref("nglayout.debug.motion_event_dumping"))
      return;
  }
  
  if (aGuiEvent->message == NS_MOUSE_ENTER || 
      aGuiEvent->message == NS_MOUSE_EXIT)
  {
    if (!debug_GetCachedBoolPref("nglayout.debug.crossing_event_dumping"))
      return;
  }

  if (!debug_GetCachedBoolPref("nglayout.debug.event_dumping"))
    return;

  nsCAutoString tempString; tempString.AssignWithConversion(debug_GuiEventToString(aGuiEvent).get());
  
  fprintf(aFileOut,
          "%4d %-26s widget=%-8p name=%-12s id=%-8p refpt=%d,%d\n",
          _GetPrintCount(),
          tempString.get(),
          (void *) aWidget,
          aWidgetName.get(),
          (void *) (aWindowID ? aWindowID : 0x0),
          aGuiEvent->refPoint.x,
          aGuiEvent->refPoint.y);
}
//////////////////////////////////////////////////////////////
/* static */ void
nsBaseWidget::debug_DumpPaintEvent(FILE *                aFileOut,
                                   nsIWidget *           aWidget,
                                   nsPaintEvent *        aPaintEvent,
                                   const nsCAutoString & aWidgetName,
                                   PRInt32               aWindowID)
{
  NS_ASSERTION(nsnull != aFileOut,"cmon, null output FILE");
  NS_ASSERTION(nsnull != aWidget,"cmon, the widget is null");
  NS_ASSERTION(nsnull != aPaintEvent,"cmon, the paint event is null");

  if (!debug_GetCachedBoolPref("nglayout.debug.paint_dumping"))
    return;
  
  fprintf(aFileOut,
          "%4d PAINT      widget=%p name=%-12s id=%-8p rect=", 
          _GetPrintCount(),
          (void *) aWidget,
          aWidgetName.get(),
          (void *) aWindowID);
  
  if (aPaintEvent->rect) 
  {
    fprintf(aFileOut,
            "%3d,%-3d %3d,%-3d",
            aPaintEvent->rect->x, 
            aPaintEvent->rect->y,
            aPaintEvent->rect->width, 
            aPaintEvent->rect->height);
  }
  else
  {
    fprintf(aFileOut,"none");
  }
  
  fprintf(aFileOut,"\n");
}
//////////////////////////////////////////////////////////////
/* static */ void
nsBaseWidget::debug_DumpInvalidate(FILE *                aFileOut,
                                   nsIWidget *           aWidget,
                                   const nsRect *        aRect,
                                   PRBool                aIsSynchronous,
                                   const nsCAutoString & aWidgetName,
                                   PRInt32               aWindowID)
{
  if (!debug_GetCachedBoolPref("nglayout.debug.invalidate_dumping"))
    return;

  NS_ASSERTION(nsnull != aFileOut,"cmon, null output FILE");
  NS_ASSERTION(nsnull != aWidget,"cmon, the widget is null");

  fprintf(aFileOut,
          "%4d Invalidate widget=%p name=%-12s id=%-8p",
          _GetPrintCount(),
          (void *) aWidget,
          aWidgetName.get(),
          (void *) aWindowID);

  if (aRect) 
  {
    fprintf(aFileOut,
            " rect=%3d,%-3d %3d,%-3d",
            aRect->x, 
            aRect->y,
            aRect->width, 
            aRect->height);
  }
  else
  {
    fprintf(aFileOut,
            " rect=%-15s",
            "none");
  }

  fprintf(aFileOut,
          " sync=%s",
          (const char *) (aIsSynchronous ? "yes" : "no "));
  
  fprintf(aFileOut,"\n");
}
//////////////////////////////////////////////////////////////

#endif // DEBUG

