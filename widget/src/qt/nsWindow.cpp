/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *  Zack Rusin <zack@kde.org>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Lars Knoll <knoll@kde.org>
 *   Zack Rusin <zack@kde.org>
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
#include "nsWindow.h"

#include "mozqwidget.h"

#include "gfxPlatform.h"
#include "gfxXlibSurface.h"

#include <qwidget.h>
#include <qlayout.h>
#include <QX11Info>

NS_IMPL_ISUPPORTS_INHERITED1(nsWindow, nsCommonWidget,
                             nsISupportsWeakReference)

nsWindow::nsWindow()
{
}

nsWindow::~nsWindow()
{
}

QWidget*
nsWindow::createQWidget(QWidget *parent, nsWidgetInitData *aInitData)
{
    Qt::WFlags flags = Qt::Widget;
    Qt::WA_StaticContents;
#ifdef DEBUG_WIDGETS
    qDebug("NEW WIDGET\n\tparent is %p (%s)", (void*)parent,
           parent ? qPrintable(parent->objectName()) : "null");
#endif
    // ok, create our windows
    switch (mWindowType) {
    case eWindowType_dialog:
    case eWindowType_popup:
    case eWindowType_toplevel:
    case eWindowType_invisible: {
        if (mWindowType == eWindowType_dialog) {
            flags |= Qt::Dialog;
            mContainer = new MozQWidget(this, parent, "topLevelDialog", flags);
            qDebug("\t\t#### dialog (%p)", (void*)mContainer);
            //SetDefaultIcon();
        }
        else if (mWindowType == eWindowType_popup) {
            flags |= Qt::Popup;
            mContainer = new MozQWidget(this, parent, "topLevelPopup", flags);
            qDebug("\t\t#### popup (%p)", (void*)mContainer);
            mContainer->setFocusPolicy(Qt::WheelFocus);
        }
        else { // must be eWindowType_toplevel
            flags |= Qt::Window;
            mContainer = new MozQWidget(this, parent, "topLevelWindow", flags);
            qDebug("\t\t#### toplevel (%p)", (void*)mContainer);
            //SetDefaultIcon();
        }
        mWidget = mContainer;
    }
        break;
    case eWindowType_child: {
        mWidget = new MozQWidget(this, parent, "paintArea", 0);
        qDebug("\t\t#### child (%p)", (void*)mWidget);
    }
        break;
    default:
        break;
    }

    mWidget->setAttribute(Qt::WA_StaticContents);
    mWidget->setAttribute(Qt::WA_OpaquePaintEvent); // Transparent Widget Background

    //mWidget->setBackgroundMode(Qt::NoBackground);
    //setAutoFillBackground(false)

    return mWidget;
}

NS_IMETHODIMP
nsWindow::PreCreateWidget(nsWidgetInitData *aWidgetInitData)
{
    if (nsnull != aWidgetInitData) {
        mWindowType = aWidgetInitData->mWindowType;
        mBorderStyle = aWidgetInitData->mBorderStyle;
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

gfxASurface*
nsWindow::GetThebesSurface()
{
    // XXXvlad always create a new thebes surface for now,
    // because the old clip doesn't get cleared otherwise.
    // we should fix this at some point, and just reset
    // the clip.
    mThebesSurface = nsnull;

    if (!mThebesSurface) {
        PRInt32 x_offset = 0, y_offset = 0;
        int width = mWidget->width(), height = mWidget->height();

        // Owen Taylor says this is the right thing to do!
        width = PR_MIN(32767, height);
        height = PR_MIN(32767, width);

        if (!gfxPlatform::UseGlitz()) {
            qDebug("QT_WIDGET NOT SURE: Func:%s::%d, [%ix%i]\n", __PRETTY_FUNCTION__, __LINE__, width, height);
            mThebesSurface = new gfxXlibSurface
            (mWidget->x11Info().display(),
             (Drawable)mWidget->x11Info().appRootWindow(),
             static_cast<Visual*>(mWidget->x11Info().visual()),
             gfxIntSize(width, height));

            // if the surface creation is reporting an error, then
            // we don't have a surface to give back
            if (mThebesSurface && mThebesSurface->CairoStatus() != 0)
                mThebesSurface = nsnull;
        } else {
#ifdef MOZ_ENABLE_GLITZ
            glitz_surface_t *gsurf;
            glitz_drawable_t *gdraw;

            glitz_drawable_format_t *gdformat = glitz_glx_find_window_format (GDK_DISPLAY(),
                                                mWidget->x11Info().appScreen(),
                                                0, NULL, 0);
            if (!gdformat)
                NS_ERROR("Failed to find glitz drawable format");

            Display* dpy = mWidget->x11Info().display(),;
            Window wnd = (Window)mWidget->x11Info().appRootWindow();

            gdraw =
                glitz_glx_create_drawable_for_window (dpy,
                                                      DefaultScreen(dpy),
                                                      gdformat,
                                                      wnd,
                                                      width,
                                                      height);
            glitz_format_t *gformat =
                glitz_find_standard_format (gdraw, GLITZ_STANDARD_RGB24);
            gsurf =
                glitz_surface_create (gdraw,
                                      gformat,
                                      width,
                                      height,
                                      0,
                                      NULL);
            glitz_surface_attach (gsurf, gdraw, GLITZ_DRAWABLE_BUFFER_FRONT_COLOR);


            //fprintf (stderr, "## nsThebesDrawingSurface::Init Glitz DRAWABLE %p (DC: %p)\n", aWidget, aDC);
            mThebesSurface = new gfxGlitzSurface (gdraw, gsurf, PR_TRUE);
#endif
        }

        if (mThebesSurface) {
            mThebesSurface->SetDeviceOffset(gfxPoint(-x_offset, -y_offset));
        }
    }

    return mThebesSurface;
}

//////////////////////////////////////////////////////////////////////
ChildWindow::ChildWindow()
{
}

ChildWindow::~ChildWindow()
{
}

PRBool
ChildWindow::IsChild() const
{
    return PR_TRUE;
}

PopupWindow::PopupWindow()
{
    qDebug("===================== popup!");
}

PopupWindow::~PopupWindow()
{
}



