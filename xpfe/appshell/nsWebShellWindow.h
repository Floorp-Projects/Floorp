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
                               public nsIWebProgressListener
{
public:

  // The implementation of non-refcounted nsIWidgetListener, which would hold a
  // strong reference on stack before calling nsWebShellWindow
  class WidgetListenerDelegate : public nsIWidgetListener
  {
  public:
    explicit WidgetListenerDelegate(nsWebShellWindow* aWebShellWindow)
      : mWebShellWindow(aWebShellWindow) {}

    virtual nsIXULWindow* GetXULWindow() override;
    virtual nsIPresShell* GetPresShell() override;
    virtual bool WindowMoved(nsIWidget* aWidget, int32_t x, int32_t y) override;
    virtual bool WindowResized(nsIWidget* aWidget, int32_t aWidth, int32_t aHeight) override;
    virtual bool RequestWindowClose(nsIWidget* aWidget) override;
    virtual void SizeModeChanged(nsSizeMode sizeMode) override;
    virtual void UIResolutionChanged() override;
    virtual void FullscreenWillChange(bool aInFullscreen) override;
    virtual void FullscreenChanged(bool aInFullscreen) override;
    virtual void OcclusionStateChanged(bool aIsFullyOccluded) override;
    virtual void OSToolbarButtonPressed() override;
    virtual bool ZLevelChanged(bool aImmediate,
                               nsWindowZ *aPlacement,
                               nsIWidget* aRequestBelow,
                               nsIWidget** aActualBelow) override;
    virtual void WindowActivated() override;
    virtual void WindowDeactivated() override;

  private:
    // The lifetime of WidgetListenerDelegate is bound to nsWebShellWindow so
    // we just use a raw pointer here.
    nsWebShellWindow* mWebShellWindow;
  };

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

  // nsIWidgetListener methods for WidgetListenerDelegate.
  nsIXULWindow* GetXULWindow() { return this; }
  nsIPresShell* GetPresShell();
  bool WindowMoved(nsIWidget* aWidget, int32_t aX, int32_t aY);
  bool WindowResized(nsIWidget* aWidget, int32_t aWidth, int32_t aHeight);
  bool RequestWindowClose(nsIWidget* aWidget);
  void SizeModeChanged(nsSizeMode aSizeMode);
  void UIResolutionChanged();
  void FullscreenWillChange(bool aInFullscreen);
  void FullscreenChanged(bool aInFullscreen);
  void OcclusionStateChanged(bool aIsFullyOccluded);
  void OSToolbarButtonPressed();
  bool ZLevelChanged(bool aImmediate, nsWindowZ *aPlacement,
                     nsIWidget* aRequestBelow, nsIWidget** aActualBelow);
  void WindowActivated();
  void WindowDeactivated();

protected:
  friend class mozilla::WebShellWindowTimerCallback;

  virtual ~nsWebShellWindow();

  bool                     ExecuteCloseHandler();
  void                     ConstrainToOpenerScreen(int32_t* aX, int32_t* aY);

  nsCOMPtr<nsITimer>      mSPTimer;
  mozilla::Mutex          mSPTimerLock;
  WidgetListenerDelegate  mWidgetListenerDelegate;

  void        SetPersistenceTimer(uint32_t aDirtyFlags);
  void        FirePersistenceTimer();
};

#endif /* nsWebShellWindow_h__ */
