/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSWINDOW_H_
#define NSWINDOW_H_

#include "nsBaseWidget.h"
#include "gfxPoint.h"
#include "nsIIdleServiceInternal.h"
#include "nsTArray.h"

#ifdef MOZ_JAVA_COMPOSITOR
#include "AndroidJavaWrappers.h"
#include "Layers.h"
#endif

class gfxASurface;

struct ANPEvent;

namespace mozilla {
    class AndroidGeckoEvent;
    class AndroidKeyEvent;

    namespace layers {
        class CompositorParent;
        class CompositorChild;
    }
}

class nsWindow :
    public nsBaseWidget
{
public:
    using nsBaseWidget::GetLayerManager;

    nsWindow();
    virtual ~nsWindow();

    NS_DECL_ISUPPORTS_INHERITED

    static void OnGlobalAndroidEvent(mozilla::AndroidGeckoEvent *ae);
    static gfxIntSize GetAndroidScreenBounds();
    static nsWindow* TopWindow();

    nsWindow* FindWindowForPoint(const nsIntPoint& pt);

    void OnAndroidEvent(mozilla::AndroidGeckoEvent *ae);
    void OnDraw(mozilla::AndroidGeckoEvent *ae);
    bool OnMultitouchEvent(mozilla::AndroidGeckoEvent *ae);
    void OnGestureEvent(mozilla::AndroidGeckoEvent *ae);
    void OnMouseEvent(mozilla::AndroidGeckoEvent *ae);
    void OnKeyEvent(mozilla::AndroidGeckoEvent *ae);
    void OnIMEEvent(mozilla::AndroidGeckoEvent *ae);

    void OnSizeChanged(const gfxIntSize& aSize);

    void InitEvent(nsGUIEvent& event, nsIntPoint* aPoint = 0);

    //
    // nsIWidget
    //

    NS_IMETHOD Create(nsIWidget *aParent,
                      nsNativeWidget aNativeParent,
                      const nsIntRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsDeviceContext *aContext,
                      nsWidgetInitData *aInitData);
    NS_IMETHOD Destroy(void);
    NS_IMETHOD ConfigureChildren(const nsTArray<nsIWidget::Configuration>&);
    NS_IMETHOD SetParent(nsIWidget* aNewParent);
    virtual nsIWidget *GetParent(void);
    virtual float GetDPI();
    NS_IMETHOD Show(bool aState);
    NS_IMETHOD SetModal(bool aModal);
    virtual bool IsVisible() const;
    NS_IMETHOD ConstrainPosition(bool aAllowSlop,
                                 PRInt32 *aX,
                                 PRInt32 *aY);
    NS_IMETHOD Move(PRInt32 aX,
                    PRInt32 aY);
    NS_IMETHOD Resize(PRInt32 aWidth,
                      PRInt32 aHeight,
                      bool    aRepaint);
    NS_IMETHOD Resize(PRInt32 aX,
                      PRInt32 aY,
                      PRInt32 aWidth,
                      PRInt32 aHeight,
                      bool aRepaint);
    NS_IMETHOD SetZIndex(PRInt32 aZIndex);
    NS_IMETHOD PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                           nsIWidget *aWidget,
                           bool aActivate);
    NS_IMETHOD SetSizeMode(PRInt32 aMode);
    NS_IMETHOD Enable(bool aState);
    NS_IMETHOD IsEnabled(bool *aState);
    NS_IMETHOD Invalidate(const nsIntRect &aRect);
    NS_IMETHOD SetFocus(bool aRaise = false);
    NS_IMETHOD GetScreenBounds(nsIntRect &aRect);
    virtual nsIntPoint WidgetToScreenOffset();
    NS_IMETHOD DispatchEvent(nsGUIEvent *aEvent, nsEventStatus &aStatus);
    nsEventStatus DispatchEvent(nsGUIEvent *aEvent);
    NS_IMETHOD MakeFullScreen(bool aFullScreen);
    NS_IMETHOD SetWindowClass(const nsAString& xulWinType);



    NS_IMETHOD SetForegroundColor(const nscolor &aColor) { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD SetBackgroundColor(const nscolor &aColor) { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD SetCursor(nsCursor aCursor) { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD SetCursor(imgIContainer* aCursor,
                         PRUint32 aHotspotX,
                         PRUint32 aHotspotY) { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD SetHasTransparentBackground(bool aTransparent) { return NS_OK; }
    NS_IMETHOD GetHasTransparentBackground(bool& aTransparent) { aTransparent = false; return NS_OK; }
    NS_IMETHOD HideWindowChrome(bool aShouldHide) { return NS_ERROR_NOT_IMPLEMENTED; }
    virtual void* GetNativeData(PRUint32 aDataType);
    NS_IMETHOD SetTitle(const nsAString& aTitle) { return NS_OK; }
    NS_IMETHOD SetIcon(const nsAString& aIconSpec) { return NS_OK; }
    NS_IMETHOD EnableDragDrop(bool aEnable) { return NS_OK; }
    NS_IMETHOD CaptureMouse(bool aCapture) { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD CaptureRollupEvents(nsIRollupListener *aListener,
                                   bool aDoCapture,
                                   bool aConsumeRollupEvent) { return NS_ERROR_NOT_IMPLEMENTED; }

    NS_IMETHOD GetAttention(PRInt32 aCycleCount) { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD BeginResizeDrag(nsGUIEvent* aEvent, PRInt32 aHorizontal, PRInt32 aVertical) { return NS_ERROR_NOT_IMPLEMENTED; }

    NS_IMETHOD ResetInputState();
    NS_IMETHOD_(void) SetInputContext(const InputContext& aContext,
                                      const InputContextAction& aAction);
    NS_IMETHOD_(InputContext) GetInputContext();
    NS_IMETHOD CancelIMEComposition();

    NS_IMETHOD OnIMEFocusChange(bool aFocus);
    NS_IMETHOD OnIMETextChange(PRUint32 aStart, PRUint32 aOldEnd, PRUint32 aNewEnd);
    NS_IMETHOD OnIMESelectionChange(void);
    virtual nsIMEUpdatePreference GetIMEUpdatePreference();

    LayerManager* GetLayerManager (PLayersChild* aShadowManager = nsnull,
                                   LayersBackend aBackendHint = mozilla::layers::LAYERS_NONE,
                                   LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT,
                                   bool* aAllowRetaining = nsnull);

    NS_IMETHOD ReparentNativeWidget(nsIWidget* aNewParent);

#ifdef MOZ_JAVA_COMPOSITOR
    virtual void DrawWindowUnderlay(LayerManager* aManager, nsIntRect aRect);
    virtual void DrawWindowOverlay(LayerManager* aManager, nsIntRect aRect);

    static void SetCompositor(mozilla::layers::CompositorParent* aCompositorParent,
                              mozilla::layers::CompositorChild* aCompositorChild);
    static void ScheduleComposite();
    static void SchedulePauseComposition();
    static void ScheduleResumeComposition(int width, int height);

    virtual bool WidgetPaintsBackground() { return true; }
#endif

protected:
    void BringToFront();
    nsWindow *FindTopLevel();
    bool DrawTo(gfxASurface *targetSurface);
    bool DrawTo(gfxASurface *targetSurface, const nsIntRect &aRect);
    bool IsTopLevel();
    void OnIMEAddRange(mozilla::AndroidGeckoEvent *ae);

    // Call this function when the users activity is the direct cause of an
    // event (like a keypress or mouse click).
    void UserActivity();

    bool mIsVisible;
    nsTArray<nsWindow*> mChildren;
    nsWindow* mParent;
    nsWindow* mFocus;

    bool mGestureFinished;
    double mStartDist;
    double mLastDist;
    nsAutoPtr<nsIntPoint> mStartPoint;

    // Multitouch swipe thresholds in screen pixels
    double mSwipeMaxPinchDelta;
    double mSwipeMinDistance;

    nsCOMPtr<nsIIdleServiceInternal> mIdleService;

    bool mIMEComposing;
    nsString mIMEComposingText;
    nsString mIMELastDispatchedComposingText;
    nsAutoTArray<nsTextRange, 4> mIMERanges;

    InputContext mInputContext;

    static void DumpWindows();
    static void DumpWindows(const nsTArray<nsWindow*>& wins, int indent = 0);
    static void LogWindow(nsWindow *win, int index, int indent);

private:
    void InitKeyEvent(nsKeyEvent& event, mozilla::AndroidGeckoEvent& key,
                      ANPEvent* pluginEvent);
    bool DispatchMultitouchEvent(nsTouchEvent &event,
                             mozilla::AndroidGeckoEvent *ae);
    void DispatchMotionEvent(nsInputEvent &event,
                             mozilla::AndroidGeckoEvent *ae,
                             const nsIntPoint &refPoint);
    void DispatchGestureEvent(PRUint32 msg, PRUint32 direction, double delta,
                              const nsIntPoint &refPoint, PRUint64 time);
    void HandleSpecialKey(mozilla::AndroidGeckoEvent *ae);
    void RedrawAll();

#ifdef MOZ_JAVA_COMPOSITOR
    mozilla::AndroidLayerRendererFrame mLayerRendererFrame;

    static nsRefPtr<mozilla::layers::CompositorParent> sCompositorParent;
    static nsRefPtr<mozilla::layers::CompositorChild> sCompositorChild;
    static bool sCompositorPaused;
#endif
};

#endif /* NSWINDOW_H_ */
