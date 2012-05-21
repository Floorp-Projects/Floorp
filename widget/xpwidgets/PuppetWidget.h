/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This "puppet widget" isn't really a platform widget.  It's intended
 * to be used in widgetless rendering contexts, such as sandboxed
 * content processes.  If any "real" widgetry is needed, the request
 * is forwarded to and/or data received from elsewhere.
 */

#ifndef mozilla_widget_PuppetWidget_h__
#define mozilla_widget_PuppetWidget_h__

#include "nsBaseScreen.h"
#include "nsBaseWidget.h"
#include "nsIScreenManager.h"
#include "nsThreadUtils.h"
#include "nsWeakReference.h"

class gfxASurface;

namespace mozilla {
namespace widget {

class PuppetWidget : public nsBaseWidget, public nsSupportsWeakReference
{
  typedef nsBaseWidget Base;

  // The width and height of the "widget" are clamped to this.
  static const size_t kMaxDimension;

public:
  PuppetWidget(PBrowserChild *aTabChild);
  virtual ~PuppetWidget();

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD Create(nsIWidget*        aParent,
                    nsNativeWidget    aNativeParent,
                    const nsIntRect&  aRect,
                    EVENT_CALLBACK    aHandleEventFunction,
                    nsDeviceContext*  aContext,
                    nsWidgetInitData* aInitData = nsnull);

  virtual already_AddRefed<nsIWidget>
  CreateChild(const nsIntRect  &aRect,
              EVENT_CALLBACK   aHandleEventFunction,
              nsDeviceContext  *aContext,
              nsWidgetInitData *aInitData = nsnull,
              bool             aForceUseIWidgetParent = false);

  NS_IMETHOD Destroy();

  NS_IMETHOD Show(bool aState);
  NS_IMETHOD IsVisible(bool& aState)
  { aState = mVisible; return NS_OK; }

  NS_IMETHOD ConstrainPosition(bool     /*ignored aAllowSlop*/,
                               PRInt32* aX,
                               PRInt32* aY)
  { *aX = kMaxDimension;  *aY = kMaxDimension;  return NS_OK; }

  // We're always at <0, 0>, and so ignore move requests.
  NS_IMETHOD Move(PRInt32 aX, PRInt32 aY)
  { return NS_OK; }

  NS_IMETHOD Resize(PRInt32 aWidth,
                    PRInt32 aHeight,
                    bool    aRepaint);
  NS_IMETHOD Resize(PRInt32 aX,
                    PRInt32 aY,
                    PRInt32 aWidth,
                    PRInt32 aHeight,
                    bool    aRepaint)
  // (we're always at <0, 0>)
  { return Resize(aWidth, aHeight, aRepaint); }

  // XXX/cjones: copying gtk behavior here; unclear what disabling a
  // widget is supposed to entail
  NS_IMETHOD Enable(bool aState)
  { mEnabled = aState;  return NS_OK; }
  NS_IMETHOD IsEnabled(bool *aState)
  { *aState = mEnabled;  return NS_OK; }

  NS_IMETHOD SetFocus(bool aRaise = false);

  // PuppetWidgets don't care about children.
  virtual nsresult ConfigureChildren(const nsTArray<Configuration>& aConfigurations)
  { return NS_OK; }

  NS_IMETHOD Invalidate(const nsIntRect& aRect);

  // This API is going away, steer clear.
  virtual void Scroll(const nsIntPoint& aDelta,
                      const nsTArray<nsIntRect>& aDestRects,
                      const nsTArray<Configuration>& aReconfigureChildren)
  { /* dead man walking */ }

  // PuppetWidgets don't have native data, as they're purely nonnative.
  virtual void* GetNativeData(PRUint32 aDataType);
  NS_IMETHOD ReparentNativeWidget(nsIWidget* aNewParent)
  { return NS_ERROR_UNEXPECTED; }

  // PuppetWidgets don't have any concept of titles. 
  NS_IMETHOD SetTitle(const nsAString& aTitle)
  { return NS_ERROR_UNEXPECTED; }
  
  // PuppetWidgets are always at <0, 0>.
  virtual nsIntPoint WidgetToScreenOffset()
  { return nsIntPoint(0, 0); }

  void InitEvent(nsGUIEvent& event, nsIntPoint* aPoint = nsnull);

  NS_IMETHOD DispatchEvent(nsGUIEvent* event, nsEventStatus& aStatus);

  NS_IMETHOD CaptureRollupEvents(nsIRollupListener* aListener,
                                 bool aDoCapture, bool aConsumeRollupEvent)
  { return NS_ERROR_UNEXPECTED; }

  //
  // nsBaseWidget methods we override
  //

//NS_IMETHOD              CaptureMouse(bool aCapture);
  virtual LayerManager*
  GetLayerManager(PLayersChild* aShadowManager = nsnull,
                  LayersBackend aBackendHint = LayerManager::LAYERS_NONE,
                  LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT,
                  bool* aAllowRetaining = nsnull);
//  virtual nsDeviceContext* GetDeviceContext();
  virtual gfxASurface*      GetThebesSurface();

  NS_IMETHOD ResetInputState();
  NS_IMETHOD_(void) SetInputContext(const InputContext& aContext,
                                    const InputContextAction& aAction);
  NS_IMETHOD_(InputContext) GetInputContext();
  NS_IMETHOD CancelComposition();
  NS_IMETHOD OnIMEFocusChange(bool aFocus);
  NS_IMETHOD OnIMETextChange(PRUint32 aOffset, PRUint32 aEnd,
                             PRUint32 aNewEnd);
  NS_IMETHOD OnIMESelectionChange(void);

  NS_IMETHOD SetCursor(nsCursor aCursor);
  NS_IMETHOD SetCursor(imgIContainer* aCursor,
                       PRUint32 aHotspotX, PRUint32 aHotspotY)
  {
    return nsBaseWidget::SetCursor(aCursor, aHotspotX, aHotspotY);
  }

  // Gets the DPI of the screen corresponding to this widget.
  // Contacts the parent process which gets the DPI from the
  // proper widget there. TODO: Handle DPI changes that happen
  // later on.
  virtual float GetDPI();

private:
  nsresult DispatchPaintEvent();
  nsresult DispatchResizeEvent();

  void SetChild(PuppetWidget* aChild);

  nsresult IMEEndComposition(bool aCancel);

  class PaintTask : public nsRunnable {
  public:
    NS_DECL_NSIRUNNABLE
    PaintTask(PuppetWidget* widget) : mWidget(widget) {}
    void Revoke() { mWidget = nsnull; }
  private:
    PuppetWidget* mWidget;
  };

  // TabChild normally holds a strong reference to this PuppetWidget
  // or its root ancestor, but each PuppetWidget also needs a reference
  // back to TabChild (e.g. to delegate nsIWidget IME calls to chrome)
  // So we hold a weak reference to TabChild (PBrowserChild) here.
  // Since it's possible for TabChild to outlive the PuppetWidget,
  // we clear this weak reference in Destroy()
  PBrowserChild *mTabChild;
  // The "widget" to which we delegate events if we don't have an
  // event handler.
  nsRefPtr<PuppetWidget> mChild;
  nsIntRegion mDirtyRegion;
  nsRevocableEventPtr<PaintTask> mPaintTask;
  bool mEnabled;
  bool mVisible;
  // XXX/cjones: keeping this around until we teach LayerManager to do
  // retained-content-only transactions
  nsRefPtr<gfxASurface> mSurface;
  // IME
  nsIMEUpdatePreference mIMEPreference;
  bool mIMEComposing;
  // Latest seqno received through events
  PRUint32 mIMELastReceivedSeqno;
  // Chrome's seqno value when last blur occurred
  // arriving events with seqno up to this should be discarded
  // Note that if seqno overflows (~50 days at 1 ms increment rate),
  // events will be discarded until new focus/blur occurs
  PRUint32 mIMELastBlurSeqno;

  // The DPI of the screen corresponding to this widget
  float mDPI;
};

class PuppetScreen : public nsBaseScreen
{
public:
    PuppetScreen(void* nativeScreen);
    ~PuppetScreen();

    NS_IMETHOD GetRect(PRInt32* aLeft, PRInt32* aTop, PRInt32* aWidth, PRInt32* aHeight) MOZ_OVERRIDE;
    NS_IMETHOD GetAvailRect(PRInt32* aLeft, PRInt32* aTop, PRInt32* aWidth, PRInt32* aHeight) MOZ_OVERRIDE;
    NS_IMETHOD GetPixelDepth(PRInt32* aPixelDepth) MOZ_OVERRIDE;
    NS_IMETHOD GetColorDepth(PRInt32* aColorDepth) MOZ_OVERRIDE;
    NS_IMETHOD GetRotation(PRUint32* aRotation) MOZ_OVERRIDE;
    NS_IMETHOD SetRotation(PRUint32  aRotation) MOZ_OVERRIDE;
};

class PuppetScreenManager : public nsIScreenManager
{
public:
    PuppetScreenManager();
    ~PuppetScreenManager();

    NS_DECL_ISUPPORTS
    NS_DECL_NSISCREENMANAGER

protected:
    nsCOMPtr<nsIScreen> mOneScreen;
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_PuppetWidget_h__
