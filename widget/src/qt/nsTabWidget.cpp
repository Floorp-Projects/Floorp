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

#include "nsTabWidget.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"


//=============================================================================
//
// nsQTabBar class
//
//=============================================================================
nsQTabBar::nsQTabBar(nsWidget * widget,
                     QWidget * parent, 
                     const char * name)
	: QTabBar(parent, name), nsQBaseWidget(widget)
{
}

nsQTabBar::~nsQTabBar()
{
}

NS_IMPL_ADDREF(nsTabWidget)
NS_IMPL_RELEASE(nsTabWidget)

//-------------------------------------------------------------------------
//
// nsTabWidget constructor
//
//-------------------------------------------------------------------------
nsTabWidget::nsTabWidget() : nsWidget()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTabWidget::nsTabWidget()\n"));
    NS_INIT_REFCNT();
}

//-------------------------------------------------------------------------
//
// nsTabWidget destructor
//
//-------------------------------------------------------------------------
nsTabWidget::~nsTabWidget()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTabWidget::~nsTabWidget()\n"));
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsTabWidget::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTabWidget::QueryInterface()\n"));
    nsresult result = nsWidget::QueryInterface(aIID, aInstancePtr);

    static NS_DEFINE_IID(kInsTabWidgetIID, NS_ITABWIDGET_IID);
    if (result == NS_NOINTERFACE && aIID.Equals(kInsTabWidgetIID)) 
    {
        *aInstancePtr = (void*) ((nsITabWidget*)this);
        NS_ADDREF_THIS();
        result = NS_OK;
    }

    return result;
}

//-------------------------------------------------------------------------
//
// Set the tab labels
//
//-------------------------------------------------------------------------

NS_METHOD nsTabWidget::SetTabs(PRUint32 aNumberOfTabs, 
                               const nsString aTabLabels[])
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTabWidget::SetTabs()\n"));

    if (mWidget)
    {
        for (PRUint32 i = 0; i < aNumberOfTabs; i++)
        {
            QTab * qTab = new QTab();

            if (qTab)
            {
                char * label = aTabLabels[i].ToNewCString();
                qTab->label = label;
                ((QTabBar *)mWidget)->addTab(qTab);
                delete [] label;
            }
        }
    }

/*
  TC_ITEM tie;
  char labelTemp[256];

  for (PRUint32 i = 0; i < aNumberOfTabs; i++) {
  tie.mask = TCIF_TEXT;
  aTabLabels[i].ToCString(labelTemp, 256);
  tie.pszText = labelTemp;
  TabCtrl_InsertItem(mWnd, i, &tie);
  }
*/
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the currently selected tab
//
//-------------------------------------------------------------------------


PRUint32 nsTabWidget::GetSelectedTab(PRUint32& aTabNumber)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTabWidget::GetSelectedTab()\n"));
/*
  aTabNumber = TabCtrl_GetCurSel(mWnd);
*/
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// paint message. Don't send the paint out
//
//-------------------------------------------------------------------------

NS_METHOD nsTabWidget::GetBounds(nsRect &aRect)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTabWidget::GetBounds()\n"));
    return nsWidget::GetBounds(aRect);
}
  
NS_METHOD nsTabWidget::CreateNative(QWidget * parentWidget)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTabWidget::CreateNative()\n"));

    mWidget = new nsQTabBar(this, parentWidget, QTabBar::tr("nsTabWidget"));

    return nsWidget::CreateNative(parentWidget);
}
