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

#include "nsCheckButton.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"

//=============================================================================
//
// nsQCheckBox class
//
//=============================================================================
nsQCheckBox::nsQCheckBox(nsWidget * widget,
                         QWidget * parent, 
                         const char * name)
	: QCheckBox(parent, name), nsQBaseWidget(widget)
{
}

nsQCheckBox::~nsQCheckBox()
{
}

NS_IMPL_ADDREF_INHERITED(nsCheckButton, nsWidget)
NS_IMPL_RELEASE_INHERITED(nsCheckButton, nsWidget)
NS_IMPL_QUERY_INTERFACE2(nsCheckButton, nsICheckButton, nsIWidget)

//-------------------------------------------------------------------------
//
// nsCheckButton constructor
//
//-------------------------------------------------------------------------
nsCheckButton::nsCheckButton() : nsWidget() , nsICheckButton()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsCheckButton::nsCheckButton()\n"));
    NS_INIT_REFCNT();
    mWidget = nsnull;
}

//-------------------------------------------------------------------------
//
// nsCheckButton destructor
//
//-------------------------------------------------------------------------
nsCheckButton::~nsCheckButton()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsCheckButton::~nsCheckButton()\n"));
    if (mWidget)
    {
        delete ((QCheckBox *)mWidget);
        mWidget = nsnull;
    }
}

//-------------------------------------------------------------------------
//
// Set this Checkbox state
//
//-------------------------------------------------------------------------
NS_METHOD nsCheckButton::SetState(const PRBool aState)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsCheckButton::SetState() %p:%d\n", this, aState));
    ((QCheckBox *)mWidget)->setChecked(aState);

    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get this Checkbox state
//
//-------------------------------------------------------------------------
NS_METHOD nsCheckButton::GetState(PRBool& aState)
{
    aState = ((QCheckBox *)mWidget)->isChecked();
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsCheckButton::GetState() %p:%d\n", this, aState));

    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set this Checkbox label
//
//-------------------------------------------------------------------------
NS_METHOD nsCheckButton::SetLabel(const nsString& aText)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsCheckButton::SetLabel()\n"));
    NS_ALLOC_STR_BUF(label, aText, 256);

    ((QButton *)mWidget)->setText(label);

    NS_FREE_STR_BUF(label);
    return (NS_OK);    
}


//-------------------------------------------------------------------------
//
// Get this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsCheckButton::GetLabel(nsString& aBuffer)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsCheckButton::GetLabel()\n"));
    QString string = ((QButton *)mWidget)->text();

    aBuffer.SetLength(0);
    aBuffer.AppendWithConversion((const char *) string);

    return (NS_OK);
}

