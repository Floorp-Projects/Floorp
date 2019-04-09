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
#include "nsIRemoteTab.h"

/* Forward declarations.... */
class nsIURI;

struct nsWidgetInitData;

namespace mozilla {
class PresShell;
class WebShellWindowTimerCallback;
}  // namespace mozilla

class nsWebShellWindow final : public nsXULWindow,
                               public nsIWebProgressListener {
 public:
  // The implementation of non-refcounted nsIWidgetListener, which would hold a
  // strong reference on stack before calling nsWebShellWindow's
  // MOZ_CAN_RUN_SCRIPT methods.
  class WidgetListenerDelegate : public nsIWidgetListener {
   public:
    explicit WidgetListenerDelegate(nsWebShellWindow* aWebShellWindow)
        : mWebShellWindow(aWebShellWindow) {}

    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual nsIXULWindow* GetXULWindow() override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual mozilla::PresShell* GetPresShell() override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual bool WindowMoved(nsIWidget* aWidget, int32_t x, int32_t y) override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual bool WindowResized(nsIWidget* aWidget, int32_t aWidth,
                               int32_t aHeight) override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual bool RequestWindowClose(nsIWidget* aWidget) override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual void SizeModeChanged(nsSizeMode sizeMode) override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual void UIResolutionChanged() override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual void FullscreenWillChange(bool aInFullscreen) override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual void FullscreenChanged(bool aInFullscreen) override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual void OcclusionStateChanged(bool aIsFullyOccluded) override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual void OSToolbarButtonPressed() override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual bool ZLevelChanged(bool aImmediate, nsWindowZ* aPlacement,
                               nsIWidget* aRequestBelow,
                               nsIWidget** aActualBelow) override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
    virtual void WindowActivated() override;
    MOZ_CAN_RUN_SCRIPT_BOUNDARY
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
  nsresult Initialize(nsIXULWindow* aParent, nsIXULWindow* aOpener,
                      nsIURI* aUrl, int32_t aInitialWidth,
                      int32_t aInitialHeight, bool aIsHiddenWindow,
                      nsIRemoteTab* aOpeningTab,
                      mozIDOMWindowProxy* aOpenerWIndow,
                      nsWidgetInitData& widgetInitData);

  nsresult Toolbar();

  // nsIWebProgressListener
  NS_DECL_NSIWEBPROGRESSLISTENER

  // nsIBaseWindow
  NS_IMETHOD Destroy() override;

  // nsIWidgetListener methods for WidgetListenerDelegate.
  nsIXULWindow* GetXULWindow() { return this; }
  mozilla::PresShell* GetPresShell();
  MOZ_CAN_RUN_SCRIPT
  bool WindowMoved(nsIWidget* aWidget, int32_t aX, int32_t aY);
  MOZ_CAN_RUN_SCRIPT
  bool WindowResized(nsIWidget* aWidget, int32_t aWidth, int32_t aHeight);
  MOZ_CAN_RUN_SCRIPT bool RequestWindowClose(nsIWidget* aWidget);
  MOZ_CAN_RUN_SCRIPT void SizeModeChanged(nsSizeMode aSizeMode);
  MOZ_CAN_RUN_SCRIPT void UIResolutionChanged();
  MOZ_CAN_RUN_SCRIPT void FullscreenWillChange(bool aInFullscreen);
  MOZ_CAN_RUN_SCRIPT void FullscreenChanged(bool aInFullscreen);
  MOZ_CAN_RUN_SCRIPT void OcclusionStateChanged(bool aIsFullyOccluded);
  MOZ_CAN_RUN_SCRIPT void OSToolbarButtonPressed();
  MOZ_CAN_RUN_SCRIPT
  bool ZLevelChanged(bool aImmediate, nsWindowZ* aPlacement,
                     nsIWidget* aRequestBelow, nsIWidget** aActualBelow);
  MOZ_CAN_RUN_SCRIPT void WindowActivated();
  MOZ_CAN_RUN_SCRIPT void WindowDeactivated();

 protected:
  friend class mozilla::WebShellWindowTimerCallback;

  virtual ~nsWebShellWindow();

  bool ExecuteCloseHandler();
  void ConstrainToOpenerScreen(int32_t* aX, int32_t* aY);

  nsCOMPtr<nsITimer> mSPTimer;
  mozilla::Mutex mSPTimerLock;
  WidgetListenerDelegate mWidgetListenerDelegate;

  void SetPersistenceTimer(uint32_t aDirtyFlags);
  void FirePersistenceTimer();
};

#endif /* nsWebShellWindow_h__ */
