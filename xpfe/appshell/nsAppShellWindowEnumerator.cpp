/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAppShellWindowEnumerator.h"

#include "nsIDocumentViewer.h"
#include "nsIDocShell.h"
#include "mozilla/dom/Document.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIAppWindow.h"
#include "mozilla/dom/Element.h"

#include "nsWindowMediator.h"

using mozilla::dom::Document;
using mozilla::dom::Element;

//
// static helper functions
//

static void GetAttribute(nsIAppWindow* inWindow, const nsAString& inAttribute,
                         nsAString& outValue);
static void GetWindowType(nsIAppWindow* inWindow, nsString& outType);

static Element* GetElementFromDocShell(nsIDocShell* aShell) {
  nsCOMPtr<nsIDocumentViewer> viewer;
  aShell->GetDocViewer(getter_AddRefs(viewer));
  if (viewer) {
    RefPtr<Document> doc = viewer->GetDocument();
    if (doc) {
      return doc->GetDocumentElement();
    }
  }

  return nullptr;
}

// generic "retrieve the value of a XUL attribute" function
void GetAttribute(nsIAppWindow* inWindow, const nsAString& inAttribute,
                  nsAString& outValue) {
  nsCOMPtr<nsIDocShell> shell;
  if (inWindow && NS_SUCCEEDED(inWindow->GetDocShell(getter_AddRefs(shell)))) {
    RefPtr<Element> webshellElement = GetElementFromDocShell(shell);
    if (webshellElement) {
      webshellElement->GetAttribute(inAttribute, outValue);
    }
  }
}

// retrieve the window type, stored as the value of a particular
// attribute in its XUL window tag
void GetWindowType(nsIAppWindow* aWindow, nsString& outType) {
  GetAttribute(aWindow, u"windowtype"_ns, outType);
}

//
// nsWindowInfo
//

nsWindowInfo::nsWindowInfo(nsIAppWindow* inWindow, int32_t inTimeStamp)
    : mWindow(inWindow),
      mTimeStamp(inTimeStamp),
      mZLevel(nsIAppWindow::normalZ) {
  ReferenceSelf(true, true);
}

nsWindowInfo::~nsWindowInfo() {}

// return true if the window described by this WindowInfo has a type
// equal to the given type
bool nsWindowInfo::TypeEquals(const nsAString& aType) {
  nsAutoString rtnString;
  GetWindowType(mWindow, rtnString);
  return rtnString == aType;
}

// insert the struct into their two linked lists, in position after the
// given (independent) method arguments
void nsWindowInfo::InsertAfter(nsWindowInfo* inOlder, nsWindowInfo* inHigher) {
  if (inOlder) {
    mOlder = inOlder;
    mYounger = inOlder->mYounger;
    mOlder->mYounger = this;
    if (mOlder->mOlder == mOlder) mOlder->mOlder = this;
    mYounger->mOlder = this;
    if (mYounger->mYounger == mYounger) mYounger->mYounger = this;
  }
  if (inHigher) {
    mHigher = inHigher;
    mLower = inHigher->mLower;
    mHigher->mLower = this;
    if (mHigher->mHigher == mHigher) mHigher->mHigher = this;
    mLower->mHigher = this;
    if (mLower->mLower == mLower) mLower->mLower = this;
  }
}

// remove the struct from its linked lists
void nsWindowInfo::Unlink(bool inAge, bool inZ) {
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
void nsWindowInfo::ReferenceSelf(bool inAge, bool inZ) {
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

nsAppShellWindowEnumerator::nsAppShellWindowEnumerator(
    const char16_t* aTypeString, nsWindowMediator& aMediator)
    : mWindowMediator(&aMediator),
      mType(aTypeString),
      mCurrentPosition(nullptr) {
  mWindowMediator->AddEnumerator(this);
  NS_ADDREF(mWindowMediator);
}

nsAppShellWindowEnumerator::~nsAppShellWindowEnumerator() {
  mWindowMediator->RemoveEnumerator(this);
  NS_RELEASE(mWindowMediator);
}

// after mCurrentPosition has been initialized to point to the beginning
// of the appropriate list, adjust it if necessary
void nsAppShellWindowEnumerator::AdjustInitialPosition() {
  if (!mType.IsEmpty() && mCurrentPosition &&
      !mCurrentPosition->TypeEquals(mType))
    mCurrentPosition = FindNext();
}

NS_IMETHODIMP nsAppShellWindowEnumerator::HasMoreElements(bool* retval) {
  if (!retval) return NS_ERROR_INVALID_ARG;

  *retval = mCurrentPosition ? true : false;
  return NS_OK;
}

// if a window is being removed adjust the iterator's current position
void nsAppShellWindowEnumerator::WindowRemoved(nsWindowInfo* inInfo) {
  if (mCurrentPosition == inInfo) mCurrentPosition = FindNext();
}

//
// nsASDOMWindowEnumerator
//

nsASDOMWindowEnumerator::nsASDOMWindowEnumerator(const char16_t* aTypeString,
                                                 nsWindowMediator& aMediator)
    : nsAppShellWindowEnumerator(aTypeString, aMediator) {}

nsASDOMWindowEnumerator::~nsASDOMWindowEnumerator() {}

NS_IMETHODIMP nsASDOMWindowEnumerator::GetNext(nsISupports** retval) {
  if (!retval) return NS_ERROR_INVALID_ARG;

  *retval = nullptr;
  while (mCurrentPosition) {
    nsCOMPtr<nsPIDOMWindowOuter> domWindow;
    nsWindowMediator::GetDOMWindow(mCurrentPosition->mWindow, domWindow);
    mCurrentPosition = FindNext();
    if (domWindow) return CallQueryInterface(domWindow, retval);
  }
  return NS_ERROR_FAILURE;
}

//
// nsASAppWindowEnumerator
//

nsASAppWindowEnumerator::nsASAppWindowEnumerator(const char16_t* aTypeString,
                                                 nsWindowMediator& aMediator)
    : nsAppShellWindowEnumerator(aTypeString, aMediator) {}

nsASAppWindowEnumerator::~nsASAppWindowEnumerator() {}

NS_IMETHODIMP nsASAppWindowEnumerator::GetNext(nsISupports** retval) {
  if (!retval) return NS_ERROR_INVALID_ARG;

  *retval = nullptr;
  if (mCurrentPosition) {
    CallQueryInterface(mCurrentPosition->mWindow, retval);
    mCurrentPosition = FindNext();
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

//
// nsASDOMWindowEarlyToLateEnumerator
//

nsASDOMWindowEarlyToLateEnumerator::nsASDOMWindowEarlyToLateEnumerator(
    const char16_t* aTypeString, nsWindowMediator& aMediator)
    : nsASDOMWindowEnumerator(aTypeString, aMediator) {
  mCurrentPosition = aMediator.mOldestWindow;
  AdjustInitialPosition();
}

nsASDOMWindowEarlyToLateEnumerator::~nsASDOMWindowEarlyToLateEnumerator() {}

nsWindowInfo* nsASDOMWindowEarlyToLateEnumerator::FindNext() {
  nsWindowInfo *info, *listEnd;
  bool allWindows = mType.IsEmpty();

  // see AppWindowEarlyToLateEnumerator::FindNext
  if (!mCurrentPosition) return nullptr;

  info = mCurrentPosition->mYounger;
  listEnd = mWindowMediator->mOldestWindow;

  while (info != listEnd) {
    if (allWindows || info->TypeEquals(mType)) return info;
    info = info->mYounger;
  }

  return nullptr;
}

//
// nsASAppWindowEarlyToLateEnumerator
//

nsASAppWindowEarlyToLateEnumerator::nsASAppWindowEarlyToLateEnumerator(
    const char16_t* aTypeString, nsWindowMediator& aMediator)
    : nsASAppWindowEnumerator(aTypeString, aMediator) {
  mCurrentPosition = aMediator.mOldestWindow;
  AdjustInitialPosition();
}

nsASAppWindowEarlyToLateEnumerator::~nsASAppWindowEarlyToLateEnumerator() {}

nsWindowInfo* nsASAppWindowEarlyToLateEnumerator::FindNext() {
  nsWindowInfo *info, *listEnd;
  bool allWindows = mType.IsEmpty();

  /* mCurrentPosition null is assumed to mean that the enumerator has run
     its course and is now basically useless. It could also be interpreted
     to mean that it was created at a time when there were no windows. In
     that case it would probably be more appropriate to check to see whether
     windows have subsequently been added. But it's not guaranteed that we'll
     pick up newly added windows anyway (if they occurred previous to our
     current position) so we just don't worry about that. */
  if (!mCurrentPosition) return nullptr;

  info = mCurrentPosition->mYounger;
  listEnd = mWindowMediator->mOldestWindow;

  while (info != listEnd) {
    if (allWindows || info->TypeEquals(mType)) return info;
    info = info->mYounger;
  }

  return nullptr;
}

//
// nsASAppWindowFrontToBackEnumerator
//

nsASAppWindowFrontToBackEnumerator::nsASAppWindowFrontToBackEnumerator(
    const char16_t* aTypeString, nsWindowMediator& aMediator)
    : nsASAppWindowEnumerator(aTypeString, aMediator) {
  mCurrentPosition = aMediator.mTopmostWindow;
  AdjustInitialPosition();
}

nsASAppWindowFrontToBackEnumerator::~nsASAppWindowFrontToBackEnumerator() {}

nsWindowInfo* nsASAppWindowFrontToBackEnumerator::FindNext() {
  nsWindowInfo *info, *listEnd;
  bool allWindows = mType.IsEmpty();

  // see AppWindowEarlyToLateEnumerator::FindNext
  if (!mCurrentPosition) return nullptr;

  info = mCurrentPosition->mLower;
  listEnd = mWindowMediator->mTopmostWindow;

  while (info != listEnd) {
    if (allWindows || info->TypeEquals(mType)) return info;
    info = info->mLower;
  }

  return nullptr;
}

//
// nsASAppWindowBackToFrontEnumerator
//

nsASAppWindowBackToFrontEnumerator::nsASAppWindowBackToFrontEnumerator(
    const char16_t* aTypeString, nsWindowMediator& aMediator)
    : nsASAppWindowEnumerator(aTypeString, aMediator) {
  mCurrentPosition =
      aMediator.mTopmostWindow ? aMediator.mTopmostWindow->mHigher : nullptr;
  AdjustInitialPosition();
}

nsASAppWindowBackToFrontEnumerator::~nsASAppWindowBackToFrontEnumerator() {}

nsWindowInfo* nsASAppWindowBackToFrontEnumerator::FindNext() {
  nsWindowInfo *info, *listEnd;
  bool allWindows = mType.IsEmpty();

  // see AppWindowEarlyToLateEnumerator::FindNext
  if (!mCurrentPosition) return nullptr;

  info = mCurrentPosition->mHigher;
  listEnd = mWindowMediator->mTopmostWindow;
  if (listEnd) listEnd = listEnd->mHigher;

  while (info != listEnd) {
    if (allWindows || info->TypeEquals(mType)) return info;
    info = info->mHigher;
  }

  return nullptr;
}
