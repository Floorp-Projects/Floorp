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

NS_IMPL_ADDREF_INHERITED(nsLabel,nsWidget)
NS_IMPL_RELEASE_INHERITED(nsLabel,nsWidget)
NS_IMPL_QUERY_INTERFACE2(nsLabel, nsILabel, nsIWidget)

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
    NS_INIT_REFCNT();
    mAlignment = eAlign_Left;
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
    aBuffer.AppendWithConversion((const char *)string);

    return NS_OK;
}
