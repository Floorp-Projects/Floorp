/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAppShellWindowEnumerator_h
#define nsAppShellWindowEnumerator_h

#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsSimpleEnumerator.h"
#include "nsIAppWindow.h"

class nsWindowMediator;

//
// nsWindowInfo
//

struct nsWindowInfo {
  nsWindowInfo(nsIAppWindow* inWindow, int32_t inTimeStamp);
  ~nsWindowInfo();

  nsCOMPtr<nsIAppWindow> mWindow;
  int32_t mTimeStamp;
  uint32_t mZLevel;

  // each struct is in two, independent, circular, doubly-linked lists
  nsWindowInfo *mYounger,  // next younger in sequence
      *mOlder;
  nsWindowInfo *mLower,  // next lower in z-order
      *mHigher;

  bool TypeEquals(const nsAString& aType);
  void InsertAfter(nsWindowInfo* inOlder, nsWindowInfo* inHigher);
  void Unlink(bool inAge, bool inZ);
  void ReferenceSelf(bool inAge, bool inZ);
};

//
// virtual enumerators
//

class nsAppShellWindowEnumerator : public nsSimpleEnumerator {
  friend class nsWindowMediator;

 public:
  nsAppShellWindowEnumerator(const char16_t* aTypeString,
                             nsWindowMediator& inMediator);
  NS_IMETHOD GetNext(nsISupports** retval) override = 0;
  NS_IMETHOD HasMoreElements(bool* retval) override;

 protected:
  ~nsAppShellWindowEnumerator() override;

  void AdjustInitialPosition();
  virtual nsWindowInfo* FindNext() = 0;

  void WindowRemoved(nsWindowInfo* inInfo);

  nsWindowMediator* mWindowMediator;
  nsString mType;
  nsWindowInfo* mCurrentPosition;
};

class nsASDOMWindowEnumerator : public nsAppShellWindowEnumerator {
 public:
  nsASDOMWindowEnumerator(const char16_t* aTypeString,
                          nsWindowMediator& inMediator);
  virtual ~nsASDOMWindowEnumerator();
  NS_IMETHOD GetNext(nsISupports** retval) override;
};

class nsASAppWindowEnumerator : public nsAppShellWindowEnumerator {
 public:
  nsASAppWindowEnumerator(const char16_t* aTypeString,
                          nsWindowMediator& inMediator);
  virtual ~nsASAppWindowEnumerator();
  NS_IMETHOD GetNext(nsISupports** retval) override;

  const nsID& DefaultInterface() override { return NS_GET_IID(nsIAppWindow); }
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
  virtual nsWindowInfo* FindNext() override;
};

class nsASAppWindowEarlyToLateEnumerator : public nsASAppWindowEnumerator {
 public:
  nsASAppWindowEarlyToLateEnumerator(const char16_t* aTypeString,
                                     nsWindowMediator& inMediator);

  virtual ~nsASAppWindowEarlyToLateEnumerator();

 protected:
  virtual nsWindowInfo* FindNext() override;
};

class nsASAppWindowFrontToBackEnumerator : public nsASAppWindowEnumerator {
 public:
  nsASAppWindowFrontToBackEnumerator(const char16_t* aTypeString,
                                     nsWindowMediator& inMediator);

  virtual ~nsASAppWindowFrontToBackEnumerator();

 protected:
  virtual nsWindowInfo* FindNext() override;
};

class nsASAppWindowBackToFrontEnumerator : public nsASAppWindowEnumerator {
 public:
  nsASAppWindowBackToFrontEnumerator(const char16_t* aTypeString,
                                     nsWindowMediator& inMediator);

  virtual ~nsASAppWindowBackToFrontEnumerator();

 protected:
  virtual nsWindowInfo* FindNext() override;
};

#endif
