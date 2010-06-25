/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
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

#include "nsAppShell.h"
#include "nsIdleService.h"
#include "nsWindow.h"

#include "nsIDeviceContext.h"
#include "nsIRenderingContext.h"

#include "nsWidgetAtoms.h"
#include "nsWidgetsCID.h"
#include "nsGfxCIID.h"

#include "gfxImageSurface.h"
#include "gfxContext.h"

#include "nsTArray.h"

#include "AndroidBridge.h"

/* OpenGL */
#define USE_GLES2

#ifdef USE_GLES2
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#else
#include <GLES/gl.h>
#include <GLES/glext.h>
#endif

#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif

using namespace mozilla;

NS_IMPL_ISUPPORTS_INHERITED0(nsWindow, nsBaseWidget)

// The dimensions of the current android view
static gfxIntSize gAndroidBounds;

static PRBool gLeftShift;
static PRBool gRightShift;
static PRBool gLeftAlt;
static PRBool gRightAlt;
static PRBool gSym;

// All the toplevel windows that have been created; these are in
// stacking order, so the window at gAndroidBounds[0] is the topmost
// one.
static nsTArray<nsWindow*> gTopLevelWindows;
static nsWindow* gFocusedWindow = nsnull;
static PRUint32 gIMEState;

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
    for (int i = 0; i < wins.Length(); ++i) {
        nsWindow *w = wins[i];
        LogWindow(w, i, indent);
        DumpWindows(w->mChildren, indent+1);
    }
}

nsWindow::nsWindow() :
    mIsVisible(PR_FALSE),
    mParent(nsnull),
    mSpecialKeyTracking(0)
{
}

nsWindow::~nsWindow()
{
    gTopLevelWindows.RemoveElement(this);
    if (gFocusedWindow == this)
        gFocusedWindow = nsnull;
    ALOG("nsWindow %p destructor", (void*)this);
}

PRBool
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
                 nsIDeviceContext *aContext,
                 nsIAppShell *aAppShell,
                 nsIToolkit *aToolkit,
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

    BaseCreate(nsnull, mBounds, aHandleEventFunction, aContext,
               aAppShell, aToolkit, aInitData);

    NS_ASSERTION(IsTopLevel() || parent, "non top level windowdoesn't have a parent!");

    if (IsTopLevel()) {
        gTopLevelWindows.AppendElement(this);
    }

    if (parent) {
        parent->mChildren.AppendElement(this);
        mParent = parent;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Destroy(void)
{
    for (PRUint32 i = 0; i < mChildren.Length(); ++i) {
        // why do we still have children?
        ALOG("### Warning: Destroying window %p and reparenting child %p to null!", (void*)this, (void*)mChildren[i]);
        mChildren[i]->SetParent(nsnull);
    }

    if (IsTopLevel())
        gTopLevelWindows.RemoveElement(this);

    if (mParent)
        mParent->mChildren.RemoveElement(this);

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
                         PR_FALSE);
    }

    return NS_OK;
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
        nsAppShell::gAppShell->PostEvent(new AndroidGeckoEvent(TopWindow(), -1, -1, -1, -1));

    return NS_OK;
}

nsIWidget*
nsWindow::GetParent()
{
    return mParent;
}

NS_IMETHODIMP
nsWindow::Show(PRBool aState)
{
    ALOG("nsWindow[%p]::Show %d", (void*)this, aState);

    if (mWindowType == eWindowType_invisible) {
        ALOG("trying to show invisible window! ignoring..");
        return NS_ERROR_FAILURE;
    }

    if ((aState && !mIsVisible) ||
        (!aState && mIsVisible))
    {
        mIsVisible = aState;

        if (IsTopLevel()) {
            // XXX should we bring this to the front when it's shown,
            // if it's a toplevel widget?

            // XXX we should synthesize a NS_MOUSE_EXIT (for old top
            // window)/NS_MOUSE_ENTER (for new top window) since we need
            // to pretend that the top window always has focus.  Not sure
            // if Show() is the right place to do this, though.

            if (mIsVisible) {
                // It just became visible, so send a resize update if necessary
                // and bring it to the front.
                Resize(0, 0, gAndroidBounds.width, gAndroidBounds.height, PR_FALSE);
                BringToFront();
            }
        } else if (FindTopLevel() == TopWindow()) {
            nsAppShell::gAppShell->PostEvent(new AndroidGeckoEvent(TopWindow(), -1, -1, -1, -1));
        }
    }

#ifdef ANDROID_DEBUG_WIDGET
    DumpWindows();
#endif

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetModal(PRBool aState)
{
    ALOG("nsWindow[%p]::SetModal %d ignored", (void*)this, aState);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::IsVisible(PRBool& aState)
{
    aState = mIsVisible;
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::ConstrainPosition(PRBool aAllowSlop,
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
                  PR_TRUE);
}

NS_IMETHODIMP
nsWindow::Resize(PRInt32 aWidth,
                 PRInt32 aHeight,
                 PRBool aRepaint)
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
                 PRBool aRepaint)
{
    ALOG("nsWindow[%p]::Resize [%d %d %d %d] (repaint %d)", (void*)this, aX, aY, aWidth, aHeight, aRepaint);

    PRBool needSizeDispatch = aWidth != mBounds.width || aHeight != mBounds.height;

    if (IsTopLevel()) {
        ALOG("... ignoring Resize sizes on toplevel window");
        aX = 0;
        aY = 0;
        aWidth = gAndroidBounds.width;
        aHeight = gAndroidBounds.height;
    }

    mBounds.x = aX;
    mBounds.y = aY;
    mBounds.width = aWidth;
    mBounds.height = aHeight;

    if (needSizeDispatch)
        OnSizeChanged(gfxIntSize(aWidth, aHeight));

    // Should we skip honoring aRepaint here?
    if (aRepaint && FindTopLevel() == TopWindow())
        nsAppShell::gAppShell->PostEvent(new AndroidGeckoEvent(TopWindow(), -1, -1, -1, -1));

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
                      PRBool aActivate)
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
    }
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Enable(PRBool aState)
{
    ALOG("nsWindow[%p]::Enable %d ignored", (void*)this, aState);
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::IsEnabled(PRBool *aState)
{
    *aState = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Invalidate(const nsIntRect &aRect,
                     PRBool aIsSynchronous)
{
    ALOG("nsWindow::Invalidate %p [%d %d %d %d]", (void*) this, aRect.x, aRect.y, aRect.width, aRect.height);
    nsAppShell::gAppShell->PostEvent(new AndroidGeckoEvent(TopWindow(), -1, -1, -1, -1));
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Update()
{
    return NS_OK;
}

void
nsWindow::Scroll(const nsIntPoint&,
                 const nsTArray<nsIntRect>&,
                 const nsTArray<nsIWidget::Configuration>&)
{
    ALOG("nsWindow[%p]::Scroll ignored!", (void*)this);
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
nsWindow::SetFocus(PRBool aRaise)
{
    if (!aRaise)
        ALOG("nsWindow::SetFocus: can't set focus without raising, ignoring aRaise = false!");

    gFocusedWindow = this;
    FindTopLevel()->BringToFront();

    return NS_OK;
}

void
nsWindow::BringToFront()
{
    if (FindTopLevel() == TopWindow())
        return;

    if (!IsTopLevel()) {
        FindTopLevel()->BringToFront();
        return;
    }

    nsWindow *oldTop = nsnull;
    if (!gTopLevelWindows.IsEmpty())
        oldTop = gTopLevelWindows[0];

    gTopLevelWindows.RemoveElement(this);
    gTopLevelWindows.InsertElementAt(0, this);

    if (oldTop) {
        nsGUIEvent event(PR_TRUE, NS_DEACTIVATE, gTopLevelWindows[0]);
        DispatchEvent(&event);
    }

    nsGUIEvent event(PR_TRUE, NS_ACTIVATE, this);
    DispatchEvent(&event);

    nsAppShell::gAppShell->PostEvent(new AndroidGeckoEvent(TopWindow(), -1, -1, -1, -1));
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
    if (mEventCallback)
        return (*mEventCallback)(aEvent);
    return nsEventStatus_eIgnore;
}

NS_IMETHODIMP
nsWindow::SetWindowClass(const nsAString& xulWinType)
{
    return NS_OK;
}

gfxASurface*
nsWindow::GetThebesSurface()
{
    /* This is really a dummy surface; this is only used when doing reflow, because
     * we need a RenderingContext to measure text against.
     */

    // XXX this really wants to return already_AddRefed, but this only really gets used
    // on direct assignment to a gfxASurface
    return new gfxImageSurface(gfxIntSize(5,5), gfxImageSurface::ImageFormatRGB24);
}

void
nsWindow::OnGlobalAndroidEvent(AndroidGeckoEvent *ae)
{
    switch (ae->Type()) {
        case AndroidGeckoEvent::SIZE_CHANGED: {
            int nw = ae->P0().x;
            int nh = ae->P0().y;

            if (nw == gAndroidBounds.width &&
                nh == gAndroidBounds.height)
            {
                return;
            }

            gAndroidBounds.width = nw;
            gAndroidBounds.height = nh;

            // tell all the windows about the new size
            for (size_t i = 0; i < gTopLevelWindows.Length(); ++i) {
                if (gTopLevelWindows[i]->mIsVisible)
                    gTopLevelWindows[i]->Resize(gAndroidBounds.width, gAndroidBounds.height, PR_TRUE);
            }

            break;
        }

        case AndroidGeckoEvent::MOTION_EVENT: {
            TopWindow()->UserActivity();
            if (!gTopLevelWindows.IsEmpty()) {
                nsIntPoint pt(ae->P0());
                pt.x = NS_MIN(NS_MAX(pt.x, 0), gAndroidBounds.width - 1);
                pt.y = NS_MIN(NS_MAX(pt.y, 0), gAndroidBounds.height - 1);
                nsWindow *target = TopWindow()->FindWindowForPoint(pt);

#if 0
                ALOG("MOTION_EVENT %f,%f -> %p (visible: %d children: %d)", ae->P0().x, ae->P0().y, (void*)target,
                     target ? target->mIsVisible : 0,
                     target ? target->mChildren.Length() : 0);

                DumpWindows();
#endif

                if (target)
                    target->OnMotionEvent(ae);
            }
            break;
        }

        case AndroidGeckoEvent::KEY_EVENT:
            TopWindow()->UserActivity();
            if (gFocusedWindow)
                gFocusedWindow->OnKeyEvent(ae);
            break;

        case AndroidGeckoEvent::DRAW:
            if (TopWindow())
                TopWindow()->OnDraw(ae);
            break;

        case AndroidGeckoEvent::IME_EVENT:
            TopWindow()->UserActivity();
            if (gFocusedWindow) {
                gFocusedWindow->OnIMEEvent(ae);
            } else {
                NS_WARNING("Sending unexpected IME event to top window");
                TopWindow()->OnIMEEvent(ae);
            }
            break;

        default:
            break;
    }
}

void
nsWindow::OnAndroidEvent(AndroidGeckoEvent *ae)
{
    switch (ae->Type()) {
        case AndroidGeckoEvent::DRAW:
            OnDraw(ae);
            break;

        default:
            ALOG("Window got targetted android event type %d, but didn't handle!", ae->Type());
            break;
    }
}

PRBool
nsWindow::DrawTo(gfxASurface *targetSurface)
{
    if (!mIsVisible)
        return PR_FALSE;

    nsRefPtr<gfxContext> ctx = new gfxContext(targetSurface);

    nsIntRect boundsRect(0, 0, mBounds.width, mBounds.height);

    nsPaintEvent event(PR_TRUE, NS_PAINT, this);
    event.region = boundsRect;
    {
        AutoLayerManagerSetup setupLayerManager(this, ctx);
        nsEventStatus status = DispatchEvent(&event);
    }

    // XXX uhh.. we can't just ignore this because we no longer have
    // what we needed before, but let's keep drawing the children anyway?
#if 0
    if (status == nsEventStatus_eIgnore)
        return PR_FALSE;
#endif

    // XXX if we got an ignore for the parent, do we still want to draw the children?
    // We don't really have a good way not to...

    gfxPoint offset = targetSurface->GetDeviceOffset();

    for (PRUint32 i = 0; i < mChildren.Length(); ++i) {
        targetSurface->SetDeviceOffset(offset + gfxPoint(mChildren[i]->mBounds.x,
                                                         mChildren[i]->mBounds.y));
        if (!mChildren[i]->DrawTo(targetSurface)) {
            ALOG("nsWindow[%p]::DrawTo child %d[%p] returned FALSE!", (void*) this, i, (void*)mChildren[i]);
        }
    }

    targetSurface->SetDeviceOffset(offset);

    return PR_TRUE;
}

static int
next_power_of_two(int v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    return v;
}

void
nsWindow::OnDraw(AndroidGeckoEvent *ae)
{
    AndroidBridge::AutoLocalJNIFrame jniFrame;

    static bool firstDraw = true;

    ALOG(">> OnDraw");

    AndroidGeckoSurfaceView& sview(AndroidBridge::Bridge()->SurfaceView());

    NS_ASSERTION(!sview.isNull(), "SurfaceView is null!");

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

    int drawType = sview.BeginDrawing();

    if (drawType == AndroidGeckoSurfaceView::DRAW_ERROR) {
        ALOG("##### BeginDrawing failed!");
        return;
    }

    nsRefPtr<gfxImageSurface> targetSurface;

    if (drawType == AndroidGeckoSurfaceView::DRAW_SOFTWARE) {
        int bufCap;
        unsigned char *buf = sview.GetSoftwareDrawBuffer(&bufCap);
        if (!buf || bufCap != mBounds.width * mBounds.height * 4) {
            ALOG("### Software drawing, but too small a buffer %d expected %d (or no buffer %p)!", bufCap, mBounds.width * mBounds.height * 4, (void*)buf);
            sview.EndDrawing();
            return;
        }
        targetSurface =
            new gfxImageSurface(buf,
                                gfxIntSize(mBounds.width, mBounds.height),
                                mBounds.width * 4,
                                gfxASurface::ImageFormatARGB32);

        DrawTo(targetSurface);

        // need to swap B and R channels, to get ABGR instead of ARGB
        unsigned int *ibuf = (unsigned int*) buf;
        unsigned int *ibufMax = ibuf + mBounds.width * mBounds.height;
        while (ibuf < ibufMax) {
            *ibuf++ = (*ibuf & 0xff00ff00) | ((*ibuf & 0x00ff0000) >> 16) | ((*ibuf & 0x000000ff) << 16);
        }

        sview.EndDrawing();
        return;
    }

    targetSurface =
        new gfxImageSurface(gfxIntSize(mBounds.width, mBounds.height), gfxASurface::ImageFormatARGB32);

    if (!DrawTo(targetSurface)) {
        sview.EndDrawing();
        return;
    }

#ifdef USE_GLES2
    if (drawType != AndroidGeckoSurfaceView::DRAW_GLES_2) {
        ALOG("#### GL drawing path, but beingDrawing wanted something else!");
        sview.EndDrawing();
        return;
    }

    static bool hasBGRA = false;

    if (firstDraw) {
        const char *ext = (const char *) glGetString(GL_EXTENSIONS);
        ALOG("GL extensions: %s", ext);
        if (strstr(ext, "GL_EXT_bgra") ||
            strstr(ext, "GL_IMG_texture_format_BGRA8888") ||
            strstr(ext, "GL_EXT_texture_format_BGRA8888"))
            hasBGRA = true;

        firstDraw = false;
    }

    static GLuint textureId = GLuint(-1);
    static GLuint programId = GLuint(-1);
    static GLint positionLoc, texCoordLoc, textureLoc;
    if (textureId == GLuint(-1) || !glIsTexture(textureId)) {
        glGenTextures(1, &textureId);

        glBindTexture(GL_TEXTURE_2D, textureId);

        /* Set required NPOT texture params, for baseline ES2 NPOT support */
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        const char *vsString =
            "attribute vec3 aPosition; \n"
            "attribute vec2 aTexCoord; \n"
            "varying vec2 vTexCoord; \n"
            "void main() { \n"
            "  gl_Position = vec4(aPosition, 1.0); \n"
            "  vTexCoord = aTexCoord; \n"
            "}";

        /* Note that while the swizzle is, afaik, "free", the texture upload
         * can potentially be faster if the native hardware format is BGRA.  So
         * we use BGRA if it's available.
         */

        const char *fsStringNoBGRA =
            "precision mediump float; \n"
            "varying vec2 vTexCoord; \n"
            "uniform sampler2D sTexture; \n"
            "void main() { \n"
            "  gl_FragColor = vec4(texture2D(sTexture, vTexCoord).bgr, 1.0); \n"
            "}";

        const char *fsStringBGRA =
            "precision mediump float; \n"
            "varying vec2 vTexCoord; \n"
            "uniform sampler2D sTexture; \n"
            "void main() { \n"
            "  gl_FragColor = vec4(texture2D(sTexture, vTexCoord).rgb, 1.0); \n"
            "}";

        GLint status;

        GLuint vsh = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vsh, 1, &vsString, NULL);
        glCompileShader(vsh);
        glGetShaderiv(vsh, GL_COMPILE_STATUS, &status);
        if (!status) {
            ALOG("Failed to compile vertex shader");
            return;
        }

        GLuint fsh = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fsh, 1, hasBGRA ? &fsStringBGRA : &fsStringNoBGRA, NULL);
        glCompileShader(fsh);
        glGetShaderiv(fsh, GL_COMPILE_STATUS, &status);
        if (!status) {
            ALOG("Failed to compile fragment shader");
            return;
        }

        programId = glCreateProgram();
        glAttachShader(programId, vsh);
        glAttachShader(programId, fsh);

        glLinkProgram(programId);
        glGetProgramiv(programId, GL_LINK_STATUS, &status);
        if (!status) {
            ALOG("Failed to link program");
            return;
        }

        positionLoc = glGetAttribLocation(programId, "aPosition");
        texCoordLoc = glGetAttribLocation(programId, "aTexCoord");
        textureLoc = glGetUniformLocation(programId, "sTexture");
    }

    int texDimWidth = targetSurface->Width();
    int texDimHeight = targetSurface->Height();

    glClearColor(1.0f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glFrontFace(GL_CCW);

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    glBindTexture(GL_TEXTURE_2D, textureId);

    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 hasBGRA ? GL_BGRA_EXT : GL_RGBA,
                 texDimWidth,
                 texDimHeight,
                 0,
                 hasBGRA ? GL_BGRA_EXT : GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 targetSurface->Data());

    GLfloat texCoords[] = { 0.0f, 1.0f,
                            0.0f, 0.0f,
                            1.0f, 1.0f,
                            1.0f, 0.0f };

    GLfloat vCoords[] = { -1.0f, -1.0f, 0.0f,
                          -1.0f,  1.0f, 0.0f,
                           1.0f, -1.0f, 0.0f,
                           1.0f,  1.0f, 0.0f };

    glUseProgram(programId);

    glVertexAttribPointer(positionLoc, 3, GL_FLOAT, GL_FALSE, 0, vCoords);
    glVertexAttribPointer(texCoordLoc, 2, GL_FLOAT, GL_FALSE, 0, texCoords);

    glEnableVertexAttribArray(positionLoc);
    glEnableVertexAttribArray(texCoordLoc);

    glUniform1i(textureLoc, 0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    int err = glGetError();
    if (err)
        ALOG("GL error: %d", err);
#else

    /* GLES 1.0[1?] code.
     *
     * Two things:
     *   NPOT textures are not supported by default, unlike with GLES 2
     *   BGRA texture support is generally available, and might have
     *   one of three different extension names.
     */

    static bool hasBGRA = false;
    static bool hasNPOT = false;
    static bool hasDrawTex = false;

    if (firstDraw) {
        const char *ext = (const char *) glGetString(GL_EXTENSIONS);
        ALOG("GL extensions: %s", ext);
        if (strstr(ext, "GL_EXT_bgra") ||
            strstr(ext, "GL_IMG_texture_format_BGRA8888") ||
            strstr(ext, "GL_EXT_texture_format_BGRA8888"))
            hasBGRA = true;

        if (strstr(ext, "GL_ARB_texture_non_power_of_two"))
            hasNPOT = true;

        if (strstr(ext, "GL_OES_draw_texture"))
            hasDrawTex = true;

        if (!hasBGRA)
            ALOG("No BGRA extension found -- colors will be weird! XXX FIXME");

        firstDraw = false;
    }

    glClearColor(1.0f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // This is all the way a hack and will need to be changed with
    // real code to handle lost/reset context issues (e.g. when
    // rotating.
    static int lastTexDimWidth = -1;
    static int lastTexDimHeight = -1;
    static GLuint textureId = GLuint(-1);
    if (textureId == GLuint(-1) || !glIsTexture(textureId)) {
        glGenTextures(1, &textureId);

        // Reset these to ensure that the non-NPOT path
        // calls TexImage2D first.
        lastTexDimWidth = -1;
        lastTexDimHeight = -1;
    }

    glBindTexture(GL_TEXTURE_2D, textureId);

    int texDimWidth = targetSurface->Width();
    int texDimHeight = targetSurface->Height();

    // Not sure if it would be faster to just call TexImage2D
    // if we have NPOT textures -- I'd hope that the driver
    // can turn that SubImage2D to an equivalent operation,
    // given that the dimensions are going to cover the full
    // size of the texture.


    // There seems to be a Tegra bug here, where we can't
    // do TexSubImage2D with GL_BGRA_EXT.  We have NPOT there,
    // but it means that we have to have a separate codepath
    // for when we have NPOT to avoid the update with SubImage2D.

    if (hasNPOT) {
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     hasBGRA ? GL_BGRA_EXT : GL_RGBA,
                     texDimWidth,
                     texDimHeight,
                     0,
                     hasBGRA ? GL_BGRA_EXT : GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     targetSurface->Data());
    } else {
        texDimWidth = next_power_of_two(targetSurface->Width());
        texDimHeight = next_power_of_two(targetSurface->Height());

        if (lastTexDimWidth != texDimWidth ||
            lastTexDimHeight != texDimHeight)
        {
            // Set the texture size, but don't load
            // data.
            glTexImage2D(GL_TEXTURE_2D,
                         0,
                         hasBGRA ? GL_BGRA_EXT : GL_RGBA,
                         texDimWidth,
                         texDimHeight,
                         0,
                         hasBGRA ? GL_BGRA_EXT : GL_RGBA,
                         GL_UNSIGNED_BYTE,
                         NULL);

            lastTexDimWidth = texDimWidth;
            lastTexDimHeight = texDimHeight;
        }

        // then actually load the data in the sub-rectangle
        glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        0, 0,
                        targetSurface->Width(),
                        targetSurface->Height(),
                        hasBGRA ? GL_BGRA_EXT : GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        targetSurface->Data());
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glEnable(GL_TEXTURE_2D);

    glFrontFace(GL_CCW);

    glDisable(GL_CULL_FACE);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    GLfloat texCoordW = GLfloat(targetSurface->Width()) / GLfloat(texDimWidth);
    GLfloat texCoordH = GLfloat(targetSurface->Height()) / GLfloat(texDimHeight);

    GLfloat texCoords[] = { 0.0f, texCoordH,
                            0.0f, 0.0f,
                            texCoordW, texCoordH,
                            texCoordW, 0.0f };

    GLfloat vCoords[] = { -1.0f, -1.0f, 0.0f,
                          -1.0f,  1.0f, 0.0f,
                           1.0f, -1.0f, 0.0f,
                           1.0f,  1.0f, 0.0f };

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glVertexPointer(3, GL_FLOAT, 0, vCoords);
    glTexCoordPointer(2, GL_FLOAT, 0, texCoords);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
#endif

    sview.EndDrawing();
}

void
nsWindow::OnSizeChanged(const gfxIntSize& aSize)
{
    int w = aSize.width;
    int h = aSize.height;

    ALOG("nsWindow: %p OnSizeChanged [%d %d]", (void*)this, w, h);

    nsSizeEvent event(PR_TRUE, NS_SIZE, this);
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

void
nsWindow::SetInitialAndroidBounds(const gfxIntSize& sz)
{
    if (!gTopLevelWindows.IsEmpty()) {
        NS_WARNING("SetInitialAndroidBounds called way too late, we already have toplevel windows!");
    }

    gAndroidBounds = sz;
}

gfxIntSize
nsWindow::GetAndroidBounds()
{
    return gAndroidBounds;
}

void *
nsWindow::GetNativeData(PRUint32 aDataType)
{
    switch (aDataType) {
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
            // we don't handle any other motion events yet
            return;
    }

    nsIntPoint pt(ae->P0());
    nsIntPoint offset = WidgetToScreenOffset();

    //ALOG("#### motion pt: %d %d offset: %d %d", pt.x, pt.y, offset.x, offset.y);

    pt.x -= offset.x;
    pt.y -= offset.y;

    // XXX possibly bound the range of pt here. some code may get confused.

send_again:

    nsMouseEvent event(PR_TRUE,
                       msg, this,
                       nsMouseEvent::eReal, nsMouseEvent::eNormal);
    InitEvent(event, &pt);

    event.time = ae->Time();
    event.isShift = gLeftShift || gRightShift;
    event.isControl = gSym;
    event.isMeta = PR_FALSE;
    event.isAlt = gLeftAlt || gRightAlt;

    // XXX can we synthesize different buttons?
    event.button = nsMouseEvent::eLeftButton;

    if (msg != NS_MOUSE_MOVE)
        event.clickCount = 1;

    // XXX add the double-click handling logic here

    DispatchEvent(&event);

    if (msg == NS_MOUSE_BUTTON_DOWN) {
        msg = NS_MOUSE_MOVE;
        goto send_again;
    }
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

    event.charCode = key.UnicodeChar();
    event.isShift = gLeftShift || gRightShift;
    event.isControl = PR_FALSE;
    event.isAlt = PR_FALSE;
    event.isMeta = PR_FALSE;
    event.time = key.Time();
}

void
nsWindow::HandleSpecialKey(AndroidGeckoEvent *ae)
{
    nsCOMPtr<nsIAtom> command;
    PRBool isDown = ae->Action() == AndroidKeyEvent::ACTION_DOWN;
    PRBool doCommand = PR_FALSE;
    PRUint32 keyCode = ae->KeyCode();

    if (isDown) {
        switch (keyCode) {
            case AndroidKeyEvent::KEYCODE_BACK:
            case AndroidKeyEvent::KEYCODE_MENU:
            case AndroidKeyEvent::KEYCODE_SEARCH:
                mSpecialKeyTracking = keyCode;
                break;

            case AndroidKeyEvent::KEYCODE_VOLUME_UP:
                command = nsWidgetAtoms::VolumeUp;
                doCommand = PR_TRUE;
                break;
            case AndroidKeyEvent::KEYCODE_VOLUME_DOWN:
                command = nsWidgetAtoms::VolumeDown;
                doCommand = PR_TRUE;
                break;
        }
    } else {
        // Dispatch BACK, MENU, SEARCH key events only on key-up after the corresponding key-down
        if (mSpecialKeyTracking != keyCode) {
            mSpecialKeyTracking = 0;
            return;
        }
        switch (keyCode) {
            case AndroidKeyEvent::KEYCODE_BACK: {
                nsKeyEvent pressEvent(PR_TRUE, NS_KEY_PRESS, this);
                InitKeyEvent(pressEvent, *ae);
                DispatchEvent(&pressEvent);
                return;
            }
            case AndroidKeyEvent::KEYCODE_MENU:
                command = nsWidgetAtoms::Menu;
                doCommand = PR_TRUE;
                break;
            case AndroidKeyEvent::KEYCODE_SEARCH:
                command = nsWidgetAtoms::Search;
                doCommand = PR_TRUE;
                break;
            default:
                ALOG("Unknown special key code!");
                return;
        }
    }
    if (doCommand) {
        nsCommandEvent event(PR_TRUE, nsWidgetAtoms::onAppCommand, command, this);
        InitEvent(event);
        DispatchEvent(&event);
        mSpecialKeyTracking = 0;
    }
}

void
nsWindow::OnKeyEvent(AndroidGeckoEvent *ae)
{
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
            nsTextEvent event(PR_TRUE, NS_TEXT_TEXT, this);
            event.theText.Assign(ae->Characters());
            DispatchEvent(&event);
        }
        return;
    default:
        ALOG("Unknown key action event!");
        return;
    }

    PRBool isDown = ae->Action() == AndroidKeyEvent::ACTION_DOWN;
    switch (ae->KeyCode()) {
    case AndroidKeyEvent::KEYCODE_SHIFT_LEFT:
        gLeftShift = isDown;
        break;
    case AndroidKeyEvent::KEYCODE_SHIFT_RIGHT:
        gRightShift = isDown;
        break;
    case AndroidKeyEvent::KEYCODE_ALT_LEFT:
        gLeftAlt = isDown;
        break;
    case AndroidKeyEvent::KEYCODE_ALT_RIGHT:
        gRightAlt = isDown;
        break;
    case AndroidKeyEvent::KEYCODE_SYM:
        gSym = isDown;
        break;
    case AndroidKeyEvent::KEYCODE_BACK:
    case AndroidKeyEvent::KEYCODE_MENU:
    case AndroidKeyEvent::KEYCODE_SEARCH:
    case AndroidKeyEvent::KEYCODE_VOLUME_UP:
    case AndroidKeyEvent::KEYCODE_VOLUME_DOWN:
        HandleSpecialKey(ae);
        return;
    }

    mSpecialKeyTracking = 0;

    nsKeyEvent event(PR_TRUE, msg, this);
    InitKeyEvent(event, *ae);
    if (event.charCode)
        event.keyCode = 0;
    DispatchEvent(&event);

    if (isDown) {
        nsKeyEvent pressEvent(PR_TRUE, NS_KEY_PRESS, this);
        InitKeyEvent(pressEvent, *ae);
#ifdef ANDROID_DEBUG_WIDGET
        ALOG("Dispatching key event with keyCode %d charCode %d shift %d alt %d sym/ctrl %d metamask %d", event.keyCode, event.charCode, event.isShift, event.isAlt, event.isControl, ae->MetaState());
#endif
        DispatchEvent(&pressEvent);
    }
}

nsresult
nsWindow::GetCurrentOffset(PRUint32 &aOffset, PRUint32 &aLength)
{
    nsQueryContentEvent event(PR_TRUE, NS_QUERY_SELECTED_TEXT, this);
    DispatchEvent(&event);

    if (!event.mSucceeded)
        return NS_ERROR_FAILURE;

    aOffset = event.mReply.mOffset;
    aLength = event.mReply.mString.Length();
    return NS_OK;
}

nsresult
nsWindow::DeleteRange(int aOffset, int aLen)
{
    nsSelectionEvent selectEvent(PR_TRUE, NS_SELECTION_SET, this);
    selectEvent.mOffset = aOffset;
    selectEvent.mLength = aLen;
    DispatchEvent(&selectEvent);
    NS_ENSURE_TRUE(selectEvent.mSucceeded, NS_ERROR_FAILURE);

    nsContentCommandEvent event(PR_TRUE, NS_CONTENT_COMMAND_DELETE, this);
    DispatchEvent(&event);
    NS_ENSURE_TRUE(event.mSucceeded, NS_ERROR_FAILURE);

    return NS_OK;
}

void
nsWindow::OnIMEEvent(AndroidGeckoEvent *ae)
{
    switch (ae->Action()) {
    case AndroidGeckoEvent::IME_BATCH_END:
        {
            nsCompositionEvent event(PR_TRUE, NS_COMPOSITION_END, this);
            event.time = PR_Now() / 1000;
            DispatchEvent(&event);
        }
        return;
    case AndroidGeckoEvent::IME_BATCH_BEGIN:
        {
            nsCompositionEvent event(PR_TRUE, NS_COMPOSITION_START, this);
            event.time = PR_Now() / 1000;
            DispatchEvent(&event);
        }
        return;
    case AndroidGeckoEvent::IME_SET_TEXT:
        {
            nsTextEvent event(PR_TRUE, NS_TEXT_TEXT, this);
            event.theText.Assign(ae->Characters());
            event.time = PR_Now() / 1000;
            DispatchEvent(&event);
        }
        return;
    case AndroidGeckoEvent::IME_GET_TEXT:
        {
            PRUint32 offset, len;
            if (NS_FAILED(GetCurrentOffset(offset, len))) {
                AndroidBridge::Bridge()->ReturnIMEQueryResult(nsnull, 0, 0, 0);
                return;
            }

            PRUint32 readLen = ae->Count();
            PRUint32 readOffset = offset;
            if (!ae->Count() && !ae->Count2()) {
                readOffset = 0;
                readLen = PR_UINT32_MAX;
            } else if (!readLen) { // backwards
                readLen = ae->Count2();
                if (readLen > offset) {
                    readLen = offset;
                    readOffset = 0;
                } else
                    readOffset -= readLen;
            } else
                readOffset += len;

            nsQueryContentEvent event(PR_TRUE, NS_QUERY_TEXT_CONTENT, this);
            event.InitForQueryTextContent(0, PR_UINT32_MAX);
            DispatchEvent(&event);

            if (!event.mSucceeded) {
                AndroidBridge::Bridge()->ReturnIMEQueryResult(nsnull, 0, 0, 0);
                return;
            }

            nsAutoString textContent(Substring(event.mReply.mString, readOffset, readLen));
            AndroidBridge::Bridge()->ReturnIMEQueryResult(textContent.get(), textContent.Length(), offset, offset + len);
        }
        return;
    case AndroidGeckoEvent::IME_DELETE_TEXT:
        {
            PRUint32 offset, len;
            PRUint32 count = ae->Count();
            if (NS_FAILED(GetCurrentOffset(offset, len)))
                return;

            DeleteRange(offset + len, ae->Count2());
            offset -= ae->Count();
            if (offset < 0) {
                count += offset;
                offset = 0;
            }
            DeleteRange(offset, count);
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
nsWindow::SetIMEEnabled(PRUint32 aState)
{
    gIMEState = aState;
    if (AndroidBridge::Bridge())
        AndroidBridge::Bridge()->ShowIME(aState);
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::GetIMEEnabled(PRUint32* aState)
{
    *aState = gIMEState;
    return NS_OK;
}
