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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsCOMPtr.h"
#include "nsAutoLock.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsVoidArray.h"
#include "nsIBaseWindow.h"
#include "nsIWidget.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMElement.h"
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIServiceManager.h"
#include "nsISimpleEnumerator.h"
#include "nsIWebShell.h"
#include "nsAppShellWindowEnumerator.h"
#include "nsWindowMediator.h"
#include "nsIWindowMediatorListener.h"
#include "nsXPIDLString.h"

// Interfaces Needed
#include "nsIDocShell.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIXULWindow.h"


static nsresult GetDOMWindow( nsIXULWindow* inWindow,
                  nsCOMPtr< nsIDOMWindowInternal>& outDOMWindow);

static PRBool notifyOpenWindow(nsISupports *aElement, void* aData);
static PRBool notifyCloseWindow(nsISupports *aElement, void* aData);
static PRBool notifyWindowTitleChange(nsISupports *aElement, void* aData);

// for notifyWindowTitleChange
struct windowData {
  nsIXULWindow* mWindow;
  const PRUnichar *mTitle;
};


nsresult
GetDOMWindow( nsIXULWindow* inWindow, nsCOMPtr< nsIDOMWindowInternal>& outDOMWindow)
{
  nsCOMPtr<nsIDocShell> docShell;

  inWindow->GetDocShell(getter_AddRefs(docShell));
   outDOMWindow = do_GetInterface(docShell);
   return outDOMWindow ? NS_OK : NS_ERROR_FAILURE;
}

PRInt32   nsWindowMediator::gRefCnt = 0;

nsWindowMediator::nsWindowMediator() :
  mEnumeratorList(), mOldestWindow(0), mTopmostWindow(0),
  mTimeStamp(0), mSortingZOrder(PR_FALSE), mListLock(0)
{
   // This should really be done in the static constructor fn.
   nsresult rv;
   rv = Init();
   NS_ASSERTION(NS_SUCCEEDED(rv), "uh oh, couldn't Init() for some reason");
}

nsWindowMediator::~nsWindowMediator()
{
  if (--gRefCnt == 0) {

    // Delete data
    while (mOldestWindow)
      UnregisterWindow(mOldestWindow);

    if (mListLock)
      PR_DestroyLock(mListLock);
  }
}



NS_IMETHODIMP nsWindowMediator::RegisterWindow(nsIXULWindow* inWindow)
{
  if (GetInfoFor(inWindow)) {
    NS_ERROR("multiple window registration");
    return NS_ERROR_FAILURE;
  }

  mTimeStamp++;

  // Create window info struct and add to list of windows
  nsWindowInfo* windowInfo = new nsWindowInfo (inWindow, mTimeStamp);
  if (windowInfo == NULL)
    return NS_ERROR_OUT_OF_MEMORY;

  if (mListeners) {
    windowData winData = { inWindow, nsnull };
    mListeners->EnumerateForwards(notifyOpenWindow, (void*)&winData);
  }
  
  nsAutoLock lock(mListLock);
  if (mOldestWindow)
    windowInfo->InsertAfter(mOldestWindow->mOlder, 0);
  else
    mOldestWindow = windowInfo;

  return NS_OK;
}



NS_IMETHODIMP
nsWindowMediator::UnregisterWindow(nsIXULWindow* inWindow)
{
  nsAutoLock lock(mListLock);
  nsWindowInfo *info = GetInfoFor(inWindow);
  if (info)
    return UnregisterWindow(info);
  return NS_ERROR_INVALID_ARG;
}



NS_IMETHODIMP
nsWindowMediator::UnregisterWindow(nsWindowInfo *inInfo)
{
  // Inform the iterators
  PRInt32 index = -1;
  while (++index < mEnumeratorList.Count()) 
    ((nsAppShellWindowEnumerator*)mEnumeratorList[index])->WindowRemoved(inInfo);
  
  if (mListeners) {
    windowData winData = {inInfo->mWindow.get(), nsnull };
    mListeners->EnumerateForwards(notifyCloseWindow, (void*)&winData);
  }

  // Remove from the lists and free up 
  if (inInfo == mOldestWindow)
    mOldestWindow = inInfo->mYounger;
  if (inInfo == mTopmostWindow)
    mTopmostWindow = inInfo->mLower;
  inInfo->Unlink(PR_TRUE, PR_TRUE);
  if (inInfo == mOldestWindow)
    mOldestWindow = 0;
  if (inInfo == mTopmostWindow)
    mTopmostWindow = 0;
  delete inInfo;  

  return NS_OK;
}

nsWindowInfo *
nsWindowMediator::GetInfoFor(nsIXULWindow *aWindow)
{
  nsWindowInfo *info,
               *listEnd;

  if (!aWindow)
    return 0;

  info = mOldestWindow;
  listEnd = 0;
  while (info != listEnd) {
    if (info->mWindow.get() == aWindow)
      return info;
    info = info->mYounger;
    listEnd = mOldestWindow;
  }
  return 0;
}

nsWindowInfo *
nsWindowMediator::GetInfoFor(nsIWidget *aWindow)
{
  nsWindowInfo *info,
               *listEnd;

  if (!aWindow)
    return 0;

  info = mOldestWindow;
  listEnd = 0;

  nsCOMPtr<nsIWidget> scanWidget;
  while (info != listEnd) {
    nsCOMPtr<nsIBaseWindow> base(do_QueryInterface(info->mWindow));
    if (base)
      base->GetMainWidget(getter_AddRefs(scanWidget));
    if (aWindow == scanWidget.get())
      return info;
    info = info->mYounger;
    listEnd = mOldestWindow;
  }
  return 0;
}

NS_METHOD
nsWindowMediator::GetEnumerator( const PRUnichar* inType, nsISimpleEnumerator** outEnumerator)
{
  if (outEnumerator == NULL)
    return NS_ERROR_INVALID_ARG;

  nsAutoLock lock(mListLock);
  nsAppShellWindowEnumerator *enumerator = new nsASDOMWindowEarlyToLateEnumerator(inType, *this);
  if (enumerator)
    return enumerator->QueryInterface(NS_GET_IID(nsISimpleEnumerator) , (void**)outEnumerator);

  return NS_ERROR_OUT_OF_MEMORY;
}



NS_METHOD
nsWindowMediator::GetXULWindowEnumerator(const PRUnichar* inType, nsISimpleEnumerator** outEnumerator)
{
  if (outEnumerator == NULL)
    return NS_ERROR_INVALID_ARG;

  nsAutoLock lock(mListLock);
  nsAppShellWindowEnumerator *enumerator = new nsASXULWindowEarlyToLateEnumerator(inType, *this);
  if (enumerator)
    return enumerator->QueryInterface( NS_GET_IID(nsISimpleEnumerator) , (void**)outEnumerator);

  return NS_ERROR_OUT_OF_MEMORY;
} 



NS_METHOD
nsWindowMediator::GetZOrderDOMWindowEnumerator(
            const PRUnichar *aWindowType, PRBool aFrontToBack,
            nsISimpleEnumerator **_retval)
{
  if (!_retval)
    return NS_ERROR_INVALID_ARG;

  nsAutoLock lock(mListLock);
  nsAppShellWindowEnumerator *enumerator;
  if (aFrontToBack)
    enumerator = new nsASDOMWindowFrontToBackEnumerator(aWindowType, *this);
  else
    enumerator = new nsASDOMWindowBackToFrontEnumerator(aWindowType, *this);
  if (enumerator)
    return enumerator->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void**) _retval);

  return NS_ERROR_OUT_OF_MEMORY;
}



NS_METHOD
nsWindowMediator::GetZOrderXULWindowEnumerator(
            const PRUnichar *aWindowType, PRBool aFrontToBack,
            nsISimpleEnumerator **_retval)
{
  if (!_retval)
    return NS_ERROR_INVALID_ARG;

  nsAutoLock lock(mListLock);
  nsAppShellWindowEnumerator *enumerator;
  if (aFrontToBack)
    enumerator = new nsASXULWindowFrontToBackEnumerator(aWindowType, *this);
  else
    enumerator = new nsASXULWindowBackToFrontEnumerator(aWindowType, *this);
  if (enumerator)
    return enumerator->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void**) _retval);

  return NS_ERROR_OUT_OF_MEMORY;
}



PRInt32
nsWindowMediator::AddEnumerator(nsAppShellWindowEnumerator * inEnumerator)
{
  return mEnumeratorList.AppendElement(inEnumerator);
}



PRInt32
nsWindowMediator::RemoveEnumerator( nsAppShellWindowEnumerator * inEnumerator)
{
  return mEnumeratorList.RemoveElement(inEnumerator);
}



/*
  Returns the window of type inType ( if null return any window type ) which has the most recent
  time stamp
*/
NS_IMETHODIMP
nsWindowMediator::GetMostRecentWindow( const PRUnichar* inType, nsIDOMWindowInternal** outWindow)
{
  NS_ENSURE_ARG_POINTER(outWindow);
  *outWindow = NULL;

  // Find the most window with the highest time stamp that matches
  // the requested type

  nsAutoLock lock(mListLock);
  nsWindowInfo *info = MostRecentWindowInfo(inType);

  if (info && info->mWindow) {
    nsCOMPtr <nsIDOMWindowInternal> DOMWindow;
    if(NS_SUCCEEDED(GetDOMWindow(info->mWindow, DOMWindow))) {  
      *outWindow = DOMWindow;
      NS_ADDREF(*outWindow);
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}



nsWindowInfo *
nsWindowMediator::MostRecentWindowInfo(const PRUnichar* inType)
{
  PRInt32       lastTimeStamp = -1;
  nsAutoString  typeString(inType);
  PRBool        allWindows = !inType || typeString.IsEmpty();

  // Find the most window with the highest time stamp that matches
  // the requested type
  nsWindowInfo *searchInfo,
               *listEnd,
               *foundInfo = 0;

  searchInfo = mOldestWindow;
  listEnd = 0;
  while (searchInfo != listEnd) {
    if ((allWindows || searchInfo->TypeEquals(typeString)) &&
        searchInfo->mTimeStamp >= lastTimeStamp) {

      foundInfo = searchInfo;
      lastTimeStamp = searchInfo->mTimeStamp;
    }
    searchInfo = searchInfo->mYounger;
    listEnd = mOldestWindow;
  }
  return foundInfo;
}



NS_IMETHODIMP
nsWindowMediator::UpdateWindowTimeStamp( nsIXULWindow* inWindow)
{
  nsAutoLock lock(mListLock);
  nsWindowInfo *info = GetInfoFor(inWindow);
  if (info) {
    // increment the window's time stamp
    info->mTimeStamp = ++mTimeStamp;
    return NS_OK;
  }
  return NS_ERROR_FAILURE; 
}



NS_IMETHODIMP
nsWindowMediator::UpdateWindowTitle( nsIXULWindow* inWindow,
                                     const PRUnichar* inTitle )
{
  nsAutoLock lock(mListLock);
  if (mListeners && GetInfoFor(inWindow)) {
    windowData winData = { inWindow, inTitle };
    mListeners->EnumerateForwards(notifyWindowTitleChange, (void *)&winData);
  }

  return NS_OK;
}



/* This method's plan is to intervene only when absolutely necessary.
   We will get requests to place our windows behind unknown windows.
   For the most part, we need to leave those alone (turning them into
   explicit requests to be on top breaks Windows.) So generally we
   calculate a change as seldom as possible.
*/
NS_IMETHODIMP
nsWindowMediator::CalculateZPosition(
                nsIXULWindow   *inWindow,
                PRUint32        inPosition,
                nsIWidget      *inBelow,
                PRUint32       *outPosition,
                nsIWidget     **outBelow,
                PRBool         *outAltered) {

  if (!outBelow)
    return NS_ERROR_NULL_POINTER;

  *outBelow = 0;

  if (!inWindow || !outPosition || !outAltered)
    return NS_ERROR_NULL_POINTER;

  if (inPosition != nsIWindowMediator::zLevelTop &&
      inPosition != nsIWindowMediator::zLevelBottom &&
      inPosition != nsIWindowMediator::zLevelBelow)

    return NS_ERROR_INVALID_ARG;

  nsWindowInfo *info = mTopmostWindow;
  nsIXULWindow *belowWindow = 0;
  PRBool        found = PR_FALSE;
  nsresult      result = NS_OK;
  
  *outPosition = inPosition;
  *outAltered = PR_FALSE;

  if (mSortingZOrder) { // don't fight SortZOrder()
    *outBelow = inBelow;
    NS_IF_ADDREF(*outBelow);
    return NS_OK;
  }

  PRUint32 inZ;
  GetZLevel(inWindow, &inZ);

  nsAutoLock lock(mListLock);

  if (inPosition == nsIWindowMediator::zLevelBelow) {

    // locate inBelow. use topmost if it can't be found or isn't in the
    // z-order list
    info = GetInfoFor(inBelow);
    if (!info || info->mYounger != info && info->mLower == info)
      info = mTopmostWindow;
    else
      found = PR_TRUE;

    if (!found) {
      /* Treat unknown windows as a request to be on top.
         Not as it should be, but that's what Windows gives us.
         Note we change inPosition, but not *outPosition. This forces
         us to go through the "on top" calculation just below, without
         necessarily changing the output parameters. */
      inPosition = nsIWindowMediator::zLevelTop;
    }
  }

  if (inPosition == nsIWindowMediator::zLevelTop) {
    if (mTopmostWindow && mTopmostWindow->mZLevel > inZ) {

      // asked for topmost, can't have it. locate highest allowed position.
      do {
        if (info->mZLevel <= inZ)
          break;
        info = info->mLower;
      } while (info != mTopmostWindow);

      *outPosition = nsIWindowMediator::zLevelBelow;
      belowWindow = info->mHigher->mWindow;
      *outAltered = PR_TRUE;

    }
  } else if (inPosition == nsIWindowMediator::zLevelBottom) {
    if (mTopmostWindow && mTopmostWindow->mHigher->mZLevel < inZ) {

      // asked for bottommost, can't have it. locate lowest allowed position.
      do {
        info = info->mHigher;
        if (info->mZLevel >= inZ)
          break;
      } while (info != mTopmostWindow);

      *outPosition = nsIWindowMediator::zLevelBelow;
      belowWindow = info->mWindow;
      *outAltered = PR_TRUE;
    }
  } else {

    unsigned long relativeZ;

    // check that we're in the right z-plane
    if (found) {
      belowWindow = info->mWindow;
      relativeZ = info->mZLevel;
      if (relativeZ > inZ) {

        // might be OK. is lower window, if any, lower?
        if (info->mLower != info && info->mLower->mZLevel > inZ) {

          do {
            if (info->mZLevel <= inZ)
              break;
            info = info->mLower;
          } while (info != mTopmostWindow);

          belowWindow = info->mHigher->mWindow;
          *outAltered = PR_TRUE;
        }
      } else if (relativeZ < inZ) {

        // nope. look for a higher window to be behind.
        do {
          info = info->mHigher;
          if (info->mZLevel >= inZ)
            break;
        } while (info != mTopmostWindow);

        if (info->mZLevel >= inZ)
          belowWindow = info->mWindow;
        else
          *outPosition = nsIWindowMediator::zLevelTop;
        *outAltered = PR_TRUE;

      } // else they're equal, so it's OK
    }
  }

  if (NS_SUCCEEDED(result) && belowWindow) {
    nsCOMPtr<nsIBaseWindow> base(do_QueryInterface(belowWindow));
    if (base)
      base->GetMainWidget(outBelow);
    else
      result = NS_ERROR_NO_INTERFACE;
  }

  return result;
}



NS_IMETHODIMP
nsWindowMediator::SetZPosition(
                nsIXULWindow *inWindow,
                PRUint32      inPosition,
                nsIXULWindow *inBelow) {

  nsWindowInfo *inInfo,
               *belowInfo;

  if (inPosition != nsIWindowMediator::zLevelTop &&
      inPosition != nsIWindowMediator::zLevelBottom &&
      inPosition != nsIWindowMediator::zLevelBelow ||

      !inWindow)

//    inPosition == nsIWindowMediator::zLevelBelow && !inBelow)

    return NS_ERROR_INVALID_ARG;

  if (mSortingZOrder) // don't fight SortZOrder()
    return NS_OK;

  nsAutoLock lock(mListLock);

  /* Locate inWindow and unlink it from the z-order list.
     It's important we look for it in the age list, not the z-order list.
     This is because the former is guaranteed complete, while
     now may be this window's first exposure to the latter. */
  inInfo = GetInfoFor(inWindow);
  if (!inInfo)
    return NS_ERROR_INVALID_ARG;

  // locate inBelow, place inWindow behind it
  if (inPosition == nsIWindowMediator::zLevelBelow) {
    belowInfo = GetInfoFor(inBelow);
    // it had better also be in the z-order list
    if (belowInfo &&
        belowInfo->mYounger != belowInfo && belowInfo->mLower == belowInfo)
      belowInfo = 0;
    if (!belowInfo)
      if (inBelow)
        return NS_ERROR_INVALID_ARG;
      else
        inPosition = nsIWindowMediator::zLevelTop;
  }
  if (inPosition == nsIWindowMediator::zLevelTop ||
      inPosition == nsIWindowMediator::zLevelBottom)
    belowInfo = mTopmostWindow ? mTopmostWindow->mHigher : 0;

  if (inInfo != belowInfo) {
    inInfo->Unlink(PR_FALSE, PR_TRUE);
    inInfo->InsertAfter(0, belowInfo);
  }
  if (inPosition == nsIWindowMediator::zLevelTop)
    mTopmostWindow = inInfo;

  return NS_OK;
}

NS_IMETHODIMP
nsWindowMediator::GetZLevel(nsIXULWindow *aWindow, PRUint32 *_retval) {

  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsIXULWindow::normalZ;
  nsWindowInfo *info = GetInfoFor(aWindow);
  if (info)
    *_retval = info->mZLevel;
  else {
    NS_WARNING("getting z level of unregistered window");
    // this goes off during window destruction
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWindowMediator::SetZLevel(nsIXULWindow *aWindow, PRUint32 aZLevel) {

  nsAutoLock lock(mListLock);

  nsWindowInfo *info = GetInfoFor(aWindow);
  NS_ASSERTION(info, "setting z level of unregistered window");
  if (!info)
    return NS_ERROR_FAILURE;

  if (info->mZLevel != aZLevel) {
    PRBool lowered = info->mZLevel > aZLevel;
    info->mZLevel = aZLevel;
    if (lowered)
      SortZOrderFrontToBack();
    else
      SortZOrderBackToFront();
  }
  return NS_OK;
}

/*   Fix potentially out-of-order windows by performing an insertion sort
   on the z-order list. The method will work no matter how broken the
   list, but its assumed usage is immediately after one window's z level
   has been changed, so one window is potentially out of place. Such a sort
   is most efficiently done in a particular direction. Use this one
   if a window's z level has just been reduced, so the sort is most efficiently
   done front to back. Assumes caller has locked mListLock.
     Note it's hardly worth going to all the trouble to write two versions
   of this method except that if we choose the inefficient sorting direction,
   on slow systems windows could visibly bubble around the window that
   was moved.
*/
void
nsWindowMediator::SortZOrderFrontToBack() {

  nsWindowInfo *scan,   // scans list looking for problems
               *search, // searches for correct placement for scan window
               *prev,   // previous search element
               *lowest; // bottom-most window in list
  PRBool       finished;

  if (!mTopmostWindow) // early during program execution there's no z list yet
    return;            // there's also only one window, so this is not dangerous

  mSortingZOrder = PR_TRUE;

  /* Step through the list from top to bottom. If we find a window which
     should be moved down in the list, move it to its highest legal position. */
  do {
    finished = PR_TRUE;
    lowest = mTopmostWindow->mHigher;
    scan = mTopmostWindow;
    while (scan != lowest) {
      PRUint32 scanZ = scan->mZLevel;
      if (scanZ < scan->mLower->mZLevel) { // out of order
        search = scan->mLower;
        do {
          prev = search;
          search = search->mLower;
        } while (prev != lowest && scanZ < search->mZLevel);

        // reposition |scan| within the list
        if (scan == mTopmostWindow)
          mTopmostWindow = scan->mLower;
        scan->Unlink(PR_FALSE, PR_TRUE);
        scan->InsertAfter(0, prev);

        // fix actual window order
        nsCOMPtr<nsIBaseWindow> base;
        nsCOMPtr<nsIWidget> scanWidget;
        nsCOMPtr<nsIWidget> prevWidget;
        base = do_QueryInterface(scan->mWindow);
        if (base)
          base->GetMainWidget(getter_AddRefs(scanWidget));
        base = do_QueryInterface(prev->mWindow);
        if (base)
          base->GetMainWidget(getter_AddRefs(prevWidget));
        if (scanWidget)
          scanWidget->PlaceBehind(eZPlacementBelow, prevWidget, PR_FALSE);

        finished = PR_FALSE;
        break;
      }
      scan = scan->mLower;
    }
  } while (!finished);

  mSortingZOrder = PR_FALSE;
}

// see comment for SortZOrderFrontToBack
void
nsWindowMediator::SortZOrderBackToFront() {

  nsWindowInfo *scan,   // scans list looking for problems
               *search, // searches for correct placement for scan window
               *lowest; // bottom-most window in list
  PRBool       finished;

  if (!mTopmostWindow) // early during program execution there's no z list yet
    return;            // there's also only one window, so this is not dangerous

  mSortingZOrder = PR_TRUE;

  /* Step through the list from bottom to top. If we find a window which
     should be moved up in the list, move it to its lowest legal position. */
  do {
    finished = PR_TRUE;
    lowest = mTopmostWindow->mHigher;
    scan = lowest;
    while (scan != mTopmostWindow) {
      PRUint32 scanZ = scan->mZLevel;
      if (scanZ > scan->mHigher->mZLevel) { // out of order
        search = scan;
        do
          search = search->mHigher;
        while (search != lowest && scanZ > search->mZLevel);

        // reposition |scan| within the list
        if (scan != search && scan != search->mLower) {
          scan->Unlink(PR_FALSE, PR_TRUE);
          scan->InsertAfter(0, search);
        }
        if (search == lowest)
          mTopmostWindow = scan;

        // fix actual window order
        nsCOMPtr<nsIBaseWindow> base;
        nsCOMPtr<nsIWidget> scanWidget;
        nsCOMPtr<nsIWidget> searchWidget;
        base = do_QueryInterface(scan->mWindow);
        if (base)
          base->GetMainWidget(getter_AddRefs(scanWidget));
        if (mTopmostWindow != scan) {
          base = do_QueryInterface(search->mWindow);
          if (base)
            base->GetMainWidget(getter_AddRefs(searchWidget));
        }
        if (scanWidget)
          scanWidget->PlaceBehind(eZPlacementBelow, searchWidget, PR_FALSE);

        finished = PR_FALSE;
        break;
      }
      scan = scan->mHigher;
    }
  } while (!finished);

  mSortingZOrder = PR_FALSE;
}

// COM
NS_IMPL_ADDREF( nsWindowMediator )
NS_IMPL_QUERY_INTERFACE1(nsWindowMediator, nsIWindowMediator)
NS_IMPL_RELEASE(nsWindowMediator)

nsresult
nsWindowMediator::Init()
{
  if (gRefCnt++ == 0)
  {
    mListLock = PR_NewLock();
    if (!mListLock)
        return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWindowMediator::AddListener(nsIWindowMediatorListener* aListener)
{
  NS_ENSURE_ARG_POINTER(aListener);
  
  nsresult rv;
  if (!mListeners) {
    rv = NS_NewISupportsArray(getter_AddRefs(mListeners));
    if (NS_FAILED(rv)) return rv;
  }

  mListeners->AppendElement(aListener);
  
  return NS_OK;
}

NS_IMETHODIMP
nsWindowMediator::RemoveListener(nsIWindowMediatorListener* aListener)
{
  NS_ENSURE_ARG_POINTER(aListener);

  if (!mListeners) return NS_OK;

  mListeners->RemoveElement(aListener);
  
  return NS_OK;
}

PRBool notifyOpenWindow(nsISupports *aElement, void* aData)
{
  nsIWindowMediatorListener* listener =
    NS_REINTERPRET_CAST(nsIWindowMediatorListener*, aElement);
  windowData* winData = (windowData*) aData;

  listener->OnOpenWindow(winData->mWindow);
  return PR_TRUE;
}

PRBool notifyCloseWindow(nsISupports *aElement, void* aData)
{
  nsIWindowMediatorListener* listener =
    NS_REINTERPRET_CAST(nsIWindowMediatorListener*, aElement);
  windowData* winData = (windowData*) aData;
  
  listener->OnCloseWindow(winData->mWindow);
  return PR_TRUE;
}

PRBool notifyWindowTitleChange(nsISupports *aElement, void* aData)
{
  nsIWindowMediatorListener* listener =
    NS_REINTERPRET_CAST(nsIWindowMediatorListener*, aElement);

  windowData* titleData =
    NS_REINTERPRET_CAST(windowData*, aData);
  listener->OnWindowTitleChange(titleData->mWindow,
                                titleData->mTitle);

  return PR_TRUE;
}

