/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsIContentViewer.h"
#include "nsIDocShell.h"
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIFactory.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIXULWindow.h"

#include "nsAppShellWindowEnumerator.h"
#include "nsWindowMediator.h"

/********************************************************************/
/************************ static helper functions *******************/
/********************************************************************/

static nsresult GetDOMWindow(nsIXULWindow* inWindow,
                  nsCOMPtr<nsIDOMWindowInternal> &outDOMWindow);
static nsCOMPtr<nsIDOMNode> GetDOMNodeFromDocShell(nsIDocShell *aShell);
static void GetAttribute( nsIXULWindow* inWindow,
              const nsAutoString& inAttribute, nsAutoString& outValue);
static void GetWindowType(nsIXULWindow* inWindow, nsString &outType);

// fetch the nsIDOMWindow(Internal) from a XUL Window
nsresult GetDOMWindow(nsIXULWindow *aWindow, nsCOMPtr<nsIDOMWindowInternal> &aDOMWindow)
{
  nsCOMPtr<nsIDocShell> docShell;

  aWindow->GetDocShell(getter_AddRefs(docShell));
  aDOMWindow = do_GetInterface(docShell);
  return aDOMWindow ? NS_OK : NS_ERROR_FAILURE;
}


// QueryInterface fu
nsCOMPtr<nsIDOMNode> GetDOMNodeFromDocShell(nsIDocShell *aShell)
{
  nsCOMPtr<nsIDOMNode> node;

  nsCOMPtr<nsIContentViewer> cv;
  aShell->GetContentViewer(getter_AddRefs(cv));
  if (cv) {
    nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
    if (docv) {
      nsCOMPtr<nsIDocument> doc;
      docv->GetDocument(*getter_AddRefs(doc));
      if (doc) {
        nsCOMPtr<nsIDOMDocument> domdoc(do_QueryInterface(doc));
        if (domdoc) {
          nsCOMPtr<nsIDOMElement> element;
          domdoc->GetDocumentElement(getter_AddRefs(element));
          if (element)
            node = do_QueryInterface(element);
        }
      }
    }
  }

  return node;
}

// generic "retrieve the value of a XUL attribute" function
void GetAttribute(nsIXULWindow* aWindow, const nsAReadableString &anAttribute,
                  nsAWritableString &outValue)
{
  nsCOMPtr<nsIDocShell> shell;
  if (aWindow &&
      NS_SUCCEEDED(aWindow->GetDocShell(getter_AddRefs(shell)))) {

    nsCOMPtr<nsIDOMNode> node(GetDOMNodeFromDocShell(shell));

    if (node) {
      nsCOMPtr<nsIDOMElement> webshellElement(do_QueryInterface(node));
      if (webshellElement)
        webshellElement->GetAttribute(anAttribute, outValue);
    }
  }
}

// retrieve the window type, stored as the value of a particular
// attribute in its XUL window tag
void GetWindowType(nsIXULWindow* aWindow, nsString &outType)
{
  GetAttribute(aWindow, NS_LITERAL_STRING("windowtype"), outType);
}

/********************************************************************/
/**************************** nsWindowInfo **************************/
/********************************************************************/

nsWindowInfo::nsWindowInfo(nsIXULWindow* inWindow, PRInt32 inTimeStamp) :
  mWindow(inWindow),mTimeStamp(inTimeStamp)
{
  ReferenceSelf(PR_TRUE, PR_TRUE);
}

nsWindowInfo::~nsWindowInfo()
{
}
  
// return true if the window described by this WindowInfo has a type
// equal to the given type
PRBool nsWindowInfo::TypeEquals(nsAReadableString &aType)
{ 
  nsAutoString rtnString;
  GetWindowType(mWindow, rtnString);
  return rtnString == aType;
}

// insert the struct into their two linked lists, in position after the
// given (independent) method arguments
void nsWindowInfo::InsertAfter(nsWindowInfo *inOlder , nsWindowInfo *inHigher) {
  if (inOlder) {
    mOlder = inOlder;
    mYounger = inOlder->mYounger;
    mOlder->mYounger = this;
    if (mOlder->mOlder == mOlder)
      mOlder->mOlder = this;
    mYounger->mOlder = this;
    if (mYounger->mYounger == mYounger)
      mYounger->mYounger = this;
  }
  if (inHigher) {
    mHigher = inHigher;
    mLower = inHigher->mLower;
    mHigher->mLower = this;
    if (mHigher->mHigher == mHigher)
      mHigher->mHigher = this;
    mLower->mHigher = this;
    if (mLower->mLower == mLower)
      mLower->mLower = this;
  }
}

// remove the struct from its linked lists
void nsWindowInfo::Unlink(PRBool inAge, PRBool inZ) {

  if (inAge) {
    mOlder->mYounger = mYounger;
    mYounger->mOlder = mOlder;
  }
  if (inZ) {
    mLower->mHigher = mHigher;
    mHigher->mLower = mLower;
  }
  ReferenceSelf( inAge, inZ );
}

// initialize the struct to be a valid linked list of one element
void nsWindowInfo::ReferenceSelf(PRBool inAge, PRBool inZ) {

  if (inAge) {
    mYounger = this;
    mOlder = this;
  }
  if (inZ) {
    mLower = this;
    mHigher = this;
  }
}

/********************************************************************/
/*********************** nsAppShellWindowEnumerator *****************/
/********************************************************************/

NS_IMPL_ISUPPORTS1(nsAppShellWindowEnumerator, nsISimpleEnumerator);

nsAppShellWindowEnumerator::nsAppShellWindowEnumerator (
    const PRUnichar* aTypeString,
    nsWindowMediator& aMediator) :

    mWindowMediator(&aMediator), mType(aTypeString),
    mCurrentPosition(0)
{
  NS_INIT_REFCNT();

  mWindowMediator->AddEnumerator(this);
  mWindowMediator->AddRef();
}

nsAppShellWindowEnumerator::~nsAppShellWindowEnumerator() {

  mWindowMediator->RemoveEnumerator(this);
  mWindowMediator->Release();
}

// after mCurrentPosition has been initialized to point to the beginning
// of the appropriate list, adjust it if necessary
void nsAppShellWindowEnumerator::AdjustInitialPosition() {

  if (!mType.IsEmpty() && mCurrentPosition && !mCurrentPosition->TypeEquals(mType))
    mCurrentPosition = FindNext();
}

NS_IMETHODIMP nsAppShellWindowEnumerator::HasMoreElements(PRBool *retval)
{
  if (!retval)
    return NS_ERROR_INVALID_ARG;

  *retval = mCurrentPosition ? PR_TRUE : PR_FALSE;
  return NS_OK;
}
	
// if a window is being removed adjust the iterator's current position
void nsAppShellWindowEnumerator::WindowRemoved(nsWindowInfo *inInfo) {

  if (mCurrentPosition == inInfo)
    mCurrentPosition = FindNext();
}

/********************************************************************/
/*********************** nsASDOMWindowEnumerator ********************/
/********************************************************************/

nsASDOMWindowEnumerator::nsASDOMWindowEnumerator(
    const PRUnichar* aTypeString,
    nsWindowMediator& aMediator) :

  nsAppShellWindowEnumerator(aTypeString, aMediator) {

}

nsASDOMWindowEnumerator::~nsASDOMWindowEnumerator() {
}

NS_IMETHODIMP nsASDOMWindowEnumerator::GetNext(nsISupports **retval) {

  if (!retval)
    return NS_ERROR_INVALID_ARG;

  *retval = NULL;
  if (mCurrentPosition) {
    nsCOMPtr<nsIDOMWindowInternal> domWindow;
    GetDOMWindow(mCurrentPosition->mWindow, domWindow);
    CallQueryInterface(domWindow, retval);
    mCurrentPosition = FindNext();
  }
  return NS_OK;
};

/********************************************************************/
/*********************** nsASXULWindowEnumerator ********************/
/********************************************************************/

nsASXULWindowEnumerator::nsASXULWindowEnumerator(
    const PRUnichar* aTypeString,
    nsWindowMediator& aMediator) :

  nsAppShellWindowEnumerator(aTypeString, aMediator) {

}

nsASXULWindowEnumerator::~nsASXULWindowEnumerator() {
}

NS_IMETHODIMP nsASXULWindowEnumerator::GetNext(nsISupports **retval) {

  if (!retval)
    return NS_ERROR_INVALID_ARG;

  *retval = NULL;
  if (mCurrentPosition) {
    CallQueryInterface(mCurrentPosition->mWindow, retval);
    mCurrentPosition = FindNext();
  }
  return NS_OK;
};

/********************************************************************/
/****************** nsASDOMWindowEarlyToLateEnumerator **************/
/********************************************************************/

nsASDOMWindowEarlyToLateEnumerator::nsASDOMWindowEarlyToLateEnumerator(
    const PRUnichar *aTypeString,
    nsWindowMediator &aMediator) :

  nsASDOMWindowEnumerator(aTypeString, aMediator) {

  mCurrentPosition = aMediator.mOldestWindow;
  AdjustInitialPosition();
}

nsASDOMWindowEarlyToLateEnumerator::~nsASDOMWindowEarlyToLateEnumerator() {
}

nsWindowInfo *nsASDOMWindowEarlyToLateEnumerator::FindNext() {

  nsWindowInfo *info,
               *listEnd;
  PRBool        allWindows = mType.Length() == 0;

  // see nsXULWindowEarlyToLateEnumerator::FindNext
  if (!mCurrentPosition)
    return 0;

  info = mCurrentPosition->mYounger;
  listEnd = mWindowMediator->mOldestWindow;

  while (info != listEnd) {
    if (allWindows || info->TypeEquals(mType))
      return info;
    info = info->mYounger;
  }

  return 0;
}

/********************************************************************/
/****************** nsASXULWindowEarlyToLateEnumerator **************/
/********************************************************************/

nsASXULWindowEarlyToLateEnumerator::nsASXULWindowEarlyToLateEnumerator(
    const PRUnichar *aTypeString,
    nsWindowMediator &aMediator) :

  nsASXULWindowEnumerator(aTypeString, aMediator) {

  mCurrentPosition = aMediator.mOldestWindow;
  AdjustInitialPosition();
}

nsASXULWindowEarlyToLateEnumerator::~nsASXULWindowEarlyToLateEnumerator() {
}

nsWindowInfo *nsASXULWindowEarlyToLateEnumerator::FindNext() {

  nsWindowInfo *info,
               *listEnd;
  PRBool        allWindows = mType.Length() == 0;

  /* mCurrentPosition null is assumed to mean that the enumerator has run
     its course and is now basically useless. It could also be interpreted
     to mean that it was created at a time when there were no windows. In
     that case it would probably be more appropriate to check to see whether
     windows have subsequently been added. But it's not guaranteed that we'll
     pick up newly added windows anyway (if they occurred previous to our
     current position) so we just don't worry about that. */
  if (!mCurrentPosition)
    return 0;

  info = mCurrentPosition->mYounger;
  listEnd = mWindowMediator->mOldestWindow;

  while (info != listEnd) {
    if (allWindows || info->TypeEquals(mType))
      return info;
    info = info->mYounger;
  }

  return 0;
}

/********************************************************************/
/****************** nsASDOMWindowFrontToBackEnumerator **************/
/********************************************************************/

nsASDOMWindowFrontToBackEnumerator::nsASDOMWindowFrontToBackEnumerator(
    const PRUnichar *aTypeString,
    nsWindowMediator &aMediator) :

  nsASDOMWindowEnumerator(aTypeString, aMediator) {

  mCurrentPosition = aMediator.mTopmostWindow;
  AdjustInitialPosition();
}

nsASDOMWindowFrontToBackEnumerator::~nsASDOMWindowFrontToBackEnumerator() {
}

nsWindowInfo *nsASDOMWindowFrontToBackEnumerator::FindNext() {

  nsWindowInfo *info,
               *listEnd;
  PRBool        allWindows = mType.Length() == 0;

  // see nsXULWindowEarlyToLateEnumerator::FindNext
  if (!mCurrentPosition)
    return 0;

  info = mCurrentPosition->mLower;
  listEnd = mWindowMediator->mTopmostWindow;

  while (info != listEnd) {
    if (allWindows || info->TypeEquals(mType))
      return info;
    info = info->mLower;
  }

  return 0;
}

/********************************************************************/
/****************** nsASXULWindowFrontToBackEnumerator **************/
/********************************************************************/

nsASXULWindowFrontToBackEnumerator::nsASXULWindowFrontToBackEnumerator(
    const PRUnichar *aTypeString,
    nsWindowMediator &aMediator) :

  nsASXULWindowEnumerator(aTypeString, aMediator) {

  mCurrentPosition = aMediator.mTopmostWindow;
  AdjustInitialPosition();
}

nsASXULWindowFrontToBackEnumerator::~nsASXULWindowFrontToBackEnumerator()
{
}

nsWindowInfo *nsASXULWindowFrontToBackEnumerator::FindNext() {

  nsWindowInfo *info,
               *listEnd;
  PRBool        allWindows = mType.Length() == 0;

  // see nsXULWindowEarlyToLateEnumerator::FindNext
  if (!mCurrentPosition)
    return 0;

  info = mCurrentPosition->mLower;
  listEnd = mWindowMediator->mTopmostWindow;

  while (info != listEnd) {
    if (allWindows || info->TypeEquals(mType))
      return info;
    info = info->mLower;
  }

  return 0;
}

/********************************************************************/
/****************** nsASDOMWindowBackToFrontEnumerator **************/
/********************************************************************/

nsASDOMWindowBackToFrontEnumerator::nsASDOMWindowBackToFrontEnumerator(
    const PRUnichar *aTypeString,
    nsWindowMediator &aMediator) :

  nsASDOMWindowEnumerator(aTypeString, aMediator) {

  mCurrentPosition = aMediator.mTopmostWindow ?
                     aMediator.mTopmostWindow->mHigher : 0;
  AdjustInitialPosition();
}

nsASDOMWindowBackToFrontEnumerator::~nsASDOMWindowBackToFrontEnumerator() {
}

nsWindowInfo *nsASDOMWindowBackToFrontEnumerator::FindNext() {

  nsWindowInfo *info,
               *listEnd;
  PRBool        allWindows = mType.Length() == 0;

  // see nsXULWindowEarlyToLateEnumerator::FindNext
  if (!mCurrentPosition)
    return 0;

  info = mCurrentPosition->mHigher;
  listEnd = mWindowMediator->mTopmostWindow;
  if (listEnd)
    listEnd = listEnd->mHigher;

  while (info != listEnd) {
    if (allWindows || info->TypeEquals(mType))
      return info;
    info = info->mHigher;
  }

  return 0;
}

/********************************************************************/
/****************** nsASXULWindowBackToFrontEnumerator **************/
/********************************************************************/

nsASXULWindowBackToFrontEnumerator::nsASXULWindowBackToFrontEnumerator(
    const PRUnichar *aTypeString,
    nsWindowMediator &aMediator) :

  nsASXULWindowEnumerator(aTypeString, aMediator) {

  mCurrentPosition = aMediator.mTopmostWindow ?
                     aMediator.mTopmostWindow->mHigher : 0;
  AdjustInitialPosition();
}

nsASXULWindowBackToFrontEnumerator::~nsASXULWindowBackToFrontEnumerator()
{
}

nsWindowInfo *nsASXULWindowBackToFrontEnumerator::FindNext() {

  nsWindowInfo *info,
               *listEnd;
  PRBool        allWindows = mType.Length() == 0;

  // see nsXULWindowEarlyToLateEnumerator::FindNext
  if (!mCurrentPosition)
    return 0;

  info = mCurrentPosition->mHigher;
  listEnd = mWindowMediator->mTopmostWindow;
  if (listEnd)
    listEnd = listEnd->mHigher;

  while (info != listEnd) {
    if (allWindows || info->TypeEquals(mType))
      return info;
    info = info->mHigher;
  }

  return 0;
}

