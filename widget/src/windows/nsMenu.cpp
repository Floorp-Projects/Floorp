/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsMenu.h"
#include "nsIMenu.h"

#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"
#include <windows.h>

#include "nsIAppShell.h"
#include "nsGUIEvent.h"
#include "nsIDeviceContext.h"
#include "nsRect.h"
#include "nsGfxCIID.h"

static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);

NS_IMPL_ADDREF(nsMenu)
NS_IMPL_RELEASE(nsMenu)

//-------------------------------------------------------------------------
//
// nsMenu constructor
//
//-------------------------------------------------------------------------
nsMenu::nsMenu() : nsWindow() , nsIMenu()
{
  NS_INIT_REFCNT();
  mNumMenuItems = 0;
}

//-------------------------------------------------------------------------
//
// nsMenu destructor
//
//-------------------------------------------------------------------------
nsMenu::~nsMenu()
{
}

/**
 * Implement the standard QueryInterface for NS_IWIDGET_IID and NS_ISUPPORTS_IID
 * @modify gpk 8/4/98
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 * 
*/ 
nsresult nsMenu::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }

    static NS_DEFINE_IID(kIMenuBar, NS_IMENU_IID);
    if (aIID.Equals(kIMenuBar)) {
        *aInstancePtr = (void*) ((nsIMenu*)this);
        AddRef();
        return NS_OK;
    }

    return nsWindow::QueryInterface(aIID,aInstancePtr);
}

//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
NS_METHOD nsMenu::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
    BaseCreate(aParent, aRect, aHandleEventFunction, aContext, 
       aAppShell, aToolkit, aInitData);

    //
    // Switch to the "main gui thread" if necessary... This method must
    // be executed on the "gui thread"...
    //
    nsToolkit* toolkit = (nsToolkit *)mToolkit;
    if (! toolkit->IsGuiThread()) {
        DWORD args[5];
        args[0] = (DWORD)aParent;
        args[1] = (DWORD)&aRect;
        args[2] = (DWORD)aHandleEventFunction;
        args[3] = (DWORD)aContext;
        args[4] = (DWORD)aToolkit;
        args[5] = (DWORD)aInitData;
        MethodInfo info(this, nsWindow::CREATE, 6, args);
        toolkit->CallMethod(&info);
        return NS_OK;
    }


    // See if the caller wants to explictly set clip children
    DWORD style = WindowStyle();
    if (nsnull != aInitData) {
      if (aInitData->clipChildren) {
        style |= WS_CLIPCHILDREN;
      } else {
        style &= ~WS_CLIPCHILDREN;
      }
    }

    mWnd = CreatePopupMenu();
    
    if (aParent) {
        aParent->AddChild(this);
    }

    VERIFY(mWnd);

    // call the event callback to notify about creation

    DispatchStandardEvent(NS_CREATE);
    //SubclassWindow(TRUE);
    return NS_OK;
}


//-------------------------------------------------------------------------
//
// create with a native parent
//
//-------------------------------------------------------------------------
NS_METHOD nsMenu::Create(nsNativeWidget aParent,
                         const nsRect &aRect,
                         EVENT_CALLBACK aHandleEventFunction,
                         nsIDeviceContext *aContext,
                         nsIAppShell *aAppShell,
                         nsIToolkit *aToolkit,
                         nsWidgetInitData *aInitData)
{

  NS_ASSERTION(PR_FALSE, "NOT IMPLEMENTED");

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetLabel(const nsString &aText)
{
  mLabel = aText;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetLabel(nsString &aText)
{
  aText = mLabel;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddItem(const nsString &aText)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddItem(nsIMenuItem * aMenuItem)
{
  PRUint32 command;
  nsString name;

  aMenuItem->GetCommand(command);
  aMenuItem->GetLabel(name);
  char * nameStr = name.ToNewCString();

  MENUITEMINFO menuInfo;
  menuInfo.cbSize     = sizeof(menuInfo);
  menuInfo.fMask      = MIIM_TYPE | MIIM_ID;
  menuInfo.fType      = MFT_STRING;
  menuInfo.dwTypeData = nameStr;
  menuInfo.wID        = (DWORD)command;
  menuInfo.cch        = strlen(nameStr);

  BOOL status = InsertMenuItem(mWnd, mNumMenuItems++, TRUE, &menuInfo);

  delete[] nameStr;

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenu(nsIMenu * aMenu)
{
  nsString name;
  aMenu->GetLabel(name);
  char * nameStr = name.ToNewCString();

  HWND hwnd = nsnull;
	nsIWidget* 	widget;
	if (NS_OK == aMenu->QueryInterface(kIWidgetIID,(void**)&widget)) {
    hwnd = (HWND)widget->GetNativeData(NS_NATIVE_WIDGET);
  }
  NS_RELEASE(widget);

  MENUITEMINFO menuInfo;

  menuInfo.cbSize     = sizeof(menuInfo);
  menuInfo.fMask      = MIIM_SUBMENU | MIIM_TYPE;
  menuInfo.hSubMenu   = hwnd;
  menuInfo.fType      = MFT_STRING;
  menuInfo.dwTypeData = nameStr;

  BOOL status = InsertMenuItem(mWnd, mNumMenuItems++, TRUE, &menuInfo);

  delete[] nameStr;

  return NS_OK;

}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddSeparator() 
{
  MENUITEMINFO menuInfo;

  menuInfo.cbSize = sizeof(menuInfo);
  menuInfo.fMask  = MIIM_TYPE;
  menuInfo.fType  = MFT_SEPARATOR;

  BOOL status = InsertMenuItem(mWnd, mNumMenuItems++, TRUE, &menuInfo);

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetItemCount(PRUint32 &aCount)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetItemAt(const PRUint32 aCount, nsIMenuItem *& aMenuItem)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::InsertItemAt(const PRUint32 aCount, nsIMenuItem *& aMenuItem)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::InsertItemAt(const PRUint32 aCount, const nsString & aMenuItemName)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::InsertSeparator(const PRUint32 aCount)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveItem(const PRUint32 aCount)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveAll()
{
  return NS_OK;
}



//-------------------------------------------------------------------------
//
// move, paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsMenu::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

PRBool nsMenu::OnPaint()
{
  return PR_FALSE;
}

PRBool nsMenu::OnResize(nsRect &aWindowRect)
{
    return PR_FALSE;
}



