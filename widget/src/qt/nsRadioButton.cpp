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

#include "nsRadioButton.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"

//=============================================================================
//
// nsQRadioButton class
//
//=============================================================================
nsQRadioButton::nsQRadioButton(nsWidget * widget,
                               QWidget * parent, 
                               const char * name)
	: QRadioButton(parent, name), nsQBaseWidget(widget)
{
}

nsQRadioButton::~nsQRadioButton()
{
}

NS_IMPL_ADDREF(nsRadioButton)
NS_IMPL_RELEASE(nsRadioButton)

//-------------------------------------------------------------------------
//
// nsRadioButton constructor
//
//-------------------------------------------------------------------------
nsRadioButton::nsRadioButton() : nsWidget(), nsIRadioButton()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsRadioButton::nsRadioButton()\n"));
    //NS_INIT_REFCNT();
}


//-------------------------------------------------------------------------
//
// nsRadioButton destructor
//
//-------------------------------------------------------------------------
nsRadioButton::~nsRadioButton()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsRadioButton::~nsRadioButton()\n"));
    if (mWidget)
    {
        delete ((QRadioButton *) mWidget);
        mWidget = nsnull;
    }
}


//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsRadioButton::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsRadioButton::QueryInterface()\n"));
    nsresult result = nsWidget::QueryInterface(aIID, aInstancePtr);

    static NS_DEFINE_IID(kIRadioButtonIID, NS_IRADIOBUTTON_IID);
    if (result == NS_NOINTERFACE && aIID.Equals(kIRadioButtonIID)) 
    {
        *aInstancePtr = (void*) ((nsIRadioButton*)this);
        AddRef();
        result = NS_OK;
    }
    return result;
}


//-------------------------------------------------------------------------
//
// Create the native RadioButton widget
//
//-------------------------------------------------------------------------
NS_METHOD  nsRadioButton::CreateNative(QWidget *parentWindow)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsRadioButton::CreateNative()\n"));
    mWidget = new nsQRadioButton(this,
                                 parentWindow, 
                                 QRadioButton::tr("nsRadioButton"));

    if (mWidget)
    {
        mWidget->installEventFilter(mWidget);
    }

    return nsWidget::CreateNative(parentWindow);
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsRadioButton::SetState(const PRBool aState)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsRadioButton::SetState()\n"));
    ((QRadioButton *)mWidget)->setChecked(aState);

    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set this button state
//-------------------------------------------------------------------------
NS_METHOD nsRadioButton::GetState(PRBool& aState)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsRadioButton::GetState()\n"));
    aState = ((QRadioButton *)mWidget)->isChecked();

    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsRadioButton::SetLabel(const nsString& aText)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsRadioButton::SetLabel()\n"));
    NS_ALLOC_STR_BUF(label, aText, 256);
//    g_print("nsRadioButton::SetLabel(%s)\n",label);

    ((QRadioButton *)mWidget)->setText(label);

    NS_FREE_STR_BUF(label);

    return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsRadioButton::GetLabel(nsString& aBuffer)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsRadioButton::GetLabel()\n"));
    QString string = ((QRadioButton *)mWidget)->text();
    aBuffer.SetLength(0);
    aBuffer.Append((const char *) string);

    return NS_OK;
}



