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

#include "nsButton.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"

//=============================================================================
//
// nsQPushButton class
//
//=============================================================================
nsQPushButton::nsQPushButton(nsWidget * widget,
                             QWidget * parent, 
                             const char * name)
	: QPushButton(parent, name), nsQBaseWidget(widget)
{
}

nsQPushButton::~nsQPushButton()
{
}

NS_IMPL_ADDREF(nsButton)
NS_IMPL_RELEASE(nsButton)

//-------------------------------------------------------------------------
//
// nsButton constructor
//
//-------------------------------------------------------------------------
nsButton::nsButton() : nsWidget() , nsIButton()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsButton::nsButton()\n"));
}

//-------------------------------------------------------------------------
//
// Create the native Button widget
//
//-------------------------------------------------------------------------
NS_METHOD nsButton::CreateNative(QWidget *parentWindow)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsButton::CreateNative()\n"));
    mWidget = new nsQPushButton(this, 
                                parentWindow, 
                                QPushButton::tr("nsButton"));

    return nsWidget::CreateNative(parentWindow);
}

//-------------------------------------------------------------------------
//
// nsButton destructor
//
//-------------------------------------------------------------------------
nsButton::~nsButton()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsButton::~nsButton()\n"));
    if (mWidget)
    {
        delete ((QPushButton *) mWidget);
        mWidget = nsnull;
    }
}

void nsButton::InitCallbacks(char * aName)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsButton::InitCallbacks()\n"));
    //nsWidget::InitCallbacks();

#if 0
    QObject::connect((QPushButton *) mWidget,
                     SIGNAL(pressed()),
                     (QPushButton *) mWidget,
                     SLOT(Pressed()));

    QObject::connect((QPushButton *)mWidget,
                     SIGNAL(released()),
                     (QPushButton *) mWidget,
                     SLOT(Released()));
#endif

}

/**
 * Implement the standard QueryInterface for NS_IWIDGET_IID and NS_ISUPPORTS_IID
 * @modify gpk 8/4/98
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 *
*/
nsresult nsButton::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsButton::QueryInterface()\n"));
    if (NULL == aInstancePtr) 
    {
        return NS_ERROR_NULL_POINTER;
    }

    static NS_DEFINE_IID(kIButton, NS_IBUTTON_IID);
    if (aIID.Equals(kIButton)) 
    {
        *aInstancePtr = (void*) ((nsIButton*)this);
        AddRef();
        return NS_OK;
    }

    return nsWidget::QueryInterface(aIID, aInstancePtr);
}


//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsButton::SetLabel(const nsString& aText)
{
    NS_ALLOC_STR_BUF(label, aText, 256);

    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsButton::SetLabel to %s()\n", label));

    ((QPushButton *)mWidget)->setText(label);

    NS_FREE_STR_BUF(label);

    return (NS_OK);
}

//-------------------------------------------------------------------------
//
// Get this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsButton::GetLabel(nsString& aBuffer)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsButton::GetLabel()\n"));
    QString string = ((QPushButton *)mWidget)->text();

    aBuffer.SetLength(0);
    aBuffer.Append((const char *) string);

    return (NS_OK);
}

