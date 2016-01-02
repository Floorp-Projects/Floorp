/* -*- Mode: c++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <android/log.h>
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
#include "mozilla/unused.h"
#include "mozilla/Preferences.h"
#include "mozilla/layers/RenderTrace.h"
#include <algorithm>

using mozilla::dom::ContentParent;
using mozilla::dom::ContentChild;
using mozilla::Unused;

#include "nsWindow.h"

#include "nsIBaseWindow.h"
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
#include "mozilla/layers/APZCTreeManager.h"
#include "mozilla/layers/APZEventState.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "ScopedGLHelpers.h"
#include "mozilla/layers/CompositorOGL.h"
#include "AndroidContentController.h"

#include "nsTArray.h"

#include "AndroidBridge.h"
#include "AndroidBridgeUtilities.h"
#include "android_npapi.h"
#include "GeneratedJNINatives.h"
#include "KeyEvent.h"

#include "imgIEncoder.h"

#include "nsString.h"
#include "GeckoProfiler.h" // For PROFILER_LABEL
#include "nsIXULRuntime.h"
#include "nsPrintfCString.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::widget;
using namespace mozilla::layers;

NS_IMPL_ISUPPORTS_INHERITED0(nsWindow, nsBaseWidget)

// The dimensions of the current android view
static gfx::IntSize gAndroidBounds = gfx::IntSize(0, 0);
static gfx::IntSize gAndroidScreenBounds;

#include "mozilla/layers/CompositorChild.h"
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/LayerTransactionParent.h"
#include "mozilla/Mutex.h"
#include "mozilla/Services.h"
#include "nsThreadUtils.h"

class ContentCreationNotifier;
static StaticRefPtr<ContentCreationNotifier> gContentCreationNotifier;

// A helper class to send updates when content processes
// are created. Currently an update for the screen size is sent.
class ContentCreationNotifier final : public nsIObserver
{
private:
    ~ContentCreationNotifier() {}

public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Observe(nsISupports* aSubject,
                       const char* aTopic,
                       const char16_t* aData) override
    {
        if (!strcmp(aTopic, "ipc:content-created")) {
            nsCOMPtr<nsIObserver> cpo = do_QueryInterface(aSubject);
            ContentParent* cp = static_cast<ContentParent*>(cpo.get());
            Unused << cp->SendScreenSizeChanged(gAndroidScreenBounds);
        } else if (!strcmp(aTopic, "xpcom-shutdown")) {
            nsCOMPtr<nsIObserverService> obs =
                mozilla::services::GetObserverService();
            if (obs) {
                obs->RemoveObserver(static_cast<nsIObserver*>(this),
                                    "xpcom-shutdown");
                obs->RemoveObserver(static_cast<nsIObserver*>(this),
                                    "ipc:content-created");
            }
            gContentCreationNotifier = nullptr;
        }

        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS(ContentCreationNotifier,
                  nsIObserver)

// All the toplevel windows that have been created; these are in
// stacking order, so the window at gAndroidBounds[0] is the topmost
// one.
static nsTArray<nsWindow*> gTopLevelWindows;

// FIXME: because we don't support multiple GeckoViews for every feature
// yet, we have to default to a particular GeckoView for certain calls.
static nsWindow* gGeckoViewWindow;

static bool sFailedToCreateGLContext = false;

// Multitouch swipe thresholds in inches
static const double SWIPE_MAX_PINCH_DELTA_INCHES = 0.4;
static const double SWIPE_MIN_DISTANCE_INCHES = 0.6;


class nsWindow::GeckoViewSupport final
    : public GeckoView::Window::Natives<GeckoViewSupport>
    , public GeckoEditable::Natives<GeckoViewSupport>
    , public SupportsWeakPtr<GeckoViewSupport>
    , public UsesGeckoThreadProxy
{
    nsWindow& window;

public:
    template<typename T>
    class WindowEvent : public nsAppShell::LambdaEvent<T>
    {
        typedef nsAppShell::LambdaEvent<T> Base;

        // Static calls are never stale since they don't need native instances.
        template<bool Static>
        typename mozilla::EnableIf<Static, bool>::Type IsStaleCall()
        { return false; }

        template<bool Static>
        typename mozilla::EnableIf<!Static, bool>::Type IsStaleCall()
        {
            JNIEnv* const env = mozilla::jni::GetEnvForThread();
            const auto& thisArg = Base::lambda.GetThisArg();

            const auto natives = reinterpret_cast<
                    mozilla::WeakPtr<typename T::TargetClass>*>(
                    jni::GetNativeHandle(env, thisArg.Get()));
            MOZ_CATCH_JNI_EXCEPTION(env);

            // The call is stale if the nsWindow has been destroyed on the
            // Gecko side, but the Java object is still attached to it through
            // a weak pointer. Stale calls should be discarded. Note that it's
            // an error if natives is nullptr here; we return false but the
            // native call will throw an error.
            return natives && !natives->get();
        }

    public:
        WindowEvent(T&& l) : Base(mozilla::Move(l)) {}

        void Run() override
        {
            if (!IsStaleCall<T::isStatic>()) {
                return Base::Run();
            }
        }

        nsAppShell::Event::Type ActivityType() const override
        {
            // Events that result in user-visible changes count as UI events.
            if (Base::lambda.IsTarget(&GeckoViewSupport::OnKeyEvent) ||
                Base::lambda.IsTarget(&GeckoViewSupport::OnImeReplaceText) ||
                Base::lambda.IsTarget(&GeckoViewSupport::OnImeUpdateComposition))
            {
                return nsAppShell::Event::Type::kUIActivity;
            }
            return Base::ActivityType();
        }
    };

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
        return nsAppShell::PostEvent(mozilla::MakeUnique<
                WindowEvent<Functor>>(mozilla::Move(aCall)));
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
    {
        Reattach(aInstance);
        EditableBase::AttachNative(mEditable, this);
    }

    ~GeckoViewSupport();

    void Reattach(const GeckoView::Window::LocalRef& aInstance)
    {
        Base::AttachNative(aInstance, this);
    }

    using Base::DisposeNative;
    using EditableBase::DisposeNative;

    /**
     * GeckoView methods
     */
public:
    // Create and attach a window.
    static void Open(const jni::ClassObject::LocalRef& aCls,
                     GeckoView::Window::Param aWindow,
                     GeckoView::Param aView, jni::Object::Param aGLController,
                     int32_t aWidth, int32_t aHeight);

    // Close and destroy the nsWindow.
    void Close();

    // Reattach this nsWindow to a new GeckoView.
    void Reattach(GeckoView::Param aView);

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
            MOZ_ASSERT(aIMENotification.mMessage ==
                           mozilla::widget::NOTIFY_IME_OF_TEXT_CHANGE,
                       "IMETextChange initialized with wrong notification");
            MOZ_ASSERT(aIMENotification.mTextChangeData.IsValid(),
                       "The text change notification isn't initialized");
            MOZ_ASSERT(aIMENotification.mTextChangeData.IsInInt32Range(),
                       "The text change notification is out of range");
        }

        bool IsEmpty() const { return mStart < 0; }
    };

    // GeckoEditable instance used by this nsWindow;
    mozilla::widget::GeckoEditable::GlobalRef mEditable;
    nsAutoTArray<mozilla::UniquePtr<mozilla::WidgetEvent>, 8> mIMEKeyEvents;
    nsAutoTArray<IMETextChange, 4> mIMETextChanges;
    InputContext mInputContext;
    RefPtr<mozilla::TextRangeArray> mIMERanges;
    int32_t mIMEMaskEventsCount; // Mask events when > 0
    bool mIMEUpdatingContext;
    bool mIMESelectionChanged;
    bool mIMETextChangedDuringFlush;

    void SendIMEDummyKeyEvents();
    void AddIMETextChange(const IMETextChange& aChange);

    enum FlushChangesFlag {
        FLUSH_FLAG_NONE,
        FLUSH_FLAG_RETRY
    };
    void PostFlushIMEChanges(FlushChangesFlag aFlags = FLUSH_FLAG_NONE);
    void FlushIMEChanges(FlushChangesFlag aFlags = FLUSH_FLAG_NONE);

public:
    bool NotifyIME(const IMENotification& aIMENotification);
    void SetInputContext(const InputContext& aContext,
                         const InputContextAction& aAction);
    InputContext GetInputContext();

    // Handle an Android KeyEvent.
    void OnKeyEvent(int32_t aAction, int32_t aKeyCode, int32_t aScanCode,
                    int32_t aMetaState, int64_t aTime, int32_t aUnicodeChar,
                    int32_t aBaseUnicodeChar, int32_t aDomPrintableKeyValue,
                    int32_t aRepeatCount, int32_t aFlags,
                    bool aIsSynthesizedImeKey);

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
};

/**
 * GLController has some unique requirements for its native calls, so make it
 * separate from GeckoViewSupport.
 */
class nsWindow::GLControllerSupport final
    : public GLController::Natives<GLControllerSupport>
    , public SupportsWeakPtr<GLControllerSupport>
    , public UsesGeckoThreadProxy
{
    nsWindow& window;
    GLController::GlobalRef mGLController;
    GeckoLayerClient::GlobalRef mLayerClient;
    Atomic<bool, ReleaseAcquire> mCompositorPaused;

    // In order to use Event::HasSameTypeAs in PostTo(), we cannot make
    // GLControllerEvent a template because each template instantiation is
    // a different type. So implement GLControllerEvent as a ProxyEvent.
    class GLControllerEvent final : public nsAppShell::ProxyEvent
    {
        using Event = nsAppShell::Event;

    public:
        static UniquePtr<Event> MakeEvent(UniquePtr<Event>&& event)
        {
            return MakeUnique<GLControllerEvent>(mozilla::Move(event));
        }

        GLControllerEvent(UniquePtr<Event>&& event)
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
    typedef GLController::Natives<GLControllerSupport> Base;

    MOZ_DECLARE_WEAKREFERENCE_TYPENAME(GLControllerSupport);

    template<class Functor>
    static void OnNativeCall(Functor&& aCall)
    {
        if (aCall.IsTarget(&GLControllerSupport::CreateCompositor) ||
            aCall.IsTarget(&GLControllerSupport::PauseCompositor)) {

            // These calls are blocking.
            nsAppShell::SyncRunEvent(GeckoViewSupport::WindowEvent<Functor>(
                    mozilla::Move(aCall)), &GLControllerEvent::MakeEvent);
            return;

        } else if (aCall.IsTarget(
                &GLControllerSupport::SyncResumeResizeCompositor)) {
            // This call is synchronous. Perform the original call using a copy
            // of the lambda. Then redirect the original lambda to
            // OnResumedCompositor, to be run on the Gecko thread. We use
            // Functor instead of our own lambda so that Functor can handle
            // object lifetimes for us.
            (Functor(aCall))();
            aCall.SetTarget(&GLControllerSupport::OnResumedCompositor);

        } else if (aCall.IsTarget(
                &GLControllerSupport::SyncInvalidateAndScheduleComposite)) {
            // This call is synchronous.
            return aCall();
        }

        nsAppShell::PostEvent(
                mozilla::MakeUnique<GLControllerEvent>(
                mozilla::MakeUnique<GeckoViewSupport::WindowEvent<Functor>>(
                mozilla::Move(aCall))));
    }

    GLControllerSupport(nsWindow* aWindow,
                        const GLController::LocalRef& aInstance)
        : window(*aWindow)
        , mGLController(aInstance)
        , mCompositorPaused(true)
    {
        Reattach(aInstance);
    }

    ~GLControllerSupport()
    {
        GLController::GlobalRef glController(mozilla::Move(mGLController));
        nsAppShell::PostEvent([glController] {
            GLControllerSupport::DisposeNative(GLController::LocalRef(
                        jni::GetGeckoThreadEnv(), glController));
        });
    }

    void Reattach(const GLController::LocalRef& aInstance)
    {
        Base::AttachNative(aInstance, this);
    }

    const GeckoLayerClient::Ref& GetLayerClient() const
    {
        return mLayerClient;
    }

    bool CompositorPaused() const
    {
        return mCompositorPaused;
    }

    EGLSurface CreateEGLSurface()
    {
        static jfieldID eglSurfacePointerField;

        JNIEnv* const env = jni::GetEnvForThread();

        if (!eglSurfacePointerField) {
            AutoJNIClass egl(env, "com/google/android/gles_jni/EGLSurfaceImpl");
            // The pointer type moved to a 'long' in Android L, API version 20
            eglSurfacePointerField = egl.getField("mEGLSurface",
                    AndroidBridge::Bridge()->GetAPIVersion() >= 20 ? "J" : "I");
        }

        // Called on the compositor thread.
        auto eglSurface = mGLController->CreateEGLSurface();
        return reinterpret_cast<EGLSurface>(
                AndroidBridge::Bridge()->GetAPIVersion() >= 20 ?
                env->GetLongField(eglSurface.Get(), eglSurfacePointerField) :
                env->GetIntField(eglSurface.Get(), eglSurfacePointerField));
    }

private:
    void OnResumedCompositor(int32_t aWidth, int32_t aHeight)
    {
        // When we receive this, the compositor has already been told to
        // resume. (It turns out that waiting till we reach here to tell
        // the compositor to resume takes too long, resulting in a black
        // flash.) This means it's now safe for layer updates to occur.
        // Since we might have prevented one or more draw events from
        // occurring while the compositor was paused, we need to schedule
        // a draw event now.
        if (!mCompositorPaused) {
            window.RedrawAll();
        }
    }

    /**
     * GLController methods
     */
public:
    using Base::DisposeNative;

    void SetLayerClient(jni::Object::Param aClient)
    {
        const auto& layerClient = GeckoLayerClient::Ref::From(aClient);

        AndroidBridge::Bridge()->SetLayerClient(layerClient);

        // If resetting is true, Android destroyed our GeckoApp activity and we
        // had to recreate it, but all the Gecko-side things were not
        // destroyed.  We therefore need to link up the new java objects to
        // Gecko, and that's what we do here.
        const bool resetting = !!mLayerClient;
        mLayerClient = layerClient;

        if (resetting) {
            // Since we are re-linking the new java objects to Gecko, we need
            // to get the viewport from the compositor (since the Java copy was
            // thrown away) and we do that by setting the first-paint flag.
            if (window.mCompositorParent) {
                window.mCompositorParent->ForceIsFirstPaint();
            }
        }
    }

    void CreateCompositor(int32_t aWidth, int32_t aHeight)
    {
        window.CreateLayerManager(aWidth, aHeight);
        mCompositorPaused = false;
        OnResumedCompositor(aWidth, aHeight);
    }

    void PauseCompositor()
    {
        // The compositor gets paused when the app is about to go into the
        // background. While the compositor is paused, we need to ensure that
        // no layer tree updates (from draw events) occur, since the compositor
        // cannot make a GL context current in order to process updates.
        if (window.mCompositorChild) {
            window.mCompositorChild->SendPause();
        }
        mCompositorPaused = true;
    }

    void SyncPauseCompositor()
    {
        if (window.mCompositorParent) {
            window.mCompositorParent->SchedulePauseOnCompositorThread();
            mCompositorPaused = true;
        }
    }

    void SyncResumeCompositor()
    {
        if (window.mCompositorParent &&
                window.mCompositorParent->ScheduleResumeOnCompositorThread()) {
            mCompositorPaused = false;
        }
    }

    void SyncResumeResizeCompositor(int32_t aWidth, int32_t aHeight)
    {
        if (window.mCompositorParent && window.mCompositorParent->
                ScheduleResumeOnCompositorThread(aWidth, aHeight)) {
            mCompositorPaused = false;
        }
    }

    void SyncInvalidateAndScheduleComposite()
    {
        if (window.mCompositorParent) {
            window.mCompositorParent->InvalidateOnCompositorThread();
            window.mCompositorParent->ScheduleRenderOnCompositorThread();
        }
    }
};

nsWindow::GeckoViewSupport::~GeckoViewSupport()
{
    // Disassociate our GeckoEditable instance with our native object.
    // OnDestroy will call disposeNative after any pending native calls have
    // been made.
    MOZ_ASSERT(mEditable);
    mEditable->OnDestroy();
}

/* static */ void
nsWindow::GeckoViewSupport::Open(const jni::ClassObject::LocalRef& aCls,
                                 GeckoView::Window::Param aWindow,
                                 GeckoView::Param aView,
                                 jni::Object::Param aGLController,
                                 int32_t aWidth, int32_t aHeight)
{
    MOZ_ASSERT(NS_IsMainThread());

    PROFILER_LABEL("nsWindow", "GeckoViewSupport::Open",
                   js::ProfileEntry::Category::OTHER);

    nsCOMPtr<nsIWindowWatcher> ww = do_GetService(NS_WINDOWWATCHER_CONTRACTID);
    MOZ_ASSERT(ww);

    nsAdoptingCString url = Preferences::GetCString("toolkit.defaultChromeURI");
    if (!url) {
        url = NS_LITERAL_CSTRING("chrome://browser/content/browser.xul");
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

    nsCOMPtr<nsIDOMWindow> domWindow;
    ww->OpenWindow(nullptr, url, "_blank", "chrome,dialog=no,all",
                   args, getter_AddRefs(domWindow));
    MOZ_ASSERT(domWindow);

    nsCOMPtr<nsIWidget> widget = WidgetUtils::DOMWindowToWidget(domWindow);
    MOZ_ASSERT(widget);

    const auto window = static_cast<nsWindow*>(widget.get());

    // Attach a new GeckoView support object to the new window.
    window->mGeckoViewSupport  = mozilla::MakeUnique<GeckoViewSupport>(
            window, GeckoView::Window::LocalRef(aCls.Env(), aWindow), aView);

    // Attach the GLController to the new window.
    window->mGLControllerSupport = mozilla::MakeUnique<GLControllerSupport>(
            window, GLController::LocalRef(
            aCls.Env(), GLController::Ref::From(aGLController)));

    gGeckoViewWindow = window;
}

void
nsWindow::GeckoViewSupport::Close()
{
    nsIWidgetListener* const widgetListener = window.mWidgetListener;

    if (!widgetListener) {
        return;
    }

    nsCOMPtr<nsIXULWindow> xulWindow(widgetListener->GetXULWindow());
    // GeckoView-created top-level windows should be a XUL window.
    MOZ_ASSERT(xulWindow);

    nsCOMPtr<nsIBaseWindow> baseWindow(do_QueryInterface(xulWindow));
    MOZ_ASSERT(baseWindow);

    baseWindow->Destroy();
}

void
nsWindow::GeckoViewSupport::Reattach(GeckoView::Param aView)
{
    // Associate our previous GeckoEditable with the new GeckoView.
    mEditable->OnViewChange(aView);
}

void
nsWindow::InitNatives()
{
    nsWindow::GeckoViewSupport::Base::Init();
    nsWindow::GeckoViewSupport::EditableBase::Init();
    nsWindow::GLControllerSupport::Init();
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

    if (gGeckoViewWindow == this) {
        gGeckoViewWindow = nullptr;
    }
}

bool
nsWindow::IsTopLevel()
{
    return mWindowType == eWindowType_toplevel ||
        mWindowType == eWindowType_dialog ||
        mWindowType == eWindowType_invisible;
}

NS_IMETHODIMP
nsWindow::Create(nsIWidget* aParent,
                 nsNativeWidget aNativeParent,
                 const LayoutDeviceIntRect& aRect,
                 nsWidgetInitData* aInitData)
{
    ALOG("nsWindow[%p]::Create %p [%d %d %d %d]", (void*)this, (void*)aParent, aRect.x, aRect.y, aRect.width, aRect.height);
    nsWindow *parent = (nsWindow*) aParent;
    if (aNativeParent) {
        if (parent) {
            ALOG("Ignoring native parent on Android window [%p], since parent was specified (%p %p)", (void*)this, (void*)aNativeParent, (void*)aParent);
        } else {
            parent = (nsWindow*) aNativeParent;
        }
    }

    mBounds = aRect.ToUnknownRect();

    // for toplevel windows, bounds are fixed to full screen size
    if (!parent) {
        mBounds.x = 0;
        mBounds.y = 0;
        mBounds.width = gAndroidBounds.width;
        mBounds.height = gAndroidBounds.height;
    }

    BaseCreate(nullptr, LayoutDeviceIntRect::FromUnknownRect(mBounds),
               aInitData);

    NS_ASSERTION(IsTopLevel() || parent, "non top level windowdoesn't have a parent!");

    if (IsTopLevel()) {
        gTopLevelWindows.AppendElement(this);
    }

    if (parent) {
        parent->mChildren.AppendElement(this);
        mParent = parent;
    }

#ifdef DEBUG_ANDROID_WIDGET
    DumpWindows();
#endif

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Destroy(void)
{
    nsBaseWidget::mOnDestroyCalled = true;

    if (mGeckoViewSupport) {
        // Disassociate our native object with GeckoView.
        mGeckoViewSupport = nullptr;
    }

    while (mChildren.Length()) {
        // why do we still have children?
        ALOG("### Warning: Destroying window %p and reparenting child %p to null!", (void*)this, (void*)mChildren[0]);
        mChildren[0]->SetParent(nullptr);
    }

    if (IsTopLevel())
        gTopLevelWindows.RemoveElement(this);

    SetParent(nullptr);

    nsBaseWidget::OnDestroy();

#ifdef DEBUG_ANDROID_WIDGET
    DumpWindows();
#endif

    return NS_OK;
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

NS_IMETHODIMP
nsWindow::ReparentNativeWidget(nsIWidget *aNewParent)
{
    NS_PRECONDITION(aNewParent, "");
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
            // It just became visible, so send a resize update if necessary
            // and bring it to the front.
            Resize(0, 0, gAndroidBounds.width, gAndroidBounds.height, false);
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

NS_IMETHODIMP
nsWindow::SetModal(bool aState)
{
    ALOG("nsWindow[%p]::SetModal %d ignored", (void*)this, aState);

    return NS_OK;
}

bool
nsWindow::IsVisible() const
{
    return mIsVisible;
}

NS_IMETHODIMP
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

    return NS_OK;
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

    if (needSizeDispatch)
        OnSizeChanged(gfx::IntSize(aWidth, aHeight));

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

NS_IMETHODIMP
nsWindow::PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                      nsIWidget *aWidget,
                      bool aActivate)
{
    return NS_OK;
}

NS_IMETHODIMP
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
    return NS_OK;
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
    nsCOMPtr<nsIDOMWindow> existingTopWindow;
    fm->GetActiveWindow(getter_AddRefs(existingTopWindow));
    if (existingTopWindow && FindTopLevel() == nsWindow::TopWindow())
        return;

    if (!IsTopLevel()) {
        FindTopLevel()->BringToFront();
        return;
    }

    RefPtr<nsWindow> kungFuDeathGrip(this);

    nsWindow *oldTop = nullptr;
    nsWindow *newTop = this;
    if (!gTopLevelWindows.IsEmpty())
        oldTop = gTopLevelWindows[0];

    gTopLevelWindows.RemoveElement(this);
    gTopLevelWindows.InsertElementAt(0, this);

    if (oldTop) {
      nsIWidgetListener* listener = oldTop->GetWidgetListener();
      if (listener) {
          listener->WindowDeactivated();
      }
    }

    if (Destroyed()) {
        // somehow the deactivate event handler destroyed this window.
        // try to recover by grabbing the next window in line and activating
        // that instead
        if (gTopLevelWindows.IsEmpty())
            return;
        newTop = gTopLevelWindows[0];
    }

    if (mWidgetListener) {
        mWidgetListener->WindowActivated();
    }

    // force a window resize
    nsAppShell::Get()->ResendLastResizeEvent(newTop);
    RedrawAll();
}

NS_IMETHODIMP
nsWindow::GetScreenBounds(LayoutDeviceIntRect& aRect)
{
    LayoutDeviceIntPoint p = WidgetToScreenOffset();

    aRect.x = p.x;
    aRect.y = p.y;
    aRect.width = mBounds.width;
    aRect.height = mBounds.height;

    return NS_OK;
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

NS_IMETHODIMP
nsWindow::MakeFullScreen(bool aFullScreen, nsIScreen*)
{
    mIsFullScreen = aFullScreen;
    mAwaitingFullScreen = true;
    GeckoAppShell::SetFullScreen(aFullScreen);
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetWindowClass(const nsAString& xulWinType)
{
    return NS_OK;
}

mozilla::layers::LayerManager*
nsWindow::GetLayerManager(PLayerTransactionChild*, LayersBackend, LayerManagerPersistence,
                          bool* aAllowRetaining)
{
    if (aAllowRetaining) {
        *aAllowRetaining = true;
    }
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
nsWindow::OnGlobalAndroidEvent(AndroidGeckoEvent *ae)
{
    nsWindow *win = TopWindow();
    if (!win)
        return;

    switch (ae->Type()) {
        case AndroidGeckoEvent::FORCED_RESIZE:
            win->mBounds.width = 0;
            win->mBounds.height = 0;
            // also resize the children
            for (uint32_t i = 0; i < win->mChildren.Length(); i++) {
                win->mChildren[i]->mBounds.width = 0;
                win->mChildren[i]->mBounds.height = 0;
            }
        case AndroidGeckoEvent::SIZE_CHANGED: {
            const nsTArray<nsIntPoint>& points = ae->Points();
            NS_ASSERTION(points.Length() == 2, "Size changed does not have enough coordinates");

            int nw = points[0].x;
            int nh = points[0].y;

            if (ae->Type() == AndroidGeckoEvent::FORCED_RESIZE || nw != gAndroidBounds.width ||
                nh != gAndroidBounds.height) {
                gAndroidBounds.width = nw;
                gAndroidBounds.height = nh;

                // tell all the windows about the new size
                for (size_t i = 0; i < gTopLevelWindows.Length(); ++i) {
                    if (gTopLevelWindows[i]->mIsVisible)
                        gTopLevelWindows[i]->Resize(gAndroidBounds.width,
                                                    gAndroidBounds.height,
                                                    false);
                }
            }

            int newScreenWidth = points[1].x;
            int newScreenHeight = points[1].y;

            if (newScreenWidth == gAndroidScreenBounds.width &&
                newScreenHeight == gAndroidScreenBounds.height)
                break;

            gAndroidScreenBounds.width = newScreenWidth;
            gAndroidScreenBounds.height = newScreenHeight;

            if (!XRE_IsParentProcess() &&
                !Preferences::GetBool("browser.tabs.remote.desktopbehavior", false)) {
                break;
            }

            // Tell the content process the new screen size.
            nsTArray<ContentParent*> cplist;
            ContentParent::GetAll(cplist);
            for (uint32_t i = 0; i < cplist.Length(); ++i)
                Unused << cplist[i]->SendScreenSizeChanged(gAndroidScreenBounds);

            if (gContentCreationNotifier)
                break;

            // If the content process is not created yet, wait until it's
            // created and then tell it the screen size.
            nsCOMPtr<nsIObserverService> obs =
                mozilla::services::GetObserverService();
            if (!obs)
                break;

            RefPtr<ContentCreationNotifier> notifier = new ContentCreationNotifier;
            if (NS_SUCCEEDED(obs->AddObserver(notifier, "ipc:content-created", false))) {
                if (NS_SUCCEEDED(obs->AddObserver(notifier, "xpcom-shutdown", false)))
                    gContentCreationNotifier = notifier;
                else
                    obs->RemoveObserver(notifier, "ipc:content-created");
            }
            break;
        }

        case AndroidGeckoEvent::APZ_INPUT_EVENT: {
            win->UserActivity();

            WidgetTouchEvent touchEvent = ae->MakeTouchEvent(win);
            win->ProcessUntransformedAPZEvent(&touchEvent, ae->ApzGuid(), ae->ApzInputBlockId(), ae->ApzEventStatus());
            win->DispatchHitTest(touchEvent);
            break;
        }
        case AndroidGeckoEvent::MOTION_EVENT: {
            win->UserActivity();
            bool preventDefaultActions = win->OnMultitouchEvent(ae);
            if (!preventDefaultActions && ae->Count() < 2) {
                win->OnMouseEvent(ae);
            }
            break;
        }

        // LongPress events mostly trigger contextmenu options, but can also lead to
        // textSelection processing.
        case AndroidGeckoEvent::LONG_PRESS: {
            win->UserActivity();

            nsCOMPtr<nsIObserverService> obsServ = mozilla::services::GetObserverService();
            obsServ->NotifyObservers(nullptr, "before-build-contextmenu", nullptr);

            // Send the contextmenu event to Gecko.
            if (!win->OnContextmenuEvent(ae)) {
                // If not consumed, continue as a LongTap, possibly trigger
                // Gecko Text Selection Carets.
                win->OnLongTapEvent(ae);
            }
            break;
        }

        case AndroidGeckoEvent::NATIVE_GESTURE_EVENT: {
            win->OnNativeGestureEvent(ae);
            break;
        }
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
}

void
nsWindow::InitEvent(WidgetGUIEvent& event, LayoutDeviceIntPoint* aPoint)
{
    if (aPoint) {
        event.refPoint = *aPoint;
    } else {
        event.refPoint.x = 0;
        event.refPoint.y = 0;
    }

    event.time = PR_Now() / 1000;
}

gfx::IntSize
nsWindow::GetAndroidScreenBounds()
{
    if (XRE_IsContentProcess()) {
        return ContentChild::GetSingleton()->GetScreenSize();
    }
    return gAndroidScreenBounds;
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

        case NS_RAW_NATIVE_IME_CONTEXT:
            // We assume that there is only one context per process on Android
            return NS_ONLY_ONE_NATIVE_IME_CONTEXT;

        case NS_NATIVE_NEW_EGL_SURFACE:
            if (!mGLControllerSupport) {
                return nullptr;
            }
            return static_cast<void*>(mGLControllerSupport->CreateEGLSurface());
    }

    return nullptr;
}

void
nsWindow::OnMouseEvent(AndroidGeckoEvent *ae)
{
    RefPtr<nsWindow> kungFuDeathGrip(this);

    WidgetMouseEvent event = ae->MakeMouseEvent(this);
    if (event.mMessage == eVoidEvent) {
        // invalid event type, abort
        return;
    }

    DispatchEvent(&event);
}

bool
nsWindow::OnContextmenuEvent(AndroidGeckoEvent *ae)
{
    RefPtr<nsWindow> kungFuDeathGrip(this);

    CSSPoint pt;
    const nsTArray<nsIntPoint>& points = ae->Points();
    if (points.Length() > 0) {
        pt = CSSPoint(points[0].x, points[0].y);
    }

    // Send the contextmenu event.
    WidgetMouseEvent contextMenuEvent(true, eContextMenu, this,
                                      WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);
    contextMenuEvent.refPoint =
        RoundedToInt(pt * GetDefaultScale()) - WidgetToScreenOffset();
    contextMenuEvent.ignoreRootScrollFrame = true;
    contextMenuEvent.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;

    nsEventStatus contextMenuStatus;
    DispatchEvent(&contextMenuEvent, contextMenuStatus);

    // If the contextmenu event was consumed (preventDefault issued), we follow with a
    // touchcancel event. This avoids followup touchend events passsing through and
    // triggering further element behaviour such as link-clicks.
    if (contextMenuStatus == nsEventStatus_eConsumeNoDefault) {
        WidgetTouchEvent canceltouchEvent = ae->MakeTouchEvent(this);
        canceltouchEvent.mMessage = eTouchCancel;
        DispatchEvent(&canceltouchEvent);
        return true;
    }

    return false;
}

void
nsWindow::OnLongTapEvent(AndroidGeckoEvent *ae)
{
    RefPtr<nsWindow> kungFuDeathGrip(this);

    CSSPoint pt;
    const nsTArray<nsIntPoint>& points = ae->Points();
    if (points.Length() > 0) {
        pt = CSSPoint(points[0].x, points[0].y);
    }

    // Send the LongTap event to Gecko.
    WidgetMouseEvent event(true, eMouseLongTap, this,
        WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);
    event.button = WidgetMouseEvent::eLeftButton;
    event.refPoint =
        RoundedToInt(pt * GetDefaultScale()) - WidgetToScreenOffset();
    event.clickCount = 1;
    event.time = ae->Time();
    event.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
    event.ignoreRootScrollFrame = true;

    DispatchEvent(&event);
}

void
nsWindow::DispatchHitTest(const WidgetTouchEvent& aEvent)
{
    if (aEvent.mMessage == eTouchStart && aEvent.touches.Length() == 1) {
        // Since touch events don't get retargeted by PositionedEventTargeting.cpp
        // code on Fennec, we dispatch a dummy mouse event that *does* get
        // retargeted. The Fennec browser.js code can use this to activate the
        // highlight element in case the this touchstart is the start of a tap.
        WidgetMouseEvent hittest(true, eMouseHitTest, this,
                                 WidgetMouseEvent::eReal);
        hittest.refPoint = aEvent.touches[0]->mRefPoint;
        hittest.ignoreRootScrollFrame = true;
        hittest.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
        nsEventStatus status;
        DispatchEvent(&hittest, status);

        if (mAPZEventState && hittest.hitCluster) {
            mAPZEventState->ProcessClusterHit();
        }
    }
}

bool nsWindow::OnMultitouchEvent(AndroidGeckoEvent *ae)
{
    RefPtr<nsWindow> kungFuDeathGrip(this);

    // End any composition in progress in case the touch event listener
    // modifies the input field value (see bug 856155)
    RemoveIMEComposition();

    // This is set to true once we have called SetPreventPanning() exactly
    // once for a given sequence of touch events. It is reset on the start
    // of the next sequence.
    static bool sDefaultPreventedNotified = false;
    static bool sLastWasDownEvent = false;

    bool preventDefaultActions = false;
    bool isDownEvent = false;

    WidgetTouchEvent event = ae->MakeTouchEvent(this);
    if (event.mMessage != eVoidEvent) {
        nsEventStatus status;
        DispatchEvent(&event, status);
        // We check mMultipleActionsPrevented because that's what <input type=range>
        // sets when someone starts dragging the thumb. It doesn't set the status
        // because it doesn't want to prevent the code that gives the input focus
        // from running.
        preventDefaultActions = (status == nsEventStatus_eConsumeNoDefault ||
                                event.mFlags.mMultipleActionsPrevented);
        isDownEvent = (event.mMessage == eTouchStart);
    }

    DispatchHitTest(event);

    // if the last event we got was a down event, then by now we know for sure whether
    // this block has been default-prevented or not. if we haven't already sent the
    // notification for this block, do so now.
    if (sLastWasDownEvent && !sDefaultPreventedNotified) {
        // if this event is a down event, that means it's the start of a new block, and the
        // previous block should not be default-prevented
        bool defaultPrevented = isDownEvent ? false : preventDefaultActions;
        if (ae->Type() == AndroidGeckoEvent::APZ_INPUT_EVENT) {
            widget::android::AndroidContentController::NotifyDefaultPrevented(ae->ApzInputBlockId(), defaultPrevented);
        } else {
            GeckoAppShell::NotifyDefaultPrevented(defaultPrevented);
        }
        sDefaultPreventedNotified = true;
    }

    // now, if this event is a down event, then we might already know that it has been
    // default-prevented. if so, we send the notification right away; otherwise we wait
    // for the next event.
    if (isDownEvent) {
        if (preventDefaultActions) {
            if (ae->Type() == AndroidGeckoEvent::APZ_INPUT_EVENT) {
                widget::android::AndroidContentController::NotifyDefaultPrevented(ae->ApzInputBlockId(), true);
            } else {
                GeckoAppShell::NotifyDefaultPrevented(true);
            }
            sDefaultPreventedNotified = true;
        } else {
            sDefaultPreventedNotified = false;
        }
    }
    sLastWasDownEvent = isDownEvent;

    return preventDefaultActions;
}

void
nsWindow::OnNativeGestureEvent(AndroidGeckoEvent *ae)
{
    LayoutDeviceIntPoint pt(ae->Points()[0].x,
                            ae->Points()[0].y);
    double delta = ae->X();
    EventMessage msg = eVoidEvent;

    switch (ae->Action()) {
        case AndroidMotionEvent::ACTION_MAGNIFY_START:
            msg = eMagnifyGestureStart;
            mStartDist = delta;
            mLastDist = delta;
            break;
        case AndroidMotionEvent::ACTION_MAGNIFY:
            msg = eMagnifyGestureUpdate;
            delta -= mLastDist;
            mLastDist += delta;
            break;
        case AndroidMotionEvent::ACTION_MAGNIFY_END:
            msg = eMagnifyGesture;
            delta -= mStartDist;
            break;
        default:
            return;
    }

    RefPtr<nsWindow> kungFuDeathGrip(this);

    WidgetSimpleGestureEvent event(true, msg, this);

    event.direction = 0;
    event.delta = delta;
    event.modifiers = 0;
    event.time = ae->Time();
    event.refPoint = pt;

    DispatchEvent(&event);
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
    using mozilla::widget::sdk::KeyEvent;
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
    using mozilla::widget::sdk::KeyEvent;
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

    event.modifiers = GetModifiers(metaState);

    if (event.mMessage == eKeyPress) {
        // Android gives us \n, so filter out some control characters.
        event.isChar = (charCode >= ' ');
        event.charCode = event.isChar ? charCode : 0;
        event.keyCode = event.isChar ? 0 : domKeyCode;
        event.mPluginEvent.Clear();

        // For keypress, if the unicode char already has modifiers applied, we
        // don't specify extra modifiers. If UnicodeChar() != BaseUnicodeChar()
        // it means UnicodeChar() already has modifiers applied.
        // Note that on Android 4.x, Alt modifier isn't set when the key input
        // causes text input even while right Alt key is pressed.  However,
        // this is necessary for Android 2.3 compatibility.
        if (unicodeChar && unicodeChar != baseUnicodeChar) {
            event.modifiers &= ~(MODIFIER_ALT | MODIFIER_CONTROL
                                              | MODIFIER_META);
        }

    } else {
        event.isChar = false;
        event.charCode = 0;
        event.keyCode = domKeyCode;

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

    event.location =
        WidgetKeyboardEvent::ComputeLocationFromCodeValue(event.mCodeNameIndex);
    event.time = time;
}

void
nsWindow::GeckoViewSupport::OnKeyEvent(int32_t aAction, int32_t aKeyCode,
        int32_t aScanCode, int32_t aMetaState, int64_t aTime,
        int32_t aUnicodeChar, int32_t aBaseUnicodeChar,
        int32_t aDomPrintableKeyValue, int32_t aRepeatCount, int32_t aFlags,
        bool aIsSynthesizedImeKey)
{
    RefPtr<nsWindow> kungFuDeathGrip(&window);
    window.UserActivity();
    window.RemoveIMEComposition();

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
    }

    if (window.Destroyed() ||
            status == nsEventStatus_eConsumeNoDefault ||
            msg != eKeyDown || IsModifierKey(aKeyCode)) {
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
    MOZ_ASSERT(downEvent.keyCode == 0);
    window.DispatchEvent(&downEvent);

    WidgetKeyboardEvent upEvent(true, eKeyUp, &window);
    window.InitEvent(upEvent, nullptr);
    MOZ_ASSERT(upEvent.keyCode == 0);
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
nsWindow::GeckoViewSupport::PostFlushIMEChanges(FlushChangesFlag aFlags)
{
    if (aFlags != FLUSH_FLAG_RETRY &&
            (!mIMETextChanges.IsEmpty() || mIMESelectionChanged)) {
        // Already posted
        return;
    }

    // Keep a strong reference to the window to keep 'this' alive.
    RefPtr<nsWindow> window(&this->window);

    nsAppShell::PostEvent([this, window, aFlags] {
        if (!window->Destroyed()) {
            FlushIMEChanges(aFlags);
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
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(IMEStateManager::GetFocusSelectionAndRoot(
            getter_AddRefs(imeSelection), getter_AddRefs(imeRoot))));

    RefPtr<nsWindow> kungFuDeathGrip(&window);
    window.UserActivity();

    struct TextRecord {
        nsString text;
        int32_t start;
        int32_t oldEnd;
        int32_t newEnd;
    };
    nsAutoTArray<TextRecord, 4> textTransaction;
    if (mIMETextChanges.Length() > textTransaction.Capacity()) {
        textTransaction.SetCapacity(mIMETextChanges.Length());
    }

    mIMETextChangedDuringFlush = false;

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

        if (mIMETextChangedDuringFlush) {
            // The query event above could have triggered more text changes to
            // come in, as indicated by our flag. If that happens, try flushing
            // IME changes again later.
            if (!NS_WARN_IF(aFlags == FLUSH_FLAG_RETRY)) {
                // Don't retry if already retrying, to avoid infinite loops.
                PostFlushIMEChanges(FLUSH_FLAG_RETRY);
            }
            return;
        }

        textTransaction.AppendElement(
                TextRecord{event.mReply.mString, change.mStart,
                           change.mOldEnd, change.mNewEnd});
    }

    mIMETextChanges.Clear();

    for (const TextRecord& record : textTransaction) {
        mEditable->OnTextChange(record.text, record.start,
                                record.oldEnd, record.newEnd);
    }

    if (mIMESelectionChanged) {
        mIMESelectionChanged = false;

        WidgetQueryContentEvent event(true, eQuerySelectedText, &window);
        window.InitEvent(event, nullptr);
        window.DispatchEvent(&event);

        NS_ENSURE_TRUE_VOID(event.mSucceeded);
        NS_ENSURE_TRUE_VOID(event.mReply.mContentsRoot == imeRoot.get());

        mEditable->OnSelectionChange(int32_t(event.GetSelectionStart()),
                                     int32_t(event.GetSelectionEnd()));
    }
}

bool
nsWindow::GeckoViewSupport::NotifyIME(const IMENotification& aIMENotification)
{
    MOZ_ASSERT(mEditable);

    switch (aIMENotification.mMessage) {
        case REQUEST_TO_COMMIT_COMPOSITION: {
            ALOGIME("IME: REQUEST_TO_COMMIT_COMPOSITION");
            window.RemoveIMEComposition();
            mEditable->NotifyIME(REQUEST_TO_COMMIT_COMPOSITION);
            return true;
        }

        case REQUEST_TO_CANCEL_COMPOSITION: {
            ALOGIME("IME: REQUEST_TO_CANCEL_COMPOSITION");

            // Cancel composition on Gecko side
            if (window.GetIMEComposition()) {
                RefPtr<nsWindow> kungFuDeathGrip(&window);
                WidgetCompositionEvent compositionCommitEvent(
                        true, eCompositionCommit, &window);
                window.InitEvent(compositionCommitEvent, nullptr);
                // Dispatch it with empty mData value for canceling composition.
                window.DispatchEvent(&compositionCommitEvent);
            }

            mEditable->NotifyIME(REQUEST_TO_CANCEL_COMPOSITION);
            return true;
        }

        case NOTIFY_IME_OF_FOCUS: {
            ALOGIME("IME: NOTIFY_IME_OF_FOCUS");
            mEditable->NotifyIME(NOTIFY_IME_OF_FOCUS);
            return true;
        }

        case NOTIFY_IME_OF_BLUR: {
            ALOGIME("IME: NOTIFY_IME_OF_BLUR");

            // Mask events because we lost focus. On the next focus event,
            // Gecko will notify Java, and Java will send an acknowledge focus
            // event back to Gecko. That is where we unmask event handling
            mIMEMaskEventsCount++;

            mEditable->NotifyIME(NOTIFY_IME_OF_BLUR);
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

    if (--mIMEMaskEventsCount > 0) {
        // Still not focused; reply to events, but don't do anything else.
        return OnImeSynchronize();
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
    OnImeSynchronize();
}

void
nsWindow::GeckoViewSupport::OnImeReplaceText(int32_t aStart, int32_t aEnd,
                                             jni::String::Param aText)
{
    if (mIMEMaskEventsCount > 0) {
        // Not focused; still reply to events, but don't do anything else.
        return OnImeSynchronize();
    }

    /*
        Replace text in Gecko thread from aStart to aEnd with the string text.
    */
    RefPtr<nsWindow> kungFuDeathGrip(&window);
    nsString string(aText);

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
            WidgetSelectionEvent event(true, eSetSelection, &window);
            window.InitEvent(event, nullptr);
            event.mOffset = uint32_t(aStart);
            event.mLength = uint32_t(aEnd - aStart);
            event.mExpandToClusterBoundary = false;
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
                event->widget = &window;
                window.DispatchEvent(event, status);
            }
            mIMEKeyEvents.Clear();
            return OnImeSynchronize();
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
            range.mRangeType = NS_TEXTRANGE_RAWINPUT;
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
    OnImeSynchronize();
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
    range.mRangeType = aRangeType;
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
    if (mIMEMaskEventsCount > 0) {
        // Not focused.
        return;
    }

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
    RefPtr<nsWindow> kungFuDeathGrip(&window);
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
#ifdef MOZ_B2GDROID
    // Disable the Android keyboard on b2gdroid.
    return;
#endif

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
    return nsIMEUpdatePreference(
        nsIMEUpdatePreference::NOTIFY_SELECTION_CHANGE |
        nsIMEUpdatePreference::NOTIFY_TEXT_CHANGE);
}

void
nsWindow::DrawWindowUnderlay(LayerManagerComposite* aManager,
                             LayoutDeviceIntRect aRect)
{
    MOZ_ASSERT(mGLControllerSupport);
    GeckoLayerClient::LocalRef client = mGLControllerSupport->GetLayerClient();

    LayerRenderer::Frame::LocalRef frame = client->CreateFrame();
    mLayerRendererFrame = frame;
    if (NS_WARN_IF(!mLayerRendererFrame)) {
        return;
    }

    if (!WidgetPaintsBackground()) {
        return;
    }

    CompositorOGL *compositor = static_cast<CompositorOGL*>(aManager->GetCompositor());
    compositor->ResetProgram();
    gl::GLContext* gl = compositor->gl();
    bool scissorEnabled = gl->fIsEnabled(LOCAL_GL_SCISSOR_TEST);
    GLint scissorRect[4];
    gl->fGetIntegerv(LOCAL_GL_SCISSOR_BOX, scissorRect);

    client->ActivateProgram();
    frame->BeginDrawing();
    frame->DrawBackground();
    client->DeactivateProgramAndRestoreState(scissorEnabled,
        scissorRect[0], scissorRect[1], scissorRect[2], scissorRect[3]);
}

void
nsWindow::DrawWindowOverlay(LayerManagerComposite* aManager,
                            LayoutDeviceIntRect aRect)
{
    PROFILER_LABEL("nsWindow", "DrawWindowOverlay",
        js::ProfileEntry::Category::GRAPHICS);

    if (NS_WARN_IF(!mLayerRendererFrame)) {
        return;
    }

    CompositorOGL *compositor = static_cast<CompositorOGL*>(aManager->GetCompositor());
    compositor->ResetProgram();
    gl::GLContext* gl = compositor->gl();
    bool scissorEnabled = gl->fIsEnabled(LOCAL_GL_SCISSOR_TEST);
    GLint scissorRect[4];
    gl->fGetIntegerv(LOCAL_GL_SCISSOR_BOX, scissorRect);

    MOZ_ASSERT(mGLControllerSupport);
    GeckoLayerClient::LocalRef client = mGLControllerSupport->GetLayerClient();

    client->ActivateProgram();
    mLayerRendererFrame->DrawForeground();
    mLayerRendererFrame->EndDrawing();
    client->DeactivateProgramAndRestoreState(scissorEnabled,
        scissorRect[0], scissorRect[1], scissorRect[2], scissorRect[3]);
    mLayerRendererFrame = nullptr;
}

// off-main-thread compositor fields and functions

StaticRefPtr<mozilla::layers::APZCTreeManager> nsWindow::sApzcTreeManager;

void
nsWindow::InvalidateAndScheduleComposite()
{
    if (gGeckoViewWindow && gGeckoViewWindow->mGLControllerSupport) {
        gGeckoViewWindow->mGLControllerSupport->
                SyncInvalidateAndScheduleComposite();
    }
}

bool
nsWindow::IsCompositionPaused()
{
    if (gGeckoViewWindow && gGeckoViewWindow->mGLControllerSupport) {
        return gGeckoViewWindow->mGLControllerSupport->CompositorPaused();
    }
    return false;
}

void
nsWindow::SchedulePauseComposition()
{
    if (gGeckoViewWindow && gGeckoViewWindow->mGLControllerSupport) {
        return gGeckoViewWindow->mGLControllerSupport->SyncPauseCompositor();
    }
}

void
nsWindow::ScheduleResumeComposition()
{
    if (gGeckoViewWindow && gGeckoViewWindow->mGLControllerSupport) {
        return gGeckoViewWindow->mGLControllerSupport->SyncResumeCompositor();
    }
}

float
nsWindow::ComputeRenderIntegrity()
{
    if (gGeckoViewWindow && gGeckoViewWindow->mCompositorParent) {
        return gGeckoViewWindow->mCompositorParent->ComputeRenderIntegrity();
    }

    return 1.f;
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
    if (!mGLControllerSupport || mGLControllerSupport->CompositorPaused() ||
            // FindTopLevel() != nsWindow::TopWindow() ||
            !GetLayerManager(nullptr)) {
        return false;
    }
    return nsIWidget::NeedsPaint();
}

CompositorParent*
nsWindow::NewCompositorParent(int aSurfaceWidth, int aSurfaceHeight)
{
    return new CompositorParent(this, true, aSurfaceWidth, aSurfaceHeight);
}

mozilla::layers::APZCTreeManager*
nsWindow::GetAPZCTreeManager()
{
    return sApzcTreeManager;
}

void
nsWindow::ConfigureAPZCTreeManager()
{
    nsBaseWidget::ConfigureAPZCTreeManager();
    if (!sApzcTreeManager) {
        sApzcTreeManager = mAPZC;
    }
}

void
nsWindow::ConfigureAPZControllerThread()
{
    APZThreadUtils::SetControllerThread(nullptr);
}

already_AddRefed<GeckoContentController>
nsWindow::CreateRootContentController()
{
    RefPtr<GeckoContentController> controller = new widget::android::AndroidContentController(this, mAPZEventState, mAPZC);
    return controller.forget();
}

uint64_t
nsWindow::RootLayerTreeId()
{
    MOZ_ASSERT(gGeckoViewWindow && gGeckoViewWindow->mCompositorParent);
    return gGeckoViewWindow->mCompositorParent->RootLayerTreeId();
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
#ifdef MOZ_ANDROID_APZ
    nsBaseWidget::UpdateZoomConstraints(aPresShellId, aViewId, aConstraints);
#else
    if (!aConstraints) {
        // This is intended to "clear" previously stored constraints but in our
        // case we don't need to bother since they'll get GC'd from browser.js
        return;
    }
    nsIContent* content = nsLayoutUtils::FindContentFor(aViewId);
    nsIDocument* doc = content ? content->GetComposedDoc() : nullptr;
    if (!doc) {
        return;
    }
    nsCOMPtr<nsIObserverService> obsServ = mozilla::services::GetObserverService();
    nsPrintfCString json("{ \"allowZoom\": %s,"
                         "  \"allowDoubleTapZoom\": %s,"
                         "  \"minZoom\": %f,"
                         "  \"maxZoom\": %f }",
        aConstraints->mAllowZoom ? "true" : "false",
        aConstraints->mAllowDoubleTapZoom ? "true" : "false",
        aConstraints->mMinZoom.scale, aConstraints->mMaxZoom.scale);
    obsServ->NotifyObservers(doc, "zoom-constraints-updated",
        NS_ConvertASCIItoUTF16(json.get()).get());
#endif
}
