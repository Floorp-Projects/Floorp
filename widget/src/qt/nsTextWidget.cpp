/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsTextWidget.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"

#define DBG 0

extern int mIsPasswordCallBacksInstalled;


//=============================================================================
//
// nsQLineEdit class
//
//=============================================================================
nsQLineEdit::nsQLineEdit(nsWidget * widget,
                         QWidget * parent, 
                         const char * name)
	: QLineEdit(parent, name), nsQBaseWidget(widget)
{
}

nsQLineEdit::~nsQLineEdit()
{
}

NS_IMPL_ADDREF(nsTextWidget)
NS_IMPL_RELEASE(nsTextWidget)


//-------------------------------------------------------------------------
//
// nsTextWidget constructor
//
//-------------------------------------------------------------------------
nsTextWidget::nsTextWidget() : nsTextHelper()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextWidget::nsTextWidget()\n"));
}

//-------------------------------------------------------------------------
//
// nsTextWidget destructor
//
//-------------------------------------------------------------------------
nsTextWidget::~nsTextWidget()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextWidget::~nsTextWidget()\n"));
}

//-------------------------------------------------------------------------
//
// Create the native Entry widget
//
//-------------------------------------------------------------------------
NS_METHOD nsTextWidget::CreateNative(QWidget *parentWindow)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextWidget::CreateNative()\n"));
    mWidget = new nsQLineEdit(this, 
                              parentWindow, 
                              QLineEdit::tr("nsTextWidget"));
    
    if (mWidget)
    {
#if 0
        QObject::connect(mWidget, 
                         SIGNAL(textChanged(const QString &)), 
                         (QObject *)mEventHandler, 
                         SLOT(TextChangedEvent(const QString &)));
#endif

        PRBool oldIsReadOnly;
        SetPassword(mIsPassword);
        SetReadOnly(mIsReadOnly, oldIsReadOnly);

        mWidget->show();
    }

    return nsWidget::CreateNative(parentWindow);
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsTextWidget::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextWidget::QueryInterface()\n"));
    nsresult result = nsWidget::QueryInterface(aIID, aInstancePtr);

    static NS_DEFINE_IID(kInsTextWidgetIID, NS_ITEXTWIDGET_IID);
    if (result == NS_NOINTERFACE && aIID.Equals(kInsTextWidgetIID)) 
    {
        *aInstancePtr = (void*) ((nsITextWidget*)this);
        AddRef();
        result = NS_OK;
    }

    return result;
}



//-------------------------------------------------------------------------
//
// paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsTextWidget::OnPaint(nsPaintEvent & aEvent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextWidget::OnPaint()\n"));
    return PR_FALSE;
}


//--------------------------------------------------------------
PRBool nsTextWidget::OnResize(nsRect &aRect)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextWidget::OnResize()\n"));
    return PR_FALSE;
}


