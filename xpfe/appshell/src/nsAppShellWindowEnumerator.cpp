/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAppShellWindowEnumerator.h"

#include "nsIContentViewer.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindow.h"
#include "nsIFactory.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIXULWindow.h"

#include "nsWindowMediator.h"

//
// static helper functions
//

static nsresult GetDOMWindow(nsIXULWindow* inWindow,
                             nsCOMPtr<nsIDOMWindow> &outDOMWindow);
static nsCOMPtr<nsIDOMNode> GetDOMNodeFromDocShell(nsIDocShell *aShell);
static void GetAttribute(nsIXULWindow *inWindow, const nsAString &inAttribute,
                         nsAString &outValue);
static void GetWindowType(nsIXULWindow* inWindow, nsString &outType);

// fetch the nsIDOMWindow(Internal) from a XUL Window
nsresult GetDOMWindow(nsIXULWindow *aWindow, nsCOMPtr<nsIDOMWindow> &aDOMWindow)
{
  nsCOMPtr<nsIDocShell> docShell;

  aWindow->GetDocShell(getter_AddRefs(docShell));
  aDOMWindow = do_GetInterface(docShell);
  return aDOMWindow ? NS_OK : NS_ERROR_FAILURE;
}

nsCOMPtr<nsIDOMNode> GetDOMNodeFromDocShell(nsIDocShell *aShell)
{
  nsCOMPtr<nsIDOMNode> node;

  nsCOMPtr<nsIContentViewer> cv;
  aShell->GetContentViewer(getter_AddRefs(cv));
  if (cv) {
    nsCOMPtr<nsIDOMDocument> domdoc(do_QueryInterface(cv->GetDocument()));
    if (domdoc) {
      nsCOMPtr<nsIDOMElement> element;
      domdoc->GetDocumentElement(getter_AddRefs(element));
      if (element)
        node = element;
    }
  }

  return node;
}

// generic "retrieve the value of a XUL attribute" function
void GetAttribute(nsIXULWindow *inWindow, const nsAString &inAttribute,
                  nsAString &outValue)
{
  nsCOMPtr<nsIDocShell> shell;
  if (inWindow && NS_SUCCEEDED(inWindow->GetDocShell(getter_AddRefs(shell)))) {
    nsCOMPtr<nsIDOMNode> node(GetDOMNodeFromDocShell(shell));
    if (node) {
      nsCOMPtr<nsIDOMElement> webshellElement(do_QueryInterface(node));
      if (webshellElement)
        webshellElement->GetAttribute(inAttribute, outValue);
    }
  }
}

// retrieve the window type, stored as the value of a particular
// attribute in its XUL window tag
void GetWindowType(nsIXULWindow* aWindow, nsString &outType)
{
  GetAttribute(aWindow, NS_LITERAL_STRING("windowtype"), outType);
}

//
// nsWindowInfo
//

nsWindowInfo::nsWindowInfo(nsIXULWindow* inWindow, int32_t inTimeStamp) :
  mWindow(inWindow),mTimeStamp(inTimeStamp),mZLevel(nsIXULWindow::normalZ)
{
  ReferenceSelf(true, true);
}

nsWindowInfo::~nsWindowInfo()
{
}

// return true if the window described by this WindowInfo has a type
// equal to the given type
bool nsWindowInfo::TypeEquals(const nsAString &aType)
{ 
  nsAutoString rtnString;
  GetWindowType(mWindow, rtnString);
  return rtnString == aType;
}

// insert the struct into their two linked lists, in position after the
// given (independent) method arguments
void nsWindowInfo::InsertAfter(nsWindowInfo *inOlder , nsWindowInfo *inHigher)
{
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
void nsWindowInfo::Unlink(bool inAge, bool inZ)
{
  if (inAge) {
    mOlder->mYounger = mYounger;
    mYounger->mOlder = mOlder;
  }
  if (inZ) {
    mLower->mHigher = mHigher;
    mHigher->mLower = mLower;
  }
  ReferenceSelf(inAge, inZ);
}

// initialize the struct to be a valid linked list of one element
void nsWindowInfo::ReferenceSelf(bool inAge, bool inZ)
{
  if (inAge) {
    mYounger = this;
    mOlder = this;
  }
  if (inZ) {
    mLower = this;
    mHigher = this;
  }
}

//
// nsAppShellWindowEnumerator
//

NS_IMPL_ISUPPORTS1(nsAppShellWindowEnumerator, nsISimpleEnumerator)

nsAppShellWindowEnumerator::nsAppShellWindowEnumerator(
    const PRUnichar* aTypeString,
    nsWindowMediator& aMediator) :
      mWindowMediator(&aMediator), mType(aTypeString), mCurrentPosition(nullptr)
{
  mWindowMediator->AddEnumerator(this);
  NS_ADDREF(mWindowMediator);
}

nsAppShellWindowEnumerator::~nsAppShellWindowEnumerator()
{
  mWindowMediator->RemoveEnumerator(this);
  NS_RELEASE(mWindowMediator);
}

// after mCurrentPosition has been initialized to point to the beginning
// of the appropriate list, adjust it if necessary
void nsAppShellWindowEnumerator::AdjustInitialPosition()
{
  if (!mType.IsEmpty() && mCurrentPosition && !mCurrentPosition->TypeEquals(mType))
    mCurrentPosition = FindNext();
}

NS_IMETHODIMP nsAppShellWindowEnumerator::HasMoreElements(bool *retval)
{
  if (!retval)
    return NS_ERROR_INVALID_ARG;

  *retval = mCurrentPosition ? true : false;
  return NS_OK;
}

// if a window is being removed adjust the iterator's current position
void nsAppShellWindowEnumerator::WindowRemoved(nsWindowInfo *inInfo)
{
  if (mCurrentPosition == inInfo)
    mCurrentPosition = FindNext();
}

//
// nsASDOMWindowEnumerator
//

nsASDOMWindowEnumerator::nsASDOMWindowEnumerator(
    const PRUnichar* aTypeString,
    nsWindowMediator& aMediator) :
      nsAppShellWindowEnumerator(aTypeString, aMediator)
{
}

nsASDOMWindowEnumerator::~nsASDOMWindowEnumerator()
{
}

NS_IMETHODIMP nsASDOMWindowEnumerator::GetNext(nsISupports **retval)
{
  if (!retval)
    return NS_ERROR_INVALID_ARG;

  *retval = nullptr;
  while (mCurrentPosition) {
    nsCOMPtr<nsIDOMWindow> domWindow;
    GetDOMWindow(mCurrentPosition->mWindow, domWindow);
    mCurrentPosition = FindNext();
    if (domWindow)
      return CallQueryInterface(domWindow, retval);
  }
  return NS_OK;
}

//
// nsASXULWindowEnumerator
//

nsASXULWindowEnumerator::nsASXULWindowEnumerator(
    const PRUnichar* aTypeString,
    nsWindowMediator& aMediator) :
      nsAppShellWindowEnumerator(aTypeString, aMediator)
{
}

nsASXULWindowEnumerator::~nsASXULWindowEnumerator()
{
}

NS_IMETHODIMP nsASXULWindowEnumerator::GetNext(nsISupports **retval)
{
  if (!retval)
    return NS_ERROR_INVALID_ARG;

  *retval = nullptr;
  if (mCurrentPosition) {
    CallQueryInterface(mCurrentPosition->mWindow, retval);
    mCurrentPosition = FindNext();
  }
  return NS_OK;
}

//
// nsASDOMWindowEarlyToLateEnumerator
//

nsASDOMWindowEarlyToLateEnumerator::nsASDOMWindowEarlyToLateEnumerator(
    const PRUnichar *aTypeString,
    nsWindowMediator &aMediator) :
      nsASDOMWindowEnumerator(aTypeString, aMediator)
{
  mCurrentPosition = aMediator.mOldestWindow;
  AdjustInitialPosition();
}

nsASDOMWindowEarlyToLateEnumerator::~nsASDOMWindowEarlyToLateEnumerator()
{
}

nsWindowInfo *nsASDOMWindowEarlyToLateEnumerator::FindNext()
{
  nsWindowInfo *info,
               *listEnd;
  bool          allWindows = mType.IsEmpty();

  // see nsXULWindowEarlyToLateEnumerator::FindNext
  if (!mCurrentPosition)
    return nullptr;

  info = mCurrentPosition->mYounger;
  listEnd = mWindowMediator->mOldestWindow;

  while (info != listEnd) {
    if (allWindows || info->TypeEquals(mType))
      return info;
    info = info->mYounger;
  }

  return nullptr;
}

//
// nsASXULWindowEarlyToLateEnumerator
//

nsASXULWindowEarlyToLateEnumerator::nsASXULWindowEarlyToLateEnumerator(
    const PRUnichar *aTypeString,
    nsWindowMediator &aMediator) :
      nsASXULWindowEnumerator(aTypeString, aMediator)
{
  mCurrentPosition = aMediator.mOldestWindow;
  AdjustInitialPosition();
}

nsASXULWindowEarlyToLateEnumerator::~nsASXULWindowEarlyToLateEnumerator()
{
}

nsWindowInfo *nsASXULWindowEarlyToLateEnumerator::FindNext()
{
  nsWindowInfo *info,
               *listEnd;
  bool          allWindows = mType.IsEmpty();

  /* mCurrentPosition null is assumed to mean that the enumerator has run
     its course and is now basically useless. It could also be interpreted
     to mean that it was created at a time when there were no windows. In
     that case it would probably be more appropriate to check to see whether
     windows have subsequently been added. But it's not guaranteed that we'll
     pick up newly added windows anyway (if they occurred previous to our
     current position) so we just don't worry about that. */
  if (!mCurrentPosition)
    return nullptr;

  info = mCurrentPosition->mYounger;
  listEnd = mWindowMediator->mOldestWindow;

  while (info != listEnd) {
    if (allWindows || info->TypeEquals(mType))
      return info;
    info = info->mYounger;
  }

  return nullptr;
}

//
// nsASDOMWindowFrontToBackEnumerator
//

nsASDOMWindowFrontToBackEnumerator::nsASDOMWindowFrontToBackEnumerator(
    const PRUnichar *aTypeString,
    nsWindowMediator &aMediator) :
      nsASDOMWindowEnumerator(aTypeString, aMediator)
{
  mCurrentPosition = aMediator.mTopmostWindow;
  AdjustInitialPosition();
}

nsASDOMWindowFrontToBackEnumerator::~nsASDOMWindowFrontToBackEnumerator()
{
}

nsWindowInfo *nsASDOMWindowFrontToBackEnumerator::FindNext()
{
  nsWindowInfo *info,
               *listEnd;
  bool          allWindows = mType.IsEmpty();

  // see nsXULWindowEarlyToLateEnumerator::FindNext
  if (!mCurrentPosition)
    return nullptr;

  info = mCurrentPosition->mLower;
  listEnd = mWindowMediator->mTopmostWindow;

  while (info != listEnd) {
    if (allWindows || info->TypeEquals(mType))
      return info;
    info = info->mLower;
  }

  return nullptr;
}

//
// nsASXULWindowFrontToBackEnumerator
//

nsASXULWindowFrontToBackEnumerator::nsASXULWindowFrontToBackEnumerator(
    const PRUnichar *aTypeString,
    nsWindowMediator &aMediator) :
      nsASXULWindowEnumerator(aTypeString, aMediator)
{
  mCurrentPosition = aMediator.mTopmostWindow;
  AdjustInitialPosition();
}

nsASXULWindowFrontToBackEnumerator::~nsASXULWindowFrontToBackEnumerator()
{
}

nsWindowInfo *nsASXULWindowFrontToBackEnumerator::FindNext()
{
  nsWindowInfo *info,
               *listEnd;
  bool          allWindows = mType.IsEmpty();

  // see nsXULWindowEarlyToLateEnumerator::FindNext
  if (!mCurrentPosition)
    return nullptr;

  info = mCurrentPosition->mLower;
  listEnd = mWindowMediator->mTopmostWindow;

  while (info != listEnd) {
    if (allWindows || info->TypeEquals(mType))
      return info;
    info = info->mLower;
  }

  return nullptr;
}

//
// nsASDOMWindowBackToFrontEnumerator
//

nsASDOMWindowBackToFrontEnumerator::nsASDOMWindowBackToFrontEnumerator(
    const PRUnichar *aTypeString,
    nsWindowMediator &aMediator) :
  nsASDOMWindowEnumerator(aTypeString, aMediator)
{
  mCurrentPosition = aMediator.mTopmostWindow ?
                     aMediator.mTopmostWindow->mHigher : nullptr;
  AdjustInitialPosition();
}

nsASDOMWindowBackToFrontEnumerator::~nsASDOMWindowBackToFrontEnumerator()
{
}

nsWindowInfo *nsASDOMWindowBackToFrontEnumerator::FindNext()
{
  nsWindowInfo *info,
               *listEnd;
  bool          allWindows = mType.IsEmpty();

  // see nsXULWindowEarlyToLateEnumerator::FindNext
  if (!mCurrentPosition)
    return nullptr;

  info = mCurrentPosition->mHigher;
  listEnd = mWindowMediator->mTopmostWindow;
  if (listEnd)
    listEnd = listEnd->mHigher;

  while (info != listEnd) {
    if (allWindows || info->TypeEquals(mType))
      return info;
    info = info->mHigher;
  }

  return nullptr;
}

//
// nsASXULWindowBackToFrontEnumerator
//

nsASXULWindowBackToFrontEnumerator::nsASXULWindowBackToFrontEnumerator(
    const PRUnichar *aTypeString,
    nsWindowMediator &aMediator) :
      nsASXULWindowEnumerator(aTypeString, aMediator)
{
  mCurrentPosition = aMediator.mTopmostWindow ?
                     aMediator.mTopmostWindow->mHigher : nullptr;
  AdjustInitialPosition();
}

nsASXULWindowBackToFrontEnumerator::~nsASXULWindowBackToFrontEnumerator()
{
}

nsWindowInfo *nsASXULWindowBackToFrontEnumerator::FindNext()
{
  nsWindowInfo *info,
               *listEnd;
  bool          allWindows = mType.IsEmpty();

  // see nsXULWindowEarlyToLateEnumerator::FindNext
  if (!mCurrentPosition)
    return nullptr;

  info = mCurrentPosition->mHigher;
  listEnd = mWindowMediator->mTopmostWindow;
  if (listEnd)
    listEnd = listEnd->mHigher;

  while (info != listEnd) {
    if (allWindows || info->TypeEquals(mType))
      return info;
    info = info->mHigher;
  }

  return nullptr;
}
