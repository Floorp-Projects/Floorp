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

#include "nsLabel.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"


//=============================================================================
//
// nsQLabel class
//
//=============================================================================
nsQLabel::nsQLabel(nsWidget * widget,
                   QWidget * parent, 
                   const char * name, WFlags f)
	: QLabel(parent, name, f), nsQBaseWidget(widget)
{
}

nsQLabel::~nsQLabel()
{
}

NS_IMPL_ADDREF(nsLabel)
NS_IMPL_RELEASE(nsLabel)

//-------------------------------------------------------------------------
//
// nsLabel constructor
//
//-------------------------------------------------------------------------
nsLabel::nsLabel() : nsWidget(), nsILabel()
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsLabel::nsLabel()\n"));
    mAlignment = eAlign_Left;
}


//-------------------------------------------------------------------------
//
// Create the nativeLabel widget
//
//-------------------------------------------------------------------------
NS_METHOD  nsLabel::CreateNative(QWidget *parentWindow)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsLabel::CreateNative()\n"));
    int alignment = GetNativeAlignment();

    mWidget = new nsQLabel(this, parentWindow, QLabel::tr("nsLabel"));

    if (mWidget)
    {
        ((QLabel *)mWidget)->setAlignment(alignment);
    }

    return nsWidget::CreateNative(parentWindow);
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsLabel::PreCreateWidget(nsWidgetInitData *aInitData)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsLabel::PreCreateWidget()\n"));
    if (nsnull != aInitData) 
    {
        nsLabelInitData* data = (nsLabelInitData *) aInitData;
        mAlignment = data->mAlignment;
    }
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set alignment
//
//-------------------------------------------------------------------------
NS_METHOD nsLabel::SetAlignment(nsLabelAlignment aAlignment)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsLabel::SetAlignment()\n"));
    mAlignment = aAlignment;
    return NS_OK;
}


//-------------------------------------------------------------------------
//
// nsLabel destructor
//
//-------------------------------------------------------------------------
nsLabel::~nsLabel()
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsLabel::~nsLabel()\n"));
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsLabel::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsLabel::QueryInterface()\n"));

    nsresult result = nsWidget::QueryInterface(aIID, aInstancePtr);

    static NS_DEFINE_IID(kILabelIID, NS_ILABEL_IID);
    if (result == NS_NOINTERFACE && aIID.Equals(kILabelIID)) 
    {
        *aInstancePtr = (void*) ((nsILabel*)this);
        AddRef();
        result = NS_OK;
    }

    return result;
}


//-------------------------------------------------------------------------
//
//
//
//-------------------------------------------------------------------------
int nsLabel::GetNativeAlignment()
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsLabel::GetNativeAlignment()\n"));
    int alignment = QLabel::AlignVCenter;

    switch (mAlignment) 
    {
    case eAlign_Right:
        alignment |= QLabel::AlignRight;
        break;
    case eAlign_Left:
        alignment |= QLabel::AlignLeft;
        break;
    case eAlign_Center:
        alignment |= QLabel::AlignCenter;
        break;
    default:
        alignment |= QLabel::AlignCenter;
        break;
    }

    return alignment;
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsLabel::SetLabel(const nsString& aText)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsLabel::SetLabel()\n"));
    NS_ALLOC_STR_BUF(label, aText, 256);

    ((QLabel *)mWidget)->setText(label);

    NS_FREE_STR_BUF(label);

    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsLabel::GetLabel(nsString& aBuffer)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsLabel::GetLabel()\n"));
    QString string = ((QLabel *)mWidget)->text();

    aBuffer.SetLength(0);
    aBuffer.Append((const char *)string);

    return NS_OK;
}
