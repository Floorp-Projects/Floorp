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

//JCG #define DBG 0

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

NS_IMPL_ADDREF_INHERITED(nsTextWidget, nsWidget)
NS_IMPL_RELEASE_INHERITED(nsTextWidget, nsWidget)
NS_IMPL_QUERY_INTERFACE2(nsTextWidget, nsITextWidget, nsIWidget)

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


