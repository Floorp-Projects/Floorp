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
#include "nsITabParent.h"

/* Forward declarations.... */
class nsIURI;

struct nsWidgetInitData;

namespace mozilla {
class WebShellWindowTimerCallback;
} // namespace mozilla

class nsWebShellWindow final : public nsXULWindow,
                               public nsIWebProgressListener,
                               public nsIWidgetListener
{
public:
  explicit nsWebShellWindow(uint32_t aChromeFlags);

  // nsISupports interface...
  NS_DECL_ISUPPORTS_INHERITED

  // nsWebShellWindow methods...
  nsresult Initialize(nsIXULWindow * aParent, nsIXULWindow * aOpener,
                      nsIURI* aUrl,
                      int32_t aInitialWidth, int32_t aInitialHeight,
                      bool aIsHiddenWindow,
                      nsITabParent *aOpeningTab,
                      mozIDOMWindowProxy *aOpenerWIndow,
                      nsWidgetInitData& widgetInitData);

  nsresult Toolbar();

  // nsIWebProgressListener
  NS_DECL_NSIWEBPROGRESSLISTENER

  // nsIBaseWindow
  NS_IMETHOD Destroy() override;

  // nsIWidgetListener
  virtual nsIXULWindow* GetXULWindow() override { return this; }
  virtual nsIPresShell* GetPresShell() override;
  virtual bool WindowMoved(nsIWidget* aWidget, int32_t x, int32_t y) override;
  virtual bool WindowResized(nsIWidget* aWidget, int32_t aWidth, int32_t aHeight) override;
  virtual bool RequestWindowClose(nsIWidget* aWidget) override;
  virtual void SizeModeChanged(nsSizeMode sizeMode) override;
  virtual void UIResolutionChanged() override;
  virtual void FullscreenChanged(bool aInFullscreen) override;
  virtual void OSToolbarButtonPressed() override;
  virtual bool ZLevelChanged(bool aImmediate, nsWindowZ *aPlacement,
                             nsIWidget* aRequestBelow, nsIWidget** aActualBelow) override;
  virtual void WindowActivated() override;
  virtual void WindowDeactivated() override;

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
