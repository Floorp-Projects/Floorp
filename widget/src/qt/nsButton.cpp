/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

NS_IMPL_ADDREF_INHERITED(nsButton, nsWidget)
NS_IMPL_RELEASE_INHERITED(nsButton, nsWidget)
NS_IMPL_QUERY_INTERFACE2(nsButton, nsIButton, nsIWidget)

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
    aBuffer.AppendWithConversion((const char *) string);

    return (NS_OK);
}

