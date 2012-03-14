/* -*- Mode: c++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Matt Brubeck <mbrubeck@mozilla.com>
 *   Vivien Nicolas <vnicolas@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

#include "imgIEncoder.h"

#include "nsStringGlue.h"

using namespace mozilla;
using namespace mozilla::widget;

NS_IMPL_ISUPPORTS_INHERITED0(nsWindow, nsBaseWidget)

// The dimensions of the current android view
static gfxIntSize gAndroidBounds = gfxIntSize(0, 0);
static gfxIntSize gAndroidScreenBounds;

#ifdef ACCESSIBILITY
bool nsWindow::sAccessibilityEnabled = false;
#endif

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

static nsWindow*
TopWindow()
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
    ALOG("%s [% 2d] 0x%08x [parent 0x%08x] [% 3d,% 3d % 3dx% 3d] vis %d type %d",
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
#ifdef ACCESSIBILITY
    mRootAccessible(nsnull),
#endif
    mIMEComposing(false)
{
}

nsWindow::~nsWindow()
{
    gTopLevelWindows.RemoveElement(this);
    nsWindow *top = FindTopLevel();
    if (top->mFocus == this)
        top->mFocus = nsnull;
#ifdef ACCESSIBILITY
    if (mRootAccessible)
        mRootAccessible = nsnull;
#endif
    ALOG("nsWindow %p destructor", (void*)this);

    AndroidBridge::Bridge()->SetCompositorParent(NULL, NULL);

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

    for (PRUint32 i = 0; i < mChildren.Length(); ++i) {
        // why do we still have children?
        ALOG("### Warning: Destroying window %p and reparenting child %p to null!", (void*)this, (void*)mChildren[i]);
        mChildren[i]->SetParent(nsnull);
    }

    if (IsTopLevel())
        gTopLevelWindows.RemoveElement(this);

    if (mParent)
        mParent->mChildren.RemoveElement(this);

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
    // children.  If we didn't have a parent, then remove ourselves
    // from the list of toplevel windows if we're about to get a
    // parent.
    if (mParent)
        mParent->mChildren.RemoveElement(this);

    mParent = (nsWindow*)aNewParent;

    if (mParent)
        mParent->mChildren.AppendElement(this);

    // if we are now in the toplevel window's hierarchy, schedule a redraw
    if (FindTopLevel() == TopWindow())
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
        } else if (TopWindow() == this) {
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
    } else if (FindTopLevel() == TopWindow()) {
        RedrawAll();
    }

#ifdef ACCESSIBILITY
    static bool sAccessibilityChecked = false;
    if (!sAccessibilityChecked) {
        sAccessibilityChecked = true;
        sAccessibilityEnabled =
            AndroidBridge::Bridge()->GetAccessibilityEnabled();
     } 
    if (aState && sAccessibilityEnabled)
        CreateRootAccessible();
#endif

#ifdef DEBUG_ANDROID_WIDGET
    DumpWindows();
#endif

    return NS_OK;
}

#ifdef ACCESSIBILITY
void
nsWindow::CreateRootAccessible()
{
    if (IsTopLevel() && !mRootAccessible) {
        ALOG(("nsWindow:: Create Toplevel Accessibility\n"));
        nsAccessible *acc = DispatchAccessibleEvent();

        if (acc) {
            mRootAccessible = acc;
        }
    }
}

nsAccessible*
nsWindow::DispatchAccessibleEvent()
{
    nsAccessibleEvent event(true, NS_GETACCESSIBLE, this);

    nsEventStatus status;
    DispatchEvent(&event, status);

    return event.mAccessible;
}
#endif

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
    if (aRepaint && FindTopLevel() == TopWindow())
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
    if (existingTopWindow && FindTopLevel() == TopWindow())
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

    bool useCompositor =
        Preferences::GetBool("layers.offmainthreadcomposition.enabled", false);

    if (useCompositor) {
        CreateCompositor();
        if (mLayerManager) {
            AndroidBridge::Bridge()->SetCompositorParent(mCompositorParent, mCompositorThread);
            return mLayerManager;
        }

        // If we get here, then off main thread compositing failed to initialize.
        sFailedToCreateGLContext = true;
    }

    mUseAcceleratedRendering = GetShouldAccelerate();

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
                                                    true);
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
#ifndef MOZ_ONLY_TOUCH_EVENTS
                    if (!preventDefaultActions && ae->Count() < 2)
                        target->OnMotionEvent(ae);
#endif
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
                jobject surface = sview.GetSurface();
                if (surface) {
                    sNativeWindow = AndroidBridge::Bridge()->AcquireNativeWindow(surface);
                    if (sNativeWindow) {
                        AndroidBridge::Bridge()->SetNativeWindowFormat(sNativeWindow, 0, 0, AndroidBridge::WINDOW_FORMAT_RGB_565);
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

        nsIntRect tileRect(0, 0, gAndroidBounds.width, gAndroidBounds.height);
        event.region = boundsRect.Intersect(invalidRect).Intersect(tileRect);

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

    AndroidBridge::AutoLocalJNIFrame jniFrame;
#ifdef MOZ_JAVA_COMPOSITOR
    // We haven't been given a window-size yet, so do nothing
    if (gAndroidBounds.width <= 0 || gAndroidBounds.height <= 0) {
        return;
    }

    /*
     * Check to see whether the presentation shell corresponding to the document on the screen
     * is suppressing painting. If it is, we bail out, as continuing would result in a mismatch
     * between the content on the screen and the current viewport metrics.
     */
    nsCOMPtr<nsIAndroidDrawMetadataProvider> metadataProvider =
        AndroidBridge::Bridge()->GetDrawMetadataProvider();

    layers::renderTraceEventStart("Check supress", "424242");
    bool paintingSuppressed = false;
    if (metadataProvider) {
        metadataProvider->PaintingSuppressed(&paintingSuppressed);
    }
    if (paintingSuppressed) {
        return;
    }
    layers::renderTraceEventEnd("Check supress", "424242");

    layers::renderTraceEventStart("Get Drawable", "424343");
    nsAutoString metadata;
    if (metadataProvider) {
        metadataProvider->GetDrawMetadata(metadata);
    }
    layers::renderTraceEventEnd("Get Drawable", "424343");

    layers::renderTraceEventStart("Get surface", "424545");
    static unsigned char bits2[32 * 32 * 2];
    nsRefPtr<gfxImageSurface> targetSurface =
        new gfxImageSurface(bits2, gfxIntSize(32, 32), 32 * 2,
                            gfxASurface::ImageFormatRGB16_565);
    layers::renderTraceEventEnd("Get surface", "424545");

    layers::renderTraceEventStart("Check Bridge", "434444");
    nsIntRect dirtyRect = ae->Rect().Intersect(nsIntRect(0, 0, gAndroidBounds.width, gAndroidBounds.height));

    AndroidGeckoLayerClient &client = AndroidBridge::Bridge()->GetLayerClient();
    if (!client.BeginDrawing(gAndroidBounds.width, gAndroidBounds.height, metadata)) {
        return;
    }
    layers::renderTraceEventEnd("Check Bridge", "434444");

    layers::renderTraceEventStart("Widget draw to", "434646");
    if (targetSurface->CairoStatus()) {
        ALOG("### Failed to create a valid surface from the bitmap");
    } else {
        DrawTo(targetSurface, dirtyRect);
    }
    layers::renderTraceEventEnd("Widget draw to", "434646");

    layers::renderTraceEventStart("Widget end draw", "434747");
    client.EndDrawing();
    layers::renderTraceEventEnd("Widget end draw", "434747");
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
            jobject bitmap = sview.GetSoftwareDrawBitmap();
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
            jobject bytebuf = sview.GetSoftwareDrawBuffer();
            if (!bytebuf) {
                ALOG("no buffer to draw into - skipping draw");
                return;
            }

            JNIEnv *env = AndroidBridge::GetJNIEnv();
            if (!env)
                return;

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
nsWindow::OnMotionEvent(AndroidGeckoEvent *ae)
{
    PRUint32 msg;
    switch (ae->Action() & AndroidMotionEvent::ACTION_MASK) {
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

        default:
            return;
    }

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
    switch (ae->Action() & AndroidMotionEvent::ACTION_MASK) {
        case AndroidMotionEvent::ACTION_DOWN:
        case AndroidMotionEvent::ACTION_POINTER_DOWN: {
            nsTouchEvent event(PR_TRUE, NS_TOUCH_START, this);
            return DispatchMultitouchEvent(event, ae);
        }
        case AndroidMotionEvent::ACTION_MOVE: {
            nsTouchEvent event(PR_TRUE, NS_TOUCH_MOVE, this);
            return DispatchMultitouchEvent(event, ae);
        }
        case AndroidMotionEvent::ACTION_UP:
        case AndroidMotionEvent::ACTION_POINTER_UP: {
            nsTouchEvent event(PR_TRUE, NS_TOUCH_END, this);
            return DispatchMultitouchEvent(event, ae);
        }
        case AndroidMotionEvent::ACTION_OUTSIDE:
        case AndroidMotionEvent::ACTION_CANCEL: {
            nsTouchEvent event(PR_TRUE, NS_TOUCH_CANCEL, this);
            return DispatchMultitouchEvent(event, ae);
        }
    }
    return false;
}

bool
nsWindow::DispatchMultitouchEvent(nsTouchEvent &event, AndroidGeckoEvent *ae)
{
    nsIntPoint offset = WidgetToScreenOffset();

    event.isShift = false;
    event.isControl = false;
    event.isMeta = false;
    event.isAlt = false;
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
    bool preventPanning = (status == nsEventStatus_eConsumeNoDefault);
    if (preventPanning || action == AndroidMotionEvent::ACTION_MOVE) {
        AndroidBridge::Bridge()->SetPreventPanning(preventPanning);
    }
    return preventPanning;
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

    event.isShift = false;
    event.isControl = false;
    event.isMeta = false;
    event.isAlt = false;
    event.time = time;
    event.refPoint = refPoint;

    DispatchEvent(&event);
}


void
nsWindow::DispatchMotionEvent(nsInputEvent &event, AndroidGeckoEvent *ae,
                              const nsIntPoint &refPoint)
{
    nsIntPoint offset = WidgetToScreenOffset();

    event.isShift = PR_FALSE;
    event.isControl = PR_FALSE;
    event.isMeta = PR_FALSE;
    event.isAlt = PR_FALSE;
    event.time = ae->Time();

    // XXX possibly bound the range of event.refPoint here.
    //     some code may get confused.
    event.refPoint = refPoint - offset;

    DispatchEvent(&event);
}

void
nsWindow::InitKeyEvent(nsKeyEvent& event, AndroidGeckoEvent& key)
{
    switch (key.KeyCode()) {
    case AndroidKeyEvent::KEYCODE_UNKNOWN:
    case AndroidKeyEvent::KEYCODE_HOME:
        break;
    case AndroidKeyEvent::KEYCODE_BACK:
        event.keyCode = NS_VK_ESCAPE;
        break;
    case AndroidKeyEvent::KEYCODE_CALL:
    case AndroidKeyEvent::KEYCODE_ENDCALL:
        break;
    case AndroidKeyEvent::KEYCODE_0:
    case AndroidKeyEvent::KEYCODE_1:
    case AndroidKeyEvent::KEYCODE_2:
    case AndroidKeyEvent::KEYCODE_3:
    case AndroidKeyEvent::KEYCODE_4:
    case AndroidKeyEvent::KEYCODE_5:
    case AndroidKeyEvent::KEYCODE_6:
    case AndroidKeyEvent::KEYCODE_7:
    case AndroidKeyEvent::KEYCODE_8:
    case AndroidKeyEvent::KEYCODE_9:
        event.keyCode = key.KeyCode() - AndroidKeyEvent::KEYCODE_0 + NS_VK_0;
        break;
    case AndroidKeyEvent::KEYCODE_STAR:
        event.keyCode = NS_VK_MULTIPLY;
        break;
    case AndroidKeyEvent::KEYCODE_POUND:
        break;
    case AndroidKeyEvent::KEYCODE_DPAD_UP:
        event.keyCode = NS_VK_UP;
        break;
    case AndroidKeyEvent::KEYCODE_DPAD_DOWN:
        event.keyCode = NS_VK_DOWN;
        break;
    case AndroidKeyEvent::KEYCODE_SOFT_LEFT:
    case AndroidKeyEvent::KEYCODE_DPAD_LEFT:
        event.keyCode = NS_VK_LEFT;
        break;
    case AndroidKeyEvent::KEYCODE_SOFT_RIGHT:
    case AndroidKeyEvent::KEYCODE_DPAD_RIGHT:
        event.keyCode = NS_VK_RIGHT;
        break;
    case AndroidKeyEvent::KEYCODE_VOLUME_UP:
    case AndroidKeyEvent::KEYCODE_VOLUME_DOWN:
    case AndroidKeyEvent::KEYCODE_POWER:
    case AndroidKeyEvent::KEYCODE_CAMERA:
    case AndroidKeyEvent::KEYCODE_CLEAR:
        break;
    case AndroidKeyEvent::KEYCODE_A:
    case AndroidKeyEvent::KEYCODE_B:
    case AndroidKeyEvent::KEYCODE_C:
    case AndroidKeyEvent::KEYCODE_D:
    case AndroidKeyEvent::KEYCODE_E:
    case AndroidKeyEvent::KEYCODE_F:
    case AndroidKeyEvent::KEYCODE_G:
    case AndroidKeyEvent::KEYCODE_H:
    case AndroidKeyEvent::KEYCODE_I:
    case AndroidKeyEvent::KEYCODE_J:
    case AndroidKeyEvent::KEYCODE_K:
    case AndroidKeyEvent::KEYCODE_L:
    case AndroidKeyEvent::KEYCODE_M:
    case AndroidKeyEvent::KEYCODE_N:
    case AndroidKeyEvent::KEYCODE_O:
    case AndroidKeyEvent::KEYCODE_P:
    case AndroidKeyEvent::KEYCODE_Q:
    case AndroidKeyEvent::KEYCODE_R:
    case AndroidKeyEvent::KEYCODE_S:
    case AndroidKeyEvent::KEYCODE_T:
    case AndroidKeyEvent::KEYCODE_U:
    case AndroidKeyEvent::KEYCODE_V:
    case AndroidKeyEvent::KEYCODE_W:
    case AndroidKeyEvent::KEYCODE_X:
    case AndroidKeyEvent::KEYCODE_Y:
    case AndroidKeyEvent::KEYCODE_Z:
        event.keyCode = key.KeyCode() - AndroidKeyEvent::KEYCODE_A + NS_VK_A;
        break;
    case AndroidKeyEvent::KEYCODE_COMMA:
        event.keyCode = NS_VK_COMMA;
        break;
    case AndroidKeyEvent::KEYCODE_PERIOD:
        event.keyCode = NS_VK_PERIOD;
        break;
    case AndroidKeyEvent::KEYCODE_ALT_LEFT:
    case AndroidKeyEvent::KEYCODE_ALT_RIGHT:
    case AndroidKeyEvent::KEYCODE_SHIFT_LEFT:
    case AndroidKeyEvent::KEYCODE_SHIFT_RIGHT:
        break;
    case AndroidKeyEvent::KEYCODE_TAB:
        event.keyCode = NS_VK_TAB;
        break;
    case AndroidKeyEvent::KEYCODE_SPACE:
        event.keyCode = NS_VK_SPACE;
        break;
    case AndroidKeyEvent::KEYCODE_SYM:
    case AndroidKeyEvent::KEYCODE_EXPLORER:
    case AndroidKeyEvent::KEYCODE_ENVELOPE:
        break;
    case AndroidKeyEvent::KEYCODE_DPAD_CENTER:
    case AndroidKeyEvent::KEYCODE_ENTER:
        event.keyCode = NS_VK_RETURN;
        break;
    case AndroidKeyEvent::KEYCODE_DEL:
        event.keyCode = NS_VK_BACK;
        break;
    case AndroidKeyEvent::KEYCODE_GRAVE:
        break;
    case AndroidKeyEvent::KEYCODE_MINUS:
        event.keyCode = NS_VK_SUBTRACT;
        break;
    case AndroidKeyEvent::KEYCODE_EQUALS:
        event.keyCode = NS_VK_EQUALS;
        break;
    case AndroidKeyEvent::KEYCODE_LEFT_BRACKET:
        event.keyCode = NS_VK_OPEN_BRACKET;
        break;
    case AndroidKeyEvent::KEYCODE_RIGHT_BRACKET:
        event.keyCode = NS_VK_CLOSE_BRACKET;
        break;
    case AndroidKeyEvent::KEYCODE_BACKSLASH:
        event.keyCode = NS_VK_BACK_SLASH;
        break;
    case AndroidKeyEvent::KEYCODE_SEMICOLON:
        event.keyCode = NS_VK_SEMICOLON;
        break;
    case AndroidKeyEvent::KEYCODE_APOSTROPHE:
        event.keyCode = NS_VK_QUOTE;
        break;
    case AndroidKeyEvent::KEYCODE_SLASH:
        event.keyCode = NS_VK_SLASH;
        break;
    case AndroidKeyEvent::KEYCODE_AT:
    case AndroidKeyEvent::KEYCODE_NUM:
    case AndroidKeyEvent::KEYCODE_HEADSETHOOK:
    case AndroidKeyEvent::KEYCODE_FOCUS:
        break;
    case AndroidKeyEvent::KEYCODE_PLUS:
        event.keyCode = NS_VK_ADD;
        break;
    case AndroidKeyEvent::KEYCODE_MENU:
    case AndroidKeyEvent::KEYCODE_NOTIFICATION:
    case AndroidKeyEvent::KEYCODE_SEARCH:
    case AndroidKeyEvent::KEYCODE_MEDIA_PLAY_PAUSE:
    case AndroidKeyEvent::KEYCODE_MEDIA_STOP:
    case AndroidKeyEvent::KEYCODE_MEDIA_NEXT:
    case AndroidKeyEvent::KEYCODE_MEDIA_PREVIOUS:
    case AndroidKeyEvent::KEYCODE_MEDIA_REWIND:
    case AndroidKeyEvent::KEYCODE_MEDIA_FAST_FORWARD:
    case AndroidKeyEvent::KEYCODE_MUTE:
        break;
    default:
        ALOG("Unknown key code!");
        break;
    }

    // Android gives us \n, so filter out some control characters.
    if (event.message == NS_KEY_PRESS &&
        key.UnicodeChar() >= ' ') {
        event.charCode = key.UnicodeChar();
        if (key.UnicodeChar())
            event.keyCode = 0;
    }
    event.isShift = !!(key.MetaState() & AndroidKeyEvent::META_SHIFT_ON);
    event.isControl = gMenu;
    event.isAlt = !!(key.MetaState() & AndroidKeyEvent::META_ALT_ON);
    event.isMeta = false;
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
                InitKeyEvent(pressEvent, *ae);
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
    InitKeyEvent(event, *ae);
    DispatchEvent(&event, status);

    if (Destroyed())
        return;
    if (!firePress)
        return;

    nsKeyEvent pressEvent(true, NS_KEY_PRESS, this);
    InitKeyEvent(pressEvent, *ae);
    if (status == nsEventStatus_eConsumeNoDefault) {
        pressEvent.flags |= NS_EVENT_FLAG_NO_DEFAULT;
    }
#ifdef DEBUG_ANDROID_WIDGET
    __android_log_print(ANDROID_LOG_INFO, "Gecko", "Dispatching key pressEvent with keyCode %d charCode %d shift %d alt %d sym/ctrl %d metamask %d", pressEvent.keyCode, pressEvent.charCode, pressEvent.isShift, pressEvent.isAlt, pressEvent.isControl, ae->MetaState());
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
            mIMELastDispatchedComposingText.Truncate();
            nsCompositionEvent event(true, NS_COMPOSITION_START, this);
            InitEvent(event, nsnull);
            DispatchEvent(&event);
        }
        return;
    case AndroidGeckoEvent::IME_ADD_RANGE:
        {
            OnIMEAddRange(ae);
        }
        return;
    case AndroidGeckoEvent::IME_SET_TEXT:
        {
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

            ALOGIME("IME: IME_SET_TEXT: l=%u, r=%u",
                event.theText.Length(), mIMERanges.Length());

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
            //ALOGIME("IME:     -> l=%u", event.mReply.mString.Length());
        }
        return;
    case AndroidGeckoEvent::IME_DELETE_TEXT:
        {   
            ALOGIME("IME: IME_DELETE_TEXT");
            nsKeyEvent event(true, NS_KEY_PRESS, this);
            InitEvent(event, nsnull);
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

    AndroidBridge::NotifyIMEEnabled(int(aContext.mIMEState.mEnabled),
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
nsWindow::DrawWindowUnderlay(LayerManager* aManager, nsIntRect aRect) {
    AndroidBridge::AutoLocalJNIFrame jniFrame(GetJNIForThread());

    AndroidGeckoLayerClient& client = AndroidBridge::Bridge()->GetLayerClient();
    client.CreateFrame(mLayerRendererFrame);

    client.ActivateProgram();
    mLayerRendererFrame.BeginDrawing();
    mLayerRendererFrame.DrawBackground();
    client.DeactivateProgram();
}

void
nsWindow::DrawWindowOverlay(LayerManager* aManager, nsIntRect aRect) {
    AndroidBridge::AutoLocalJNIFrame jniFrame(GetJNIForThread());
    NS_ABORT_IF_FALSE(!mLayerRendererFrame.isNull(),
                      "Frame should have been created in DrawWindowUnderlay()!");

    AndroidGeckoLayerClient& client = AndroidBridge::Bridge()->GetLayerClient();

    client.ActivateProgram();
    mLayerRendererFrame.DrawForeground();
    mLayerRendererFrame.EndDrawing();
    client.DeactivateProgram();

    mLayerRendererFrame.Dispose();
}

// off-main-thread compositor fields and functions

nsRefPtr<mozilla::layers::CompositorParent> nsWindow::sCompositorParent = 0;
base::Thread * nsWindow::sCompositorThread = 0;

void
nsWindow::SetCompositorParent(mozilla::layers::CompositorParent* aCompositorParent,
                              ::base::Thread* aCompositorThread)
{
    sCompositorParent = aCompositorParent;
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
nsWindow::ScheduleResumeComposition()
{
    if (sCompositorParent) {
        sCompositorParent->ScheduleResumeOnCompositorThread();
    }
}

#endif

