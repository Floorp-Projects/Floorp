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

#include "nscore.h"
#include "nsMenuContainerWin.h"
#include "windows.h"

static NS_DEFINE_IID(kCIMenuContainerIID, NS_IMENUCONTAINER_IID);
static NS_DEFINE_IID(kCIMenuItemIID, NS_IMENUITEM_IID);

nsMenuContainerWin::nsMenuContainerWin() : nsMenuContainer()
{
  mHwnd = nsnull;
  mMenu = nsnull;
}

nsMenuContainerWin::~nsMenuContainerWin()
{
}

void* nsMenuContainerWin::GetNativeHandle()
{
  if (mMenu != nsnull)
    return ((void*)mMenu);

  if (GetParent() != nsnull)
    return (GetParent()->GetNativeHandle());

  return nsnull;

}

nsresult nsMenuContainerWin :: AddMenuItem(nsIMenuItem * aMenuItem)
{
  if (mMenu == nsnull)
    mMenu = ::CreatePopupMenu();

  PRUint32 flags = MF_STRING;

  char * name = aMenuItem->GetLabel().ToNewCString();

  if (aMenuItem->GetAlignmentStyle() == eAlignmentStyle_right)
    flags |= MF_HELP;

  if (aMenuItem->GetEnabled() == PR_FALSE)
    flags |= MF_GRAYED;

  if (aMenuItem->IsSeparator() == PR_TRUE)
    flags = MF_SEPARATOR;

  ::AppendMenu(mMenu, flags, aMenuItem->GetMenuID(), (LPSTR)name);

  delete name;

  return NS_OK;
}


nsresult nsMenuContainerWin :: Update()
{

  nsresult res;
  nsIIterator * iterator = nsnull;
  nsIMenuItem * item;
  nsIMenuContainer * container;

  res = mChildMenus->CreateIterator(&iterator);

  if (res != NS_OK)
    return res;

  iterator->Init();

  while(!(iterator->IsDone()))
  {
    item = (nsIMenuItem *) iterator->CurrentItem();    

    res = item->QueryInterface(kCIMenuContainerIID, (void**)&container);

    if (NS_OK == res)
    {
      container->Update();

      NS_RELEASE(container);

    } else {

      /*
       * Just add this is a basic menu item
       */

       AddMenuItem(item);
    }

    iterator->Next();
  }

  /*
   * If I am a native popup, we want to add me to the closest parent
   * with a menu.
   */
  if (mMenu != nsnull)
  {
    if (GetParent() && GetParent()->GetNativeHandle())
    {

      nsIMenuItem * container_item = nsnull;

      res = GetParent()->QueryInterface(kCIMenuItemIID,(void**)&container_item);

      if (res == NS_OK)
      {
        char * name = container_item->GetLabel().ToNewCString();

        ::AppendMenu(GetParent()->GetNativeHandle(), 
                     MF_POPUP, 
                     (PRUint32)mMenu, 
                     (LPSTR)name);

        delete name;

        NS_RELEASE(container_item);
      }
    }    
  }

  NS_RELEASE(iterator);

  if (mHwnd)
  ::DrawMenuBar(mHwnd);
      
  return NS_OK;
}

nsresult nsMenuContainerWin :: SetShellContainer(nsIShellInstance * aShellInstance,
                                                nsIWebViewerContainer * aWebViewerContainer)
{

  mHwnd = aShellInstance->GetApplicationWindowNativeInstance();
  mMenu = ::CreateMenu();
  ::SetMenu(mHwnd, mMenu);

  return (nsMenuContainer::SetShellContainer(aShellInstance,aWebViewerContainer));
}
