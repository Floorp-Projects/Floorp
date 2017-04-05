/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: set sw=4 ts=4 expandtab:
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
#include "EventDispatcher.h"
#include "GeneratedJNIWrappers.h"
#include "mozilla/EventForwards.h"
#include "mozilla/Mutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TextRange.h"
#include "mozilla/UniquePtr.h"

struct ANPEvent;

namespace mozilla {
    class WidgetTouchEvent;

    namespace layers {
        class CompositorBridgeChild;
        class LayerManager;
        class APZCTreeManager;
        class UiCompositorControllerChild;
    }

    namespace widget {
        class GeckoEditableSupport;
    } // namespace widget

    namespace ipc {
        class Shmem;
    } // namespace ipc
}

class nsWindow : public nsBaseWidget
{
private:
    virtual ~nsWindow();

public:
    using nsBaseWidget::GetLayerManager;

    nsWindow();

    NS_DECL_ISUPPORTS_INHERITED

    static void InitNatives();
    void SetScreenId(uint32_t aScreenId) { mScreenId = aScreenId; }
    void EnableEventDispatcher();

private:
    uint32_t mScreenId;

    // An Event subclass that guards against stale events.
    template<typename Lambda,
             bool IsStatic = Lambda::isStatic,
             typename InstanceType = typename Lambda::ThisArgType,
             class Impl = typename Lambda::TargetClass>
    class WindowEvent;

public:
    // Smart pointer for holding a pointer back to the nsWindow inside a native
    // object class. The nsWindow pointer is automatically cleared when the
    // nsWindow is destroyed, and a WindowPtr<Impl>::Locked class is provided
    // for thread-safe access to the nsWindow pointer off of the Gecko thread.
    template<class Impl> class WindowPtr;

    // Smart pointer for holding a pointer to a native object class. The
    // pointer is automatically cleared when the object is destroyed.
    template<class Impl>
    class NativePtr final
    {
        friend WindowPtr<Impl>;

        static const char sName[];

        WindowPtr<Impl>* mPtr;
        Impl* mImpl;
        mozilla::Mutex mImplLock;

    public:
        class Locked;

        NativePtr() : mPtr(nullptr), mImpl(nullptr), mImplLock(sName) {}
        ~NativePtr() { MOZ_ASSERT(!mPtr); }

        operator Impl*() const
        {
            MOZ_ASSERT(NS_IsMainThread());
            return mImpl;
        }

        Impl* operator->() const { return operator Impl*(); }

        template<class Instance, typename... Args>
        void Attach(Instance aInstance, nsWindow* aWindow, Args&&... aArgs);
        void Detach();
    };

    template<class Impl>
    class WindowPtr final
    {
        friend NativePtr<Impl>;

        NativePtr<Impl>* mPtr;
        nsWindow* mWindow;
        mozilla::Mutex mWindowLock;

    public:
        class Locked final : private mozilla::MutexAutoLock
        {
            nsWindow* const mWindow;

        public:
            Locked(WindowPtr<Impl>& aPtr)
                : mozilla::MutexAutoLock(aPtr.mWindowLock)
                , mWindow(aPtr.mWindow)
            {}

            operator nsWindow*() const { return mWindow; }
            nsWindow* operator->() const { return mWindow; }
        };

        WindowPtr(NativePtr<Impl>* aPtr, nsWindow* aWindow)
            : mPtr(aPtr)
            , mWindow(aWindow)
            , mWindowLock(NativePtr<Impl>::sName)
        {
            MOZ_ASSERT(NS_IsMainThread());
            if (mPtr) {
                mPtr->mPtr = this;
            }
        }

        ~WindowPtr()
        {
            MOZ_ASSERT(NS_IsMainThread());
            if (!mPtr) {
                return;
            }
            mPtr->mPtr = nullptr;
            mPtr->mImpl = nullptr;
        }

        operator nsWindow*() const
        {
            MOZ_ASSERT(NS_IsMainThread());
            return mWindow;
        }

        nsWindow* operator->() const { return operator nsWindow*(); }
    };

private:
    class AndroidView final : public nsIAndroidView
    {
        virtual ~AndroidView() {}

    public:
        const RefPtr<mozilla::widget::EventDispatcher> mEventDispatcher{
            new mozilla::widget::EventDispatcher()};

        AndroidView() {}

        NS_DECL_ISUPPORTS
        NS_DECL_NSIANDROIDVIEW

        NS_FORWARD_NSIANDROIDEVENTDISPATCHER(mEventDispatcher->)

        mozilla::java::GeckoBundle::GlobalRef mSettings;
    };

    RefPtr<AndroidView> mAndroidView;

    class LayerViewSupport;
    // Object that implements native LayerView calls.
    // Owned by the Java LayerView instance.
    NativePtr<LayerViewSupport> mLayerViewSupport;

    class NPZCSupport;
    // Object that implements native NativePanZoomController calls.
    // Owned by the Java NativePanZoomController instance.
    NativePtr<NPZCSupport> mNPZCSupport;

    // Object that implements native GeckoEditable calls.
    // Strong referenced by the Java instance.
    NativePtr<mozilla::widget::GeckoEditableSupport> mEditableSupport;
    mozilla::java::GeckoEditable::GlobalRef mEditable;

    class GeckoViewSupport;
    // Object that implements native GeckoView calls and associated states.
    // nullptr for nsWindows that were not opened from GeckoView.
    // Because other objects get destroyed in the mGeckOViewSupport destructor,
    // keep it last in the list, so its destructor is called first.
    mozilla::UniquePtr<GeckoViewSupport> mGeckoViewSupport;

    // Class that implements native PresentationMediaPlayerManager calls.
    class PMPMSupport;

public:
    static nsWindow* TopWindow();

    static mozilla::Modifiers GetModifiers(int32_t aMetaState);
    static mozilla::TimeStamp GetEventTimeStamp(int64_t aEventTime);

    void OnSizeChanged(const mozilla::gfx::IntSize& aSize);

    void InitEvent(mozilla::WidgetGUIEvent& event,
                   LayoutDeviceIntPoint* aPoint = 0);

    void UpdateOverscrollVelocity(const float aX, const float aY);
    void UpdateOverscrollOffset(const float aX, const float aY);

    //
    // nsIWidget
    //

    using nsBaseWidget::Create; // for Create signature not overridden here
    virtual MOZ_MUST_USE nsresult Create(nsIWidget* aParent,
                                         nsNativeWidget aNativeParent,
                                         const LayoutDeviceIntRect& aRect,
                                         nsWidgetInitData* aInitData) override;
    virtual void Destroy() override;
    virtual nsresult ConfigureChildren(const nsTArray<nsIWidget::Configuration>&) override;
    virtual void SetParent(nsIWidget* aNewParent) override;
    virtual nsIWidget *GetParent(void) override;
    virtual float GetDPI() override;
    virtual double GetDefaultScaleInternal() override;
    virtual void Show(bool aState) override;
    virtual bool IsVisible() const override;
    virtual void ConstrainPosition(bool aAllowSlop,
                                   int32_t *aX,
                                   int32_t *aY) override;
    virtual void Move(double aX,
                      double aY) override;
    virtual void Resize(double aWidth,
                        double aHeight,
                        bool   aRepaint) override;
    virtual void Resize(double aX,
                        double aY,
                        double aWidth,
                        double aHeight,
                        bool aRepaint) override;
    void SetZIndex(int32_t aZIndex) override;
    virtual void SetSizeMode(nsSizeMode aMode) override;
    virtual void Enable(bool aState) override;
    virtual bool IsEnabled() const override;
    virtual void Invalidate(const LayoutDeviceIntRect& aRect) override;
    virtual nsresult SetFocus(bool aRaise = false) override;
    virtual LayoutDeviceIntRect GetScreenBounds() override;
    virtual LayoutDeviceIntPoint WidgetToScreenOffset() override;
    virtual nsresult DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                                   nsEventStatus& aStatus) override;
    nsEventStatus DispatchEvent(mozilla::WidgetGUIEvent* aEvent);
    virtual already_AddRefed<nsIScreen> GetWidgetScreen() override;
    virtual nsresult MakeFullScreen(bool aFullScreen,
                                    nsIScreen* aTargetScreen = nullptr)
                                    override;

    virtual void SetCursor(nsCursor aCursor) override {}
    virtual nsresult SetCursor(imgIContainer* aCursor, uint32_t aHotspotX,
                               uint32_t aHotspotY) override { return NS_ERROR_NOT_IMPLEMENTED; }
    void* GetNativeData(uint32_t aDataType) override;
    void SetNativeData(uint32_t aDataType, uintptr_t aVal) override;
    virtual nsresult SetTitle(const nsAString& aTitle) override { return NS_OK; }
    virtual MOZ_MUST_USE nsresult GetAttention(int32_t aCycleCount) override { return NS_ERROR_NOT_IMPLEMENTED; }


    TextEventDispatcherListener* GetNativeTextEventDispatcherListener() override;
    virtual void SetInputContext(const InputContext& aContext,
                                 const InputContextAction& aAction) override;
    virtual InputContext GetInputContext() override;

    void SetSelectionDragState(bool aState);
    LayerManager* GetLayerManager(PLayerTransactionChild* aShadowManager = nullptr,
                                  LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
                                  LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT) override;

    virtual bool NeedsPaint() override;

    virtual bool WidgetPaintsBackground() override;

    virtual uint32_t GetMaxTouchPoints() const override;

    void UpdateZoomConstraints(const uint32_t& aPresShellId,
                               const FrameMetrics::ViewID& aViewId,
                               const mozilla::Maybe<ZoomConstraints>& aConstraints) override;

    nsresult SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                        TouchPointerState aPointerState,
                                        LayoutDeviceIntPoint aPoint,
                                        double aPointerPressure,
                                        uint32_t aPointerOrientation,
                                        nsIObserver* aObserver) override;
    nsresult SynthesizeNativeMouseEvent(LayoutDeviceIntPoint aPoint,
                                        uint32_t aNativeMessage,
                                        uint32_t aModifierFlags,
                                        nsIObserver* aObserver) override;
    nsresult SynthesizeNativeMouseMove(LayoutDeviceIntPoint aPoint,
                                       nsIObserver* aObserver) override;

    mozilla::layers::CompositorBridgeChild* GetCompositorBridgeChild() const;

    mozilla::jni::DependentRef<mozilla::java::GeckoLayerClient> GetLayerClient();

    // Call this function when the users activity is the direct cause of an
    // event (like a keypress or mouse click).
    void UserActivity();

    mozilla::java::GeckoEditable::Ref& GetEditableParent() { return mEditable; }

    void RecvToolbarAnimatorMessageFromCompositor(int32_t aMessage) override;
    void UpdateRootFrameMetrics(const ScreenPoint& aScrollOffset, const CSSToScreenScale& aZoom, const CSSRect& aPage) override;
    void RecvScreenPixels(mozilla::ipc::Shmem&& aMem, const ScreenIntSize& aSize) override;
protected:
    void BringToFront();
    nsWindow *FindTopLevel();
    bool IsTopLevel();

    void ConfigureAPZControllerThread() override;
    void DispatchHitTest(const mozilla::WidgetTouchEvent& aEvent);

    already_AddRefed<GeckoContentController> CreateRootContentController() override;

    bool mIsVisible;
    nsTArray<nsWindow*> mChildren;
    nsWindow* mParent;

    double mStartDist;
    double mLastDist;

    nsCOMPtr<nsIIdleServiceInternal> mIdleService;

    bool mAwaitingFullScreen;
    bool mIsFullScreen;

    bool UseExternalCompositingSurface() const override {
      return true;
    }

    static void DumpWindows();
    static void DumpWindows(const nsTArray<nsWindow*>& wins, int indent = 0);
    static void LogWindow(nsWindow *win, int index, int indent);

private:
    void CreateLayerManager(int aCompositorWidth, int aCompositorHeight);
    void RedrawAll();

    int64_t GetRootLayerId() const;
    RefPtr<mozilla::layers::UiCompositorControllerChild> GetUiCompositorControllerChild();
};

#endif /* NSWINDOW_H_ */
