/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsTextAreaWidget.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"

#define DBG 0

//=============================================================================
//
// nsQMultiLineEdit class
//
//=============================================================================
nsQMultiLineEdit::nsQMultiLineEdit(nsWidget * widget,
                                   QWidget * parent, 
                                   const char * name)
	: QMultiLineEdit(parent, name), nsQBaseWidget(widget)
{
}

nsQMultiLineEdit::~nsQMultiLineEdit()
{
}

NS_IMPL_ADDREF(nsTextAreaWidget)
NS_IMPL_RELEASE(nsTextAreaWidget)

//-------------------------------------------------------------------------
//
// nsTextAreaWidget constructor
//
//-------------------------------------------------------------------------
nsTextAreaWidget::nsTextAreaWidget()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextAreaWidget::nsTextAreaWidget()\n"));
    mBackground = NS_RGB(124, 124, 124);
}

//-------------------------------------------------------------------------
//
// nsTextAreaWidget destructor
//
//-------------------------------------------------------------------------
nsTextAreaWidget::~nsTextAreaWidget()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextAreaWidget::~nsTextAreaWidget()\n"));
}

//-------------------------------------------------------------------------
//
// Create the native Text widget
//
//-------------------------------------------------------------------------
NS_METHOD nsTextAreaWidget::CreateNative(QWidget *parentWindow)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextAreaWidget::CreateNative()\n"));
    mWidget = new nsQMultiLineEdit(this,
                                   parentWindow, 
                                   QMultiLineEdit::tr("nsTextAreaWidget"));

    PRBool oldIsReadOnly;
    SetPassword(mIsPassword);
    SetReadOnly(mIsReadOnly, oldIsReadOnly);

    return nsWidget::CreateNative(parentWindow);
}

nsresult 
nsTextAreaWidget::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextAreaWidget::QueryInterface()\n"));
    static NS_DEFINE_IID(kITextAreaWidgetIID, NS_ITEXTAREAWIDGET_IID);
    static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);

    if (aIID.Equals(kITextAreaWidgetIID)) 
    {
        nsITextAreaWidget* textArea = this;
        *aInstancePtr = (void*) (textArea);
        AddRef();
        return NS_OK;
    }
    else if (aIID.Equals(kIWidgetIID))
    {
        nsIWidget* widget = this;
        *aInstancePtr = (void*) (widget);
        AddRef();
        return NS_OK;
    }

    return nsWidget::QueryInterface(aIID, aInstancePtr);
}

//-------------------------------------------------------------------------
//
// paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsTextAreaWidget::OnPaint(nsPaintEvent & aEvent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextAreaWidget::OnPaint()\n"));
    return PR_FALSE;
}


//--------------------------------------------------------------
PRBool nsTextAreaWidget::OnResize(nsRect &aRect)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextAreaWidget::OnResize()\n"));
    return PR_FALSE;
}


