/* -*- Mode: c++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <android/log.h>
#include <math.h>
#include <unistd.h>

#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/unused.h"
#include "mozilla/Preferences.h"
#include "mozilla/layers/RenderTrace.h"

using mozilla::dom::ContentParent;
using mozilla::dom::ContentChild;
using mozilla::unused;

#include "nsAppShell.h"
#include "nsIdleService.h"
#include "nsWindow.h"
#include "nsIObserverService.h"
#include "nsFocusManager.h"

#include "nsRenderingContext.h"
#include "nsIDOMSimpleGestureEvent.h"
#include "nsDOMTouchEvent.h"

#include "nsGkAtoms.h"
#include "nsWidgetsCID.h"
#include "nsGfxCIID.h"

#include "gfxImageSurface.h"
#include "gfxContext.h"

#include "Layers.h"
#include "BasicLayers.h"
#include "LayerManagerOGL.h"
#include "GLContext.h"
#include "GLContextProvider.h"

#include "nsTArray.h"

#include "AndroidBridge.h"
#include "android_npapi.h"

#include "imgIEncoder.h"

#include "nsStringGlue.h"

using namespace mozilla;
using namespace mozilla::widget;

NS_IMPL_ISUPPORTS_INHERITED0(nsWindow, nsBaseWidget)

// The dimensions of the current android view
static gfxIntSize gAndroidBounds = gfxIntSize(0, 0);
static gfxIntSize gAndroidScreenBounds;

#ifdef MOZ_JAVA_COMPOSITOR
#include "mozilla/layers/CompositorChild.h"
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/Mutex.h"
#include "nsThreadUtils.h"
#endif


class ContentCreationNotifier;
static nsCOMPtr<ContentCreationNotifier> gContentCreationNotifier;
// A helper class to send updates when content processes
// are created. Currently an update for the screen size is sent.
class ContentCreationNotifier : public nsIObserver
{
    NS_DECL_ISUPPORTS

    NS_IMETHOD Observe(nsISupports* aSubject,
                       const char* aTopic,
                       const PRUnichar* aData)
    {
        if (!strcmp(aTopic, "ipc:content-created")) {
            nsCOMPtr<nsIObserver> cpo = do_QueryInterface(aSubject);
            ContentParent* cp = static_cast<ContentParent*>(cpo.get());
            unused << cp->SendScreenSizeChanged(gAndroidScreenBounds);
        } else if (!strcmp(aTopic, "xpcom-shutdown")) {
            nsCOMPtr<nsIObserverService>
                obs(do_GetService("@mozilla.org/observer-service;1"));
            if (obs) {
                obs->RemoveObserver(static_cast<nsIObserver*>(this),
                                    "xpcom-shutdown");
                obs->RemoveObserver(static_cast<nsIObserver*>(this),
                                    "ipc:content-created");
            }
            gContentCreationNotifier = nsnull;
        }

        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS1(ContentCreationNotifier,
                   nsIObserver)

static bool gMenu;
static bool gMenuConsumed;

// All the toplevel windows that have been created; these are in
// stacking order, so the window at gAndroidBounds[0] is the topmost
// one.
static nsTArray<nsWindow*> gTopLevelWindows;

static nsRefPtr<gl::GLContext> sGLContext;
static bool sFailedToCreateGLContext = false;
static bool sValidSurface;
static bool sSurfaceExists = false;
static void *sNativeWindow = nsnull;

// Multitouch swipe thresholds in inches
static const double SWIPE_MAX_PINCH_DELTA_INCHES = 0.4;
static const double SWIPE_MIN_DISTANCE_INCHES = 0.6;

nsWindow*
nsWindow::TopWindow()
{
    if (!gTopLevelWindows.IsEmpty())
        return gTopLevelWindows[0];
    return nsnull;
}

void
nsWindow::LogWindow(nsWindow *win, int index, int indent)
{
    char spaces[] = "                    ";
    spaces[indent < 20 ? indent : 20] = 0;
    ALOG("%s [% 2d] 0x%08x [parent 0x%08x] [% 3d,% 3dx% 3d,% 3d] vis %d type %d",
         spaces, index, (intptr_t)win, (intptr_t)win->mParent,
         win->mBounds.x, win->mBounds.y,
         win->mBounds.width, win->mBounds.height,
         win->mIsVisible, win->mWindowType);
}

void
nsWindow::DumpWindows()
{
    DumpWindows(gTopLevelWindows);
}

void
nsWindow::DumpWindows(const nsTArray<nsWindow*>& wins, int indent)
{
    for (PRUint32 i = 0; i < wins.Length(); ++i) {
        nsWindow *w = wins[i];
        LogWindow(w, i, indent);
        DumpWindows(w->mChildren, indent+1);
    }
}

nsWindow::nsWindow() :
    mIsVisible(false),
    mParent(nsnull),
    mFocus(nsnull),
    mIMEComposing(false)
{
}

nsWindow::~nsWindow()
{
    gTopLevelWindows.RemoveElement(this);
    nsWindow *top = FindTopLevel();
    if (top->mFocus == this)
        top->mFocus = nsnull;
    ALOG("nsWindow %p destructor", (void*)this);
#ifdef MOZ_JAVA_COMPOSITOR
    SetCompositor(NULL, NULL, NULL);
#endif
}

bool
nsWindow::IsTopLevel()
{
    return mWindowType == eWindowType_toplevel ||
        mWindowType == eWindowType_dialog ||
        mWindowType == eWindowType_invisible;
}

NS_IMETHODIMP
nsWindow::Create(nsIWidget *aParent,
                 nsNativeWidget aNativeParent,
                 const nsIntRect &aRect,
                 EVENT_CALLBACK aHandleEventFunction,
                 nsDeviceContext *aContext,
                 nsWidgetInitData *aInitData)
{
    ALOG("nsWindow[%p]::Create %p [%d %d %d %d]", (void*)this, (void*)aParent, aRect.x, aRect.y, aRect.width, aRect.height);
    nsWindow *parent = (nsWindow*) aParent;

    if (!AndroidBridge::Bridge()) {
        aNativeParent = nsnull;
    }

    if (aNativeParent) {
        if (parent) {
            ALOG("Ignoring native parent on Android window [%p], since parent was specified (%p %p)", (void*)this, (void*)aNativeParent, (void*)aParent);
        } else {
            parent = (nsWindow*) aNativeParent;
        }
    }

    mBounds = aRect;

    // for toplevel windows, bounds are fixed to full screen size
    if (!parent) {
        mBounds.x = 0;
        mBounds.y = 0;
        mBounds.width = gAndroidBounds.width;
        mBounds.height = gAndroidBounds.height;
    }

    BaseCreate(nsnull, mBounds, aHandleEventFunction, aContext, aInitData);

    NS_ASSERTION(IsTopLevel() || parent, "non top level windowdoesn't have a parent!");

    if (IsTopLevel()) {
        gTopLevelWindows.AppendElement(this);
    }

    if (parent) {
        parent->mChildren.AppendElement(this);
        mParent = parent;
    }

    float dpi = GetDPI();
    mSwipeMaxPinchDelta = SWIPE_MAX_PINCH_DELTA_INCHES * dpi;
    mSwipeMinDistance = SWIPE_MIN_DISTANCE_INCHES * dpi;

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Destroy(void)
{
    nsBaseWidget::mOnDestroyCalled = true;

    while (mChildren.Length()) {
        // why do we still have children?
        ALOG("### Warning: Destroying window %p and reparenting child %p to null!", (void*)this, (void*)mChildren[0]);
        mChildren[0]->SetParent(nsnull);
    }

    if (IsTopLevel())
        gTopLevelWindows.RemoveElement(this);

    SetParent(nsnull);

    nsBaseWidget::OnDestroy();

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::ConfigureChildren(const nsTArray<nsIWidget::Configuration>& config)
{
    for (PRUint32 i = 0; i < config.Length(); ++i) {
        nsWindow *childWin = (nsWindow*) config[i].mChild;
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
    nsIntRect entireRect(0, 0, gAndroidBounds.width, gAndroidBounds.height);
    AndroidGeckoEvent *event = new AndroidGeckoEvent(AndroidGeckoEvent::DRAW, entireRect);
    nsAppShell::gAppShell->PostEvent(event);
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

        // XXX we should synthesize a NS_MOUSE_EXIT (for old top
        // window)/NS_MOUSE_ENTER (for new top window) since we need
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

NS_IMETHODIMP
nsWindow::IsVisible(bool& aState)
{
    aState = mIsVisible;
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::ConstrainPosition(bool aAllowSlop,
                            PRInt32 *aX,
                            PRInt32 *aY)
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
nsWindow::Move(PRInt32 aX,
               PRInt32 aY)
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
nsWindow::Resize(PRInt32 aWidth,
                 PRInt32 aHeight,
                 bool aRepaint)
{
    return Resize(mBounds.x,
                  mBounds.y,
                  aWidth,
                  aHeight,
                  aRepaint);
}

NS_IMETHODIMP
nsWindow::Resize(PRInt32 aX,
                 PRInt32 aY,
                 PRInt32 aWidth,
                 PRInt32 aHeight,
                 bool aRepaint)
{
    ALOG("nsWindow[%p]::Resize [%d %d %d %d] (repaint %d)", (void*)this, aX, aY, aWidth, aHeight, aRepaint);

    bool needSizeDispatch = aWidth != mBounds.width || aHeight != mBounds.height;

    mBounds.x = aX;
    mBounds.y = aY;
    mBounds.width = aWidth;
    mBounds.height = aHeight;

    if (needSizeDispatch)
        OnSizeChanged(gfxIntSize(aWidth, aHeight));

    // Should we skip honoring aRepaint here?
    if (aRepaint && FindTopLevel() == nsWindow::TopWindow())
        RedrawAll();

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetZIndex(PRInt32 aZIndex)
{
    ALOG("nsWindow[%p]::SetZIndex %d ignored", (void*)this, aZIndex);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                      nsIWidget *aWidget,
                      bool aActivate)
{
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetSizeMode(PRInt32 aMode)
{
    switch (aMode) {
        case nsSizeMode_Minimized:
            AndroidBridge::Bridge()->MoveTaskToBack();
            break;
        case nsSizeMode_Fullscreen:
            MakeFullScreen(true);
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

NS_IMETHODIMP
nsWindow::IsEnabled(bool *aState)
{
    *aState = true;
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Invalidate(const nsIntRect &aRect)
{
    AndroidGeckoEvent *event = new AndroidGeckoEvent(AndroidGeckoEvent::DRAW, aRect);
    nsAppShell::gAppShell->PostEvent(event);
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
    if (!aRaise)
        ALOG("nsWindow::SetFocus: can't set focus without raising, ignoring aRaise = false!");

    if (!AndroidBridge::Bridge())
        return NS_OK;

    nsWindow *top = FindTopLevel();
    top->mFocus = this;
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

    nsRefPtr<nsWindow> kungFuDeathGrip(this);

    nsWindow *oldTop = nsnull;
    nsWindow *newTop = this;
    if (!gTopLevelWindows.IsEmpty())
        oldTop = gTopLevelWindows[0];

    gTopLevelWindows.RemoveElement(this);
    gTopLevelWindows.InsertElementAt(0, this);

    if (oldTop) {
        nsGUIEvent event(true, NS_DEACTIVATE, oldTop);
        DispatchEvent(&event);
    }

    if (Destroyed()) {
        // somehow the deactivate event handler destroyed this window.
        // try to recover by grabbing the next window in line and activating
        // that instead
        if (gTopLevelWindows.IsEmpty())
            return;
        newTop = gTopLevelWindows[0];
    }

    nsGUIEvent event(true, NS_ACTIVATE, newTop);
    DispatchEvent(&event);

    // force a window resize
    nsAppShell::gAppShell->ResendLastResizeEvent(newTop);
    RedrawAll();
}

NS_IMETHODIMP
nsWindow::GetScreenBounds(nsIntRect &aRect)
{
    nsIntPoint p = WidgetToScreenOffset();

    aRect.x = p.x;
    aRect.y = p.y;
    aRect.width = mBounds.width;
    aRect.height = mBounds.height;
    
    return NS_OK;
}

nsIntPoint
nsWindow::WidgetToScreenOffset()
{
    nsIntPoint p(0, 0);
    nsWindow *w = this;

    while (w && !w->IsTopLevel()) {
        p.x += w->mBounds.x;
        p.y += w->mBounds.y;

        w = w->mParent;
    }

    return p;
}

NS_IMETHODIMP
nsWindow::DispatchEvent(nsGUIEvent *aEvent,
                        nsEventStatus &aStatus)
{
    aStatus = DispatchEvent(aEvent);
    return NS_OK;
}

nsEventStatus
nsWindow::DispatchEvent(nsGUIEvent *aEvent)
{
    if (mEventCallback) {
        nsEventStatus status = (*mEventCallback)(aEvent);

        switch (aEvent->message) {
        case NS_COMPOSITION_START:
            mIMEComposing = true;
            break;
        case NS_COMPOSITION_END:
            mIMEComposing = false;
            break;
        case NS_TEXT_TEXT:
            mIMEComposingText = static_cast<nsTextEvent*>(aEvent)->theText;
            break;
        case NS_KEY_PRESS:
            // Sometimes the text changes after a key press do not generate notifications (see Bug 723810)
            // Call the corresponding methods explicitly to send those changes back to Java
            OnIMETextChange(0, 0, 0);
            OnIMESelectionChange();
            break;
        }
        return status;
    }
    return nsEventStatus_eIgnore;
}

NS_IMETHODIMP
nsWindow::MakeFullScreen(bool aFullScreen)
{
    AndroidBridge::Bridge()->SetFullScreen(aFullScreen);
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetWindowClass(const nsAString& xulWinType)
{
    return NS_OK;
}

mozilla::layers::LayerManager*
nsWindow::GetLayerManager(PLayersChild*, LayersBackend, LayerManagerPersistence, 
                          bool* aAllowRetaining)
{
    if (aAllowRetaining) {
        *aAllowRetaining = true;
    }
    if (mLayerManager) {
        return mLayerManager;
    }

    nsWindow *topWindow = TopWindow();

    if (!topWindow) {
        printf_stderr(" -- no topwindow\n");
        mLayerManager = CreateBasicLayerManager();
        return mLayerManager;
    }

    mUseAcceleratedRendering = GetShouldAccelerate();

#ifdef MOZ_JAVA_COMPOSITOR
    bool useCompositor = UseOffMainThreadCompositing();

    if (useCompositor) {
        CreateCompositor();
        if (mLayerManager) {
            SetCompositor(mCompositorParent, mCompositorChild, mCompositorThread);
            return mLayerManager;
        }

        // If we get here, then off main thread compositing failed to initialize.
        sFailedToCreateGLContext = true;
    }
#endif

    if (!mUseAcceleratedRendering ||
        sFailedToCreateGLContext)
    {
        printf_stderr(" -- creating basic, not accelerated\n");
        mLayerManager = CreateBasicLayerManager();
        return mLayerManager;
    }

    if (!mLayerManager) {
        if (!sGLContext) {
            // the window we give doesn't matter here
            sGLContext = mozilla::gl::GLContextProvider::CreateForWindow(this);
        }

        if (sGLContext) {
                nsRefPtr<mozilla::layers::LayerManagerOGL> layerManager =
                        new mozilla::layers::LayerManagerOGL(this);

                if (layerManager && layerManager->Initialize(sGLContext))
                        mLayerManager = layerManager;
                sValidSurface = true;
        }

        if (!sGLContext || !mLayerManager) {
                sGLContext = nsnull;
                sFailedToCreateGLContext = true;

                mLayerManager = CreateBasicLayerManager();
        }
    }

    return mLayerManager;
}

void
nsWindow::OnGlobalAndroidEvent(AndroidGeckoEvent *ae)
{
    if (!AndroidBridge::Bridge())
        return;

    nsWindow *win = TopWindow();
    if (!win)
        return;

    switch (ae->Type()) {
        case AndroidGeckoEvent::FORCED_RESIZE:
            win->mBounds.width = 0;
            win->mBounds.height = 0;
            // also resize the children
            for (PRUint32 i = 0; i < win->mChildren.Length(); i++) {
                win->mChildren[i]->mBounds.width = 0;
                win->mChildren[i]->mBounds.height = 0;
            }
        case AndroidGeckoEvent::SIZE_CHANGED: {
            nsTArray<nsIntPoint> points = ae->Points();
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

            if (XRE_GetProcessType() != GeckoProcessType_Default)
                break;

            // Tell the content process the new screen size.
            nsTArray<ContentParent*> cplist;
            ContentParent::GetAll(cplist);
            for (PRUint32 i = 0; i < cplist.Length(); ++i)
                unused << cplist[i]->SendScreenSizeChanged(gAndroidScreenBounds);

            if (gContentCreationNotifier)
                break;

            // If the content process is not created yet, wait until it's
            // created and then tell it the screen size.
            nsCOMPtr<nsIObserverService> obs = do_GetService("@mozilla.org/observer-service;1");
            if (!obs)
                break;

            nsCOMPtr<ContentCreationNotifier> notifier = new ContentCreationNotifier;
            if (NS_SUCCEEDED(obs->AddObserver(notifier, "ipc:content-created", false))) {
                if (NS_SUCCEEDED(obs->AddObserver(notifier, "xpcom-shutdown", false)))
                    gContentCreationNotifier = notifier;
                else
                    obs->RemoveObserver(notifier, "ipc:content-created");
            }
            break;
        }

        case AndroidGeckoEvent::MOTION_EVENT: {
            win->UserActivity();
            if (!gTopLevelWindows.IsEmpty()) {
                nsIntPoint pt(0,0);
                nsTArray<nsIntPoint> points = ae->Points();
                if (points.Length() > 0) {
                    pt = points[0];
                }
                pt.x = clamped(pt.x, 0, NS_MAX(gAndroidBounds.width - 1, 0));
                pt.y = clamped(pt.y, 0, NS_MAX(gAndroidBounds.height - 1, 0));
                nsWindow *target = win->FindWindowForPoint(pt);
#if 0
                ALOG("MOTION_EVENT %f,%f -> %p (visible: %d children: %d)", pt.x, pt.y, (void*)target,
                     target ? target->mIsVisible : 0,
                     target ? target->mChildren.Length() : 0);

                DumpWindows();
#endif
                if (target) {
                    bool preventDefaultActions = target->OnMultitouchEvent(ae);
                    if (!preventDefaultActions && ae->Count() == 2) {
                        target->OnGestureEvent(ae);
                    }

                    if (!preventDefaultActions && ae->Count() < 2)
                        target->OnMouseEvent(ae);
                }
            }
            break;
        }

        case AndroidGeckoEvent::KEY_EVENT:
            win->UserActivity();
            if (win->mFocus)
                win->mFocus->OnKeyEvent(ae);
            break;

        case AndroidGeckoEvent::DRAW:
            layers::renderTraceEventStart("Global draw start", "414141");
            win->OnDraw(ae);
            layers::renderTraceEventEnd("414141");
            break;

        case AndroidGeckoEvent::IME_EVENT:
            win->UserActivity();
            if (win->mFocus) {
                win->mFocus->OnIMEEvent(ae);
            } else {
                NS_WARNING("Sending unexpected IME event to top window");
                win->OnIMEEvent(ae);
            }
            break;

        case AndroidGeckoEvent::SURFACE_CREATED:
            sSurfaceExists = true;

            if (AndroidBridge::Bridge()->HasNativeWindowAccess()) {
                AndroidGeckoSurfaceView& sview(AndroidBridge::Bridge()->SurfaceView());
                JNIEnv *env = AndroidBridge::GetJNIEnv();
                if (env) {
                    AutoLocalJNIFrame jniFrame(env);
                    jobject surface = sview.GetSurface(&jniFrame);
                    if (surface) {
                        sNativeWindow = AndroidBridge::Bridge()->AcquireNativeWindow(env, surface);
                        if (sNativeWindow) {
                            AndroidBridge::Bridge()->SetNativeWindowFormat(sNativeWindow, 0, 0, AndroidBridge::WINDOW_FORMAT_RGB_565);
                        }
                    }
                }
            }
            break;

        case AndroidGeckoEvent::SURFACE_DESTROYED:
            if (sGLContext && sValidSurface) {
                sGLContext->ReleaseSurface();
            }
            if (sNativeWindow) {
                AndroidBridge::Bridge()->ReleaseNativeWindow(sNativeWindow);
                sNativeWindow = nsnull;
            }
            sSurfaceExists = false;
            sValidSurface = false;
            break;

#ifdef MOZ_JAVA_COMPOSITOR
        case AndroidGeckoEvent::COMPOSITOR_PAUSE:
            // The compositor gets paused when the app is about to go into the
            // background. While the compositor is paused, we need to ensure that
            // no layer tree updates (from draw events) occur, since the compositor
            // cannot make a GL context current in order to process updates.
            if (sCompositorChild) {
                sCompositorChild->SendPause();
            }
            sCompositorPaused = true;
            break;

        case AndroidGeckoEvent::COMPOSITOR_RESUME:
            // When we receive this, the compositor has already been told to
            // resume. (It turns out that waiting till we reach here to tell
            // the compositor to resume takes too long, resulting in a black
            // flash.) This means it's now safe for layer updates to occur.
            // Since we might have prevented one or more draw events from
            // occurring while the compositor was paused, we need to schedule
            // a draw event now.
            sCompositorPaused = false;
            win->RedrawAll();
            break;
#endif

        case AndroidGeckoEvent::GECKO_EVENT_SYNC:
            AndroidBridge::Bridge()->AcknowledgeEventSync();
            break;

        default:
            break;
    }
}

void
nsWindow::OnAndroidEvent(AndroidGeckoEvent *ae)
{
    if (!AndroidBridge::Bridge())
        return;

    switch (ae->Type()) {
        case AndroidGeckoEvent::DRAW:
            OnDraw(ae);
            break;

        default:
            ALOG("Window got targetted android event type %d, but didn't handle!", ae->Type());
            break;
    }
}

bool
nsWindow::DrawTo(gfxASurface *targetSurface)
{
    nsIntRect boundsRect(0, 0, mBounds.width, mBounds.height);
    return DrawTo(targetSurface, boundsRect);
}

bool
nsWindow::DrawTo(gfxASurface *targetSurface, const nsIntRect &invalidRect)
{
    mozilla::layers::RenderTraceScope trace("DrawTo", "717171");
    if (!mIsVisible)
        return false;

    nsRefPtr<nsWindow> kungFuDeathGrip(this);
    nsEventStatus status;
    nsIntRect boundsRect(0, 0, mBounds.width, mBounds.height);

    // Figure out if any of our children cover this widget completely
    PRInt32 coveringChildIndex = -1;
    for (PRUint32 i = 0; i < mChildren.Length(); ++i) {
        if (mChildren[i]->mBounds.IsEmpty())
            continue;

        if (mChildren[i]->mBounds.Contains(boundsRect)) {
            coveringChildIndex = PRInt32(i);
        }
    }

    // If we have no covering child, then we need to render this.
    if (coveringChildIndex == -1) {
        nsPaintEvent event(true, NS_PAINT, this);
        event.region = invalidRect;

        switch (GetLayerManager(nsnull)->GetBackendType()) {
            case LayerManager::LAYERS_BASIC: {

                nsRefPtr<gfxContext> ctx = new gfxContext(targetSurface);

                {
                    mozilla::layers::RenderTraceScope trace2("Basic DrawTo", "727272");
                    AutoLayerManagerSetup
                      setupLayerManager(this, ctx, BasicLayerManager::BUFFER_NONE);

                    status = DispatchEvent(&event);
                }

                // XXX uhh.. we can't just ignore this because we no longer have
                // what we needed before, but let's keep drawing the children anyway?
#if 0
                if (status == nsEventStatus_eIgnore)
                    return false;
#endif

                // XXX if we got an ignore for the parent, do we still want to draw the children?
                // We don't really have a good way not to...
                break;
            }

            case LayerManager::LAYERS_OPENGL: {

                static_cast<mozilla::layers::LayerManagerOGL*>(GetLayerManager(nsnull))->
                    SetClippingRegion(nsIntRegion(boundsRect));

                status = DispatchEvent(&event);
                break;
            }

            default:
                NS_ERROR("Invalid layer manager");
        }

        // We had no covering child, so make sure we draw all the children,
        // starting from index 0.
        coveringChildIndex = 0;
    }

    gfxPoint offset;

    if (targetSurface)
        offset = targetSurface->GetDeviceOffset();

    for (PRUint32 i = coveringChildIndex; i < mChildren.Length(); ++i) {
        if (mChildren[i]->mBounds.IsEmpty() ||
            !mChildren[i]->mBounds.Intersects(boundsRect)) {
            continue;
        }

        if (targetSurface)
            targetSurface->SetDeviceOffset(offset + gfxPoint(mChildren[i]->mBounds.x,
                                                             mChildren[i]->mBounds.y));

        bool ok = mChildren[i]->DrawTo(targetSurface, invalidRect);

        if (!ok) {
            ALOG("nsWindow[%p]::DrawTo child %d[%p] returned FALSE!", (void*) this, i, (void*)mChildren[i]);
        }
    }

    if (targetSurface)
        targetSurface->SetDeviceOffset(offset);

    return true;
}

void
nsWindow::OnDraw(AndroidGeckoEvent *ae)
{
    if (!IsTopLevel()) {
        ALOG("##### redraw for window %p, which is not a toplevel window -- sending to toplevel!", (void*) this);
        DumpWindows();
        return;
    }

    if (!mIsVisible) {
        ALOG("##### redraw for window %p, which is not visible -- ignoring!", (void*) this);
        DumpWindows();
        return;
    }

    nsRefPtr<nsWindow> kungFuDeathGrip(this);

    JNIEnv *env = AndroidBridge::GetJNIEnv();
    if (!env)
        return;
    AutoLocalJNIFrame jniFrame;

#ifdef MOZ_JAVA_COMPOSITOR
    // We're paused, or we haven't been given a window-size yet, so do nothing
    if (sCompositorPaused || gAndroidBounds.width <= 0 || gAndroidBounds.height <= 0) {
        return;
    }

    layers::renderTraceEventStart("Get surface", "424545");
    static unsigned char bits2[32 * 32 * 2];
    nsRefPtr<gfxImageSurface> targetSurface =
        new gfxImageSurface(bits2, gfxIntSize(32, 32), 32 * 2,
                            gfxASurface::ImageFormatRGB16_565);
    layers::renderTraceEventEnd("Get surface", "424545");

    layers::renderTraceEventStart("Widget draw to", "434646");
    if (targetSurface->CairoStatus()) {
        ALOG("### Failed to create a valid surface from the bitmap");
    } else {
        DrawTo(targetSurface, ae->Rect());
    }
    layers::renderTraceEventEnd("Widget draw to", "434646");
    return;
#endif

    if (!sSurfaceExists) {
        return;
    }

    AndroidGeckoSurfaceView& sview(AndroidBridge::Bridge()->SurfaceView());

    NS_ASSERTION(!sview.isNull(), "SurfaceView is null!");

    AndroidBridge::Bridge()->HideProgressDialogOnce();

    if (GetLayerManager(nsnull)->GetBackendType() == LayerManager::LAYERS_BASIC) {
        if (sNativeWindow) {
            unsigned char *bits;
            int width, height, format, stride;
            if (!AndroidBridge::Bridge()->LockWindow(sNativeWindow, &bits, &width, &height, &format, &stride)) {
                ALOG("failed to lock buffer - skipping draw");
                return;
            }

            if (!bits || format != AndroidBridge::WINDOW_FORMAT_RGB_565 ||
                width != mBounds.width || height != mBounds.height) {

                ALOG("surface is not expected dimensions or format - skipping draw");
                AndroidBridge::Bridge()->UnlockWindow(sNativeWindow);
                return;
            }

            nsRefPtr<gfxImageSurface> targetSurface =
                new gfxImageSurface(bits,
                                    gfxIntSize(mBounds.width, mBounds.height),
                                    stride * 2,
                                    gfxASurface::ImageFormatRGB16_565);
            if (targetSurface->CairoStatus()) {
                ALOG("### Failed to create a valid surface from the bitmap");
            } else {
                DrawTo(targetSurface);
            }

            AndroidBridge::Bridge()->UnlockWindow(sNativeWindow);
        } else if (AndroidBridge::Bridge()->HasNativeBitmapAccess()) {
            jobject bitmap = sview.GetSoftwareDrawBitmap(&jniFrame);
            if (!bitmap) {
                ALOG("no bitmap to draw into - skipping draw");
                return;
            }

            if (!AndroidBridge::Bridge()->ValidateBitmap(bitmap, mBounds.width, mBounds.height))
                return;

            void *buf = AndroidBridge::Bridge()->LockBitmap(bitmap);
            if (buf == nsnull) {
                ALOG("### Software drawing, but failed to lock bitmap.");
                return;
            }

            nsRefPtr<gfxImageSurface> targetSurface =
                new gfxImageSurface((unsigned char *)buf,
                                    gfxIntSize(mBounds.width, mBounds.height),
                                    mBounds.width * 2,
                                    gfxASurface::ImageFormatRGB16_565);
            if (targetSurface->CairoStatus()) {
                ALOG("### Failed to create a valid surface from the bitmap");
            } else {
                DrawTo(targetSurface);
            }

            AndroidBridge::Bridge()->UnlockBitmap(bitmap);
            sview.Draw2D(bitmap, mBounds.width, mBounds.height);
        } else {
            jobject bytebuf = sview.GetSoftwareDrawBuffer(&jniFrame);
            if (!bytebuf) {
                ALOG("no buffer to draw into - skipping draw");
                return;
            }

            void *buf = env->GetDirectBufferAddress(bytebuf);
            int cap = env->GetDirectBufferCapacity(bytebuf);
            if (!buf || cap != (mBounds.width * mBounds.height * 2)) {
                ALOG("### Software drawing, but unexpected buffer size %d expected %d (or no buffer %p)!", cap, mBounds.width * mBounds.height * 2, buf);
                return;
            }

            nsRefPtr<gfxImageSurface> targetSurface =
                new gfxImageSurface((unsigned char *)buf,
                                    gfxIntSize(mBounds.width, mBounds.height),
                                    mBounds.width * 2,
                                    gfxASurface::ImageFormatRGB16_565);
            if (targetSurface->CairoStatus()) {
                ALOG("### Failed to create a valid surface");
            } else {
                DrawTo(targetSurface);
            }

            sview.Draw2D(bytebuf, mBounds.width * 2);
        }
    } else {
        int drawType = sview.BeginDrawing();

        if (drawType == AndroidGeckoSurfaceView::DRAW_DISABLED) {
            return;
        }

        if (drawType == AndroidGeckoSurfaceView::DRAW_ERROR) {
            ALOG("##### BeginDrawing failed!");
            return;
        }

        if (!sValidSurface) {
            sGLContext->RenewSurface();
            sValidSurface = true;
        }


        NS_ASSERTION(sGLContext, "Drawing with GLES without a GL context?");

        DrawTo(nsnull);

        sview.EndDrawing();
    }
}

void
nsWindow::OnSizeChanged(const gfxIntSize& aSize)
{
    int w = aSize.width;
    int h = aSize.height;

    ALOG("nsWindow: %p OnSizeChanged [%d %d]", (void*)this, w, h);

    nsRefPtr<nsWindow> kungFuDeathGrip(this);
    nsSizeEvent event(true, NS_SIZE, this);
    InitEvent(event);

    nsIntRect wsz(0, 0, w, h);
    event.windowSize = &wsz;
    event.mWinWidth = w;
    event.mWinHeight = h;

    mBounds.width = w;
    mBounds.height = h;

    DispatchEvent(&event);
}

void
nsWindow::InitEvent(nsGUIEvent& event, nsIntPoint* aPoint)
{
    if (aPoint) {
        event.refPoint.x = aPoint->x;
        event.refPoint.y = aPoint->y;
    } else {
        event.refPoint.x = 0;
        event.refPoint.y = 0;
    }

    event.time = PR_Now() / 1000;
}

gfxIntSize
nsWindow::GetAndroidScreenBounds()
{
    if (XRE_GetProcessType() == GeckoProcessType_Content) {
        return ContentChild::GetSingleton()->GetScreenSize();
    }
    return gAndroidScreenBounds;
}

void *
nsWindow::GetNativeData(PRUint32 aDataType)
{
    switch (aDataType) {
        // used by GLContextProviderEGL, NULL is EGL_DEFAULT_DISPLAY
        case NS_NATIVE_DISPLAY:
            return NULL;

        case NS_NATIVE_WIDGET:
            return (void *) this;
    }

    return nsnull;
}

void
nsWindow::OnMouseEvent(AndroidGeckoEvent *ae)
{
    PRUint32 msg;
    PRInt16 buttons = nsMouseEvent::eLeftButtonFlag;
    switch (ae->Action() & AndroidMotionEvent::ACTION_MASK) {
#ifndef MOZ_ONLY_TOUCH_EVENTS
        case AndroidMotionEvent::ACTION_DOWN:
            msg = NS_MOUSE_BUTTON_DOWN;
            break;

        case AndroidMotionEvent::ACTION_MOVE:
            msg = NS_MOUSE_MOVE;
            break;

        case AndroidMotionEvent::ACTION_UP:
        case AndroidMotionEvent::ACTION_CANCEL:
            msg = NS_MOUSE_BUTTON_UP;
            break;
#endif

        case AndroidMotionEvent::ACTION_HOVER_ENTER:
        case AndroidMotionEvent::ACTION_HOVER_MOVE:
        case AndroidMotionEvent::ACTION_HOVER_EXIT:
            msg = NS_MOUSE_MOVE;
            buttons = 0;
            break;

        default:
            return;
    }

    nsRefPtr<nsWindow> kungFuDeathGrip(this);

send_again:

    nsMouseEvent event(true,
                       msg, this,
                       nsMouseEvent::eReal, nsMouseEvent::eNormal);
    // XXX can we synthesize different buttons?
    event.button = nsMouseEvent::eLeftButton;

    if (msg != NS_MOUSE_MOVE)
        event.clickCount = 1;

    // XXX add the double-click handling logic here
    if (ae->Points().Length() > 0)
        DispatchMotionEvent(event, ae, ae->Points()[0]);
    if (Destroyed())
        return;

    if (msg == NS_MOUSE_BUTTON_DOWN) {
        msg = NS_MOUSE_MOVE;
        goto send_again;
    }
}

static double
getDistance(const nsIntPoint &p1, const nsIntPoint &p2)
{
    double deltaX = p2.x - p1.x;
    double deltaY = p2.y - p1.y;
    return sqrt(deltaX*deltaX + deltaY*deltaY);
}

bool nsWindow::OnMultitouchEvent(AndroidGeckoEvent *ae)
{
    nsRefPtr<nsWindow> kungFuDeathGrip(this);

    // This is set to true once we have called SetPreventPanning() exactly
    // once for a given sequence of touch events. It is reset on the start
    // of the next sequence.
    static bool sDefaultPreventedNotified = false;
    static bool sLastWasDownEvent = false;

    bool preventDefaultActions = false;
    bool isDownEvent = false;
    switch (ae->Action() & AndroidMotionEvent::ACTION_MASK) {
        case AndroidMotionEvent::ACTION_DOWN:
        case AndroidMotionEvent::ACTION_POINTER_DOWN: {
            nsTouchEvent event(true, NS_TOUCH_START, this);
            preventDefaultActions = DispatchMultitouchEvent(event, ae);
            isDownEvent = true;
            break;
        }
        case AndroidMotionEvent::ACTION_MOVE: {
            nsTouchEvent event(true, NS_TOUCH_MOVE, this);
            preventDefaultActions = DispatchMultitouchEvent(event, ae);
            break;
        }
        case AndroidMotionEvent::ACTION_UP:
        case AndroidMotionEvent::ACTION_POINTER_UP: {
            nsTouchEvent event(true, NS_TOUCH_END, this);
            preventDefaultActions = DispatchMultitouchEvent(event, ae);
            break;
        }
        case AndroidMotionEvent::ACTION_OUTSIDE:
        case AndroidMotionEvent::ACTION_CANCEL: {
            nsTouchEvent event(true, NS_TOUCH_CANCEL, this);
            preventDefaultActions = DispatchMultitouchEvent(event, ae);
            break;
        }
    }

    // if the last event we got was a down event, then by now we know for sure whether
    // this block has been default-prevented or not. if we haven't already sent the
    // notification for this block, do so now.
    if (sLastWasDownEvent && !sDefaultPreventedNotified) {
        // if this event is a down event, that means it's the start of a new block, and the
        // previous block should not be default-prevented
        bool defaultPrevented = isDownEvent ? false : preventDefaultActions;
        AndroidBridge::Bridge()->NotifyDefaultPrevented(defaultPrevented);
        sDefaultPreventedNotified = true;
    }

    // now, if this event is a down event, then we might already know that it has been
    // default-prevented. if so, we send the notification right away; otherwise we wait
    // for the next event.
    if (isDownEvent) {
        if (preventDefaultActions) {
            AndroidBridge::Bridge()->NotifyDefaultPrevented(true);
            sDefaultPreventedNotified = true;
        } else {
            sDefaultPreventedNotified = false;
        }
    }
    sLastWasDownEvent = isDownEvent;

    return preventDefaultActions;
}

bool
nsWindow::DispatchMultitouchEvent(nsTouchEvent &event, AndroidGeckoEvent *ae)
{
    nsIntPoint offset = WidgetToScreenOffset();

    event.modifiers = 0;
    event.time = ae->Time();

    int action = ae->Action() & AndroidMotionEvent::ACTION_MASK;
    if (action == AndroidMotionEvent::ACTION_UP ||
        action == AndroidMotionEvent::ACTION_POINTER_UP) {
        event.touches.SetCapacity(1);
        int pointerIndex = ae->PointerIndex();
        nsCOMPtr<nsIDOMTouch> t(new nsDOMTouch(ae->PointIndicies()[pointerIndex],
                                               ae->Points()[pointerIndex] - offset,
                                               ae->PointRadii()[pointerIndex],
                                               ae->Orientations()[pointerIndex],
                                               ae->Pressures()[pointerIndex]));
        event.touches.AppendElement(t);
    } else {
        int count = ae->Count();
        event.touches.SetCapacity(count);
        for (int i = 0; i < count; i++) {
            nsCOMPtr<nsIDOMTouch> t(new nsDOMTouch(ae->PointIndicies()[i],
                                                   ae->Points()[i] - offset,
                                                   ae->PointRadii()[i],
                                                   ae->Orientations()[i],
                                                   ae->Pressures()[i]));
            event.touches.AppendElement(t);
        }
    }

    nsEventStatus status;
    DispatchEvent(&event, status);
    return (status == nsEventStatus_eConsumeNoDefault);
}

void
nsWindow::OnGestureEvent(AndroidGeckoEvent *ae)
{
    PRUint32 msg = 0;

    nsIntPoint midPoint;
    midPoint.x = ((ae->Points()[0].x + ae->Points()[1].x) / 2);
    midPoint.y = ((ae->Points()[0].y + ae->Points()[1].y) / 2);
    nsIntPoint refPoint = midPoint - WidgetToScreenOffset();

    double pinchDist = getDistance(ae->Points()[0], ae->Points()[1]);
    double pinchDelta = 0;

    switch (ae->Action() & AndroidMotionEvent::ACTION_MASK) {
        case AndroidMotionEvent::ACTION_POINTER_DOWN:
            msg = NS_SIMPLE_GESTURE_MAGNIFY_START;
            mStartPoint = new nsIntPoint(midPoint);
            mStartDist = mLastDist = pinchDist;
            mGestureFinished = false;
            break;
        case AndroidMotionEvent::ACTION_MOVE:
            msg = NS_SIMPLE_GESTURE_MAGNIFY_UPDATE;
            pinchDelta = pinchDist - mLastDist;
            mLastDist = pinchDist;
            break;
        case AndroidMotionEvent::ACTION_POINTER_UP:
            msg = NS_SIMPLE_GESTURE_MAGNIFY;
            pinchDelta = pinchDist - mStartDist;
            mStartPoint = nsnull;
            break;
        default:
            return;
    }

    if (!mGestureFinished) {
        nsRefPtr<nsWindow> kungFuDeathGrip(this);
        DispatchGestureEvent(msg, 0, pinchDelta, refPoint, ae->Time());
        if (Destroyed())
            return;

        // If the cumulative pinch delta goes past the threshold, treat this
        // as a pinch only, and not a swipe.
        if (fabs(pinchDist - mStartDist) > mSwipeMaxPinchDelta)
            mStartPoint = nsnull;

        // If we have traveled more than SWIPE_MIN_DISTANCE from the start
        // point, stop the pinch gesture and fire a swipe event.
        if (mStartPoint) {
            double swipeDistance = getDistance(midPoint, *mStartPoint);
            if (swipeDistance > mSwipeMinDistance) {
                PRUint32 direction = 0;
                nsIntPoint motion = midPoint - *mStartPoint;

                if (motion.x < -swipeDistance/2)
                    direction |= nsIDOMSimpleGestureEvent::DIRECTION_LEFT;
                if (motion.x > swipeDistance/2)
                    direction |= nsIDOMSimpleGestureEvent::DIRECTION_RIGHT;
                if (motion.y < -swipeDistance/2)
                    direction |= nsIDOMSimpleGestureEvent::DIRECTION_UP;
                if (motion.y > swipeDistance/2)
                    direction |= nsIDOMSimpleGestureEvent::DIRECTION_DOWN;

                // Finish the pinch gesture, then fire the swipe event:
                msg = NS_SIMPLE_GESTURE_MAGNIFY;
                DispatchGestureEvent(msg, 0, pinchDist - mStartDist, refPoint, ae->Time());
                if (Destroyed())
                    return;
                msg = NS_SIMPLE_GESTURE_SWIPE;
                DispatchGestureEvent(msg, direction, 0, refPoint, ae->Time());

                // Don't generate any more gesture events for this touch.
                mGestureFinished = true;
            }
        }
    }
}

void
nsWindow::DispatchGestureEvent(PRUint32 msg, PRUint32 direction, double delta,
                               const nsIntPoint &refPoint, PRUint64 time)
{
    nsSimpleGestureEvent event(true, msg, this, direction, delta);

    event.modifiers = 0;
    event.time = time;
    event.refPoint = refPoint;

    DispatchEvent(&event);
}


void
nsWindow::DispatchMotionEvent(nsInputEvent &event, AndroidGeckoEvent *ae,
                              const nsIntPoint &refPoint)
{
    nsIntPoint offset = WidgetToScreenOffset();

    event.modifiers = 0;
    event.time = ae->Time();

    // XXX possibly bound the range of event.refPoint here.
    //     some code may get confused.
    event.refPoint = refPoint - offset;

    DispatchEvent(&event);
}

static unsigned int ConvertAndroidKeyCodeToDOMKeyCode(int androidKeyCode)
{
    // Special-case alphanumeric keycodes because they are most common.
    if (androidKeyCode >= AndroidKeyEvent::KEYCODE_A &&
        androidKeyCode <= AndroidKeyEvent::KEYCODE_Z) {
        return androidKeyCode - AndroidKeyEvent::KEYCODE_A + NS_VK_A;
    }

    if (androidKeyCode >= AndroidKeyEvent::KEYCODE_0 &&
        androidKeyCode <= AndroidKeyEvent::KEYCODE_9) {
        return androidKeyCode - AndroidKeyEvent::KEYCODE_0 + NS_VK_0;
    }

    switch (androidKeyCode) {
        // KEYCODE_UNKNOWN (0) ... KEYCODE_HOME (3)
        case AndroidKeyEvent::KEYCODE_BACK:               return NS_VK_ESCAPE;
        // KEYCODE_CALL (5) ... KEYCODE_POUND (18)
        case AndroidKeyEvent::KEYCODE_DPAD_UP:            return NS_VK_UP;
        case AndroidKeyEvent::KEYCODE_DPAD_DOWN:          return NS_VK_DOWN;
        case AndroidKeyEvent::KEYCODE_DPAD_LEFT:          return NS_VK_LEFT;
        case AndroidKeyEvent::KEYCODE_DPAD_RIGHT:         return NS_VK_RIGHT;
        case AndroidKeyEvent::KEYCODE_DPAD_CENTER:        return NS_VK_RETURN;
        // KEYCODE_VOLUME_UP (24) ... KEYCODE_Z (54)
        case AndroidKeyEvent::KEYCODE_COMMA:              return NS_VK_COMMA;
        case AndroidKeyEvent::KEYCODE_PERIOD:             return NS_VK_PERIOD;
        case AndroidKeyEvent::KEYCODE_ALT_LEFT:           return NS_VK_ALT;
        case AndroidKeyEvent::KEYCODE_ALT_RIGHT:          return NS_VK_ALT;
        case AndroidKeyEvent::KEYCODE_SHIFT_LEFT:         return NS_VK_SHIFT;
        case AndroidKeyEvent::KEYCODE_SHIFT_RIGHT:        return NS_VK_SHIFT;
        case AndroidKeyEvent::KEYCODE_TAB:                return NS_VK_TAB;
        case AndroidKeyEvent::KEYCODE_SPACE:              return NS_VK_SPACE;
        // KEYCODE_SYM (63) ... KEYCODE_ENVELOPE (65)
        case AndroidKeyEvent::KEYCODE_ENTER:              return NS_VK_RETURN;
        case AndroidKeyEvent::KEYCODE_DEL:                return NS_VK_BACK; // Backspace
        case AndroidKeyEvent::KEYCODE_GRAVE:              return NS_VK_BACK_QUOTE;
        // KEYCODE_MINUS (69)
        case AndroidKeyEvent::KEYCODE_EQUALS:             return NS_VK_EQUALS;
        case AndroidKeyEvent::KEYCODE_LEFT_BRACKET:       return NS_VK_OPEN_BRACKET;
        case AndroidKeyEvent::KEYCODE_RIGHT_BRACKET:      return NS_VK_CLOSE_BRACKET;
        case AndroidKeyEvent::KEYCODE_BACKSLASH:          return NS_VK_BACK_SLASH;
        case AndroidKeyEvent::KEYCODE_SEMICOLON:          return NS_VK_SEMICOLON;
        // KEYCODE_APOSTROPHE (75)
        case AndroidKeyEvent::KEYCODE_SLASH:              return NS_VK_SLASH;
        // KEYCODE_AT (77) ... KEYCODE_MUTE (91)
        case AndroidKeyEvent::KEYCODE_PAGE_UP:            return NS_VK_PAGE_UP;
        case AndroidKeyEvent::KEYCODE_PAGE_DOWN:          return NS_VK_PAGE_DOWN;
        // KEYCODE_PICTSYMBOLS (94) ... KEYCODE_BUTTON_MODE (110)
        case AndroidKeyEvent::KEYCODE_ESCAPE:             return NS_VK_ESCAPE;
        case AndroidKeyEvent::KEYCODE_FORWARD_DEL:        return NS_VK_DELETE;
        case AndroidKeyEvent::KEYCODE_CTRL_LEFT:          return NS_VK_CONTROL;
        case AndroidKeyEvent::KEYCODE_CTRL_RIGHT:         return NS_VK_CONTROL;
        case AndroidKeyEvent::KEYCODE_CAPS_LOCK:          return NS_VK_CAPS_LOCK;
        case AndroidKeyEvent::KEYCODE_SCROLL_LOCK:        return NS_VK_SCROLL_LOCK;
        // KEYCODE_META_LEFT (117) ... KEYCODE_FUNCTION (119)
        case AndroidKeyEvent::KEYCODE_SYSRQ:              return NS_VK_PRINTSCREEN;
        case AndroidKeyEvent::KEYCODE_BREAK:              return NS_VK_PAUSE;
        case AndroidKeyEvent::KEYCODE_MOVE_HOME:          return NS_VK_HOME;
        case AndroidKeyEvent::KEYCODE_MOVE_END:           return NS_VK_END;
        case AndroidKeyEvent::KEYCODE_INSERT:             return NS_VK_INSERT;
        // KEYCODE_FORWARD (125) ... KEYCODE_MEDIA_RECORD (130)
        case AndroidKeyEvent::KEYCODE_F1:                 return NS_VK_F1;
        case AndroidKeyEvent::KEYCODE_F2:                 return NS_VK_F2;
        case AndroidKeyEvent::KEYCODE_F3:                 return NS_VK_F3;
        case AndroidKeyEvent::KEYCODE_F4:                 return NS_VK_F4;
        case AndroidKeyEvent::KEYCODE_F5:                 return NS_VK_F5;
        case AndroidKeyEvent::KEYCODE_F6:                 return NS_VK_F6;
        case AndroidKeyEvent::KEYCODE_F7:                 return NS_VK_F7;
        case AndroidKeyEvent::KEYCODE_F8:                 return NS_VK_F8;
        case AndroidKeyEvent::KEYCODE_F9:                 return NS_VK_F9;
        case AndroidKeyEvent::KEYCODE_F10:                return NS_VK_F10;
        case AndroidKeyEvent::KEYCODE_F11:                return NS_VK_F11;
        case AndroidKeyEvent::KEYCODE_F12:                return NS_VK_F12;
        case AndroidKeyEvent::KEYCODE_NUM_LOCK:           return NS_VK_NUM_LOCK;
        case AndroidKeyEvent::KEYCODE_NUMPAD_0:           return NS_VK_NUMPAD0;
        case AndroidKeyEvent::KEYCODE_NUMPAD_1:           return NS_VK_NUMPAD1;
        case AndroidKeyEvent::KEYCODE_NUMPAD_2:           return NS_VK_NUMPAD2;
        case AndroidKeyEvent::KEYCODE_NUMPAD_3:           return NS_VK_NUMPAD3;
        case AndroidKeyEvent::KEYCODE_NUMPAD_4:           return NS_VK_NUMPAD4;
        case AndroidKeyEvent::KEYCODE_NUMPAD_5:           return NS_VK_NUMPAD5;
        case AndroidKeyEvent::KEYCODE_NUMPAD_6:           return NS_VK_NUMPAD6;
        case AndroidKeyEvent::KEYCODE_NUMPAD_7:           return NS_VK_NUMPAD7;
        case AndroidKeyEvent::KEYCODE_NUMPAD_8:           return NS_VK_NUMPAD8;
        case AndroidKeyEvent::KEYCODE_NUMPAD_9:           return NS_VK_NUMPAD9;
        case AndroidKeyEvent::KEYCODE_NUMPAD_DIVIDE:      return NS_VK_DIVIDE;
        case AndroidKeyEvent::KEYCODE_NUMPAD_MULTIPLY:    return NS_VK_MULTIPLY;
        case AndroidKeyEvent::KEYCODE_NUMPAD_SUBTRACT:    return NS_VK_SUBTRACT;
        case AndroidKeyEvent::KEYCODE_NUMPAD_ADD:         return NS_VK_ADD;
        case AndroidKeyEvent::KEYCODE_NUMPAD_DOT:         return NS_VK_DECIMAL;
        case AndroidKeyEvent::KEYCODE_NUMPAD_COMMA:       return NS_VK_SEPARATOR;
        case AndroidKeyEvent::KEYCODE_NUMPAD_ENTER:       return NS_VK_RETURN;
        case AndroidKeyEvent::KEYCODE_NUMPAD_EQUALS:      return NS_VK_EQUALS;
        // KEYCODE_NUMPAD_LEFT_PAREN (162) ... KEYCODE_CALCULATOR (210)

        default:
            ALOG("ConvertAndroidKeyCodeToDOMKeyCode: "
                 "No DOM keycode for Android keycode %d", androidKeyCode);
        return 0;
    }
}

static void InitPluginEvent(ANPEvent* pluginEvent, ANPKeyActions keyAction,
                            AndroidGeckoEvent& key)
{
    int androidKeyCode = key.KeyCode();
    PRUint32 domKeyCode = ConvertAndroidKeyCodeToDOMKeyCode(androidKeyCode);

    int modifiers = 0;
    if (key.IsAltPressed())
      modifiers |= kAlt_ANPKeyModifier;
    if (key.IsShiftPressed())
      modifiers |= kShift_ANPKeyModifier;

    pluginEvent->inSize = sizeof(ANPEvent);
    pluginEvent->eventType = kKey_ANPEventType;
    pluginEvent->data.key.action = keyAction;
    pluginEvent->data.key.nativeCode = androidKeyCode;
    pluginEvent->data.key.virtualCode = domKeyCode;
    pluginEvent->data.key.unichar = key.UnicodeChar();
    pluginEvent->data.key.modifiers = modifiers;
    pluginEvent->data.key.repeatCount = key.RepeatCount();
}

void
nsWindow::InitKeyEvent(nsKeyEvent& event, AndroidGeckoEvent& key,
                       ANPEvent* pluginEvent)
{
    int androidKeyCode = key.KeyCode();
    PRUint32 domKeyCode = ConvertAndroidKeyCodeToDOMKeyCode(androidKeyCode);

    if (event.message == NS_KEY_PRESS) {
        // Android gives us \n, so filter out some control characters.
        event.isChar = (key.UnicodeChar() >= ' ');
        event.charCode = event.isChar ? key.UnicodeChar() : 0;
        event.keyCode = (event.charCode > 0) ? 0 : domKeyCode;
        event.pluginEvent = NULL;
    } else {
#ifdef DEBUG
        if (event.message != NS_KEY_DOWN && event.message != NS_KEY_UP) {
            ALOG("InitKeyEvent: unexpected event.message %d", event.message);
        }
#endif // DEBUG

        // Flash will want a pluginEvent for keydown and keyup events.
        ANPKeyActions action = event.message == NS_KEY_DOWN
                             ? kDown_ANPKeyAction
                             : kUp_ANPKeyAction;
        InitPluginEvent(pluginEvent, action, key);

        event.isChar = false;
        event.charCode = 0;
        event.keyCode = domKeyCode;
        event.pluginEvent = pluginEvent;
    }

    event.InitBasicModifiers(gMenu,
                             key.IsAltPressed(),
                             key.IsShiftPressed(),
                             false);
    event.location = nsIDOMKeyEvent::DOM_KEY_LOCATION_MOBILE;
    event.time = key.Time();

    if (gMenu)
        gMenuConsumed = true;
}

void
nsWindow::HandleSpecialKey(AndroidGeckoEvent *ae)
{
    nsRefPtr<nsWindow> kungFuDeathGrip(this);
    nsCOMPtr<nsIAtom> command;
    bool isDown = ae->Action() == AndroidKeyEvent::ACTION_DOWN;
    bool isLongPress = !!(ae->Flags() & AndroidKeyEvent::FLAG_LONG_PRESS);
    bool doCommand = false;
    PRUint32 keyCode = ae->KeyCode();

    if (isDown) {
        switch (keyCode) {
            case AndroidKeyEvent::KEYCODE_BACK:
                if (isLongPress) {
                    command = nsGkAtoms::Clear;
                    doCommand = true;
                }
                break;
            case AndroidKeyEvent::KEYCODE_VOLUME_UP:
                command = nsGkAtoms::VolumeUp;
                doCommand = true;
                break;
            case AndroidKeyEvent::KEYCODE_VOLUME_DOWN:
                command = nsGkAtoms::VolumeDown;
                doCommand = true;
                break;
            case AndroidKeyEvent::KEYCODE_MENU:
                gMenu = true;
                gMenuConsumed = isLongPress;
                break;
        }
    } else {
        switch (keyCode) {
            case AndroidKeyEvent::KEYCODE_BACK: {
                nsKeyEvent pressEvent(true, NS_KEY_PRESS, this);
                ANPEvent pluginEvent;
                InitKeyEvent(pressEvent, *ae, &pluginEvent);
                DispatchEvent(&pressEvent);
                return;
            }
            case AndroidKeyEvent::KEYCODE_MENU:
                gMenu = false;
                if (!gMenuConsumed) {
                    command = nsGkAtoms::Menu;
                    doCommand = true;
                }
                break;
            case AndroidKeyEvent::KEYCODE_SEARCH:
                command = nsGkAtoms::Search;
                doCommand = true;
                break;
            default:
                ALOG("Unknown special key code!");
                return;
        }
    }
    if (doCommand) {
        nsCommandEvent event(true, nsGkAtoms::onAppCommand, command, this);
        InitEvent(event);
        DispatchEvent(&event);
    }
}

void
nsWindow::OnKeyEvent(AndroidGeckoEvent *ae)
{
    nsRefPtr<nsWindow> kungFuDeathGrip(this);
    PRUint32 msg;
    switch (ae->Action()) {
    case AndroidKeyEvent::ACTION_DOWN:
        msg = NS_KEY_DOWN;
        break;
    case AndroidKeyEvent::ACTION_UP:
        msg = NS_KEY_UP;
        break;
    case AndroidKeyEvent::ACTION_MULTIPLE:
        {
            nsTextEvent event(true, NS_TEXT_TEXT, this);
            event.theText.Assign(ae->Characters());
            DispatchEvent(&event);
        }
        return;
    default:
        ALOG("Unknown key action event!");
        return;
    }

    bool firePress = ae->Action() == AndroidKeyEvent::ACTION_DOWN;
    switch (ae->KeyCode()) {
    case AndroidKeyEvent::KEYCODE_SHIFT_LEFT:
    case AndroidKeyEvent::KEYCODE_SHIFT_RIGHT:
    case AndroidKeyEvent::KEYCODE_ALT_LEFT:
    case AndroidKeyEvent::KEYCODE_ALT_RIGHT:
        firePress = false;
        break;
    case AndroidKeyEvent::KEYCODE_BACK:
    case AndroidKeyEvent::KEYCODE_MENU:
    case AndroidKeyEvent::KEYCODE_SEARCH:
    case AndroidKeyEvent::KEYCODE_VOLUME_UP:
    case AndroidKeyEvent::KEYCODE_VOLUME_DOWN:
        HandleSpecialKey(ae);
        return;
    }

    nsEventStatus status;
    nsKeyEvent event(true, msg, this);
    ANPEvent pluginEvent;
    InitKeyEvent(event, *ae, &pluginEvent);
    DispatchEvent(&event, status);

    if (Destroyed())
        return;
    if (!firePress)
        return;

    nsKeyEvent pressEvent(true, NS_KEY_PRESS, this);
    InitKeyEvent(pressEvent, *ae, &pluginEvent);
    if (status == nsEventStatus_eConsumeNoDefault) {
        pressEvent.flags |= NS_EVENT_FLAG_NO_DEFAULT;
    }
#ifdef DEBUG_ANDROID_WIDGET
    __android_log_print(ANDROID_LOG_INFO, "Gecko", "Dispatching key pressEvent with keyCode %d charCode %d shift %d alt %d sym/ctrl %d metamask %d", pressEvent.keyCode, pressEvent.charCode, pressEvent.IsShift(), pressEvent.IsAlt(), pressEvent.IsControl(), ae->MetaState());
#endif
    DispatchEvent(&pressEvent);
}

#ifdef DEBUG_ANDROID_IME
#define ALOGIME(args...) ALOG(args)
#else
#define ALOGIME(args...)
#endif

void
nsWindow::OnIMEAddRange(AndroidGeckoEvent *ae)
{
    //ALOGIME("IME: IME_ADD_RANGE");
    nsTextRange range;
    range.mStartOffset = ae->Offset();
    range.mEndOffset = range.mStartOffset + ae->Count();
    range.mRangeType = ae->RangeType();
    range.mRangeStyle.mDefinedStyles = ae->RangeStyles();
    range.mRangeStyle.mLineStyle = nsTextRangeStyle::LINESTYLE_SOLID;
    range.mRangeStyle.mForegroundColor = NS_RGBA(
        ((ae->RangeForeColor() >> 16) & 0xff),
        ((ae->RangeForeColor() >> 8) & 0xff),
        (ae->RangeForeColor() & 0xff),
        ((ae->RangeForeColor() >> 24) & 0xff));
    range.mRangeStyle.mBackgroundColor = NS_RGBA(
        ((ae->RangeBackColor() >> 16) & 0xff),
        ((ae->RangeBackColor() >> 8) & 0xff),
        (ae->RangeBackColor() & 0xff),
        ((ae->RangeBackColor() >> 24) & 0xff));
    mIMERanges.AppendElement(range);
    return;
}

void
nsWindow::OnIMEEvent(AndroidGeckoEvent *ae)
{
    nsRefPtr<nsWindow> kungFuDeathGrip(this);
    switch (ae->Action()) {
    case AndroidGeckoEvent::IME_COMPOSITION_END:
        {
            ALOGIME("IME: IME_COMPOSITION_END");
            MOZ_ASSERT(mIMEComposing,
                       "IME_COMPOSITION_END when we are not composing?!");

            nsCompositionEvent event(true, NS_COMPOSITION_END, this);
            InitEvent(event, nsnull);
            event.data = mIMELastDispatchedComposingText;
            mIMELastDispatchedComposingText.Truncate();
            DispatchEvent(&event);
        }
        return;
    case AndroidGeckoEvent::IME_COMPOSITION_BEGIN:
        {
            ALOGIME("IME: IME_COMPOSITION_BEGIN");
            MOZ_ASSERT(!mIMEComposing,
                       "IME_COMPOSITION_BEGIN when we are already composing?!");

            mIMELastDispatchedComposingText.Truncate();
            nsCompositionEvent event(true, NS_COMPOSITION_START, this);
            InitEvent(event, nsnull);
            DispatchEvent(&event);
        }
        return;
    case AndroidGeckoEvent::IME_ADD_RANGE:
        {
            NS_ASSERTION(mIMEComposing,
                         "IME_ADD_RANGE when we are not composing?!");
            OnIMEAddRange(ae);
        }
        return;
    case AndroidGeckoEvent::IME_SET_TEXT:
        {
            NS_ASSERTION(mIMEComposing,
                         "IME_SET_TEXT when we are not composing?!");

            OnIMEAddRange(ae);

            nsTextEvent event(true, NS_TEXT_TEXT, this);
            InitEvent(event, nsnull);

            event.theText.Assign(ae->Characters());
            event.rangeArray = mIMERanges.Elements();
            event.rangeCount = mIMERanges.Length();

            if (mIMEComposing &&
                event.theText != mIMELastDispatchedComposingText) {
                nsCompositionEvent compositionUpdate(true,
                                                     NS_COMPOSITION_UPDATE,
                                                     this);
                InitEvent(compositionUpdate, nsnull);
                compositionUpdate.data = event.theText;
                mIMELastDispatchedComposingText = event.theText;
                DispatchEvent(&compositionUpdate);
                if (Destroyed())
                    return;
            }

#ifdef DEBUG_ANDROID_IME
            const NS_ConvertUTF16toUTF8 theText8(event.theText);
            const char* text = theText8.get();
            ALOGIME("IME: IME_SET_TEXT: text=\"%s\", length=%u, range=%u",
                    text, event.theText.Length(), mIMERanges.Length());
#endif // DEBUG_ANDROID_IME

            DispatchEvent(&event);
            mIMERanges.Clear();
        }
        return;
    case AndroidGeckoEvent::IME_GET_TEXT:
        {
            ALOGIME("IME: IME_GET_TEXT: o=%u, l=%u", ae->Offset(), ae->Count());

            nsQueryContentEvent event(true, NS_QUERY_TEXT_CONTENT, this);
            InitEvent(event, nsnull);

            event.InitForQueryTextContent(ae->Offset(), ae->Count());
            
            DispatchEvent(&event);

            if (!event.mSucceeded) {
                ALOGIME("IME:     -> failed");
                AndroidBridge::Bridge()->ReturnIMEQueryResult(
                    nsnull, 0, 0, 0);
                return;
            } else if (!event.mWasAsync) {
                AndroidBridge::Bridge()->ReturnIMEQueryResult(
                    event.mReply.mString.get(), 
                    event.mReply.mString.Length(), 0, 0);
            }
        }
        return;
    case AndroidGeckoEvent::IME_DELETE_TEXT:
        {
            ALOGIME("IME: IME_DELETE_TEXT");
            NS_ASSERTION(mIMEComposing,
                         "IME_DELETE_TEXT when we are not composing?!");

            nsKeyEvent event(true, NS_KEY_PRESS, this);
            ANPEvent pluginEvent;
            InitKeyEvent(event, *ae, &pluginEvent);
            event.keyCode = NS_VK_BACK;
            DispatchEvent(&event);
        }
        return;
    case AndroidGeckoEvent::IME_SET_SELECTION:
        {
            ALOGIME("IME: IME_SET_SELECTION: o=%u, l=%d", ae->Offset(), ae->Count());

            nsSelectionEvent selEvent(true, NS_SELECTION_SET, this);
            InitEvent(selEvent, nsnull);

            selEvent.mOffset = PRUint32(ae->Count() >= 0 ?
                                        ae->Offset() :
                                        ae->Offset() + ae->Count());
            selEvent.mLength = PRUint32(NS_ABS(ae->Count()));
            selEvent.mReversed = ae->Count() >= 0 ? false : true;

            DispatchEvent(&selEvent);
        }
        return;
    case AndroidGeckoEvent::IME_GET_SELECTION:
        {
            ALOGIME("IME: IME_GET_SELECTION");

            nsQueryContentEvent event(true, NS_QUERY_SELECTED_TEXT, this);
            InitEvent(event, nsnull);
            DispatchEvent(&event);

            if (!event.mSucceeded) {
                ALOGIME("IME:     -> failed");
                AndroidBridge::Bridge()->ReturnIMEQueryResult(
                    nsnull, 0, 0, 0);
                return;
            } else if (!event.mWasAsync) {
                AndroidBridge::Bridge()->ReturnIMEQueryResult(
                    event.mReply.mString.get(),
                    event.mReply.mString.Length(), 
                    event.GetSelectionStart(),
                    event.GetSelectionEnd() - event.GetSelectionStart());
            }
            //ALOGIME("IME:     -> o=%u, l=%u", event.mReply.mOffset, event.mReply.mString.Length());
        }
        return;
    }
}

nsWindow *
nsWindow::FindWindowForPoint(const nsIntPoint& pt)
{
    if (!mBounds.Contains(pt))
        return nsnull;

    // children mBounds are relative to their parent
    nsIntPoint childPoint(pt.x - mBounds.x, pt.y - mBounds.y);

    for (PRUint32 i = 0; i < mChildren.Length(); ++i) {
        if (mChildren[i]->mBounds.Contains(childPoint))
            return mChildren[i]->FindWindowForPoint(childPoint);
    }

    return this;
}

void
nsWindow::UserActivity()
{
  if (!mIdleService) {
    mIdleService = do_GetService("@mozilla.org/widget/idleservice;1");
  }

  if (mIdleService) {
    mIdleService->ResetIdleTimeOut();
  }
}

NS_IMETHODIMP
nsWindow::ResetInputState()
{
    //ALOGIME("IME: ResetInputState: s=%d", aState);

    // Cancel composition on Gecko side
    if (mIMEComposing) {
        nsRefPtr<nsWindow> kungFuDeathGrip(this);

        nsTextEvent textEvent(true, NS_TEXT_TEXT, this);
        InitEvent(textEvent, nsnull);
        textEvent.theText = mIMEComposingText;
        DispatchEvent(&textEvent);
        mIMEComposingText.Truncate(0);

        nsCompositionEvent event(true, NS_COMPOSITION_END, this);
        InitEvent(event, nsnull);
        DispatchEvent(&event);
    }

    AndroidBridge::NotifyIME(AndroidBridge::NOTIFY_IME_RESETINPUTSTATE, 0);

    // Send IME text/selection change notifications
    OnIMETextChange(0, 0, 0);
    OnIMESelectionChange();

    return NS_OK;
}

NS_IMETHODIMP_(void)
nsWindow::SetInputContext(const InputContext& aContext,
                          const InputContextAction& aAction)
{
    ALOGIME("IME: SetInputContext: s=0x%X, 0x%X, action=0x%X, 0x%X",
            aContext.mIMEState.mEnabled, aContext.mIMEState.mOpen,
            aAction.mCause, aAction.mFocusChange);

    mInputContext = aContext;

    // Ensure that opening the virtual keyboard is allowed for this specific
    // InputContext depending on the content.ime.strict.policy pref
    if (aContext.mIMEState.mEnabled != IMEState::DISABLED && 
        aContext.mIMEState.mEnabled != IMEState::PLUGIN &&
        Preferences::GetBool("content.ime.strict_policy", false) &&
        !aAction.ContentGotFocusByTrustedCause() &&
        !aAction.UserMightRequestOpenVKB()) {
        return;
    }

    int enabled = int(aContext.mIMEState.mEnabled);

    // Only show the virtual keyboard for plugins if mOpen is set appropriately.
    // This avoids showing it whenever a plugin is focused. Bug 747492
    if (aContext.mIMEState.mEnabled == IMEState::PLUGIN &&
        aContext.mIMEState.mOpen != IMEState::OPEN) {
        enabled = int(IMEState::DISABLED);
    }

    AndroidBridge::NotifyIMEEnabled(enabled,
                                    aContext.mHTMLInputType,
                                    aContext.mActionHint);
}

NS_IMETHODIMP_(InputContext)
nsWindow::GetInputContext()
{
    mInputContext.mIMEState.mOpen = IMEState::OPEN_STATE_NOT_SUPPORTED;
    return mInputContext;
}

NS_IMETHODIMP
nsWindow::CancelIMEComposition()
{
    ALOGIME("IME: CancelIMEComposition");

    // Cancel composition on Gecko side
    if (mIMEComposing) {
        nsRefPtr<nsWindow> kungFuDeathGrip(this);

        nsTextEvent textEvent(true, NS_TEXT_TEXT, this);
        InitEvent(textEvent, nsnull);
        DispatchEvent(&textEvent);
        mIMEComposingText.Truncate(0);

        nsCompositionEvent compEvent(true, NS_COMPOSITION_END, this);
        InitEvent(compEvent, nsnull);
        DispatchEvent(&compEvent);
    }

    AndroidBridge::NotifyIME(AndroidBridge::NOTIFY_IME_CANCELCOMPOSITION, 0);
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::OnIMEFocusChange(bool aFocus)
{
    ALOGIME("IME: OnIMEFocusChange: f=%d", aFocus);

    AndroidBridge::NotifyIME(AndroidBridge::NOTIFY_IME_FOCUSCHANGE, 
                             int(aFocus));

    if (aFocus) {
        OnIMETextChange(0, 0, 0);
        OnIMESelectionChange();
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::OnIMETextChange(PRUint32 aStart, PRUint32 aOldEnd, PRUint32 aNewEnd)
{
    ALOGIME("IME: OnIMETextChange: s=%d, oe=%d, ne=%d",
            aStart, aOldEnd, aNewEnd);

    if (!mInputContext.mIMEState.mEnabled) {
        AndroidBridge::NotifyIMEChange(nsnull, 0, 0, 0, 0);
        return NS_OK;
    }

    // A quirk in Android makes it necessary to pass the whole text.
    // The more efficient way would have been passing the substring from index
    // aStart to index aNewEnd

    nsRefPtr<nsWindow> kungFuDeathGrip(this);
    nsQueryContentEvent event(true, NS_QUERY_TEXT_CONTENT, this);
    InitEvent(event, nsnull);
    event.InitForQueryTextContent(0, PR_UINT32_MAX);

    DispatchEvent(&event);
    if (!event.mSucceeded)
        return NS_OK;

    AndroidBridge::NotifyIMEChange(event.mReply.mString.get(),
                                   event.mReply.mString.Length(),
                                   aStart, aOldEnd, aNewEnd);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::OnIMESelectionChange(void)
{
    ALOGIME("IME: OnIMESelectionChange");

    if (!mInputContext.mIMEState.mEnabled) {
        AndroidBridge::NotifyIMEChange(nsnull, 0, 0, 0, -1);
        return NS_OK;
    }

    nsRefPtr<nsWindow> kungFuDeathGrip(this);
    nsQueryContentEvent event(true, NS_QUERY_SELECTED_TEXT, this);
    InitEvent(event, nsnull);

    DispatchEvent(&event);
    if (!event.mSucceeded)
        return NS_OK;

    AndroidBridge::NotifyIMEChange(nsnull, 0, int(event.mReply.mOffset),
                                   int(event.mReply.mOffset + 
                                       event.mReply.mString.Length()), -1);
    return NS_OK;
}

nsIMEUpdatePreference
nsWindow::GetIMEUpdatePreference()
{
    return nsIMEUpdatePreference(true, true);
}

#ifdef MOZ_JAVA_COMPOSITOR
void
nsWindow::DrawWindowUnderlay(LayerManager* aManager, nsIntRect aRect)
{
    JNIEnv *env = GetJNIForThread();
    NS_ABORT_IF_FALSE(env, "No JNI environment at DrawWindowUnderlay()!");
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);

    AndroidGeckoLayerClient& client = AndroidBridge::Bridge()->GetLayerClient();
    if (!client.CreateFrame(&jniFrame, mLayerRendererFrame)) return;
    if (!client.ActivateProgram(&jniFrame)) return;
    if (!mLayerRendererFrame.BeginDrawing(&jniFrame)) return;
    if (!mLayerRendererFrame.DrawBackground(&jniFrame)) return;
    if (!client.DeactivateProgram(&jniFrame)) return; // redundant, but in case somebody adds code after this...
}

void
nsWindow::DrawWindowOverlay(LayerManager* aManager, nsIntRect aRect)
{
    JNIEnv *env = GetJNIForThread();
    NS_ABORT_IF_FALSE(env, "No JNI environment at DrawWindowOverlay()!");
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);

    NS_ABORT_IF_FALSE(!mLayerRendererFrame.isNull(),
                      "Frame should have been created in DrawWindowUnderlay()!");

    AndroidGeckoLayerClient& client = AndroidBridge::Bridge()->GetLayerClient();

    if (!client.ActivateProgram(&jniFrame)) return;
    if (!mLayerRendererFrame.DrawForeground(&jniFrame)) return;
    if (!mLayerRendererFrame.EndDrawing(&jniFrame)) return;
    if (!client.DeactivateProgram(&jniFrame)) return;
    mLayerRendererFrame.Dispose(env);
}

// off-main-thread compositor fields and functions

nsRefPtr<mozilla::layers::CompositorParent> nsWindow::sCompositorParent = 0;
nsRefPtr<mozilla::layers::CompositorChild> nsWindow::sCompositorChild = 0;
base::Thread * nsWindow::sCompositorThread = 0;
bool nsWindow::sCompositorPaused = false;

void
nsWindow::SetCompositor(mozilla::layers::CompositorParent* aCompositorParent,
                        mozilla::layers::CompositorChild* aCompositorChild,
                        ::base::Thread* aCompositorThread)
{
    sCompositorParent = aCompositorParent;
    sCompositorChild = aCompositorChild;
    sCompositorThread = aCompositorThread;
}

void
nsWindow::ScheduleComposite()
{
    if (sCompositorParent) {
        sCompositorParent->ScheduleRenderOnCompositorThread();
    }
}

void
nsWindow::SchedulePauseComposition()
{
    if (sCompositorParent) {
        sCompositorParent->SchedulePauseOnCompositorThread();
    }
}

void
nsWindow::ScheduleResumeComposition(int width, int height)
{
    if (sCompositorParent) {
        sCompositorParent->ScheduleResumeOnCompositorThread(width, height);
    }
}

#endif

