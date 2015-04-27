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
#include "GeneratedJNIWrappers.h"
#include "mozilla/EventForwards.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TextRange.h"

struct ANPEvent;

namespace mozilla {
    class AndroidGeckoEvent;
    class TextComposition;

    namespace layers {
        class CompositorParent;
        class CompositorChild;
        class LayerManager;
        class APZCTreeManager;
    }
}

class nsWindow :
    public nsBaseWidget
{
private:
    virtual ~nsWindow();

public:
    using nsBaseWidget::GetLayerManager;

    nsWindow();

    NS_DECL_ISUPPORTS_INHERITED

    static void OnGlobalAndroidEvent(mozilla::AndroidGeckoEvent *ae);
    static gfxIntSize GetAndroidScreenBounds();
    static nsWindow* TopWindow();

    bool OnContextmenuEvent(mozilla::AndroidGeckoEvent *ae);
    void OnLongTapEvent(mozilla::AndroidGeckoEvent *ae);
    bool OnMultitouchEvent(mozilla::AndroidGeckoEvent *ae);
    void OnNativeGestureEvent(mozilla::AndroidGeckoEvent *ae);
    void OnMouseEvent(mozilla::AndroidGeckoEvent *ae);
    void OnKeyEvent(mozilla::AndroidGeckoEvent *ae);
    void OnIMEEvent(mozilla::AndroidGeckoEvent *ae);

    void OnSizeChanged(const gfxIntSize& aSize);

    void InitEvent(mozilla::WidgetGUIEvent& event, nsIntPoint* aPoint = 0);

    //
    // nsIWidget
    //

    NS_IMETHOD Create(nsIWidget *aParent,
                      nsNativeWidget aNativeParent,
                      const nsIntRect &aRect,
                      nsWidgetInitData *aInitData);
    NS_IMETHOD Destroy(void);
    NS_IMETHOD ConfigureChildren(const nsTArray<nsIWidget::Configuration>&);
    NS_IMETHOD SetParent(nsIWidget* aNewParent);
    virtual nsIWidget *GetParent(void);
    virtual float GetDPI();
    virtual double GetDefaultScaleInternal();
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
    void SetZIndex(int32_t aZIndex);
    NS_IMETHOD PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                           nsIWidget *aWidget,
                           bool aActivate);
    NS_IMETHOD SetSizeMode(int32_t aMode);
    NS_IMETHOD Enable(bool aState);
    virtual bool IsEnabled() const;
    NS_IMETHOD Invalidate(const nsIntRect &aRect);
    NS_IMETHOD SetFocus(bool aRaise = false);
    NS_IMETHOD GetScreenBounds(nsIntRect &aRect);
    virtual mozilla::LayoutDeviceIntPoint WidgetToScreenOffset();
    NS_IMETHOD DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                             nsEventStatus& aStatus);
    nsEventStatus DispatchEvent(mozilla::WidgetGUIEvent* aEvent);
    NS_IMETHOD MakeFullScreen(bool aFullScreen, nsIScreen* aTargetScreen = nullptr);
    NS_IMETHOD SetWindowClass(const nsAString& xulWinType);



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
    NS_IMETHOD BeginResizeDrag(mozilla::WidgetGUIEvent* aEvent,
                               int32_t aHorizontal,
                               int32_t aVertical)
    {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD_(void) SetInputContext(const InputContext& aContext,
                                      const InputContextAction& aAction);
    NS_IMETHOD_(InputContext) GetInputContext();

    nsresult NotifyIMEOfTextChange(const IMENotification& aIMENotification);
    virtual nsIMEUpdatePreference GetIMEUpdatePreference();

    LayerManager* GetLayerManager (PLayerTransactionChild* aShadowManager = nullptr,
                                   LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
                                   LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT,
                                   bool* aAllowRetaining = nullptr);

    NS_IMETHOD ReparentNativeWidget(nsIWidget* aNewParent);

    virtual bool NeedsPaint();
    virtual void DrawWindowUnderlay(LayerManagerComposite* aManager, nsIntRect aRect);
    virtual void DrawWindowOverlay(LayerManagerComposite* aManager, nsIntRect aRect);

    virtual mozilla::layers::CompositorParent* NewCompositorParent(int aSurfaceWidth, int aSurfaceHeight) override;

    static void SetCompositor(mozilla::layers::LayerManager* aLayerManager,
                              mozilla::layers::CompositorParent* aCompositorParent,
                              mozilla::layers::CompositorChild* aCompositorChild);
    static bool IsCompositionPaused();
    static void ScheduleComposite();
    static void SchedulePauseComposition();
    static void ScheduleResumeComposition();
    static void ScheduleResumeComposition(int width, int height);
    static void ForceIsFirstPaint();
    static float ComputeRenderIntegrity();
    static mozilla::layers::APZCTreeManager* GetAPZCTreeManager();
    /* RootLayerTreeId() can only be called when GetAPZCTreeManager() returns non-null */
    static uint64_t RootLayerTreeId();

    virtual bool WidgetPaintsBackground();

    virtual uint32_t GetMaxTouchPoints() const override;

protected:
    void BringToFront();
    nsWindow *FindTopLevel();
    bool IsTopLevel();

    struct IMEChange {
        int32_t mStart, mOldEnd, mNewEnd;

        IMEChange() :
            mStart(-1), mOldEnd(-1), mNewEnd(-1)
        {
        }
        IMEChange(const IMENotification& aIMENotification)
            : mStart(aIMENotification.mTextChangeData.mStartOffset)
            , mOldEnd(aIMENotification.mTextChangeData.mOldEndOffset)
            , mNewEnd(aIMENotification.mTextChangeData.mNewEndOffset)
        {
            MOZ_ASSERT(aIMENotification.mMessage ==
                           mozilla::widget::NOTIFY_IME_OF_TEXT_CHANGE,
                       "IMEChange initialized with wrong notification");
            MOZ_ASSERT(aIMENotification.mTextChangeData.IsInInt32Range(),
                       "The text change notification is out of range");
        }
        bool IsEmpty() const
        {
            return mStart < 0;
        }
    };

    nsRefPtr<mozilla::TextComposition> GetIMEComposition();
    void RemoveIMEComposition();
    void AddIMETextChange(const IMEChange& aChange);
    void PostFlushIMEChanges();
    void FlushIMEChanges();

    void ConfigureAPZCTreeManager() override;
    void ConfigureAPZControllerThread() override;

    already_AddRefed<GeckoContentController> CreateRootContentController() override;

    // Call this function when the users activity is the direct cause of an
    // event (like a keypress or mouse click).
    void UserActivity();

    bool mIsVisible;
    nsTArray<nsWindow*> mChildren;
    nsWindow* mParent;

    double mStartDist;
    double mLastDist;

    nsCOMPtr<nsIIdleServiceInternal> mIdleService;

    bool mIMEMaskSelectionUpdate;
    int32_t mIMEMaskEventsCount; // Mask events when > 0
    nsRefPtr<mozilla::TextRangeArray> mIMERanges;
    bool mIMEUpdatingContext;
    nsAutoTArray<mozilla::AndroidGeckoEvent, 8> mIMEKeyEvents;
    nsAutoTArray<IMEChange, 4> mIMETextChanges;
    bool mIMESelectionChanged;

    InputContext mInputContext;

    virtual nsresult NotifyIMEInternal(
                         const IMENotification& aIMENotification) override;

    static void DumpWindows();
    static void DumpWindows(const nsTArray<nsWindow*>& wins, int indent = 0);
    static void LogWindow(nsWindow *win, int index, int indent);

private:
    void InitKeyEvent(mozilla::WidgetKeyboardEvent& event,
                      mozilla::AndroidGeckoEvent& key,
                      ANPEvent* pluginEvent);
    void HandleSpecialKey(mozilla::AndroidGeckoEvent *ae);
    void CreateLayerManager(int aCompositorWidth, int aCompositorHeight);
    void RedrawAll();

    mozilla::AndroidLayerRendererFrame mLayerRendererFrame;

    static mozilla::StaticRefPtr<mozilla::layers::APZCTreeManager> sApzcTreeManager;
    static mozilla::StaticRefPtr<mozilla::layers::LayerManager> sLayerManager;
    static mozilla::StaticRefPtr<mozilla::layers::CompositorParent> sCompositorParent;
    static mozilla::StaticRefPtr<mozilla::layers::CompositorChild> sCompositorChild;
    static bool sCompositorPaused;
};

#endif /* NSWINDOW_H_ */
