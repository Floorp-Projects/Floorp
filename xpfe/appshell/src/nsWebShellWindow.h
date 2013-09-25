/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWebShellWindow_h__
#define nsWebShellWindow_h__

#include "mozilla/Mutex.h"
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
  nsWebShellWindow(uint32_t aChromeFlags);

  // nsISupports interface...
  NS_DECL_ISUPPORTS_INHERITED

  // nsWebShellWindow methods...
  nsresult Initialize(nsIXULWindow * aParent, nsIXULWindow * aOpener,
                      nsIURI* aUrl,
                      int32_t aInitialWidth, int32_t aInitialHeight,
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
  virtual bool WindowMoved(nsIWidget* aWidget, int32_t x, int32_t y);
  virtual bool WindowResized(nsIWidget* aWidget, int32_t aWidth, int32_t aHeight);
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
  void                     ConstrainToOpenerScreen(int32_t* aX, int32_t* aY);

  nsCOMPtr<nsITimer>      mSPTimer;
  mozilla::Mutex          mSPTimerLock;

  void        SetPersistenceTimer(uint32_t aDirtyFlags);
  void        FirePersistenceTimer();
};


#endif /* nsWebShellWindow_h__ */
