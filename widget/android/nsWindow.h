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
#include "AndroidJavaWrappers.h"

class gfxASurface;

struct ANPEvent;

namespace mozilla {
    class AndroidGeckoEvent;
    class AndroidKeyEvent;

    namespace layers {
        class CompositorParent;
        class CompositorChild;
        class LayerManager;
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
    void OnNativeGestureEvent(mozilla::AndroidGeckoEvent *ae);
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
                                 int32_t *aX,
                                 int32_t *aY);
    NS_IMETHOD Move(double aX,
                    double aY);
    NS_IMETHOD Resize(double aWidth,
                      double aHeight,
                      bool   aRepaint);
    NS_IMETHOD Resize(double aX,
                      double aY,
                      double aWidth,
                      double aHeight,
                      bool aRepaint);
    NS_IMETHOD SetZIndex(int32_t aZIndex);
    NS_IMETHOD PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                           nsIWidget *aWidget,
                           bool aActivate);
    NS_IMETHOD SetSizeMode(int32_t aMode);
    NS_IMETHOD Enable(bool aState);
    virtual bool IsEnabled() const;
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
                         uint32_t aHotspotX,
                         uint32_t aHotspotY) { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD SetHasTransparentBackground(bool aTransparent) { return NS_OK; }
    NS_IMETHOD GetHasTransparentBackground(bool& aTransparent) { aTransparent = false; return NS_OK; }
    NS_IMETHOD HideWindowChrome(bool aShouldHide) { return NS_ERROR_NOT_IMPLEMENTED; }
    virtual void* GetNativeData(uint32_t aDataType);
    NS_IMETHOD SetTitle(const nsAString& aTitle) { return NS_OK; }
    NS_IMETHOD SetIcon(const nsAString& aIconSpec) { return NS_OK; }
    NS_IMETHOD EnableDragDrop(bool aEnable) { return NS_OK; }
    NS_IMETHOD CaptureMouse(bool aCapture) { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD CaptureRollupEvents(nsIRollupListener *aListener,
                                   bool aDoCapture) { return NS_ERROR_NOT_IMPLEMENTED; }

    NS_IMETHOD GetAttention(int32_t aCycleCount) { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD BeginResizeDrag(nsGUIEvent* aEvent, int32_t aHorizontal, int32_t aVertical) { return NS_ERROR_NOT_IMPLEMENTED; }

    NS_IMETHOD NotifyIME(NotificationToIME aNotification) MOZ_OVERRIDE;
    NS_IMETHOD_(void) SetInputContext(const InputContext& aContext,
                                      const InputContextAction& aAction);
    NS_IMETHOD_(InputContext) GetInputContext();

    NS_IMETHOD NotifyIMEOfTextChange(uint32_t aStart,
                                     uint32_t aOldEnd,
                                     uint32_t aNewEnd) MOZ_OVERRIDE;
    virtual nsIMEUpdatePreference GetIMEUpdatePreference();

    LayerManager* GetLayerManager (PLayersChild* aShadowManager = nullptr,
                                   LayersBackend aBackendHint = mozilla::layers::LAYERS_NONE,
                                   LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT,
                                   bool* aAllowRetaining = nullptr);

    NS_IMETHOD ReparentNativeWidget(nsIWidget* aNewParent);

    virtual bool NeedsPaint();
    virtual void DrawWindowUnderlay(LayerManager* aManager, nsIntRect aRect);
    virtual void DrawWindowOverlay(LayerManager* aManager, nsIntRect aRect);

    static void SetCompositor(mozilla::layers::LayerManager* aLayerManager,
                              mozilla::layers::CompositorParent* aCompositorParent,
                              mozilla::layers::CompositorChild* aCompositorChild);
    static void ScheduleComposite();
    static void ScheduleResumeComposition(int width, int height);
    static void ForceIsFirstPaint();
    static float ComputeRenderIntegrity();

    virtual bool WidgetPaintsBackground();

protected:
    void BringToFront();
    nsWindow *FindTopLevel();
    bool DrawTo(gfxASurface *targetSurface);
    bool DrawTo(gfxASurface *targetSurface, const nsIntRect &aRect);
    bool IsTopLevel();
    void RemoveIMEComposition();
    void PostFlushIMEChanges();
    void FlushIMEChanges();

    // Call this function when the users activity is the direct cause of an
    // event (like a keypress or mouse click).
    void UserActivity();

    bool mIsVisible;
    nsTArray<nsWindow*> mChildren;
    nsWindow* mParent;
    nsWindow* mFocus;

    double mStartDist;
    double mLastDist;

    nsCOMPtr<nsIIdleServiceInternal> mIdleService;

    bool mIMEComposing;
    bool mIMEMaskSelectionUpdate, mIMEMaskTextUpdate;
    int32_t mIMEMaskEventsCount; // Mask events when > 0
    nsString mIMEComposingText;
    nsAutoTArray<nsTextRange, 4> mIMERanges;
    bool mIMEUpdatingContext;
    nsAutoTArray<mozilla::AndroidGeckoEvent, 8> mIMEKeyEvents;

    struct IMEChange {
        int32_t mStart, mOldEnd, mNewEnd;

        IMEChange() :
            mStart(-1), mOldEnd(-1), mNewEnd(-1)
        {
        }
        IMEChange(int32_t start, int32_t oldEnd, int32_t newEnd) :
            mStart(start), mOldEnd(oldEnd), mNewEnd(newEnd)
        {
        }
        bool IsEmpty()
        {
            return mStart < 0;
        }
    };
    nsAutoTArray<IMEChange, 4> mIMETextChanges;
    bool mIMESelectionChanged;

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
    void DispatchGestureEvent(uint32_t msg, uint32_t direction, double delta,
                              const nsIntPoint &refPoint, uint64_t time);
    void HandleSpecialKey(mozilla::AndroidGeckoEvent *ae);
    void CreateLayerManager(int aCompositorWidth, int aCompositorHeight);
    void RedrawAll();

    mozilla::AndroidLayerRendererFrame mLayerRendererFrame;

    static nsRefPtr<mozilla::layers::LayerManager> sLayerManager;
    static nsRefPtr<mozilla::layers::CompositorParent> sCompositorParent;
    static nsRefPtr<mozilla::layers::CompositorChild> sCompositorChild;
    static bool sCompositorPaused;
};

#endif /* NSWINDOW_H_ */
