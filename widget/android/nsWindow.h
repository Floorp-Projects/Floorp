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
#include "mozilla/UniquePtr.h"

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

    static void InitNatives();
    class Natives;
    // Object that implements native GeckoView calls;
    // nullptr for nsWindows that were not opened from GeckoView.
    mozilla::UniquePtr<Natives> mNatives;
    // GeckoEditable instance used by this nsWindow;
    // nullptr for nsWindows that are not GeckoViews.
    mozilla::widget::GeckoEditable::GlobalRef mEditable;

    static void OnGlobalAndroidEvent(mozilla::AndroidGeckoEvent *ae);
    static mozilla::gfx::IntSize GetAndroidScreenBounds();
    static nsWindow* TopWindow();

    bool OnContextmenuEvent(mozilla::AndroidGeckoEvent *ae);
    void OnLongTapEvent(mozilla::AndroidGeckoEvent *ae);
    bool OnMultitouchEvent(mozilla::AndroidGeckoEvent *ae);
    void OnNativeGestureEvent(mozilla::AndroidGeckoEvent *ae);
    void OnMouseEvent(mozilla::AndroidGeckoEvent *ae);
    void OnKeyEvent(mozilla::AndroidGeckoEvent *ae);
    void OnIMEEvent(mozilla::AndroidGeckoEvent *ae);

    void OnSizeChanged(const mozilla::gfx::IntSize& aSize);

    void InitEvent(mozilla::WidgetGUIEvent& event, nsIntPoint* aPoint = 0);

    //
    // nsIWidget
    //

    NS_IMETHOD Create(nsIWidget *aParent,
                      nsNativeWidget aNativeParent,
                      const nsIntRect &aRect,
                      nsWidgetInitData *aInitData) override;
    NS_IMETHOD Destroy(void) override;
    NS_IMETHOD ConfigureChildren(const nsTArray<nsIWidget::Configuration>&) override;
    NS_IMETHOD SetParent(nsIWidget* aNewParent) override;
    virtual nsIWidget *GetParent(void) override;
    virtual float GetDPI() override;
    virtual double GetDefaultScaleInternal() override;
    NS_IMETHOD Show(bool aState) override;
    NS_IMETHOD SetModal(bool aModal) override;
    virtual bool IsVisible() const override;
    NS_IMETHOD ConstrainPosition(bool aAllowSlop,
                                 int32_t *aX,
                                 int32_t *aY) override;
    NS_IMETHOD Move(double aX,
                    double aY) override;
    NS_IMETHOD Resize(double aWidth,
                      double aHeight,
                      bool   aRepaint) override;
    NS_IMETHOD Resize(double aX,
                      double aY,
                      double aWidth,
                      double aHeight,
                      bool aRepaint) override;
    void SetZIndex(int32_t aZIndex) override;
    NS_IMETHOD PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                           nsIWidget *aWidget,
                           bool aActivate) override;
    NS_IMETHOD SetSizeMode(nsSizeMode aMode) override;
    NS_IMETHOD Enable(bool aState) override;
    virtual bool IsEnabled() const override;
    NS_IMETHOD Invalidate(const nsIntRect &aRect) override;
    NS_IMETHOD SetFocus(bool aRaise = false) override;
    NS_IMETHOD GetScreenBounds(nsIntRect &aRect) override;
    virtual mozilla::LayoutDeviceIntPoint WidgetToScreenOffset() override;
    NS_IMETHOD DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                             nsEventStatus& aStatus) override;
    nsEventStatus DispatchEvent(mozilla::WidgetGUIEvent* aEvent);
    NS_IMETHOD MakeFullScreen(bool aFullScreen, nsIScreen* aTargetScreen = nullptr) override;
    NS_IMETHOD SetWindowClass(const nsAString& xulWinType) override;



    NS_IMETHOD SetCursor(nsCursor aCursor) override { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD SetCursor(imgIContainer* aCursor,
                         uint32_t aHotspotX,
                         uint32_t aHotspotY) override { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD SetHasTransparentBackground(bool aTransparent) { return NS_OK; }
    NS_IMETHOD GetHasTransparentBackground(bool& aTransparent) { aTransparent = false; return NS_OK; }
    NS_IMETHOD HideWindowChrome(bool aShouldHide) override { return NS_ERROR_NOT_IMPLEMENTED; }
    virtual void* GetNativeData(uint32_t aDataType) override;
    NS_IMETHOD SetTitle(const nsAString& aTitle) override { return NS_OK; }
    NS_IMETHOD SetIcon(const nsAString& aIconSpec) override { return NS_OK; }
    NS_IMETHOD EnableDragDrop(bool aEnable) override { return NS_OK; }
    NS_IMETHOD CaptureMouse(bool aCapture) override { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD CaptureRollupEvents(nsIRollupListener *aListener,
                                   bool aDoCapture) override { return NS_ERROR_NOT_IMPLEMENTED; }

    NS_IMETHOD GetAttention(int32_t aCycleCount) override { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD BeginResizeDrag(mozilla::WidgetGUIEvent* aEvent,
                               int32_t aHorizontal,
                               int32_t aVertical) override
    {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD_(void) SetInputContext(const InputContext& aContext,
                                      const InputContextAction& aAction) override;
    NS_IMETHOD_(InputContext) GetInputContext() override;

    nsresult NotifyIMEOfTextChange(const IMENotification& aIMENotification);
    virtual nsIMEUpdatePreference GetIMEUpdatePreference() override;

    LayerManager* GetLayerManager (PLayerTransactionChild* aShadowManager = nullptr,
                                   LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
                                   LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT,
                                   bool* aAllowRetaining = nullptr) override;

    NS_IMETHOD ReparentNativeWidget(nsIWidget* aNewParent) override;

    virtual bool NeedsPaint() override;
    virtual void DrawWindowUnderlay(LayerManagerComposite* aManager, nsIntRect aRect) override;
    virtual void DrawWindowOverlay(LayerManagerComposite* aManager, nsIntRect aRect) override;

    virtual mozilla::layers::CompositorParent* NewCompositorParent(int aSurfaceWidth, int aSurfaceHeight) override;

    static void SetCompositor(mozilla::layers::LayerManager* aLayerManager,
                              mozilla::layers::CompositorParent* aCompositorParent,
                              mozilla::layers::CompositorChild* aCompositorChild);
    static bool IsCompositionPaused();
    static void InvalidateAndScheduleComposite();
    static void SchedulePauseComposition();
    static void ScheduleResumeComposition();
    static void ScheduleResumeComposition(int width, int height);
    static void ForceIsFirstPaint();
    static float ComputeRenderIntegrity();
    static mozilla::layers::APZCTreeManager* GetAPZCTreeManager();
    /* RootLayerTreeId() can only be called when GetAPZCTreeManager() returns non-null */
    static uint64_t RootLayerTreeId();

    virtual bool WidgetPaintsBackground() override;

    virtual uint32_t GetMaxTouchPoints() const override;

    void UpdateZoomConstraints(const uint32_t& aPresShellId,
                               const FrameMetrics::ViewID& aViewId,
                               const mozilla::Maybe<ZoomConstraints>& aConstraints) override;

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
            , mOldEnd(aIMENotification.mTextChangeData.mRemovedEndOffset)
            , mNewEnd(aIMENotification.mTextChangeData.mAddedEndOffset)
        {
            MOZ_ASSERT(aIMENotification.mMessage ==
                           mozilla::widget::NOTIFY_IME_OF_TEXT_CHANGE,
                       "IMEChange initialized with wrong notification");
            MOZ_ASSERT(aIMENotification.mTextChangeData.IsValid(),
                       "The text change notification isn't initialized");
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
    void SendIMEDummyKeyEvents();
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

    bool mAwaitingFullScreen;
    bool mIsFullScreen;

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
