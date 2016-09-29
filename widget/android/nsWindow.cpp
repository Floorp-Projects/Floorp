/* -*- Mode: c++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <math.h>
#include <unistd.h>

#include "mozilla/IMEStateManager.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/WeakPtr.h"

#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/Unused.h"
#include "mozilla/Preferences.h"
#include "mozilla/layers/RenderTrace.h"
#include <algorithm>

using mozilla::dom::ContentParent;
using mozilla::dom::ContentChild;
using mozilla::Unused;

#include "nsWindow.h"

#include "nsIBaseWindow.h"
#include "nsIDOMChromeWindow.h"
#include "nsIObserverService.h"
#include "nsISelection.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"
#include "nsIWidgetListener.h"
#include "nsIWindowWatcher.h"
#include "nsIXULWindow.h"

#include "nsAppShell.h"
#include "nsFocusManager.h"
#include "nsIdleService.h"
#include "nsLayoutUtils.h"
#include "nsViewManager.h"

#include "WidgetUtils.h"

#include "nsIDOMSimpleGestureEvent.h"

#include "nsGkAtoms.h"
#include "nsWidgetsCID.h"
#include "nsGfxCIID.h"

#include "gfxContext.h"

#include "Layers.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/AsyncCompositionManager.h"
#include "mozilla/layers/APZEventState.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layers/IAPZCTreeManager.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "ScopedGLHelpers.h"
#include "mozilla/layers/CompositorOGL.h"
#include "AndroidContentController.h"

#include "nsTArray.h"

#include "AndroidBridge.h"
#include "AndroidBridgeUtilities.h"
#include "android_npapi.h"
#include "FennecJNINatives.h"
#include "GeneratedJNINatives.h"
#include "KeyEvent.h"
#include "MotionEvent.h"

#include "imgIEncoder.h"

#include "nsString.h"
#include "GeckoProfiler.h" // For PROFILER_LABEL
#include "nsIXULRuntime.h"
#include "nsPrintfCString.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;
using namespace mozilla::java;
using namespace mozilla::widget;

NS_IMPL_ISUPPORTS_INHERITED0(nsWindow, nsBaseWidget)

#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorSession.h"
#include "mozilla/layers/LayerTransactionParent.h"
#include "mozilla/Services.h"
#include "nsThreadUtils.h"

// All the toplevel windows that have been created; these are in
// stacking order, so the window at gTopLevelWindows[0] is the topmost
// one.
static nsTArray<nsWindow*> gTopLevelWindows;

static bool sFailedToCreateGLContext = false;

// Multitouch swipe thresholds in inches
static const double SWIPE_MAX_PINCH_DELTA_INCHES = 0.4;
static const double SWIPE_MIN_DISTANCE_INCHES = 0.6;

// Sync with GeckoEditableView class
static const int IME_MONITOR_CURSOR_ONE_SHOT = 1;
static const int IME_MONITOR_CURSOR_START_MONITOR = 2;
static const int IME_MONITOR_CURSOR_END_MONITOR = 3;

static Modifiers GetModifiers(int32_t metaState);

template<typename Lambda, bool IsStatic, typename InstanceType, class Impl>
class nsWindow::WindowEvent : public nsAppShell::LambdaEvent<Lambda>
{
    typedef nsAppShell::Event Event;
    typedef nsAppShell::LambdaEvent<Lambda> Base;

    bool IsStaleCall()
    {
        if (IsStatic) {
            // Static calls are never stale.
            return false;
        }

        JNIEnv* const env = mozilla::jni::GetEnvForThread();

        const auto natives = reinterpret_cast<mozilla::WeakPtr<Impl>*>(
                jni::GetNativeHandle(env, mInstance.Get()));
        MOZ_CATCH_JNI_EXCEPTION(env);

        // The call is stale if the nsWindow has been destroyed on the
        // Gecko side, but the Java object is still attached to it through
        // a weak pointer. Stale calls should be discarded. Note that it's
        // an error if natives is nullptr here; we return false but the
        // native call will throw an error.
        return natives && !natives->get();
    }

    const InstanceType mInstance;
    const Event::Type mEventType;

public:
    WindowEvent(Lambda&& aLambda,
                InstanceType&& aInstance,
                Event::Type aEventType = Event::Type::kGeneralActivity)
        : Base(mozilla::Move(aLambda))
        , mInstance(mozilla::Move(aInstance))
        , mEventType(aEventType)
    {}

    WindowEvent(Lambda&& aLambda,
                Event::Type aEventType = Event::Type::kGeneralActivity)
        : Base(mozilla::Move(aLambda))
        , mInstance(Base::lambda.GetThisArg())
        , mEventType(aEventType)
    {}

    void Run() override
    {
        if (!IsStaleCall()) {
            return Base::Run();
        }
    }

    Event::Type ActivityType() const override
    {
        return mEventType;
    }
};

template<class Impl>
template<class Instance, typename... Args> void
nsWindow::NativePtr<Impl>::Attach(Instance aInstance, nsWindow* aWindow,
                                  Args&&... aArgs)
{
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!mPtr && !mImpl);

    auto impl = mozilla::MakeUnique<Impl>(
            this, aWindow, mozilla::Forward<Args>(aArgs)...);
    mImpl = impl.get();

    Impl::AttachNative(aInstance, mozilla::Move(impl));
}

template<class Impl> void
nsWindow::NativePtr<Impl>::Detach()
{
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mPtr && mImpl);

    mImpl->OnDetach();
    {
        Locked implLock(*this);
        mImpl = nullptr;
    }

    typename WindowPtr<Impl>::Locked lock(*mPtr);
    mPtr->mWindow = nullptr;
    mPtr->mPtr = nullptr;
    mPtr = nullptr;
}

template<class Impl>
class nsWindow::NativePtr<Impl>::Locked final : private MutexAutoLock
{
    Impl* const mImpl;

public:
    Locked(NativePtr<Impl>& aPtr)
        : MutexAutoLock(aPtr.mImplLock)
        , mImpl(aPtr.mImpl)
    {}

    operator Impl*() const { return mImpl; }
    Impl* operator->() const { return mImpl; }
};

template<class Impl>
class nsWindow::WindowPtr final
{
    friend NativePtr<Impl>;

    NativePtr<Impl>* mPtr;
    nsWindow* mWindow;
    Mutex mWindowLock;

public:
    class Locked final : private MutexAutoLock
    {
        nsWindow* const mWindow;

    public:
        Locked(WindowPtr<Impl>& aPtr)
            : MutexAutoLock(aPtr.mWindowLock)
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
        mPtr->mPtr = this;
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


class nsWindow::GeckoViewSupport final
    : public GeckoView::Window::Natives<GeckoViewSupport>
    , public GeckoEditable::Natives<GeckoViewSupport>
    , public SupportsWeakPtr<GeckoViewSupport>
{
    nsWindow& window;

public:
    typedef GeckoView::Window::Natives<GeckoViewSupport> Base;
    typedef GeckoEditable::Natives<GeckoViewSupport> EditableBase;

    MOZ_DECLARE_WEAKREFERENCE_TYPENAME(GeckoViewSupport);

    template<typename Functor>
    static void OnNativeCall(Functor&& aCall)
    {
        if (aCall.IsTarget(&Open) && NS_IsMainThread()) {
            // Gecko state probably just switched to PROFILE_READY, and the
            // event loop is not running yet. Skip the event loop here so we
            // can get a head start on opening our window.
            return aCall();
        }

        const nsAppShell::Event::Type eventType =
                aCall.IsTarget(&GeckoViewSupport::OnKeyEvent) ||
                aCall.IsTarget(&GeckoViewSupport::OnImeReplaceText) ||
                aCall.IsTarget(&GeckoViewSupport::OnImeUpdateComposition) ?
                nsAppShell::Event::Type::kUIActivity :
                nsAppShell::Event::Type::kGeneralActivity;

        nsAppShell::PostEvent(mozilla::MakeUnique<WindowEvent<Functor>>(
                mozilla::Move(aCall), eventType));
    }

    GeckoViewSupport(nsWindow* aWindow,
                     const GeckoView::Window::LocalRef& aInstance,
                     GeckoView::Param aView)
        : window(*aWindow)
        , mEditable(GeckoEditable::New(aView))
        , mIMERanges(new TextRangeArray())
        , mIMEMaskEventsCount(1) // Mask IME events since there's no focus yet
        , mIMEUpdatingContext(false)
        , mIMESelectionChanged(false)
        , mIMETextChangedDuringFlush(false)
        , mIMEMonitorCursor(false)
    {
        Base::AttachNative(aInstance, this);
        EditableBase::AttachNative(mEditable, this);
    }

    ~GeckoViewSupport();

    using Base::DisposeNative;
    using EditableBase::DisposeNative;

    /**
     * GeckoView methods
     */
private:
    nsCOMPtr<nsPIDOMWindowOuter> mDOMWindow;

public:
    // Create and attach a window.
    static void Open(const jni::Class::LocalRef& aCls,
                     GeckoView::Window::Param aWindow,
                     GeckoView::Param aView, jni::Object::Param aCompositor,
                     jni::String::Param aChromeURI,
                     int32_t aWidth, int32_t aHeight);

    // Close and destroy the nsWindow.
    void Close();

    // Reattach this nsWindow to a new GeckoView.
    void Reattach(const GeckoView::Window::LocalRef& inst,
                  GeckoView::Param aView, jni::Object::Param aCompositor);

    void LoadUri(jni::String::Param aUri, int32_t aFlags);

    /**
     * GeckoEditable methods
     */
private:
    /*
        Rules for managing IME between Gecko and Java:

        * Gecko controls the text content, and Java shadows the Gecko text
           through text updates
        * Gecko and Java maintain separate selections, and synchronize when
           needed through selection updates and set-selection events
        * Java controls the composition, and Gecko shadows the Java
           composition through update composition events
    */

    struct IMETextChange final {
        int32_t mStart, mOldEnd, mNewEnd;

        IMETextChange() :
            mStart(-1), mOldEnd(-1), mNewEnd(-1) {}

        IMETextChange(const IMENotification& aIMENotification)
            : mStart(aIMENotification.mTextChangeData.mStartOffset)
            , mOldEnd(aIMENotification.mTextChangeData.mRemovedEndOffset)
            , mNewEnd(aIMENotification.mTextChangeData.mAddedEndOffset)
        {
            MOZ_ASSERT(aIMENotification.mMessage == NOTIFY_IME_OF_TEXT_CHANGE,
                       "IMETextChange initialized with wrong notification");
            MOZ_ASSERT(aIMENotification.mTextChangeData.IsValid(),
                       "The text change notification isn't initialized");
            MOZ_ASSERT(aIMENotification.mTextChangeData.IsInInt32Range(),
                       "The text change notification is out of range");
        }

        bool IsEmpty() const { return mStart < 0; }
    };

    // GeckoEditable instance used by this nsWindow;
    java::GeckoEditable::GlobalRef mEditable;
    AutoTArray<mozilla::UniquePtr<mozilla::WidgetEvent>, 8> mIMEKeyEvents;
    AutoTArray<IMETextChange, 4> mIMETextChanges;
    InputContext mInputContext;
    RefPtr<mozilla::TextRangeArray> mIMERanges;
    int32_t mIMEMaskEventsCount; // Mask events when > 0
    bool mIMEUpdatingContext;
    bool mIMESelectionChanged;
    bool mIMETextChangedDuringFlush;
    bool mIMEMonitorCursor;

    void SendIMEDummyKeyEvents();
    void AddIMETextChange(const IMETextChange& aChange);

    enum FlushChangesFlag {
        FLUSH_FLAG_NONE,
        FLUSH_FLAG_RETRY
    };
    void PostFlushIMEChanges();
    void FlushIMEChanges(FlushChangesFlag aFlags = FLUSH_FLAG_NONE);
    void AsyncNotifyIME(int32_t aNotification);
    void UpdateCompositionRects();

public:
    bool NotifyIME(const IMENotification& aIMENotification);
    void SetInputContext(const InputContext& aContext,
                         const InputContextAction& aAction);
    InputContext GetInputContext();

    // RAII helper class that automatically sends an event reply through
    // OnImeSynchronize, as required by events like OnImeReplaceText.
    class AutoIMESynchronize {
        GeckoViewSupport* const mGVS;
    public:
        AutoIMESynchronize(GeckoViewSupport* gvs) : mGVS(gvs) {}
        ~AutoIMESynchronize() { mGVS->OnImeSynchronize(); }
    };

    // Handle an Android KeyEvent.
    void OnKeyEvent(int32_t aAction, int32_t aKeyCode, int32_t aScanCode,
                    int32_t aMetaState, int64_t aTime, int32_t aUnicodeChar,
                    int32_t aBaseUnicodeChar, int32_t aDomPrintableKeyValue,
                    int32_t aRepeatCount, int32_t aFlags,
                    bool aIsSynthesizedImeKey, jni::Object::Param originalEvent);

    // Synchronize Gecko thread with the InputConnection thread.
    void OnImeSynchronize();

    // Acknowledge focus change and send new text and selection.
    void OnImeAcknowledgeFocus();

    // Replace a range of text with new text.
    void OnImeReplaceText(int32_t aStart, int32_t aEnd,
                          jni::String::Param aText);

    // Add styling for a range within the active composition.
    void OnImeAddCompositionRange(int32_t aStart, int32_t aEnd,
            int32_t aRangeType, int32_t aRangeStyle, int32_t aRangeLineStyle,
            bool aRangeBoldLine, int32_t aRangeForeColor,
            int32_t aRangeBackColor, int32_t aRangeLineColor);

    // Update styling for the active composition using previous-added ranges.
    void OnImeUpdateComposition(int32_t aStart, int32_t aEnd);

    // Set cursor mode whether IME requests
    void OnImeRequestCursorUpdates(int aRequestMode);
};

/**
 * NativePanZoomController handles its native calls on the UI thread, so make
 * it separate from GeckoViewSupport.
 */
class nsWindow::NPZCSupport final
    : public NativePanZoomController::Natives<NPZCSupport>
{
    using LockedWindowPtr = WindowPtr<NPZCSupport>::Locked;

    WindowPtr<NPZCSupport> mWindow;
    NativePanZoomController::GlobalRef mNPZC;
    int mPreviousButtons;

public:
    typedef NativePanZoomController::Natives<NPZCSupport> Base;

    NPZCSupport(NativePtr<NPZCSupport>* aPtr, nsWindow* aWindow,
                const NativePanZoomController::LocalRef& aNPZC)
        : mWindow(aPtr, aWindow)
        , mNPZC(aNPZC)
        , mPreviousButtons(0)
    {}

    ~NPZCSupport()
    {}

    using Base::AttachNative;
    using Base::DisposeNative;

    void OnDetach()
    {
        // There are several considerations when shutting down NPZC. 1) The
        // Gecko thread may destroy NPZC at any time when nsWindow closes. 2)
        // There may be pending events on the Gecko thread when NPZC is
        // destroyed. 3) mWindow may not be available when the pending event
        // runs. 4) The UI thread may destroy NPZC at any time when GeckoView
        // is destroyed. 5) The UI thread may destroy NPZC at the same time as
        // Gecko thread trying to destroy NPZC. 6) There may be pending calls
        // on the UI thread when NPZC is destroyed. 7) mWindow may have been
        // cleared on the Gecko thread when the pending call happens on the UI
        // thread.
        //
        // 1) happens through OnDetach, which first notifies the UI
        // thread through Destroy; Destroy then calls DisposeNative, which
        // finally disposes the native instance back on the Gecko thread. Using
        // Destroy to indirectly call DisposeNative here also solves 5), by
        // making everything go through the UI thread, avoiding contention.
        //
        // 2) and 3) are solved by clearing mWindow, which signals to the
        // pending event that we had shut down. In that case the event bails
        // and does not touch mWindow.
        //
        // 4) happens through DisposeNative directly. OnDetach is not
        // called.
        //
        // 6) is solved by keeping a destroyed flag in the Java NPZC instance,
        // and only make a pending call if the destroyed flag is not set.
        //
        // 7) is solved by taking a lock whenever mWindow is modified on the
        // Gecko thread or accessed on the UI thread. That way, we don't
        // release mWindow until the UI thread is done using it, thus avoiding
        // the race condition.

        typedef NativePanZoomController::GlobalRef NPZCRef;
        auto callDestroy = [] (const NPZCRef& npzc) {
            npzc->Destroy();
        };

        NativePanZoomController::GlobalRef npzc = mNPZC;
        AndroidBridge::Bridge()->PostTaskToUiThread(NewRunnableFunction(
                static_cast<void(*)(const NPZCRef&)>(callDestroy),
                mozilla::Move(npzc)), 0);
    }

public:
    void AdjustScrollForSurfaceShift(float aX, float aY)
    {
        MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

        RefPtr<IAPZCTreeManager> controller;

        if (LockedWindowPtr window{mWindow}) {
            controller = window->mAPZC;
        }

        if (controller) {
            controller->AdjustScrollForSurfaceShift(
                ScreenPoint(aX, aY));
        }
    }

    void SetIsLongpressEnabled(bool aIsLongpressEnabled)
    {
        MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

        RefPtr<IAPZCTreeManager> controller;

        if (LockedWindowPtr window{mWindow}) {
            controller = window->mAPZC;
        }

        if (controller) {
            controller->SetLongTapEnabled(aIsLongpressEnabled);
        }
    }

    bool HandleScrollEvent(int64_t aTime, int32_t aMetaState,
                           float aX, float aY,
                           float aHScroll, float aVScroll)
    {
        MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

        RefPtr<IAPZCTreeManager> controller;

        if (LockedWindowPtr window{mWindow}) {
            controller = window->mAPZC;
        }

        if (!controller) {
            return false;
        }

        ScreenPoint origin = ScreenPoint(aX, aY);

        ScrollWheelInput input(aTime, TimeStamp::Now(), GetModifiers(aMetaState),
                               ScrollWheelInput::SCROLLMODE_SMOOTH,
                               ScrollWheelInput::SCROLLDELTA_PIXEL,
                               origin,
                               aHScroll, aVScroll,
                               false);

        ScrollableLayerGuid guid;
        uint64_t blockId;
        nsEventStatus status = controller->ReceiveInputEvent(input, &guid, &blockId);

        if (status == nsEventStatus_eConsumeNoDefault) {
            return true;
        }

        NativePanZoomController::GlobalRef npzc = mNPZC;
        nsAppShell::PostEvent([npzc, input, guid, blockId, status] {
            MOZ_ASSERT(NS_IsMainThread());

            JNIEnv* const env = jni::GetGeckoThreadEnv();
            NPZCSupport* npzcSupport = GetNative(
                    NativePanZoomController::LocalRef(env, npzc));

            if (!npzcSupport || !npzcSupport->mWindow) {
                // We already shut down.
                env->ExceptionClear();
                return;
            }

            nsWindow* const window = npzcSupport->mWindow;
            window->UserActivity();
            WidgetWheelEvent wheelEvent = input.ToWidgetWheelEvent(window);
            window->ProcessUntransformedAPZEvent(&wheelEvent, guid,
                                                 blockId, status);
        });

        return true;
    }

private:
    static MouseInput::ButtonType GetButtonType(int button)
    {
        MouseInput::ButtonType result = MouseInput::NONE;

        switch (button) {
            case java::sdk::MotionEvent::BUTTON_PRIMARY:
                result = MouseInput::LEFT_BUTTON;
                break;
            case java::sdk::MotionEvent::BUTTON_SECONDARY:
                result = MouseInput::RIGHT_BUTTON;
                break;
            case java::sdk::MotionEvent::BUTTON_TERTIARY:
                result = MouseInput::MIDDLE_BUTTON;
                break;
            default:
                break;
        }

        return result;
    }

    static int16_t ConvertButtons(int buttons) {
        int16_t result = 0;

        if (buttons & java::sdk::MotionEvent::BUTTON_PRIMARY) {
            result |= WidgetMouseEventBase::eLeftButtonFlag;
        }
        if (buttons & java::sdk::MotionEvent::BUTTON_SECONDARY) {
            result |= WidgetMouseEventBase::eRightButtonFlag;
        }
        if (buttons & java::sdk::MotionEvent::BUTTON_TERTIARY) {
            result |= WidgetMouseEventBase::eMiddleButtonFlag;
        }
        if (buttons & java::sdk::MotionEvent::BUTTON_BACK) {
            result |= WidgetMouseEventBase::e4thButtonFlag;
        }
        if (buttons & java::sdk::MotionEvent::BUTTON_FORWARD) {
            result |= WidgetMouseEventBase::e5thButtonFlag;
        }

        return result;
    }

public:
    bool HandleMouseEvent(int32_t aAction, int64_t aTime, int32_t aMetaState,
                          float aX, float aY, int buttons)
    {
        MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

        RefPtr<IAPZCTreeManager> controller;

        if (LockedWindowPtr window{mWindow}) {
            controller = window->mAPZC;
        }

        if (!controller) {
            return false;
        }

        MouseInput::MouseType mouseType = MouseInput::MOUSE_NONE;
        MouseInput::ButtonType buttonType = MouseInput::NONE;
        switch (aAction) {
            case AndroidMotionEvent::ACTION_DOWN:
                mouseType = MouseInput::MOUSE_DOWN;
                buttonType = GetButtonType(buttons ^ mPreviousButtons);
                mPreviousButtons = buttons;
                break;
            case AndroidMotionEvent::ACTION_UP:
                mouseType = MouseInput::MOUSE_UP;
                buttonType = GetButtonType(buttons ^ mPreviousButtons);
                mPreviousButtons = buttons;
                break;
            case AndroidMotionEvent::ACTION_MOVE:
                mouseType = MouseInput::MOUSE_MOVE;
                break;
            case AndroidMotionEvent::ACTION_HOVER_MOVE:
                mouseType = MouseInput::MOUSE_MOVE;
                break;
            case AndroidMotionEvent::ACTION_HOVER_ENTER:
                mouseType = MouseInput::MOUSE_WIDGET_ENTER;
                break;
            case AndroidMotionEvent::ACTION_HOVER_EXIT:
                mouseType = MouseInput::MOUSE_WIDGET_EXIT;
                break;
            default:
                break;
        }

        if (mouseType == MouseInput::MOUSE_NONE) {
            return false;
        }

        ScreenPoint origin = ScreenPoint(aX, aY);

        MouseInput input(mouseType, buttonType, nsIDOMMouseEvent::MOZ_SOURCE_MOUSE, ConvertButtons(buttons), origin, aTime, TimeStamp(), GetModifiers(aMetaState));

        ScrollableLayerGuid guid;
        uint64_t blockId;
        nsEventStatus status = controller->ReceiveInputEvent(input, &guid, &blockId);

        if (status == nsEventStatus_eConsumeNoDefault) {
            return true;
        }

        NativePanZoomController::GlobalRef npzc = mNPZC;
        nsAppShell::PostEvent([npzc, input, guid, blockId, status] {
            MOZ_ASSERT(NS_IsMainThread());

            JNIEnv* const env = jni::GetGeckoThreadEnv();
            NPZCSupport* npzcSupport = GetNative(
                    NativePanZoomController::LocalRef(env, npzc));

            if (!npzcSupport || !npzcSupport->mWindow) {
                // We already shut down.
                env->ExceptionClear();
                return;
            }

            nsWindow* const window = npzcSupport->mWindow;
            window->UserActivity();
            WidgetMouseEvent mouseEvent = input.ToWidgetMouseEvent(window);
            window->ProcessUntransformedAPZEvent(&mouseEvent, guid,
                                                 blockId, status);
        });

        return true;
    }

    bool HandleMotionEvent(const NativePanZoomController::LocalRef& aInstance,
                           int32_t aAction, int32_t aActionIndex,
                           int64_t aTime, int32_t aMetaState,
                           jni::IntArray::Param aPointerId,
                           jni::FloatArray::Param aX,
                           jni::FloatArray::Param aY,
                           jni::FloatArray::Param aOrientation,
                           jni::FloatArray::Param aPressure,
                           jni::FloatArray::Param aToolMajor,
                           jni::FloatArray::Param aToolMinor)
    {
        MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

        RefPtr<IAPZCTreeManager> controller;

        if (LockedWindowPtr window{mWindow}) {
            controller = window->mAPZC;
        }

        if (!controller) {
            return false;
        }

        nsTArray<int32_t> pointerId(aPointerId->GetElements());
        MultiTouchInput::MultiTouchType type;
        size_t startIndex = 0;
        size_t endIndex = pointerId.Length();

        switch (aAction) {
            case sdk::MotionEvent::ACTION_DOWN:
            case sdk::MotionEvent::ACTION_POINTER_DOWN:
                type = MultiTouchInput::MULTITOUCH_START;
                break;
            case sdk::MotionEvent::ACTION_MOVE:
                type = MultiTouchInput::MULTITOUCH_MOVE;
                break;
            case sdk::MotionEvent::ACTION_UP:
            case sdk::MotionEvent::ACTION_POINTER_UP:
                // for pointer-up events we only want the data from
                // the one pointer that went up
                type = MultiTouchInput::MULTITOUCH_END;
                startIndex = aActionIndex;
                endIndex = aActionIndex + 1;
                break;
            case sdk::MotionEvent::ACTION_OUTSIDE:
            case sdk::MotionEvent::ACTION_CANCEL:
                type = MultiTouchInput::MULTITOUCH_CANCEL;
                break;
            default:
                return false;
        }

        MultiTouchInput input(type, aTime, TimeStamp(), 0);
        input.modifiers = GetModifiers(aMetaState);
        input.mTouches.SetCapacity(endIndex - startIndex);

        nsTArray<float> x(aX->GetElements());
        nsTArray<float> y(aY->GetElements());
        nsTArray<float> orientation(aOrientation->GetElements());
        nsTArray<float> pressure(aPressure->GetElements());
        nsTArray<float> toolMajor(aToolMajor->GetElements());
        nsTArray<float> toolMinor(aToolMinor->GetElements());

        MOZ_ASSERT(pointerId.Length() == x.Length());
        MOZ_ASSERT(pointerId.Length() == y.Length());
        MOZ_ASSERT(pointerId.Length() == orientation.Length());
        MOZ_ASSERT(pointerId.Length() == pressure.Length());
        MOZ_ASSERT(pointerId.Length() == toolMajor.Length());
        MOZ_ASSERT(pointerId.Length() == toolMinor.Length());

        for (size_t i = startIndex; i < endIndex; i++) {

            float orien = orientation[i] * 180.0f / M_PI;
            // w3c touchevents spec does not allow orientations == 90
            // this shifts it to -90, which will be shifted to zero below
            if (orien >= 90.0) {
                orien -= 180.0f;
            }

            nsIntPoint point = nsIntPoint(int32_t(floorf(x[i])),
                                          int32_t(floorf(y[i])));

            // w3c touchevent radii are given with an orientation between 0 and
            // 90. The radii are found by removing the orientation and
            // measuring the x and y radii of the resulting ellipse. For
            // Android orientations >= 0 and < 90, use the y radius as the
            // major radius, and x as the minor radius. However, for an
            // orientation < 0, we have to shift the orientation by adding 90,
            // and reverse which radius is major and minor.
            gfx::Size radius;
            if (orien < 0.0f) {
                orien += 90.0f;
                radius = gfx::Size(int32_t(toolMajor[i] / 2.0f),
                                   int32_t(toolMinor[i] / 2.0f));
            } else {
                radius = gfx::Size(int32_t(toolMinor[i] / 2.0f),
                                   int32_t(toolMajor[i] / 2.0f));
            }

            input.mTouches.AppendElement(SingleTouchData(
                    pointerId[i], ScreenIntPoint::FromUnknownPoint(point),
                    ScreenSize::FromUnknownSize(radius), orien, pressure[i]));
        }

        ScrollableLayerGuid guid;
        uint64_t blockId;
        nsEventStatus status =
            controller->ReceiveInputEvent(input, &guid, &blockId);

        if (status == nsEventStatus_eConsumeNoDefault) {
            return true;
        }

        // Dispatch APZ input event on Gecko thread.
        NativePanZoomController::GlobalRef npzc = mNPZC;
        nsAppShell::PostEvent([npzc, input, guid, blockId, status] {
            MOZ_ASSERT(NS_IsMainThread());

            JNIEnv* const env = jni::GetGeckoThreadEnv();
            NPZCSupport* npzcSupport = GetNative(
                    NativePanZoomController::LocalRef(env, npzc));

            if (!npzcSupport || !npzcSupport->mWindow) {
                // We already shut down.
                env->ExceptionClear();
                return;
            }

            nsWindow* const window = npzcSupport->mWindow;
            window->UserActivity();
            WidgetTouchEvent touchEvent = input.ToWidgetTouchEvent(window);
            window->ProcessUntransformedAPZEvent(&touchEvent, guid,
                                                 blockId, status);
            window->DispatchHitTest(touchEvent);
        });
        return true;
    }

    void HandleMotionEventVelocity(int64_t aTime, float aSpeedY)
    {
        MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

        RefPtr<IAPZCTreeManager> controller;

        if (LockedWindowPtr window{mWindow}) {
            controller = window->mAPZC;
        }

        if (controller) {
            controller->ProcessTouchVelocity((uint32_t)aTime, aSpeedY);
        }
    }

    void UpdateOverscrollVelocity(const float x, const float y)
    {
        mNPZC->UpdateOverscrollVelocity(x, y);
    }

    void UpdateOverscrollOffset(const float x, const float y)
    {
        mNPZC->UpdateOverscrollOffset(x, y);
    }

    void SetScrollingRootContent(const bool isRootContent)
    {
        mNPZC->SetScrollingRootContent(isRootContent);
    }

    void SetSelectionDragState(const bool aState)
    {
        mNPZC->OnSelectionDragState(aState);
    }
};

template<> const char
nsWindow::NativePtr<nsWindow::NPZCSupport>::sName[] = "NPZCSupport";

/**
 * Compositor has some unique requirements for its native calls, so make it
 * separate from GeckoViewSupport.
 */
class nsWindow::LayerViewSupport final
    : public LayerView::Compositor::Natives<LayerViewSupport>
{
    using LockedWindowPtr = WindowPtr<LayerViewSupport>::Locked;

    WindowPtr<LayerViewSupport> mWindow;
    LayerView::Compositor::GlobalRef mCompositor;
    GeckoLayerClient::GlobalRef mLayerClient;
    Atomic<bool, ReleaseAcquire> mCompositorPaused;
    jni::Object::GlobalRef mSurface;

    // In order to use Event::HasSameTypeAs in PostTo(), we cannot make
    // LayerViewEvent a template because each template instantiation is
    // a different type. So implement LayerViewEvent as a ProxyEvent.
    class LayerViewEvent final : public nsAppShell::ProxyEvent
    {
        using Event = nsAppShell::Event;

    public:
        static UniquePtr<Event> MakeEvent(UniquePtr<Event>&& event)
        {
            return MakeUnique<LayerViewEvent>(mozilla::Move(event));
        }

        LayerViewEvent(UniquePtr<Event>&& event)
            : nsAppShell::ProxyEvent(mozilla::Move(event))
        {}

        void PostTo(LinkedList<Event>& queue) override
        {
            // Give priority to compositor events, but keep in order with
            // existing compositor events.
            nsAppShell::Event* event = queue.getFirst();
            while (event && event->HasSameTypeAs(this)) {
                event = event->getNext();
            }
            if (event) {
                event->setPrevious(this);
            } else {
                queue.insertBack(this);
            }
        }
    };

public:
    typedef LayerView::Compositor::Natives<LayerViewSupport> Base;

    template<class Functor>
    static void OnNativeCall(Functor&& aCall)
    {
        if (aCall.IsTarget(&LayerViewSupport::CreateCompositor)) {
            // This call is blocking.
            nsAppShell::SyncRunEvent(nsAppShell::LambdaEvent<Functor>(
                    mozilla::Move(aCall)), &LayerViewEvent::MakeEvent);
            return;
        }
    }

    static LayerViewSupport*
    FromNative(const LayerView::Compositor::LocalRef& instance)
    {
        return GetNative(instance);
    }

    LayerViewSupport(NativePtr<LayerViewSupport>* aPtr, nsWindow* aWindow,
                     const LayerView::Compositor::LocalRef& aInstance)
        : mWindow(aPtr, aWindow)
        , mCompositor(aInstance)
        , mCompositorPaused(true)
    {}

    ~LayerViewSupport()
    {}

    using Base::AttachNative;
    using Base::DisposeNative;

    void OnDetach()
    {
        mCompositor->Destroy();
    }

    const GeckoLayerClient::Ref& GetLayerClient() const
    {
        return mLayerClient;
    }

    bool CompositorPaused() const
    {
        return mCompositorPaused;
    }

    jni::Object::Param GetSurface()
    {
        return mSurface;
    }

private:
    void OnResumedCompositor()
    {
        MOZ_ASSERT(NS_IsMainThread());

        // When we receive this, the compositor has already been told to
        // resume. (It turns out that waiting till we reach here to tell
        // the compositor to resume takes too long, resulting in a black
        // flash.) This means it's now safe for layer updates to occur.
        // Since we might have prevented one or more draw events from
        // occurring while the compositor was paused, we need to schedule
        // a draw event now.
        if (!mCompositorPaused) {
            mWindow->RedrawAll();
        }
    }

    /**
     * Compositor methods
     */
public:
    void AttachToJava(jni::Object::Param aClient, jni::Object::Param aNPZC)
    {
        MOZ_ASSERT(NS_IsMainThread());
        if (!mWindow) {
            return; // Already shut down.
        }

        const auto& layerClient = GeckoLayerClient::Ref::From(aClient);

        AndroidBridge::Bridge()->SetLayerClient(layerClient);

        // If resetting is true, Android destroyed our GeckoApp activity and we
        // had to recreate it, but all the Gecko-side things were not
        // destroyed.  We therefore need to link up the new java objects to
        // Gecko, and that's what we do here.
        const bool resetting = !!mLayerClient;
        mLayerClient = layerClient;

        MOZ_ASSERT(aNPZC);
        auto npzc = NativePanZoomController::LocalRef(
                jni::GetGeckoThreadEnv(),
                NativePanZoomController::Ref::From(aNPZC));
        mWindow->mNPZCSupport.Attach(npzc, mWindow, npzc);

        layerClient->OnGeckoReady();

        if (resetting) {
            // Since we are re-linking the new java objects to Gecko, we need
            // to get the viewport from the compositor (since the Java copy was
            // thrown away) and we do that by setting the first-paint flag.
            if (RefPtr<CompositorBridgeParent> bridge = mWindow->GetCompositorBridgeParent()) {
                bridge->ForceIsFirstPaint();
            }
        }
    }

    void OnSizeChanged(int32_t aWindowWidth, int32_t aWindowHeight,
                       int32_t aScreenWidth, int32_t aScreenHeight)
    {
        MOZ_ASSERT(NS_IsMainThread());
        if (!mWindow) {
            return; // Already shut down.
        }

        if (aWindowWidth != mWindow->mBounds.width ||
            aWindowHeight != mWindow->mBounds.height) {

            mWindow->Resize(aWindowWidth, aWindowHeight, /* repaint */ false);
        }
    }

    void CreateCompositor(int32_t aWidth, int32_t aHeight,
                          jni::Object::Param aSurface)
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(mWindow);

        mSurface = aSurface;
        mWindow->CreateLayerManager(aWidth, aHeight);

        mCompositorPaused = false;
        OnResumedCompositor();
    }

    void SyncPauseCompositor()
    {
        MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

        RefPtr<CompositorBridgeParent> bridge;

        if (LockedWindowPtr window{mWindow}) {
            bridge = window->GetCompositorBridgeParent();
        }

        if (bridge) {
            mCompositorPaused = true;
            bridge->SchedulePauseOnCompositorThread();
        }
    }

    void SyncResumeCompositor()
    {
        MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

        RefPtr<CompositorBridgeParent> bridge;

        if (LockedWindowPtr window{mWindow}) {
            bridge = window->GetCompositorBridgeParent();
        }

        if (bridge && bridge->ScheduleResumeOnCompositorThread()) {
            mCompositorPaused = false;
        }
    }

    void SyncResumeResizeCompositor(const LayerView::Compositor::LocalRef& aObj,
                                    int32_t aWidth, int32_t aHeight,
                                    jni::Object::Param aSurface)
    {
        MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

        RefPtr<CompositorBridgeParent> bridge;

        if (LockedWindowPtr window{mWindow}) {
            bridge = window->GetCompositorBridgeParent();
        }

        mSurface = aSurface;

        if (!bridge || !bridge->ScheduleResumeOnCompositorThread(aWidth,
                                                                 aHeight)) {
            return;
        }

        mCompositorPaused = false;

        class OnResumedEvent : public nsAppShell::Event
        {
            LayerView::Compositor::GlobalRef mCompositor;

        public:
            OnResumedEvent(LayerView::Compositor::GlobalRef&& aCompositor)
                : mCompositor(mozilla::Move(aCompositor))
            {}

            void Run() override
            {
                MOZ_ASSERT(NS_IsMainThread());

                JNIEnv* const env = jni::GetGeckoThreadEnv();
                LayerViewSupport* const lvs = GetNative(
                        LayerView::Compositor::LocalRef(env, mCompositor));
                MOZ_CATCH_JNI_EXCEPTION(env);

                lvs->OnResumedCompositor();
            }
        };

        nsAppShell::PostEvent(MakeUnique<LayerViewEvent>(
                MakeUnique<OnResumedEvent>(aObj)));
    }

    void SyncInvalidateAndScheduleComposite()
    {
        RefPtr<CompositorBridgeParent> bridge;

        if (LockedWindowPtr window{mWindow}) {
            bridge = window->GetCompositorBridgeParent();
        }

        if (bridge) {
            bridge->InvalidateOnCompositorThread();
            bridge->ScheduleRenderOnCompositorThread();
        }
    }
};

template<> const char
nsWindow::NativePtr<nsWindow::LayerViewSupport>::sName[] = "LayerViewSupport";

/* PresentationMediaPlayerManager native calls access inner nsWindow functionality so PMPMSupport is a child class of nsWindow */
class nsWindow::PMPMSupport final
    : public PresentationMediaPlayerManager::Natives<PMPMSupport>
{
    PMPMSupport() = delete;

    static LayerViewSupport* GetLayerViewSupport(jni::Object::Param aView)
    {
        const auto& layerView = LayerView::Ref::From(aView);

        LayerView::Compositor::LocalRef compositor = layerView->GetCompositor();
        if (!layerView->CompositorCreated() || !compositor) {
            return nullptr;
        }

        LayerViewSupport* const lvs = LayerViewSupport::FromNative(compositor);
        if (!lvs) {
            // There is a pending exception whenever FromNative returns nullptr.
            compositor.Env()->ExceptionClear();
        }
        return lvs;
    }

public:
    static ANativeWindow* sWindow;
    static EGLSurface sSurface;

    static void InvalidateAndScheduleComposite(jni::Object::Param aView)
    {
        LayerViewSupport* const lvs = GetLayerViewSupport(aView);
        if (lvs) {
            lvs->SyncInvalidateAndScheduleComposite();
        }
    }

    static void AddPresentationSurface(const jni::Class::LocalRef& aCls,
                                       jni::Object::Param aView,
                                       jni::Object::Param aSurface)
    {
        RemovePresentationSurface();

        LayerViewSupport* const lvs = GetLayerViewSupport(aView);
        if (!lvs) {
            return;
        }

        ANativeWindow* const window = ANativeWindow_fromSurface(
                aCls.Env(), aSurface.Get());
        if (!window) {
            return;
        }

        sWindow = window;

        const bool wasAlreadyPaused = lvs->CompositorPaused();
        if (!wasAlreadyPaused) {
            lvs->SyncPauseCompositor();
        }

        if (sSurface) {
            // Destroy the EGL surface! The compositor is paused so it should
            // be okay to destroy the surface here.
            mozilla::gl::GLContextProvider::DestroyEGLSurface(sSurface);
            sSurface = nullptr;
        }

        if (!wasAlreadyPaused) {
            lvs->SyncResumeCompositor();
        }

        lvs->SyncInvalidateAndScheduleComposite();
    }

    static void RemovePresentationSurface()
    {
        if (sWindow) {
            ANativeWindow_release(sWindow);
            sWindow = nullptr;
        }
    }
};

ANativeWindow* nsWindow::PMPMSupport::sWindow;
EGLSurface nsWindow::PMPMSupport::sSurface;


nsWindow::GeckoViewSupport::~GeckoViewSupport()
{
    // Disassociate our GeckoEditable instance with our native object.
    // OnDestroy will call disposeNative after any pending native calls have
    // been made.
    MOZ_ASSERT(mEditable);
    mEditable->OnViewChange(nullptr);

    if (window.mNPZCSupport) {
        window.mNPZCSupport.Detach();
    }

    if (window.mLayerViewSupport) {
        window.mLayerViewSupport.Detach();
    }
}

/* static */ void
nsWindow::GeckoViewSupport::Open(const jni::Class::LocalRef& aCls,
                                 GeckoView::Window::Param aWindow,
                                 GeckoView::Param aView,
                                 jni::Object::Param aCompositor,
                                 jni::String::Param aChromeURI,
                                 int32_t aWidth, int32_t aHeight)
{
    MOZ_ASSERT(NS_IsMainThread());

    PROFILER_LABEL("nsWindow", "GeckoViewSupport::Open",
                   js::ProfileEntry::Category::OTHER);

    nsCOMPtr<nsIWindowWatcher> ww = do_GetService(NS_WINDOWWATCHER_CONTRACTID);
    MOZ_RELEASE_ASSERT(ww);

    nsAdoptingCString url;
    if (aChromeURI) {
        url = aChromeURI->ToCString();
    } else {
        url = Preferences::GetCString("toolkit.defaultChromeURI");
        if (!url) {
            url = NS_LITERAL_CSTRING("chrome://browser/content/browser.xul");
        }
    }

    nsCOMPtr<nsISupportsArray> args
            = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID);
    nsCOMPtr<nsISupportsPRInt32> widthArg
            = do_CreateInstance(NS_SUPPORTS_PRINT32_CONTRACTID);
    nsCOMPtr<nsISupportsPRInt32> heightArg
            = do_CreateInstance(NS_SUPPORTS_PRINT32_CONTRACTID);

    // Arguments are optional so it's okay if this fails.
    if (args && widthArg && heightArg &&
            NS_SUCCEEDED(widthArg->SetData(aWidth)) &&
            NS_SUCCEEDED(heightArg->SetData(aHeight)))
    {
        args->AppendElement(widthArg);
        args->AppendElement(heightArg);
    }

    nsCOMPtr<mozIDOMWindowProxy> domWindow;
    ww->OpenWindow(nullptr, url, nullptr, "chrome,dialog=0,resizable",
                   args, getter_AddRefs(domWindow));
    MOZ_RELEASE_ASSERT(domWindow);

    nsCOMPtr<nsPIDOMWindowOuter> pdomWindow =
            nsPIDOMWindowOuter::From(domWindow);
    nsCOMPtr<nsIWidget> widget = WidgetUtils::DOMWindowToWidget(pdomWindow);
    MOZ_ASSERT(widget);

    const auto window = static_cast<nsWindow*>(widget.get());

    // Attach a new GeckoView support object to the new window.
    window->mGeckoViewSupport  = mozilla::MakeUnique<GeckoViewSupport>(
            window, GeckoView::Window::LocalRef(aCls.Env(), aWindow), aView);

    window->mGeckoViewSupport->mDOMWindow = pdomWindow;

    // Attach the Compositor to the new window.
    auto compositor = LayerView::Compositor::LocalRef(
            aCls.Env(), LayerView::Compositor::Ref::From(aCompositor));
    window->mLayerViewSupport.Attach(compositor, window, compositor);

    if (window->mWidgetListener) {
        nsCOMPtr<nsIXULWindow> xulWindow(
                window->mWidgetListener->GetXULWindow());
        if (xulWindow) {
            // Our window is not intrinsically sized, so tell nsXULWindow to
            // not set a size for us.
            xulWindow->SetIntrinsicallySized(false);
        }
    }
}

void
nsWindow::GeckoViewSupport::Close()
{
    if (!mDOMWindow) {
        return;
    }

    mDOMWindow->ForceClose();
    mDOMWindow = nullptr;
}

void
nsWindow::GeckoViewSupport::Reattach(const GeckoView::Window::LocalRef& inst,
                                     GeckoView::Param aView,
                                     jni::Object::Param aCompositor)
{
    // Associate our previous GeckoEditable with the new GeckoView.
    mEditable->OnViewChange(aView);

    // mNPZCSupport might have already been detached through the Java side calling
    // NativePanZoomController.destroy().
    if (window.mNPZCSupport) {
        window.mNPZCSupport.Detach();
    }

    MOZ_ASSERT(window.mLayerViewSupport);
    window.mLayerViewSupport.Detach();

    auto compositor = LayerView::Compositor::LocalRef(
            inst.Env(), LayerView::Compositor::Ref::From(aCompositor));
    window.mLayerViewSupport.Attach(compositor, &window, compositor);
    compositor->Reattach();
}

void
nsWindow::GeckoViewSupport::LoadUri(jni::String::Param aUri, int32_t aFlags)
{
    if (!mDOMWindow) {
        return;
    }

    nsCOMPtr<nsIURI> uri = nsAppShell::ResolveURI(aUri->ToCString());
    if (NS_WARN_IF(!uri)) {
        return;
    }

    nsCOMPtr<nsIDOMChromeWindow> chromeWin = do_QueryInterface(mDOMWindow);
    nsCOMPtr<nsIBrowserDOMWindow> browserWin;

    if (NS_WARN_IF(!chromeWin) || NS_WARN_IF(NS_FAILED(
            chromeWin->GetBrowserDOMWindow(getter_AddRefs(browserWin))))) {
        return;
    }

    const int flags = aFlags == GeckoView::LOAD_NEW_TAB ?
                        nsIBrowserDOMWindow::OPEN_NEWTAB :
                      aFlags == GeckoView::LOAD_SWITCH_TAB ?
                        nsIBrowserDOMWindow::OPEN_SWITCHTAB :
                        nsIBrowserDOMWindow::OPEN_CURRENTWINDOW;
    nsCOMPtr<mozIDOMWindowProxy> newWin;

    if (NS_FAILED(browserWin->OpenURI(
            uri, nullptr, flags, nsIBrowserDOMWindow::OPEN_EXTERNAL,
            getter_AddRefs(newWin)))) {
        NS_WARNING("Failed to open URI");
    }
}

void
nsWindow::InitNatives()
{
    nsWindow::GeckoViewSupport::Base::Init();
    nsWindow::GeckoViewSupport::EditableBase::Init();
    nsWindow::LayerViewSupport::Init();
    nsWindow::NPZCSupport::Init();
    if (jni::IsFennec()) {
        nsWindow::PMPMSupport::Init();
    }
}

nsWindow*
nsWindow::TopWindow()
{
    if (!gTopLevelWindows.IsEmpty())
        return gTopLevelWindows[0];
    return nullptr;
}

void
nsWindow::LogWindow(nsWindow *win, int index, int indent)
{
#if defined(DEBUG) || defined(FORCE_ALOG)
    char spaces[] = "                    ";
    spaces[indent < 20 ? indent : 20] = 0;
    ALOG("%s [% 2d] 0x%08x [parent 0x%08x] [% 3d,% 3dx% 3d,% 3d] vis %d type %d",
         spaces, index, (intptr_t)win, (intptr_t)win->mParent,
         win->mBounds.x, win->mBounds.y,
         win->mBounds.width, win->mBounds.height,
         win->mIsVisible, win->mWindowType);
#endif
}

void
nsWindow::DumpWindows()
{
    DumpWindows(gTopLevelWindows);
}

void
nsWindow::DumpWindows(const nsTArray<nsWindow*>& wins, int indent)
{
    for (uint32_t i = 0; i < wins.Length(); ++i) {
        nsWindow *w = wins[i];
        LogWindow(w, i, indent);
        DumpWindows(w->mChildren, indent+1);
    }
}

nsWindow::nsWindow() :
    mIsVisible(false),
    mParent(nullptr),
    mAwaitingFullScreen(false),
    mIsFullScreen(false)
{
}

nsWindow::~nsWindow()
{
    gTopLevelWindows.RemoveElement(this);
    ALOG("nsWindow %p destructor", (void*)this);
}

bool
nsWindow::IsTopLevel()
{
    return mWindowType == eWindowType_toplevel ||
        mWindowType == eWindowType_dialog ||
        mWindowType == eWindowType_invisible;
}

nsresult
nsWindow::Create(nsIWidget* aParent,
                 nsNativeWidget aNativeParent,
                 const LayoutDeviceIntRect& aRect,
                 nsWidgetInitData* aInitData)
{
    ALOG("nsWindow[%p]::Create %p [%d %d %d %d]", (void*)this, (void*)aParent,
         aRect.x, aRect.y, aRect.width, aRect.height);

    nsWindow *parent = (nsWindow*) aParent;
    if (aNativeParent) {
        if (parent) {
            ALOG("Ignoring native parent on Android window [%p], "
                 "since parent was specified (%p %p)", (void*)this,
                 (void*)aNativeParent, (void*)aParent);
        } else {
            parent = (nsWindow*) aNativeParent;
        }
    }

    mBounds = aRect;

    BaseCreate(nullptr, aInitData);

    NS_ASSERTION(IsTopLevel() || parent,
                 "non-top-level window doesn't have a parent!");

    if (IsTopLevel()) {
        gTopLevelWindows.AppendElement(this);

    } else if (parent) {
        parent->mChildren.AppendElement(this);
        mParent = parent;
    }

#ifdef DEBUG_ANDROID_WIDGET
    DumpWindows();
#endif

    return NS_OK;
}

void
nsWindow::Destroy()
{
    nsBaseWidget::mOnDestroyCalled = true;

    if (mGeckoViewSupport) {
        // Disassociate our native object with GeckoView.
        mGeckoViewSupport = nullptr;
    }

    // Stuff below may release the last ref to this
    nsCOMPtr<nsIWidget> kungFuDeathGrip(this);

    while (mChildren.Length()) {
        // why do we still have children?
        ALOG("### Warning: Destroying window %p and reparenting child %p to null!", (void*)this, (void*)mChildren[0]);
        mChildren[0]->SetParent(nullptr);
    }

    nsBaseWidget::Destroy();

    if (IsTopLevel())
        gTopLevelWindows.RemoveElement(this);

    SetParent(nullptr);

    nsBaseWidget::OnDestroy();

#ifdef DEBUG_ANDROID_WIDGET
    DumpWindows();
#endif
}

NS_IMETHODIMP
nsWindow::ConfigureChildren(const nsTArray<nsIWidget::Configuration>& config)
{
    for (uint32_t i = 0; i < config.Length(); ++i) {
        nsWindow *childWin = (nsWindow*) config[i].mChild.get();
        childWin->Resize(config[i].mBounds.x,
                         config[i].mBounds.y,
                         config[i].mBounds.width,
                         config[i].mBounds.height,
                         false);
    }

    return NS_OK;
}

void
nsWindow::RedrawAll()
{
    if (mAttachedWidgetListener) {
        mAttachedWidgetListener->RequestRepaint();
    } else if (mWidgetListener) {
        mWidgetListener->RequestRepaint();
    }
}

NS_IMETHODIMP
nsWindow::SetParent(nsIWidget *aNewParent)
{
    if ((nsIWidget*)mParent == aNewParent)
        return NS_OK;

    // If we had a parent before, remove ourselves from its list of
    // children.
    if (mParent)
        mParent->mChildren.RemoveElement(this);

    mParent = (nsWindow*)aNewParent;

    if (mParent)
        mParent->mChildren.AppendElement(this);

    // if we are now in the toplevel window's hierarchy, schedule a redraw
    if (FindTopLevel() == nsWindow::TopWindow())
        RedrawAll();

    return NS_OK;
}

nsIWidget*
nsWindow::GetParent()
{
    return mParent;
}

float
nsWindow::GetDPI()
{
    if (AndroidBridge::Bridge())
        return AndroidBridge::Bridge()->GetDPI();
    return 160.0f;
}

double
nsWindow::GetDefaultScaleInternal()
{
    static double density = 0.0;

    if (density != 0.0) {
        return density;
    }

    density = GeckoAppShell::GetDensity();

    if (!density) {
        density = 1.0;
    }

    return density;
}

NS_IMETHODIMP
nsWindow::Show(bool aState)
{
    ALOG("nsWindow[%p]::Show %d", (void*)this, aState);

    if (mWindowType == eWindowType_invisible) {
        ALOG("trying to show invisible window! ignoring..");
        return NS_ERROR_FAILURE;
    }

    if (aState == mIsVisible)
        return NS_OK;

    mIsVisible = aState;

    if (IsTopLevel()) {
        // XXX should we bring this to the front when it's shown,
        // if it's a toplevel widget?

        // XXX we should synthesize a eMouseExitFromWidget (for old top
        // window)/eMouseEnterIntoWidget (for new top window) since we need
        // to pretend that the top window always has focus.  Not sure
        // if Show() is the right place to do this, though.

        if (aState) {
            // It just became visible, so bring it to the front.
            BringToFront();

        } else if (nsWindow::TopWindow() == this) {
            // find the next visible window to show
            unsigned int i;
            for (i = 1; i < gTopLevelWindows.Length(); i++) {
                nsWindow *win = gTopLevelWindows[i];
                if (!win->mIsVisible)
                    continue;

                win->BringToFront();
                break;
            }
        }
    } else if (FindTopLevel() == nsWindow::TopWindow()) {
        RedrawAll();
    }

#ifdef DEBUG_ANDROID_WIDGET
    DumpWindows();
#endif

    return NS_OK;
}

bool
nsWindow::IsVisible() const
{
    return mIsVisible;
}

void
nsWindow::ConstrainPosition(bool aAllowSlop,
                            int32_t *aX,
                            int32_t *aY)
{
    ALOG("nsWindow[%p]::ConstrainPosition %d [%d %d]", (void*)this, aAllowSlop, *aX, *aY);

    // constrain toplevel windows; children we don't care about
    if (IsTopLevel()) {
        *aX = 0;
        *aY = 0;
    }
}

NS_IMETHODIMP
nsWindow::Move(double aX,
               double aY)
{
    if (IsTopLevel())
        return NS_OK;

    return Resize(aX,
                  aY,
                  mBounds.width,
                  mBounds.height,
                  true);
}

NS_IMETHODIMP
nsWindow::Resize(double aWidth,
                 double aHeight,
                 bool aRepaint)
{
    return Resize(mBounds.x,
                  mBounds.y,
                  aWidth,
                  aHeight,
                  aRepaint);
}

NS_IMETHODIMP
nsWindow::Resize(double aX,
                 double aY,
                 double aWidth,
                 double aHeight,
                 bool aRepaint)
{
    ALOG("nsWindow[%p]::Resize [%f %f %f %f] (repaint %d)", (void*)this, aX, aY, aWidth, aHeight, aRepaint);

    bool needSizeDispatch = aWidth != mBounds.width || aHeight != mBounds.height;

    mBounds.x = NSToIntRound(aX);
    mBounds.y = NSToIntRound(aY);
    mBounds.width = NSToIntRound(aWidth);
    mBounds.height = NSToIntRound(aHeight);

    if (needSizeDispatch) {
        OnSizeChanged(gfx::IntSize::Truncate(aWidth, aHeight));
    }

    // Should we skip honoring aRepaint here?
    if (aRepaint && FindTopLevel() == nsWindow::TopWindow())
        RedrawAll();

    nsIWidgetListener* listener = GetWidgetListener();
    if (mAwaitingFullScreen && listener) {
      listener->FullscreenChanged(mIsFullScreen);
      mAwaitingFullScreen = false;
    }

    return NS_OK;
}

void
nsWindow::SetZIndex(int32_t aZIndex)
{
    ALOG("nsWindow[%p]::SetZIndex %d ignored", (void*)this, aZIndex);
}

void
nsWindow::SetSizeMode(nsSizeMode aMode)
{
    switch (aMode) {
        case nsSizeMode_Minimized:
            GeckoAppShell::MoveTaskToBack();
            break;
        case nsSizeMode_Fullscreen:
            MakeFullScreen(true);
            break;
        default:
            break;
    }
}

NS_IMETHODIMP
nsWindow::Enable(bool aState)
{
    ALOG("nsWindow[%p]::Enable %d ignored", (void*)this, aState);
    return NS_OK;
}

bool
nsWindow::IsEnabled() const
{
    return true;
}

NS_IMETHODIMP
nsWindow::Invalidate(const LayoutDeviceIntRect& aRect)
{
    return NS_OK;
}

nsWindow*
nsWindow::FindTopLevel()
{
    nsWindow *toplevel = this;
    while (toplevel) {
        if (toplevel->IsTopLevel())
            return toplevel;

        toplevel = toplevel->mParent;
    }

    ALOG("nsWindow::FindTopLevel(): couldn't find a toplevel or dialog window in this [%p] widget's hierarchy!", (void*)this);
    return this;
}

NS_IMETHODIMP
nsWindow::SetFocus(bool aRaise)
{
    nsWindow *top = FindTopLevel();
    top->BringToFront();

    return NS_OK;
}

void
nsWindow::BringToFront()
{
    // If the window to be raised is the same as the currently raised one,
    // do nothing. We need to check the focus manager as well, as the first
    // window that is created will be first in the window list but won't yet
    // be focused.
    nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
    nsCOMPtr<mozIDOMWindowProxy> existingTopWindow;
    fm->GetActiveWindow(getter_AddRefs(existingTopWindow));
    if (existingTopWindow && FindTopLevel() == nsWindow::TopWindow())
        return;

    if (!IsTopLevel()) {
        FindTopLevel()->BringToFront();
        return;
    }

    RefPtr<nsWindow> kungFuDeathGrip(this);

    nsWindow *oldTop = nullptr;
    if (!gTopLevelWindows.IsEmpty()) {
        oldTop = gTopLevelWindows[0];
    }

    gTopLevelWindows.RemoveElement(this);
    gTopLevelWindows.InsertElementAt(0, this);

    if (oldTop) {
      nsIWidgetListener* listener = oldTop->GetWidgetListener();
      if (listener) {
          listener->WindowDeactivated();
      }
    }

    if (mWidgetListener) {
        mWidgetListener->WindowActivated();
    }

    RedrawAll();
}

LayoutDeviceIntRect
nsWindow::GetScreenBounds()
{
    return LayoutDeviceIntRect(WidgetToScreenOffset(), mBounds.Size());
}

LayoutDeviceIntPoint
nsWindow::WidgetToScreenOffset()
{
    LayoutDeviceIntPoint p(0, 0);
    nsWindow *w = this;

    while (w && !w->IsTopLevel()) {
        p.x += w->mBounds.x;
        p.y += w->mBounds.y;

        w = w->mParent;
    }

    return p;
}

NS_IMETHODIMP
nsWindow::DispatchEvent(WidgetGUIEvent* aEvent,
                        nsEventStatus& aStatus)
{
    aStatus = DispatchEvent(aEvent);
    return NS_OK;
}

nsEventStatus
nsWindow::DispatchEvent(WidgetGUIEvent* aEvent)
{
    if (mAttachedWidgetListener) {
        return mAttachedWidgetListener->HandleEvent(aEvent, mUseAttachedEvents);
    } else if (mWidgetListener) {
        return mWidgetListener->HandleEvent(aEvent, mUseAttachedEvents);
    }
    return nsEventStatus_eIgnore;
}

nsresult
nsWindow::MakeFullScreen(bool aFullScreen, nsIScreen*)
{
    mIsFullScreen = aFullScreen;
    mAwaitingFullScreen = true;
    GeckoAppShell::SetFullScreen(aFullScreen);
    return NS_OK;
}

mozilla::layers::LayerManager*
nsWindow::GetLayerManager(PLayerTransactionChild*, LayersBackend, LayerManagerPersistence)
{
    if (mLayerManager) {
        return mLayerManager;
    }
    return nullptr;
}

void
nsWindow::CreateLayerManager(int aCompositorWidth, int aCompositorHeight)
{
    if (mLayerManager) {
        return;
    }

    nsWindow *topLevelWindow = FindTopLevel();
    if (!topLevelWindow || topLevelWindow->mWindowType == eWindowType_invisible) {
        // don't create a layer manager for an invisible top-level window
        return;
    }

    // Ensure that gfxPlatform is initialized first.
    gfxPlatform::GetPlatform();

    if (ShouldUseOffMainThreadCompositing()) {
        CreateCompositor(aCompositorWidth, aCompositorHeight);
        if (mLayerManager) {
            return;
        }

        // If we get here, then off main thread compositing failed to initialize.
        sFailedToCreateGLContext = true;
    }

    if (!ComputeShouldAccelerate() || sFailedToCreateGLContext) {
        printf_stderr(" -- creating basic, not accelerated\n");
        mLayerManager = CreateBasicLayerManager();
    }
}

void
nsWindow::OnSizeChanged(const gfx::IntSize& aSize)
{
    ALOG("nsWindow: %p OnSizeChanged [%d %d]", (void*)this, aSize.width, aSize.height);

    mBounds.width = aSize.width;
    mBounds.height = aSize.height;

    if (mWidgetListener) {
        mWidgetListener->WindowResized(this, aSize.width, aSize.height);
    }

    if (mAttachedWidgetListener) {
        mAttachedWidgetListener->WindowResized(this, aSize.width, aSize.height);
    }
}

void
nsWindow::InitEvent(WidgetGUIEvent& event, LayoutDeviceIntPoint* aPoint)
{
    if (aPoint) {
        event.mRefPoint = *aPoint;
    } else {
        event.mRefPoint = LayoutDeviceIntPoint(0, 0);
    }

    event.mTime = PR_Now() / 1000;
}

void
nsWindow::UpdateOverscrollVelocity(const float aX, const float aY)
{
    if (NativePtr<NPZCSupport>::Locked npzcs{mNPZCSupport}) {
        npzcs->UpdateOverscrollVelocity(aX, aY);
    }
}

void
nsWindow::UpdateOverscrollOffset(const float aX, const float aY)
{
    if (NativePtr<NPZCSupport>::Locked npzcs{mNPZCSupport}) {
        npzcs->UpdateOverscrollOffset(aX, aY);
    }
}

void
nsWindow::SetScrollingRootContent(const bool isRootContent)
{
    // On Android, the Controller thread and UI thread are the same.
    MOZ_ASSERT(APZThreadUtils::IsControllerThread(), "nsWindow::SetScrollingRootContent must be called from the controller thread");

    if (NativePtr<NPZCSupport>::Locked npzcs{mNPZCSupport}) {
        npzcs->SetScrollingRootContent(isRootContent);
    }
}

void
nsWindow::SetSelectionDragState(bool aState)
{
    if (NativePtr<NPZCSupport>::Locked npzcs{mNPZCSupport}) {
        npzcs->SetSelectionDragState(aState);
    }
}

void *
nsWindow::GetNativeData(uint32_t aDataType)
{
    switch (aDataType) {
        // used by GLContextProviderEGL, nullptr is EGL_DEFAULT_DISPLAY
        case NS_NATIVE_DISPLAY:
            return nullptr;

        case NS_NATIVE_WIDGET:
            return (void *) this;

        case NS_RAW_NATIVE_IME_CONTEXT: {
            void* pseudoIMEContext = GetPseudoIMEContext();
            if (pseudoIMEContext) {
                return pseudoIMEContext;
            }
            // We assume that there is only one context per process on Android
            return NS_ONLY_ONE_NATIVE_IME_CONTEXT;
        }

        case NS_JAVA_SURFACE:
            if (NativePtr<LayerViewSupport>::Locked lvs{mLayerViewSupport}) {
                return lvs->GetSurface().Get();
            }
            return nullptr;

        case NS_PRESENTATION_WINDOW:
            return PMPMSupport::sWindow;

        case NS_PRESENTATION_SURFACE:
            return PMPMSupport::sSurface;
    }

    return nullptr;
}

void
nsWindow::SetNativeData(uint32_t aDataType, uintptr_t aVal)
{
    switch (aDataType) {
        case NS_PRESENTATION_SURFACE:
            PMPMSupport::sSurface = reinterpret_cast<EGLSurface>(aVal);
            break;
    }
}

void
nsWindow::DispatchHitTest(const WidgetTouchEvent& aEvent)
{
    if (aEvent.mMessage == eTouchStart && aEvent.mTouches.Length() == 1) {
        // Since touch events don't get retargeted by PositionedEventTargeting.cpp
        // code on Fennec, we dispatch a dummy mouse event that *does* get
        // retargeted. The Fennec browser.js code can use this to activate the
        // highlight element in case the this touchstart is the start of a tap.
        WidgetMouseEvent hittest(true, eMouseHitTest, this,
                                 WidgetMouseEvent::eReal);
        hittest.mRefPoint = aEvent.mTouches[0]->mRefPoint;
        hittest.mIgnoreRootScrollFrame = true;
        hittest.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
        nsEventStatus status;
        DispatchEvent(&hittest, status);

        if (mAPZEventState && hittest.hitCluster) {
            mAPZEventState->ProcessClusterHit();
        }
    }
}

static unsigned int ConvertAndroidKeyCodeToDOMKeyCode(int androidKeyCode)
{
    // Special-case alphanumeric keycodes because they are most common.
    if (androidKeyCode >= AKEYCODE_A &&
        androidKeyCode <= AKEYCODE_Z) {
        return androidKeyCode - AKEYCODE_A + NS_VK_A;
    }

    if (androidKeyCode >= AKEYCODE_0 &&
        androidKeyCode <= AKEYCODE_9) {
        return androidKeyCode - AKEYCODE_0 + NS_VK_0;
    }

    switch (androidKeyCode) {
        // KEYCODE_UNKNOWN (0) ... KEYCODE_HOME (3)
        case AKEYCODE_BACK:               return NS_VK_ESCAPE;
        // KEYCODE_CALL (5) ... KEYCODE_POUND (18)
        case AKEYCODE_DPAD_UP:            return NS_VK_UP;
        case AKEYCODE_DPAD_DOWN:          return NS_VK_DOWN;
        case AKEYCODE_DPAD_LEFT:          return NS_VK_LEFT;
        case AKEYCODE_DPAD_RIGHT:         return NS_VK_RIGHT;
        case AKEYCODE_DPAD_CENTER:        return NS_VK_RETURN;
        case AKEYCODE_VOLUME_UP:          return NS_VK_VOLUME_UP;
        case AKEYCODE_VOLUME_DOWN:        return NS_VK_VOLUME_DOWN;
        // KEYCODE_VOLUME_POWER (26) ... KEYCODE_Z (54)
        case AKEYCODE_COMMA:              return NS_VK_COMMA;
        case AKEYCODE_PERIOD:             return NS_VK_PERIOD;
        case AKEYCODE_ALT_LEFT:           return NS_VK_ALT;
        case AKEYCODE_ALT_RIGHT:          return NS_VK_ALT;
        case AKEYCODE_SHIFT_LEFT:         return NS_VK_SHIFT;
        case AKEYCODE_SHIFT_RIGHT:        return NS_VK_SHIFT;
        case AKEYCODE_TAB:                return NS_VK_TAB;
        case AKEYCODE_SPACE:              return NS_VK_SPACE;
        // KEYCODE_SYM (63) ... KEYCODE_ENVELOPE (65)
        case AKEYCODE_ENTER:              return NS_VK_RETURN;
        case AKEYCODE_DEL:                return NS_VK_BACK; // Backspace
        case AKEYCODE_GRAVE:              return NS_VK_BACK_QUOTE;
        // KEYCODE_MINUS (69)
        case AKEYCODE_EQUALS:             return NS_VK_EQUALS;
        case AKEYCODE_LEFT_BRACKET:       return NS_VK_OPEN_BRACKET;
        case AKEYCODE_RIGHT_BRACKET:      return NS_VK_CLOSE_BRACKET;
        case AKEYCODE_BACKSLASH:          return NS_VK_BACK_SLASH;
        case AKEYCODE_SEMICOLON:          return NS_VK_SEMICOLON;
        // KEYCODE_APOSTROPHE (75)
        case AKEYCODE_SLASH:              return NS_VK_SLASH;
        // KEYCODE_AT (77) ... KEYCODE_MEDIA_FAST_FORWARD (90)
        case AKEYCODE_MUTE:               return NS_VK_VOLUME_MUTE;
        case AKEYCODE_PAGE_UP:            return NS_VK_PAGE_UP;
        case AKEYCODE_PAGE_DOWN:          return NS_VK_PAGE_DOWN;
        // KEYCODE_PICTSYMBOLS (94) ... KEYCODE_BUTTON_MODE (110)
        case AKEYCODE_ESCAPE:             return NS_VK_ESCAPE;
        case AKEYCODE_FORWARD_DEL:        return NS_VK_DELETE;
        case AKEYCODE_CTRL_LEFT:          return NS_VK_CONTROL;
        case AKEYCODE_CTRL_RIGHT:         return NS_VK_CONTROL;
        case AKEYCODE_CAPS_LOCK:          return NS_VK_CAPS_LOCK;
        case AKEYCODE_SCROLL_LOCK:        return NS_VK_SCROLL_LOCK;
        // KEYCODE_META_LEFT (117) ... KEYCODE_FUNCTION (119)
        case AKEYCODE_SYSRQ:              return NS_VK_PRINTSCREEN;
        case AKEYCODE_BREAK:              return NS_VK_PAUSE;
        case AKEYCODE_MOVE_HOME:          return NS_VK_HOME;
        case AKEYCODE_MOVE_END:           return NS_VK_END;
        case AKEYCODE_INSERT:             return NS_VK_INSERT;
        // KEYCODE_FORWARD (125) ... KEYCODE_MEDIA_RECORD (130)
        case AKEYCODE_F1:                 return NS_VK_F1;
        case AKEYCODE_F2:                 return NS_VK_F2;
        case AKEYCODE_F3:                 return NS_VK_F3;
        case AKEYCODE_F4:                 return NS_VK_F4;
        case AKEYCODE_F5:                 return NS_VK_F5;
        case AKEYCODE_F6:                 return NS_VK_F6;
        case AKEYCODE_F7:                 return NS_VK_F7;
        case AKEYCODE_F8:                 return NS_VK_F8;
        case AKEYCODE_F9:                 return NS_VK_F9;
        case AKEYCODE_F10:                return NS_VK_F10;
        case AKEYCODE_F11:                return NS_VK_F11;
        case AKEYCODE_F12:                return NS_VK_F12;
        case AKEYCODE_NUM_LOCK:           return NS_VK_NUM_LOCK;
        case AKEYCODE_NUMPAD_0:           return NS_VK_NUMPAD0;
        case AKEYCODE_NUMPAD_1:           return NS_VK_NUMPAD1;
        case AKEYCODE_NUMPAD_2:           return NS_VK_NUMPAD2;
        case AKEYCODE_NUMPAD_3:           return NS_VK_NUMPAD3;
        case AKEYCODE_NUMPAD_4:           return NS_VK_NUMPAD4;
        case AKEYCODE_NUMPAD_5:           return NS_VK_NUMPAD5;
        case AKEYCODE_NUMPAD_6:           return NS_VK_NUMPAD6;
        case AKEYCODE_NUMPAD_7:           return NS_VK_NUMPAD7;
        case AKEYCODE_NUMPAD_8:           return NS_VK_NUMPAD8;
        case AKEYCODE_NUMPAD_9:           return NS_VK_NUMPAD9;
        case AKEYCODE_NUMPAD_DIVIDE:      return NS_VK_DIVIDE;
        case AKEYCODE_NUMPAD_MULTIPLY:    return NS_VK_MULTIPLY;
        case AKEYCODE_NUMPAD_SUBTRACT:    return NS_VK_SUBTRACT;
        case AKEYCODE_NUMPAD_ADD:         return NS_VK_ADD;
        case AKEYCODE_NUMPAD_DOT:         return NS_VK_DECIMAL;
        case AKEYCODE_NUMPAD_COMMA:       return NS_VK_SEPARATOR;
        case AKEYCODE_NUMPAD_ENTER:       return NS_VK_RETURN;
        case AKEYCODE_NUMPAD_EQUALS:      return NS_VK_EQUALS;
        // KEYCODE_NUMPAD_LEFT_PAREN (162) ... KEYCODE_CALCULATOR (210)

        // Needs to confirm the behavior.  If the key switches the open state
        // of Japanese IME (or switches input character between Hiragana and
        // Roman numeric characters), then, it might be better to use
        // NS_VK_KANJI which is used for Alt+Zenkaku/Hankaku key on Windows.
        case AKEYCODE_ZENKAKU_HANKAKU:    return 0;
        case AKEYCODE_EISU:               return NS_VK_EISU;
        case AKEYCODE_MUHENKAN:           return NS_VK_NONCONVERT;
        case AKEYCODE_HENKAN:             return NS_VK_CONVERT;
        case AKEYCODE_KATAKANA_HIRAGANA:  return 0;
        case AKEYCODE_YEN:                return NS_VK_BACK_SLASH; // Same as other platforms.
        case AKEYCODE_RO:                 return NS_VK_BACK_SLASH; // Same as other platforms.
        case AKEYCODE_KANA:               return NS_VK_KANA;
        case AKEYCODE_ASSIST:             return NS_VK_HELP;

        // the A key is the action key for gamepad devices.
        case AKEYCODE_BUTTON_A:          return NS_VK_RETURN;

        default:
            ALOG("ConvertAndroidKeyCodeToDOMKeyCode: "
                 "No DOM keycode for Android keycode %d", androidKeyCode);
        return 0;
    }
}

static KeyNameIndex
ConvertAndroidKeyCodeToKeyNameIndex(int keyCode, int action,
                                    int domPrintableKeyValue)
{
    // Special-case alphanumeric keycodes because they are most common.
    if (keyCode >= AKEYCODE_A && keyCode <= AKEYCODE_Z) {
        return KEY_NAME_INDEX_USE_STRING;
    }

    if (keyCode >= AKEYCODE_0 && keyCode <= AKEYCODE_9) {
        return KEY_NAME_INDEX_USE_STRING;
    }

    switch (keyCode) {

#define NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, aKeyNameIndex) \
        case aNativeKey: return aKeyNameIndex;

#include "NativeKeyToDOMKeyName.h"

#undef NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX

        // KEYCODE_0 (7) ... KEYCODE_9 (16)
        case AKEYCODE_STAR:               // '*' key
        case AKEYCODE_POUND:              // '#' key

        // KEYCODE_A (29) ... KEYCODE_Z (54)

        case AKEYCODE_COMMA:              // ',' key
        case AKEYCODE_PERIOD:             // '.' key
        case AKEYCODE_SPACE:
        case AKEYCODE_GRAVE:              // '`' key
        case AKEYCODE_MINUS:              // '-' key
        case AKEYCODE_EQUALS:             // '=' key
        case AKEYCODE_LEFT_BRACKET:       // '[' key
        case AKEYCODE_RIGHT_BRACKET:      // ']' key
        case AKEYCODE_BACKSLASH:          // '\' key
        case AKEYCODE_SEMICOLON:          // ';' key
        case AKEYCODE_APOSTROPHE:         // ''' key
        case AKEYCODE_SLASH:              // '/' key
        case AKEYCODE_AT:                 // '@' key
        case AKEYCODE_PLUS:               // '+' key

        case AKEYCODE_NUMPAD_0:
        case AKEYCODE_NUMPAD_1:
        case AKEYCODE_NUMPAD_2:
        case AKEYCODE_NUMPAD_3:
        case AKEYCODE_NUMPAD_4:
        case AKEYCODE_NUMPAD_5:
        case AKEYCODE_NUMPAD_6:
        case AKEYCODE_NUMPAD_7:
        case AKEYCODE_NUMPAD_8:
        case AKEYCODE_NUMPAD_9:
        case AKEYCODE_NUMPAD_DIVIDE:
        case AKEYCODE_NUMPAD_MULTIPLY:
        case AKEYCODE_NUMPAD_SUBTRACT:
        case AKEYCODE_NUMPAD_ADD:
        case AKEYCODE_NUMPAD_DOT:
        case AKEYCODE_NUMPAD_COMMA:
        case AKEYCODE_NUMPAD_EQUALS:
        case AKEYCODE_NUMPAD_LEFT_PAREN:
        case AKEYCODE_NUMPAD_RIGHT_PAREN:

        case AKEYCODE_YEN:                // yen sign key
        case AKEYCODE_RO:                 // Japanese Ro key
            return KEY_NAME_INDEX_USE_STRING;

        case AKEYCODE_ENDCALL:
        case AKEYCODE_NUM:                // XXX Not sure
        case AKEYCODE_HEADSETHOOK:
        case AKEYCODE_NOTIFICATION:       // XXX Not sure
        case AKEYCODE_PICTSYMBOLS:

        case AKEYCODE_BUTTON_A:
        case AKEYCODE_BUTTON_B:
        case AKEYCODE_BUTTON_C:
        case AKEYCODE_BUTTON_X:
        case AKEYCODE_BUTTON_Y:
        case AKEYCODE_BUTTON_Z:
        case AKEYCODE_BUTTON_L1:
        case AKEYCODE_BUTTON_R1:
        case AKEYCODE_BUTTON_L2:
        case AKEYCODE_BUTTON_R2:
        case AKEYCODE_BUTTON_THUMBL:
        case AKEYCODE_BUTTON_THUMBR:
        case AKEYCODE_BUTTON_START:
        case AKEYCODE_BUTTON_SELECT:
        case AKEYCODE_BUTTON_MODE:

        case AKEYCODE_MUTE: // mutes the microphone
        case AKEYCODE_MEDIA_CLOSE:

        case AKEYCODE_DVR:

        case AKEYCODE_BUTTON_1:
        case AKEYCODE_BUTTON_2:
        case AKEYCODE_BUTTON_3:
        case AKEYCODE_BUTTON_4:
        case AKEYCODE_BUTTON_5:
        case AKEYCODE_BUTTON_6:
        case AKEYCODE_BUTTON_7:
        case AKEYCODE_BUTTON_8:
        case AKEYCODE_BUTTON_9:
        case AKEYCODE_BUTTON_10:
        case AKEYCODE_BUTTON_11:
        case AKEYCODE_BUTTON_12:
        case AKEYCODE_BUTTON_13:
        case AKEYCODE_BUTTON_14:
        case AKEYCODE_BUTTON_15:
        case AKEYCODE_BUTTON_16:

        case AKEYCODE_MANNER_MODE:
        case AKEYCODE_3D_MODE:
        case AKEYCODE_CONTACTS:
            return KEY_NAME_INDEX_Unidentified;

        case AKEYCODE_UNKNOWN:
            MOZ_ASSERT(
                action != AKEY_EVENT_ACTION_MULTIPLE,
                "Don't call this when action is AKEY_EVENT_ACTION_MULTIPLE!");
            // It's actually an unknown key if the action isn't ACTION_MULTIPLE.
            // However, it might cause text input.  So, let's check the value.
            return domPrintableKeyValue ?
                KEY_NAME_INDEX_USE_STRING : KEY_NAME_INDEX_Unidentified;

        default:
            ALOG("ConvertAndroidKeyCodeToKeyNameIndex: "
                 "No DOM key name index for Android keycode %d", keyCode);
            return KEY_NAME_INDEX_Unidentified;
    }
}

static CodeNameIndex
ConvertAndroidScanCodeToCodeNameIndex(int scanCode)
{
    switch (scanCode) {

#define NS_NATIVE_KEY_TO_DOM_CODE_NAME_INDEX(aNativeKey, aCodeNameIndex) \
        case aNativeKey: return aCodeNameIndex;

#include "NativeKeyToDOMCodeName.h"

#undef NS_NATIVE_KEY_TO_DOM_CODE_NAME_INDEX

        default:
          return CODE_NAME_INDEX_UNKNOWN;
    }
}

static bool
IsModifierKey(int32_t keyCode)
{
    using mozilla::java::sdk::KeyEvent;
    return keyCode == KeyEvent::KEYCODE_ALT_LEFT ||
           keyCode == KeyEvent::KEYCODE_ALT_RIGHT ||
           keyCode == KeyEvent::KEYCODE_SHIFT_LEFT ||
           keyCode == KeyEvent::KEYCODE_SHIFT_RIGHT ||
           keyCode == KeyEvent::KEYCODE_CTRL_LEFT ||
           keyCode == KeyEvent::KEYCODE_CTRL_RIGHT ||
           keyCode == KeyEvent::KEYCODE_META_LEFT ||
           keyCode == KeyEvent::KEYCODE_META_RIGHT;
}

static Modifiers
GetModifiers(int32_t metaState)
{
    using mozilla::java::sdk::KeyEvent;
    return (metaState & KeyEvent::META_ALT_MASK ? MODIFIER_ALT : 0)
        | (metaState & KeyEvent::META_SHIFT_MASK ? MODIFIER_SHIFT : 0)
        | (metaState & KeyEvent::META_CTRL_MASK ? MODIFIER_CONTROL : 0)
        | (metaState & KeyEvent::META_META_MASK ? MODIFIER_META : 0)
        | (metaState & KeyEvent::META_FUNCTION_ON ? MODIFIER_FN : 0)
        | (metaState & KeyEvent::META_CAPS_LOCK_ON ? MODIFIER_CAPSLOCK : 0)
        | (metaState & KeyEvent::META_NUM_LOCK_ON ? MODIFIER_NUMLOCK : 0)
        | (metaState & KeyEvent::META_SCROLL_LOCK_ON ? MODIFIER_SCROLLLOCK : 0);
}

static void
InitKeyEvent(WidgetKeyboardEvent& event,
             int32_t action, int32_t keyCode, int32_t scanCode,
             int32_t metaState, int64_t time, int32_t unicodeChar,
             int32_t baseUnicodeChar, int32_t domPrintableKeyValue,
             int32_t repeatCount, int32_t flags)
{
    const uint32_t domKeyCode = ConvertAndroidKeyCodeToDOMKeyCode(keyCode);
    const int32_t charCode = unicodeChar ? unicodeChar : baseUnicodeChar;

    event.mModifiers = GetModifiers(metaState);

    if (event.mMessage == eKeyPress) {
        // Android gives us \n, so filter out some control characters.
        event.mIsChar = (charCode >= ' ');
        event.mCharCode = event.mIsChar ? charCode : 0;
        event.mKeyCode = event.mIsChar ? 0 : domKeyCode;
        event.mPluginEvent.Clear();

        // For keypress, if the unicode char already has modifiers applied, we
        // don't specify extra modifiers. If UnicodeChar() != BaseUnicodeChar()
        // it means UnicodeChar() already has modifiers applied.
        // Note that on Android 4.x, Alt modifier isn't set when the key input
        // causes text input even while right Alt key is pressed.  However,
        // this is necessary for Android 2.3 compatibility.
        if (unicodeChar && unicodeChar != baseUnicodeChar) {
            event.mModifiers &= ~(MODIFIER_ALT | MODIFIER_CONTROL
                                               | MODIFIER_META);
        }

    } else {
        event.mIsChar = false;
        event.mCharCode = 0;
        event.mKeyCode = domKeyCode;

        ANPEvent pluginEvent;
        pluginEvent.inSize = sizeof(pluginEvent);
        pluginEvent.eventType = kKey_ANPEventType;
        pluginEvent.data.key.action = event.mMessage == eKeyDown
                ? kDown_ANPKeyAction : kUp_ANPKeyAction;
        pluginEvent.data.key.nativeCode = keyCode;
        pluginEvent.data.key.virtualCode = domKeyCode;
        pluginEvent.data.key.unichar = charCode;
        pluginEvent.data.key.modifiers =
                (metaState & sdk::KeyEvent::META_SHIFT_MASK
                        ? kShift_ANPKeyModifier : 0) |
                (metaState & sdk::KeyEvent::META_ALT_MASK
                        ? kAlt_ANPKeyModifier : 0);
        pluginEvent.data.key.repeatCount = repeatCount;
        event.mPluginEvent.Copy(pluginEvent);
    }

    event.mIsRepeat =
        (event.mMessage == eKeyDown || event.mMessage == eKeyPress) &&
        ((flags & sdk::KeyEvent::FLAG_LONG_PRESS) || repeatCount);

    event.mKeyNameIndex = ConvertAndroidKeyCodeToKeyNameIndex(
            keyCode, action, domPrintableKeyValue);
    event.mCodeNameIndex = ConvertAndroidScanCodeToCodeNameIndex(scanCode);

    if (event.mKeyNameIndex == KEY_NAME_INDEX_USE_STRING &&
            domPrintableKeyValue) {
        event.mKeyValue = char16_t(domPrintableKeyValue);
    }

    event.mLocation =
        WidgetKeyboardEvent::ComputeLocationFromCodeValue(event.mCodeNameIndex);
    event.mTime = time;
}

void
nsWindow::GeckoViewSupport::OnKeyEvent(int32_t aAction, int32_t aKeyCode,
        int32_t aScanCode, int32_t aMetaState, int64_t aTime,
        int32_t aUnicodeChar, int32_t aBaseUnicodeChar,
        int32_t aDomPrintableKeyValue, int32_t aRepeatCount, int32_t aFlags,
        bool aIsSynthesizedImeKey, jni::Object::Param originalEvent)
{
    RefPtr<nsWindow> kungFuDeathGrip(&window);
    if (!aIsSynthesizedImeKey) {
        window.UserActivity();
        window.RemoveIMEComposition();
    }

    EventMessage msg;
    if (aAction == sdk::KeyEvent::ACTION_DOWN) {
        msg = eKeyDown;
    } else if (aAction == sdk::KeyEvent::ACTION_UP) {
        msg = eKeyUp;
    } else if (aAction == sdk::KeyEvent::ACTION_MULTIPLE) {
        // Keys with multiple action are handled in Java,
        // and we should never see one here
        MOZ_CRASH("Cannot handle key with multiple action");
    } else {
        ALOG("Unknown key action event!");
        return;
    }

    nsEventStatus status = nsEventStatus_eIgnore;
    WidgetKeyboardEvent event(true, msg, &window);
    window.InitEvent(event, nullptr);
    InitKeyEvent(event, aAction, aKeyCode, aScanCode, aMetaState, aTime,
                 aUnicodeChar, aBaseUnicodeChar, aDomPrintableKeyValue,
                 aRepeatCount, aFlags);

    if (aIsSynthesizedImeKey) {
        // Keys synthesized by Java IME code are saved in the mIMEKeyEvents
        // array until the next IME_REPLACE_TEXT event, at which point
        // these keys are dispatched in sequence.
        mIMEKeyEvents.AppendElement(
                mozilla::UniquePtr<WidgetEvent>(event.Duplicate()));

    } else {
        window.DispatchEvent(&event, status);

        if (window.Destroyed() || status == nsEventStatus_eConsumeNoDefault) {
            // Skip default processing.
            return;
        }

        mEditable->OnDefaultKeyEvent(originalEvent);
    }

    if (msg != eKeyDown || IsModifierKey(aKeyCode)) {
        // Skip sending key press event.
        return;
    }

    WidgetKeyboardEvent pressEvent(true, eKeyPress, &window);
    window.InitEvent(pressEvent, nullptr);
    InitKeyEvent(pressEvent, aAction, aKeyCode, aScanCode, aMetaState, aTime,
                 aUnicodeChar, aBaseUnicodeChar, aDomPrintableKeyValue,
                 aRepeatCount, aFlags);

    if (aIsSynthesizedImeKey) {
        mIMEKeyEvents.AppendElement(
                mozilla::UniquePtr<WidgetEvent>(pressEvent.Duplicate()));
    } else {
        window.DispatchEvent(&pressEvent, status);
    }
}

#ifdef DEBUG_ANDROID_IME
#define ALOGIME(args...) ALOG(args)
#else
#define ALOGIME(args...) ((void)0)
#endif

static nscolor
ConvertAndroidColor(uint32_t aArgb)
{
    return NS_RGBA((aArgb & 0x00ff0000) >> 16,
                   (aArgb & 0x0000ff00) >> 8,
                   (aArgb & 0x000000ff),
                   (aArgb & 0xff000000) >> 24);
}

/*
 * Get the current composition object, if any.
 */
RefPtr<mozilla::TextComposition>
nsWindow::GetIMEComposition()
{
    MOZ_ASSERT(this == FindTopLevel());
    return mozilla::IMEStateManager::GetTextCompositionFor(this);
}

/*
    Remove the composition but leave the text content as-is
*/
void
nsWindow::RemoveIMEComposition()
{
    // Remove composition on Gecko side
    const RefPtr<mozilla::TextComposition> composition(GetIMEComposition());
    if (!composition) {
        return;
    }

    RefPtr<nsWindow> kungFuDeathGrip(this);

    // We have to use eCompositionCommit instead of eCompositionCommitAsIs
    // because TextComposition has a workaround for eCompositionCommitAsIs
    // that prevents compositions containing a single ideographic space
    // character from working (see bug 1209465)..
    WidgetCompositionEvent compositionCommitEvent(
            true, eCompositionCommit, this);
    compositionCommitEvent.mData = composition->String();
    InitEvent(compositionCommitEvent, nullptr);
    DispatchEvent(&compositionCommitEvent);
}

/*
 * Send dummy key events for pages that are unaware of input events,
 * to provide web compatibility for pages that depend on key events.
 * Our dummy key events have 0 as the keycode.
 */
void
nsWindow::GeckoViewSupport::SendIMEDummyKeyEvents()
{
    WidgetKeyboardEvent downEvent(true, eKeyDown, &window);
    window.InitEvent(downEvent, nullptr);
    MOZ_ASSERT(downEvent.mKeyCode == 0);
    window.DispatchEvent(&downEvent);

    WidgetKeyboardEvent upEvent(true, eKeyUp, &window);
    window.InitEvent(upEvent, nullptr);
    MOZ_ASSERT(upEvent.mKeyCode == 0);
    window.DispatchEvent(&upEvent);
}

void
nsWindow::GeckoViewSupport::AddIMETextChange(const IMETextChange& aChange)
{
    mIMETextChanges.AppendElement(aChange);

    // We may not be in the middle of flushing,
    // in which case this flag is meaningless.
    mIMETextChangedDuringFlush = true;

    // Now that we added a new range we need to go back and
    // update all the ranges before that.
    // Ranges that have offsets which follow this new range
    // need to be updated to reflect new offsets
    const int32_t delta = aChange.mNewEnd - aChange.mOldEnd;
    for (int32_t i = mIMETextChanges.Length() - 2; i >= 0; i--) {
        IMETextChange& previousChange = mIMETextChanges[i];
        if (previousChange.mStart > aChange.mOldEnd) {
            previousChange.mStart += delta;
            previousChange.mOldEnd += delta;
            previousChange.mNewEnd += delta;
        }
    }

    // Now go through all ranges to merge any ranges that are connected
    // srcIndex is the index of the range to merge from
    // dstIndex is the index of the range to potentially merge into
    int32_t srcIndex = mIMETextChanges.Length() - 1;
    int32_t dstIndex = srcIndex;

    while (--dstIndex >= 0) {
        IMETextChange& src = mIMETextChanges[srcIndex];
        IMETextChange& dst = mIMETextChanges[dstIndex];
        // When merging a more recent change into an older
        // change, we need to compare recent change's (start, oldEnd)
        // range to the older change's (start, newEnd)
        if (src.mOldEnd < dst.mStart || dst.mNewEnd < src.mStart) {
            // No overlap between ranges
            continue;
        }
        // When merging two ranges, there are generally four posibilities:
        // [----(----]----), (----[----]----),
        // [----(----)----], (----[----)----]
        // where [----] is the first range and (----) is the second range
        // As seen above, the start of the merged range is always the lesser
        // of the two start offsets. OldEnd and NewEnd then need to be
        // adjusted separately depending on the case. In any case, the change
        // in text length of the merged range should be the sum of text length
        // changes of the two original ranges, i.e.,
        // newNewEnd - newOldEnd == newEnd1 - oldEnd1 + newEnd2 - oldEnd2
        dst.mStart = std::min(dst.mStart, src.mStart);
        if (src.mOldEnd < dst.mNewEnd) {
            // New range overlaps or is within previous range; merge
            dst.mNewEnd += src.mNewEnd - src.mOldEnd;
        } else { // src.mOldEnd >= dst.mNewEnd
            // New range overlaps previous range; merge
            dst.mOldEnd += src.mOldEnd - dst.mNewEnd;
            dst.mNewEnd = src.mNewEnd;
        }
        // src merged to dst; delete src.
        mIMETextChanges.RemoveElementAt(srcIndex);
        // Any ranges that we skip over between src and dst are not mergeable
        // so we can safely continue the merge starting at dst
        srcIndex = dstIndex;
    }
}

void
nsWindow::GeckoViewSupport::PostFlushIMEChanges()
{
    if (!mIMETextChanges.IsEmpty() || mIMESelectionChanged) {
        // Already posted
        return;
    }

    // Keep a strong reference to the window to keep 'this' alive.
    RefPtr<nsWindow> window(&this->window);

    nsAppShell::PostEvent([this, window] {
        if (!window->Destroyed()) {
            FlushIMEChanges();
        }
    });
}

void
nsWindow::GeckoViewSupport::FlushIMEChanges(FlushChangesFlag aFlags)
{
    // Only send change notifications if we are *not* masking events,
    // i.e. if we have a focused editor,
    NS_ENSURE_TRUE_VOID(!mIMEMaskEventsCount);

    nsCOMPtr<nsISelection> imeSelection;
    nsCOMPtr<nsIContent> imeRoot;

    // If we are receiving notifications, we must have selection/root content.
    MOZ_ALWAYS_SUCCEEDS(IMEStateManager::GetFocusSelectionAndRoot(
            getter_AddRefs(imeSelection), getter_AddRefs(imeRoot)));

    // Make sure we still have a valid selection/root. We can potentially get
    // a stale selection/root if the editor becomes hidden, for example.
    NS_ENSURE_TRUE_VOID(imeRoot->IsInComposedDoc());

    RefPtr<nsWindow> kungFuDeathGrip(&window);
    window.UserActivity();

    struct TextRecord {
        nsString text;
        int32_t start;
        int32_t oldEnd;
        int32_t newEnd;
    };
    AutoTArray<TextRecord, 4> textTransaction;
    if (mIMETextChanges.Length() > textTransaction.Capacity()) {
        textTransaction.SetCapacity(mIMETextChanges.Length());
    }

    mIMETextChangedDuringFlush = false;

    auto shouldAbort = [=] () -> bool {
        if (!mIMETextChangedDuringFlush) {
            return false;
        }
        // A query event could have triggered more text changes to come in, as
        // indicated by our flag. If that happens, try flushing IME changes
        // again.
        if (aFlags != FLUSH_FLAG_RETRY) {
            FlushIMEChanges(FLUSH_FLAG_RETRY);
        } else {
            // Don't retry if already retrying, to avoid infinite loops.
            __android_log_print(ANDROID_LOG_WARN, "GeckoViewSupport",
                    "Already retrying IME flush");
        }
        return true;
    };

    for (const IMETextChange &change : mIMETextChanges) {
        if (change.mStart == change.mOldEnd &&
                change.mStart == change.mNewEnd) {
            continue;
        }

        WidgetQueryContentEvent event(true, eQueryTextContent, &window);

        if (change.mNewEnd != change.mStart) {
            window.InitEvent(event, nullptr);
            event.InitForQueryTextContent(change.mStart,
                                          change.mNewEnd - change.mStart);
            window.DispatchEvent(&event);
            NS_ENSURE_TRUE_VOID(event.mSucceeded);
            NS_ENSURE_TRUE_VOID(event.mReply.mContentsRoot == imeRoot.get());
        }

        if (shouldAbort()) {
            return;
        }

        textTransaction.AppendElement(
                TextRecord{event.mReply.mString, change.mStart,
                           change.mOldEnd, change.mNewEnd});
    }

    int32_t selStart = -1;
    int32_t selEnd = -1;

    if (mIMESelectionChanged) {
        WidgetQueryContentEvent event(true, eQuerySelectedText, &window);
        window.InitEvent(event, nullptr);
        window.DispatchEvent(&event);

        NS_ENSURE_TRUE_VOID(event.mSucceeded);
        NS_ENSURE_TRUE_VOID(event.mReply.mContentsRoot == imeRoot.get());

        if (shouldAbort()) {
            return;
        }

        selStart = int32_t(event.GetSelectionStart());
        selEnd = int32_t(event.GetSelectionEnd());
    }

    // Commit the text change and selection change transaction.
    mIMETextChanges.Clear();

    for (const TextRecord& record : textTransaction) {
        mEditable->OnTextChange(record.text, record.start,
                                record.oldEnd, record.newEnd);
    }

    if (mIMESelectionChanged) {
        mIMESelectionChanged = false;
        mEditable->OnSelectionChange(selStart, selEnd);
    }
}

static jni::ObjectArray::LocalRef
ConvertRectArrayToJavaRectFArray(JNIEnv* aJNIEnv, const nsTArray<LayoutDeviceIntRect>& aRects, const LayoutDeviceIntPoint& aOffset, const CSSToLayoutDeviceScale aScale)
{
    size_t length = aRects.Length();
    jobjectArray rects = aJNIEnv->NewObjectArray(length, sdk::RectF::Context().ClassRef(), nullptr);
    auto rectsRef = jni::ObjectArray::LocalRef::Adopt(aJNIEnv, rects);
    for (size_t i = 0; i < length; i++) {
        sdk::RectF::LocalRef rect(aJNIEnv);
        LayoutDeviceIntRect tmp = aRects[i] + aOffset;
        sdk::RectF::New(tmp.x / aScale.scale, tmp.y / aScale.scale,
                        (tmp.x + tmp.width) / aScale.scale,
                        (tmp.y + tmp.height) / aScale.scale,
                        &rect);
        rectsRef->SetElement(i, rect);
    }
    return rectsRef;
}

void
nsWindow::GeckoViewSupport::UpdateCompositionRects()
{
    const auto composition(window.GetIMEComposition());
    if (NS_WARN_IF(!composition)) {
        return;
    }

    uint32_t offset = composition->NativeOffsetOfStartComposition();
    WidgetQueryContentEvent textRects(true, eQueryTextRectArray, &window);
    textRects.InitForQueryTextRectArray(offset, composition->String().Length());
    window.DispatchEvent(&textRects);

    auto rects =
        ConvertRectArrayToJavaRectFArray(jni::GetGeckoThreadEnv(),
                                         textRects.mReply.mRectArray,
                                         window.WidgetToScreenOffset(),
                                         window.GetDefaultScale());

    mEditable->UpdateCompositionRects(rects);
}

void
nsWindow::GeckoViewSupport::AsyncNotifyIME(int32_t aNotification)
{
    // Keep a strong reference to the window to keep 'this' alive.
    RefPtr<nsWindow> window(&this->window);

    nsAppShell::PostEvent([this, window, aNotification] {
        if (mIMEMaskEventsCount) {
            return;
        }

        mEditable->NotifyIME(aNotification);
    });
}

bool
nsWindow::GeckoViewSupport::NotifyIME(const IMENotification& aIMENotification)
{
    MOZ_ASSERT(mEditable);

    switch (aIMENotification.mMessage) {
        case REQUEST_TO_COMMIT_COMPOSITION: {
            ALOGIME("IME: REQUEST_TO_COMMIT_COMPOSITION");

            window.RemoveIMEComposition();

            AsyncNotifyIME(GeckoEditableListener::
                           NOTIFY_IME_TO_COMMIT_COMPOSITION);
            return true;
        }

        case REQUEST_TO_CANCEL_COMPOSITION: {
            ALOGIME("IME: REQUEST_TO_CANCEL_COMPOSITION");
            RefPtr<nsWindow> kungFuDeathGrip(&window);

            // Cancel composition on Gecko side
            if (window.GetIMEComposition()) {
                WidgetCompositionEvent compositionCommitEvent(
                        true, eCompositionCommit, &window);
                window.InitEvent(compositionCommitEvent, nullptr);
                // Dispatch it with empty mData value for canceling composition.
                window.DispatchEvent(&compositionCommitEvent);
            }

            AsyncNotifyIME(GeckoEditableListener::
                           NOTIFY_IME_TO_CANCEL_COMPOSITION);
            return true;
        }

        case NOTIFY_IME_OF_FOCUS: {
            ALOGIME("IME: NOTIFY_IME_OF_FOCUS");
            // IME will call requestCursorUpdates after getting context.
            // So reset cursor update mode before getting context.
            mIMEMonitorCursor = false;
            mEditable->NotifyIME(GeckoEditableListener::NOTIFY_IME_OF_FOCUS);
            return true;
        }

        case NOTIFY_IME_OF_BLUR: {
            ALOGIME("IME: NOTIFY_IME_OF_BLUR");

            // Mask events because we lost focus. On the next focus event,
            // Gecko will notify Java, and Java will send an acknowledge focus
            // event back to Gecko. That is where we unmask event handling
            mIMEMaskEventsCount++;

            mEditable->NotifyIME(GeckoEditableListener::NOTIFY_IME_OF_BLUR);
            return true;
        }

        case NOTIFY_IME_OF_SELECTION_CHANGE: {
            ALOGIME("IME: NOTIFY_IME_OF_SELECTION_CHANGE");

            PostFlushIMEChanges();
            mIMESelectionChanged = true;
            return true;
        }

        case NOTIFY_IME_OF_TEXT_CHANGE: {
            ALOGIME("IME: NotifyIMEOfTextChange: s=%d, oe=%d, ne=%d",
                    aIMENotification.mTextChangeData.mStartOffset,
                    aIMENotification.mTextChangeData.mRemovedEndOffset,
                    aIMENotification.mTextChangeData.mAddedEndOffset);

            /* Make sure Java's selection is up-to-date */
            PostFlushIMEChanges();
            mIMESelectionChanged = true;
            AddIMETextChange(IMETextChange(aIMENotification));
            return true;
        }

        case NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED: {
            ALOGIME("IME: NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED");

            // Hardware keyboard support requires each string rect.
            if (AndroidBridge::Bridge() && AndroidBridge::Bridge()->GetAPIVersion() >= 21 && mIMEMonitorCursor) {
                UpdateCompositionRects();
            }
            return true;
        }

        default:
            return false;
    }
}

void
nsWindow::GeckoViewSupport::SetInputContext(const InputContext& aContext,
                                            const InputContextAction& aAction)
{
    MOZ_ASSERT(mEditable);

    ALOGIME("IME: SetInputContext: s=0x%X, 0x%X, action=0x%X, 0x%X",
            aContext.mIMEState.mEnabled, aContext.mIMEState.mOpen,
            aAction.mCause, aAction.mFocusChange);

    // Ensure that opening the virtual keyboard is allowed for this specific
    // InputContext depending on the content.ime.strict.policy pref
    if (aContext.mIMEState.mEnabled != IMEState::DISABLED &&
        aContext.mIMEState.mEnabled != IMEState::PLUGIN &&
        Preferences::GetBool("content.ime.strict_policy", false) &&
        !aAction.ContentGotFocusByTrustedCause() &&
        !aAction.UserMightRequestOpenVKB()) {
        return;
    }

    IMEState::Enabled enabled = aContext.mIMEState.mEnabled;

    // Only show the virtual keyboard for plugins if mOpen is set appropriately.
    // This avoids showing it whenever a plugin is focused. Bug 747492
    if (aContext.mIMEState.mEnabled == IMEState::PLUGIN &&
        aContext.mIMEState.mOpen != IMEState::OPEN) {
        enabled = IMEState::DISABLED;
    }

    mInputContext = aContext;
    mInputContext.mIMEState.mEnabled = enabled;

    if (enabled == IMEState::ENABLED && aAction.UserMightRequestOpenVKB()) {
        // Don't reset keyboard when we should simply open the vkb
        mEditable->NotifyIME(GeckoEditableListener::NOTIFY_IME_OPEN_VKB);
        return;
    }

    if (mIMEUpdatingContext) {
        return;
    }

    // Keep a strong reference to the window to keep 'this' alive.
    RefPtr<nsWindow> window(&this->window);
    mIMEUpdatingContext = true;

    nsAppShell::PostEvent([this, window] {
        mIMEUpdatingContext = false;
        if (window->Destroyed()) {
            return;
        }
        MOZ_ASSERT(mEditable);
        mEditable->NotifyIMEContext(mInputContext.mIMEState.mEnabled,
                                    mInputContext.mHTMLInputType,
                                    mInputContext.mHTMLInputInputmode,
                                    mInputContext.mActionHint);
    });
}

InputContext
nsWindow::GeckoViewSupport::GetInputContext()
{
    InputContext context = mInputContext;
    context.mIMEState.mOpen = IMEState::OPEN_STATE_NOT_SUPPORTED;
    return context;
}

void
nsWindow::GeckoViewSupport::OnImeSynchronize()
{
    if (!mIMEMaskEventsCount) {
        FlushIMEChanges();
    }
    mEditable->NotifyIME(GeckoEditableListener::NOTIFY_IME_REPLY_EVENT);
}

void
nsWindow::GeckoViewSupport::OnImeAcknowledgeFocus()
{
    MOZ_ASSERT(mIMEMaskEventsCount > 0);

    AutoIMESynchronize as(this);

    if (--mIMEMaskEventsCount > 0) {
        // Still not focused; reply to events, but don't do anything else.
        return;
    }

    // The focusing handshake sequence is complete, and Java is waiting
    // on Gecko. Now we can notify Java of the newly focused content
    mIMETextChanges.Clear();
    mIMESelectionChanged = false;
    // NotifyIMEOfTextChange also notifies selection
    // Use 'INT32_MAX / 2' here because subsequent text changes might
    // combine with this text change, and overflow might occur if
    // we just use INT32_MAX
    IMENotification notification(NOTIFY_IME_OF_TEXT_CHANGE);
    notification.mTextChangeData.mStartOffset = 0;
    notification.mTextChangeData.mRemovedEndOffset =
        notification.mTextChangeData.mAddedEndOffset = INT32_MAX / 2;
    NotifyIME(notification);
}

void
nsWindow::GeckoViewSupport::OnImeReplaceText(int32_t aStart, int32_t aEnd,
                                             jni::String::Param aText)
{
    AutoIMESynchronize as(this);

    if (mIMEMaskEventsCount > 0) {
        // Not focused; still reply to events, but don't do anything else.
        return;
    }

    /*
        Replace text in Gecko thread from aStart to aEnd with the string text.
    */
    RefPtr<nsWindow> kungFuDeathGrip(&window);
    nsString string(aText->ToString());

    const auto composition(window.GetIMEComposition());
    MOZ_ASSERT(!composition || !composition->IsEditorHandlingEvent());

    if (!mIMEKeyEvents.IsEmpty() || !composition ||
        uint32_t(aStart) != composition->NativeOffsetOfStartComposition() ||
        uint32_t(aEnd) != composition->NativeOffsetOfStartComposition() +
                          composition->String().Length())
    {
        // Only start a new composition if we have key events,
        // if we don't have an existing composition, or
        // the replaced text does not match our composition.
        window.RemoveIMEComposition();

        {
            // Use text selection to set target postion(s) for
            // insert, or replace, of text.
            WidgetSelectionEvent event(true, eSetSelection, &window);
            window.InitEvent(event, nullptr);
            event.mOffset = uint32_t(aStart);
            event.mLength = uint32_t(aEnd - aStart);
            event.mExpandToClusterBoundary = false;
            event.mReason = nsISelectionListener::IME_REASON;
            window.DispatchEvent(&event);
        }

        if (!mIMEKeyEvents.IsEmpty()) {
            nsEventStatus status;
            for (uint32_t i = 0; i < mIMEKeyEvents.Length(); i++) {
                const auto event = static_cast<WidgetGUIEvent*>(
                        mIMEKeyEvents[i].get());
                if (event->mMessage == eKeyPress &&
                        status == nsEventStatus_eConsumeNoDefault) {
                    MOZ_ASSERT(i > 0 &&
                            mIMEKeyEvents[i - 1]->mMessage == eKeyDown);
                    // The previous key down event resulted in eConsumeNoDefault
                    // so we should not dispatch the current key press event.
                    continue;
                }
                // widget for duplicated events is initially nullptr.
                event->mWidget = &window;
                window.DispatchEvent(event, status);
            }
            mIMEKeyEvents.Clear();
            return;
        }

        {
            WidgetCompositionEvent event(true, eCompositionStart, &window);
            window.InitEvent(event, nullptr);
            window.DispatchEvent(&event);
        }

    } else if (composition->String().Equals(string)) {
        /* If the new text is the same as the existing composition text,
         * the NS_COMPOSITION_CHANGE event does not generate a text
         * change notification. However, the Java side still expects
         * one, so we manually generate a notification. */
        IMETextChange dummyChange;
        dummyChange.mStart = aStart;
        dummyChange.mOldEnd = dummyChange.mNewEnd = aEnd;
        AddIMETextChange(dummyChange);
    }

    const bool composing = !mIMERanges->IsEmpty();

    // Previous events may have destroyed our composition; bail in that case.
    if (window.GetIMEComposition()) {
        WidgetCompositionEvent event(true, eCompositionChange, &window);
        window.InitEvent(event, nullptr);
        event.mData = string;

        if (composing) {
            event.mRanges = new TextRangeArray();
            mIMERanges.swap(event.mRanges);

        } else if (event.mData.Length()) {
            // Include proper text ranges to make the editor happy.
            TextRange range;
            range.mStartOffset = 0;
            range.mEndOffset = event.mData.Length();
            range.mRangeType = TextRangeType::eRawClause;
            event.mRanges = new TextRangeArray();
            event.mRanges->AppendElement(range);
        }

        window.DispatchEvent(&event);

    } else if (composing) {
        // Ensure IME ranges are empty.
        mIMERanges->Clear();
    }

    // Don't end composition when composing text or composition was destroyed.
    if (!composing) {
        window.RemoveIMEComposition();
    }

    if (mInputContext.mMayBeIMEUnaware) {
        SendIMEDummyKeyEvents();
    }
}

void
nsWindow::GeckoViewSupport::OnImeAddCompositionRange(
        int32_t aStart, int32_t aEnd, int32_t aRangeType, int32_t aRangeStyle,
        int32_t aRangeLineStyle, bool aRangeBoldLine, int32_t aRangeForeColor,
        int32_t aRangeBackColor, int32_t aRangeLineColor)
{
    if (mIMEMaskEventsCount > 0) {
        // Not focused.
        return;
    }

    TextRange range;
    range.mStartOffset = aStart;
    range.mEndOffset = aEnd;
    range.mRangeType = ToTextRangeType(aRangeType);
    range.mRangeStyle.mDefinedStyles = aRangeStyle;
    range.mRangeStyle.mLineStyle = aRangeLineStyle;
    range.mRangeStyle.mIsBoldLine = aRangeBoldLine;
    range.mRangeStyle.mForegroundColor =
            ConvertAndroidColor(uint32_t(aRangeForeColor));
    range.mRangeStyle.mBackgroundColor =
            ConvertAndroidColor(uint32_t(aRangeBackColor));
    range.mRangeStyle.mUnderlineColor =
            ConvertAndroidColor(uint32_t(aRangeLineColor));
    mIMERanges->AppendElement(range);
}

void
nsWindow::GeckoViewSupport::OnImeUpdateComposition(int32_t aStart, int32_t aEnd)
{
    AutoIMESynchronize as(this);

    if (mIMEMaskEventsCount > 0) {
        // Not focused.
        return;
    }

    RefPtr<nsWindow> kungFuDeathGrip(&window);

    // A composition with no ranges means we want to set the selection.
    if (mIMERanges->IsEmpty()) {
        MOZ_ASSERT(aStart >= 0 && aEnd >= 0);
        window.RemoveIMEComposition();

        WidgetSelectionEvent selEvent(true, eSetSelection, &window);
        window.InitEvent(selEvent, nullptr);

        selEvent.mOffset = std::min(aStart, aEnd);
        selEvent.mLength = std::max(aStart, aEnd) - selEvent.mOffset;
        selEvent.mReversed = aStart > aEnd;
        selEvent.mExpandToClusterBoundary = false;

        window.DispatchEvent(&selEvent);
        return;
    }

    /*
        Update the composition from aStart to aEnd using
          information from added ranges. This is only used for
          visual indication and does not affect the text content.
          Only the offsets are specified and not the text content
          to eliminate the possibility of this event altering the
          text content unintentionally.
    */
    const auto composition(window.GetIMEComposition());
    MOZ_ASSERT(!composition || !composition->IsEditorHandlingEvent());

    WidgetCompositionEvent event(true, eCompositionChange, &window);
    window.InitEvent(event, nullptr);

    event.mRanges = new TextRangeArray();
    mIMERanges.swap(event.mRanges);

    if (!composition ||
        uint32_t(aStart) != composition->NativeOffsetOfStartComposition() ||
        uint32_t(aEnd) != composition->NativeOffsetOfStartComposition() +
                          composition->String().Length())
    {
        // Only start new composition if we don't have an existing one,
        // or if the existing composition doesn't match the new one.
        window.RemoveIMEComposition();

        {
            WidgetSelectionEvent event(true, eSetSelection, &window);
            window.InitEvent(event, nullptr);
            event.mOffset = uint32_t(aStart);
            event.mLength = uint32_t(aEnd - aStart);
            event.mExpandToClusterBoundary = false;
            event.mReason = nsISelectionListener::IME_REASON;
            window.DispatchEvent(&event);
        }

        {
            WidgetQueryContentEvent queryEvent(true, eQuerySelectedText,
                                               &window);
            window.InitEvent(queryEvent, nullptr);
            window.DispatchEvent(&queryEvent);
            MOZ_ASSERT(queryEvent.mSucceeded);
            event.mData = queryEvent.mReply.mString;
        }

        {
            WidgetCompositionEvent event(true, eCompositionStart, &window);
            window.InitEvent(event, nullptr);
            window.DispatchEvent(&event);
        }

    } else {
        // If the new composition matches the existing composition,
        // reuse the old composition.
        event.mData = composition->String();
    }

#ifdef DEBUG_ANDROID_IME
    const NS_ConvertUTF16toUTF8 data(event.mData);
    const char* text = data.get();
    ALOGIME("IME: IME_SET_TEXT: text=\"%s\", length=%u, range=%u",
            text, event.mData.Length(), event.mRanges->Length());
#endif // DEBUG_ANDROID_IME

    // Previous events may have destroyed our composition; bail in that case.
    if (window.GetIMEComposition()) {
        window.DispatchEvent(&event);
    }
}

void
nsWindow::GeckoViewSupport::OnImeRequestCursorUpdates(int aRequestMode)
{
    if (aRequestMode == IME_MONITOR_CURSOR_ONE_SHOT) {
        UpdateCompositionRects();
        return;
    }

    mIMEMonitorCursor = (aRequestMode == IME_MONITOR_CURSOR_START_MONITOR);
}

void
nsWindow::UserActivity()
{
  if (!mIdleService) {
    mIdleService = do_GetService("@mozilla.org/widget/idleservice;1");
  }

  if (mIdleService) {
    mIdleService->ResetIdleTimeOut(0);
  }
}

nsresult
nsWindow::NotifyIMEInternal(const IMENotification& aIMENotification)
{
    MOZ_ASSERT(this == FindTopLevel());

    if (!mGeckoViewSupport) {
        // Non-GeckoView windows don't support IME operations.
        return NS_ERROR_NOT_AVAILABLE;
    }

    if (mGeckoViewSupport->NotifyIME(aIMENotification)) {
        return NS_OK;
    }
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP_(void)
nsWindow::SetInputContext(const InputContext& aContext,
                          const InputContextAction& aAction)
{
    nsWindow* top = FindTopLevel();
    MOZ_ASSERT(top);

    if (!top->mGeckoViewSupport) {
        // Non-GeckoView windows don't support IME operations.
        return;
    }

    // We are using an IME event later to notify Java, and the IME event
    // will be processed by the top window. Therefore, to ensure the
    // IME event uses the correct mInputContext, we need to let the top
    // window process SetInputContext
    top->mGeckoViewSupport->SetInputContext(aContext, aAction);
}

NS_IMETHODIMP_(InputContext)
nsWindow::GetInputContext()
{
    nsWindow* top = FindTopLevel();
    MOZ_ASSERT(top);

    if (!top->mGeckoViewSupport) {
        // Non-GeckoView windows don't support IME operations.
        return InputContext();
    }

    // We let the top window process SetInputContext,
    // so we should let it process GetInputContext as well.
    return top->mGeckoViewSupport->GetInputContext();
}

nsIMEUpdatePreference
nsWindow::GetIMEUpdatePreference()
{
    // While a plugin has focus, nsWindow for Android doesn't need any
    // notifications.
    if (GetInputContext().mIMEState.mEnabled == IMEState::PLUGIN) {
      return nsIMEUpdatePreference();
    }
    return nsIMEUpdatePreference(nsIMEUpdatePreference::NOTIFY_TEXT_CHANGE);
}

nsresult
nsWindow::SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                     TouchPointerState aPointerState,
                                     LayoutDeviceIntPoint aPoint,
                                     double aPointerPressure,
                                     uint32_t aPointerOrientation,
                                     nsIObserver* aObserver)
{
    mozilla::widget::AutoObserverNotifier notifier(aObserver, "touchpoint");

    int eventType;
    switch (aPointerState) {
    case TOUCH_CONTACT:
        // This could be a ACTION_DOWN or ACTION_MOVE depending on the
        // existing state; it is mapped to the right thing in Java.
        eventType = sdk::MotionEvent::ACTION_POINTER_DOWN;
        break;
    case TOUCH_REMOVE:
        // This could be turned into a ACTION_UP in Java
        eventType = sdk::MotionEvent::ACTION_POINTER_UP;
        break;
    case TOUCH_CANCEL:
        eventType = sdk::MotionEvent::ACTION_CANCEL;
        break;
    case TOUCH_HOVER:   // not supported for now
    default:
        return NS_ERROR_UNEXPECTED;
    }

    MOZ_ASSERT(mLayerViewSupport);
    GeckoLayerClient::LocalRef client = mLayerViewSupport->GetLayerClient();
    client->SynthesizeNativeTouchPoint(aPointerId, eventType,
        aPoint.x, aPoint.y, aPointerPressure, aPointerOrientation);

    return NS_OK;
}

nsresult
nsWindow::SynthesizeNativeMouseEvent(LayoutDeviceIntPoint aPoint,
                                     uint32_t aNativeMessage,
                                     uint32_t aModifierFlags,
                                     nsIObserver* aObserver)
{
    mozilla::widget::AutoObserverNotifier notifier(aObserver, "mouseevent");

    MOZ_ASSERT(mLayerViewSupport);
    GeckoLayerClient::LocalRef client = mLayerViewSupport->GetLayerClient();
    client->SynthesizeNativeMouseEvent(aNativeMessage, aPoint.x, aPoint.y);

    return NS_OK;
}

nsresult
nsWindow::SynthesizeNativeMouseMove(LayoutDeviceIntPoint aPoint,
                                    nsIObserver* aObserver)
{
    mozilla::widget::AutoObserverNotifier notifier(aObserver, "mouseevent");

    MOZ_ASSERT(mLayerViewSupport);
    GeckoLayerClient::LocalRef client = mLayerViewSupport->GetLayerClient();
    client->SynthesizeNativeMouseEvent(sdk::MotionEvent::ACTION_HOVER_MOVE, aPoint.x, aPoint.y);

    return NS_OK;
}

bool
nsWindow::PreRender(LayerManagerComposite* aManager)
{
    if (Destroyed()) {
        return true;
    }

    layers::Compositor* compositor = aManager->GetCompositor();

    GeckoLayerClient::LocalRef client;

    if (NativePtr<LayerViewSupport>::Locked lvs{mLayerViewSupport}) {
        client = lvs->GetLayerClient();
    }

    if (compositor && client) {
        // Android Color is ARGB which is apparently unusual.
        compositor->SetDefaultClearColor(gfx::Color::UnusualFromARGB((uint32_t)client->ClearColor()));
    }

    return true;
}
void
nsWindow::DrawWindowUnderlay(LayerManagerComposite* aManager,
                             LayoutDeviceIntRect aRect)
{
    if (Destroyed()) {
        return;
    }

    GeckoLayerClient::LocalRef client;

    if (NativePtr<LayerViewSupport>::Locked lvs{mLayerViewSupport}) {
        client = lvs->GetLayerClient();
    }

    if (!client) {
        return;
    }

    LayerRenderer::Frame::LocalRef frame = client->CreateFrame();
    mLayerRendererFrame = frame;
    if (NS_WARN_IF(!mLayerRendererFrame)) {
        return;
    }

    if (!WidgetPaintsBackground()) {
        return;
    }

    frame->BeginDrawing();
}

void
nsWindow::DrawWindowOverlay(LayerManagerComposite* aManager,
                            LayoutDeviceIntRect aRect)
{
    PROFILER_LABEL("nsWindow", "DrawWindowOverlay",
        js::ProfileEntry::Category::GRAPHICS);

    if (Destroyed() || NS_WARN_IF(!mLayerRendererFrame)) {
        return;
    }

    mLayerRendererFrame->EndDrawing();
    mLayerRendererFrame = nullptr;
}

bool
nsWindow::WidgetPaintsBackground()
{
    static bool sWidgetPaintsBackground = true;
    static bool sWidgetPaintsBackgroundPrefCached = false;

    if (!sWidgetPaintsBackgroundPrefCached) {
        sWidgetPaintsBackgroundPrefCached = true;
        mozilla::Preferences::AddBoolVarCache(&sWidgetPaintsBackground,
                                              "android.widget_paints_background",
                                              true);
    }

    return sWidgetPaintsBackground;
}

bool
nsWindow::NeedsPaint()
{
    if (!mLayerViewSupport || mLayerViewSupport->CompositorPaused() ||
            // FindTopLevel() != nsWindow::TopWindow() ||
            !GetLayerManager(nullptr)) {
        return false;
    }
    return nsIWidget::NeedsPaint();
}

void
nsWindow::ConfigureAPZControllerThread()
{
    APZThreadUtils::SetControllerThread(nullptr);
}

already_AddRefed<GeckoContentController>
nsWindow::CreateRootContentController()
{
    RefPtr<GeckoContentController> controller = new AndroidContentController(this, mAPZEventState, mAPZC);
    return controller.forget();
}

uint32_t
nsWindow::GetMaxTouchPoints() const
{
    return GeckoAppShell::GetMaxTouchPoints();
}

void
nsWindow::UpdateZoomConstraints(const uint32_t& aPresShellId,
                                const FrameMetrics::ViewID& aViewId,
                                const mozilla::Maybe<ZoomConstraints>& aConstraints)
{
    nsBaseWidget::UpdateZoomConstraints(aPresShellId, aViewId, aConstraints);
}

CompositorBridgeParent*
nsWindow::GetCompositorBridgeParent() const
{
  return mCompositorSession ? mCompositorSession->GetInProcessBridge() : nullptr;
}
