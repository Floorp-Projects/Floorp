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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#include "nsString.h"

#include "nsISimpleEnumerator.h"
#include "nsIXULWindow.h"

class nsWindowMediator;

//
// nsWindowInfo
//

struct nsWindowInfo
{
  nsWindowInfo(nsIXULWindow* inWindow, PRInt32 inTimeStamp);
  ~nsWindowInfo();

  nsCOMPtr<nsIXULWindow>    mWindow;
  PRInt32                   mTimeStamp;
  PRUint32                  mZLevel;

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
  nsAppShellWindowEnumerator(const PRUnichar* aTypeString,
                             nsWindowMediator& inMediator);
  virtual ~nsAppShellWindowEnumerator();
  NS_IMETHOD GetNext(nsISupports **retval) = 0;
  NS_IMETHOD HasMoreElements(bool *retval);

  NS_DECL_ISUPPORTS

protected:

  void AdjustInitialPosition();
  virtual nsWindowInfo *FindNext() = 0;

  void WindowRemoved(nsWindowInfo *inInfo);

  nsWindowMediator *mWindowMediator;
  nsString          mType;
  nsWindowInfo     *mCurrentPosition;
};

class nsASDOMWindowEnumerator : public nsAppShellWindowEnumerator {

public:
  nsASDOMWindowEnumerator(const PRUnichar* aTypeString,
                          nsWindowMediator& inMediator);
  virtual ~nsASDOMWindowEnumerator();
  NS_IMETHOD GetNext(nsISupports **retval);
};

class nsASXULWindowEnumerator : public nsAppShellWindowEnumerator {

public:
  nsASXULWindowEnumerator(const PRUnichar* aTypeString,
                          nsWindowMediator& inMediator);
  virtual ~nsASXULWindowEnumerator();
  NS_IMETHOD GetNext(nsISupports **retval);
};

//
// concrete enumerators
//

class nsASDOMWindowEarlyToLateEnumerator : public nsASDOMWindowEnumerator {

public:
  nsASDOMWindowEarlyToLateEnumerator(const PRUnichar* aTypeString,
                                     nsWindowMediator& inMediator);

  virtual ~nsASDOMWindowEarlyToLateEnumerator();

protected:
  virtual nsWindowInfo *FindNext();
};

class nsASXULWindowEarlyToLateEnumerator : public nsASXULWindowEnumerator {

public:
  nsASXULWindowEarlyToLateEnumerator(const PRUnichar* aTypeString,
                                     nsWindowMediator& inMediator);

  virtual ~nsASXULWindowEarlyToLateEnumerator();

protected:
  virtual nsWindowInfo *FindNext();
};

class nsASDOMWindowFrontToBackEnumerator : public nsASDOMWindowEnumerator {

public:
  nsASDOMWindowFrontToBackEnumerator(const PRUnichar* aTypeString,
                                     nsWindowMediator& inMediator);

  virtual ~nsASDOMWindowFrontToBackEnumerator();

protected:
  virtual nsWindowInfo *FindNext();
};

class nsASXULWindowFrontToBackEnumerator : public nsASXULWindowEnumerator {

public:
  nsASXULWindowFrontToBackEnumerator(const PRUnichar* aTypeString,
                                     nsWindowMediator& inMediator);

  virtual ~nsASXULWindowFrontToBackEnumerator();

protected:
  virtual nsWindowInfo *FindNext();
};

class nsASDOMWindowBackToFrontEnumerator : public nsASDOMWindowEnumerator {

public:
  nsASDOMWindowBackToFrontEnumerator(const PRUnichar* aTypeString,
                                     nsWindowMediator& inMediator);

  virtual ~nsASDOMWindowBackToFrontEnumerator();

protected:
  virtual nsWindowInfo *FindNext();
};

class nsASXULWindowBackToFrontEnumerator : public nsASXULWindowEnumerator {

public:
  nsASXULWindowBackToFrontEnumerator(const PRUnichar* aTypeString,
                                     nsWindowMediator& inMediator);

  virtual ~nsASXULWindowBackToFrontEnumerator();

protected:
  virtual nsWindowInfo *FindNext();
};
