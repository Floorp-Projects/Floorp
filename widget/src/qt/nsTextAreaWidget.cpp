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

#include "nsTextAreaWidget.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"

//JCG #define DBG 0

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
