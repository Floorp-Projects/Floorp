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

#include <qvbox.h>
#include <qwidget.h>
#include <qlayout.h>

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
    Qt::WFlags flags = Qt::WNoAutoErase|Qt::WStaticContents;
    qDebug("NEW WIDGET\n\tparent is %p (%s)", (void*)parent,
           parent ? parent->name() : "null");
    // ok, create our windows
    switch (mWindowType) {
    case eWindowType_dialog:
    case eWindowType_popup:
    case eWindowType_toplevel:
    case eWindowType_invisible: {
        if (mWindowType == eWindowType_dialog) {
            flags |= Qt::WType_Dialog;
            mContainer = new MozQWidget(this, parent, "topLevelDialog", flags);
            qDebug("\t\t#### dialog (%p)", (void*)mContainer);
            //SetDefaultIcon();
        }
        else if (mWindowType == eWindowType_popup) {
            flags |= Qt::WType_Popup;
            mContainer = new MozQWidget(this, parent, "topLevelPopup", flags);
            qDebug("\t\t#### popup (%p)", (void*)mContainer);
            mContainer->setFocusPolicy(QWidget::WheelFocus);
        }
        else { // must be eWindowType_toplevel
            flags |= Qt::WType_TopLevel;
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

    mWidget->setBackgroundMode(Qt::NoBackground);

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

