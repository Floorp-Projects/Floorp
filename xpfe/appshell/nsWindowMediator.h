/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWindowMediator_h_
#define nsWindowMediator_h_

#include "nsCOMPtr.h"
#include "nsIWindowMediator.h"
#include "nsIObserver.h"
#include "nsTArray.h"
#include "nsXPIDLString.h"
#include "nsWeakReference.h"
#include "nsCOMArray.h"

class nsAppShellWindowEnumerator;
class nsASXULWindowEarlyToLateEnumerator;
class nsASDOMWindowEarlyToLateEnumerator;
class nsASDOMWindowFrontToBackEnumerator;
class nsASXULWindowFrontToBackEnumerator;
class nsASDOMWindowBackToFrontEnumerator;
class nsASXULWindowBackToFrontEnumerator;
class nsIWindowMediatorListener;
struct nsWindowInfo;

class nsWindowMediator :
  public nsIWindowMediator_44,
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

protected:
  virtual ~nsWindowMediator();

public:
  nsWindowMediator();

  nsresult Init();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWINDOWMEDIATOR
  NS_DECL_NSIWINDOWMEDIATOR_44
  NS_DECL_NSIOBSERVER

  static nsresult GetDOMWindow(nsIXULWindow* inWindow,
                               nsCOMPtr<nsIDOMWindow>& outDOMWindow);

private:
  int32_t AddEnumerator(nsAppShellWindowEnumerator* inEnumerator);
  int32_t RemoveEnumerator(nsAppShellWindowEnumerator* inEnumerator);
  nsWindowInfo* MostRecentWindowInfo(const char16_t* inType,
                                     bool aSkipPrivateBrowsingOrClosed = false);

  nsresult      UnregisterWindow(nsWindowInfo *inInfo);
  nsWindowInfo *GetInfoFor(nsIXULWindow *aWindow);
  nsWindowInfo *GetInfoFor(nsIWidget *aWindow);
  void          SortZOrderFrontToBack();
  void          SortZOrderBackToFront();

  nsTArray<nsAppShellWindowEnumerator*> mEnumeratorList;
  nsWindowInfo *mOldestWindow;
  nsWindowInfo *mTopmostWindow;
  int32_t       mTimeStamp;
  bool          mSortingZOrder;
  bool          mReady;

  nsCOMArray<nsIWindowMediatorListener> mListeners;
};

#endif
