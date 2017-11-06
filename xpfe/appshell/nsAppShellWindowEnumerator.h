/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAppShellWindowEnumerator_h
#define nsAppShellWindowEnumerator_h

#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsISimpleEnumerator.h"
#include "nsIXULWindow.h"

class nsWindowMediator;

//
// nsWindowInfo
//

struct nsWindowInfo
{
  nsWindowInfo(nsIXULWindow* inWindow, int32_t inTimeStamp);
  ~nsWindowInfo();

  nsCOMPtr<nsIXULWindow>    mWindow;
  int32_t                   mTimeStamp;
  uint32_t                  mZLevel;

  // each struct is in two, independent, circular, doubly-linked lists
  nsWindowInfo              *mYounger, // next younger in sequence
                            *mOlder;
  nsWindowInfo              *mLower,   // next lower in z-order
                            *mHigher;

  bool TypeEquals(const nsAString &aType);
  void   InsertAfter(nsWindowInfo *inOlder, nsWindowInfo *inHigher);
  void   Unlink(bool inAge, bool inZ);
  void   ReferenceSelf(bool inAge, bool inZ);
};

//
// virtual enumerators
//

class nsAppShellWindowEnumerator : public nsISimpleEnumerator {

friend class nsWindowMediator;

public:
  nsAppShellWindowEnumerator(const char16_t* aTypeString,
                             nsWindowMediator& inMediator);
  NS_IMETHOD GetNext(nsISupports **retval) override = 0;
  NS_IMETHOD HasMoreElements(bool *retval) override;

  NS_DECL_ISUPPORTS

protected:

  virtual ~nsAppShellWindowEnumerator();

  void AdjustInitialPosition();
  virtual nsWindowInfo *FindNext() = 0;

  void WindowRemoved(nsWindowInfo *inInfo);

  nsWindowMediator *mWindowMediator;
  nsString          mType;
  nsWindowInfo     *mCurrentPosition;
};

class nsASDOMWindowEnumerator : public nsAppShellWindowEnumerator {

public:
  nsASDOMWindowEnumerator(const char16_t* aTypeString,
                          nsWindowMediator& inMediator);
  virtual ~nsASDOMWindowEnumerator();
  NS_IMETHOD GetNext(nsISupports **retval) override;
};

class nsASXULWindowEnumerator : public nsAppShellWindowEnumerator {

public:
  nsASXULWindowEnumerator(const char16_t* aTypeString,
                          nsWindowMediator& inMediator);
  virtual ~nsASXULWindowEnumerator();
  NS_IMETHOD GetNext(nsISupports **retval) override;
};

//
// concrete enumerators
//

class nsASDOMWindowEarlyToLateEnumerator : public nsASDOMWindowEnumerator {

public:
  nsASDOMWindowEarlyToLateEnumerator(const char16_t* aTypeString,
                                     nsWindowMediator& inMediator);

  virtual ~nsASDOMWindowEarlyToLateEnumerator();

protected:
  virtual nsWindowInfo *FindNext() override;
};

class nsASXULWindowEarlyToLateEnumerator : public nsASXULWindowEnumerator {

public:
  nsASXULWindowEarlyToLateEnumerator(const char16_t* aTypeString,
                                     nsWindowMediator& inMediator);

  virtual ~nsASXULWindowEarlyToLateEnumerator();

protected:
  virtual nsWindowInfo *FindNext() override;
};

class nsASDOMWindowFrontToBackEnumerator : public nsASDOMWindowEnumerator {

public:
  nsASDOMWindowFrontToBackEnumerator(const char16_t* aTypeString,
                                     nsWindowMediator& inMediator);

  virtual ~nsASDOMWindowFrontToBackEnumerator();

protected:
  virtual nsWindowInfo *FindNext() override;
};

class nsASXULWindowFrontToBackEnumerator : public nsASXULWindowEnumerator {

public:
  nsASXULWindowFrontToBackEnumerator(const char16_t* aTypeString,
                                     nsWindowMediator& inMediator);

  virtual ~nsASXULWindowFrontToBackEnumerator();

protected:
  virtual nsWindowInfo *FindNext() override;
};

class nsASDOMWindowBackToFrontEnumerator : public nsASDOMWindowEnumerator {

public:
  nsASDOMWindowBackToFrontEnumerator(const char16_t* aTypeString,
                                     nsWindowMediator& inMediator);

  virtual ~nsASDOMWindowBackToFrontEnumerator();

protected:
  virtual nsWindowInfo *FindNext() override;
};

class nsASXULWindowBackToFrontEnumerator : public nsASXULWindowEnumerator {

public:
  nsASXULWindowBackToFrontEnumerator(const char16_t* aTypeString,
                                     nsWindowMediator& inMediator);

  virtual ~nsASXULWindowBackToFrontEnumerator();

protected:
  virtual nsWindowInfo *FindNext() override;
};

#endif
