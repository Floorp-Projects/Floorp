/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWindowMediator_h_
#define nsWindowMediator_h_

#include "mozilla/Mutex.h"
#include "nsCOMPtr.h"
#include "nsIWindowMediator.h"
#include "nsISupportsArray.h"
#include "nsIObserver.h"
#include "nsTArray.h"
#include "nsXPIDLString.h"
#include "nsWeakReference.h"
#include "nsCRT.h"

class nsAppShellWindowEnumerator;
class nsASXULWindowEarlyToLateEnumerator;
class nsASDOMWindowEarlyToLateEnumerator;
class nsASDOMWindowFrontToBackEnumerator;
class nsASXULWindowFrontToBackEnumerator;
class nsASDOMWindowBackToFrontEnumerator;
class nsASXULWindowBackToFrontEnumerator;
struct nsWindowInfo;
struct PRLock;

class nsWindowMediator :
  public nsIWindowMediator,
  public nsIObserver,
  public nsSupportsWeakReference
{
friend class nsAppShellWindowEnumerator;
friend class nsASXULWindowEarlyToLateEnumerator;
friend class nsASDOMWindowEarlyToLateEnumerator;
friend class nsASDOMWindowFrontToBackEnumerator;
friend class nsASXULWindowFrontToBackEnumerator;
friend class nsASDOMWindowBackToFrontEnumerator;
friend class nsASXULWindowBackToFrontEnumerator;

public:
  nsWindowMediator();
  virtual ~nsWindowMediator();

  nsresult Init();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWINDOWMEDIATOR
  NS_DECL_NSIOBSERVER

private:
  PRInt32 AddEnumerator(nsAppShellWindowEnumerator* inEnumerator);
  PRInt32 RemoveEnumerator(nsAppShellWindowEnumerator* inEnumerator);
  nsWindowInfo *MostRecentWindowInfo(const PRUnichar* inType);

  nsresult      UnregisterWindow(nsWindowInfo *inInfo);
  nsWindowInfo *GetInfoFor(nsIXULWindow *aWindow);
  nsWindowInfo *GetInfoFor(nsIWidget *aWindow);
  void          SortZOrderFrontToBack();
  void          SortZOrderBackToFront();

  nsTArray<nsAppShellWindowEnumerator*> mEnumeratorList;
  nsWindowInfo *mOldestWindow;
  nsWindowInfo *mTopmostWindow;
  PRInt32       mTimeStamp;
  bool          mSortingZOrder;
  bool          mReady;
  mozilla::Mutex mListLock;

  nsCOMPtr<nsISupportsArray> mListeners;
};

#endif
