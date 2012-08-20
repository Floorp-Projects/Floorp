/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWebShellWindow_h__
#define nsWebShellWindow_h__

#include "mozilla/Mutex.h"
#include "nsEvent.h"
#include "nsIWebProgressListener.h"
#include "nsITimer.h"
#include "nsCOMPtr.h"
#include "nsXULWindow.h"
#include "nsIWidgetListener.h"

/* Forward declarations.... */
class nsIURI;

namespace mozilla {
class WebShellWindowTimerCallback;
} // namespace mozilla

class nsWebShellWindow : public nsXULWindow,
                         public nsIWebProgressListener,
                         public nsIWidgetListener
{
public:
  nsWebShellWindow(PRUint32 aChromeFlags);

  // nsISupports interface...
  NS_DECL_ISUPPORTS_INHERITED

  // nsWebShellWindow methods...
  nsresult Initialize(nsIXULWindow * aParent, nsIXULWindow * aOpener,
                      nsIURI* aUrl,
                      PRInt32 aInitialWidth, PRInt32 aInitialHeight,
                      bool aIsHiddenWindow,
                      nsWidgetInitData& widgetInitData);

  nsresult Toolbar();

  // nsIWebProgressListener
  NS_DECL_NSIWEBPROGRESSLISTENER

  // nsIBaseWindow
  NS_IMETHOD Destroy();

  // nsIWidgetListener
  virtual nsIXULWindow* GetXULWindow() { return this; }
  virtual nsIPresShell* GetPresShell();
  virtual bool WindowMoved(nsIWidget* aWidget, PRInt32 x, PRInt32 y);
  virtual bool WindowResized(nsIWidget* aWidget, PRInt32 aWidth, PRInt32 aHeight);
  virtual bool RequestWindowClose(nsIWidget* aWidget);
  virtual void SizeModeChanged(nsSizeMode sizeMode);
  virtual void OSToolbarButtonPressed();
  virtual bool ZLevelChanged(bool aImmediate, nsWindowZ *aPlacement,
                             nsIWidget* aRequestBelow, nsIWidget** aActualBelow);
  virtual void WindowActivated();
  virtual void WindowDeactivated();

protected:
  friend class mozilla::WebShellWindowTimerCallback;
  
  virtual ~nsWebShellWindow();

  void                     LoadContentAreas();
  bool                     ExecuteCloseHandler();
  void                     ConstrainToOpenerScreen(PRInt32* aX, PRInt32* aY);

  nsCOMPtr<nsITimer>      mSPTimer;
  mozilla::Mutex          mSPTimerLock;

  void        SetPersistenceTimer(PRUint32 aDirtyFlags);
  void        FirePersistenceTimer();
};


#endif /* nsWebShellWindow_h__ */
