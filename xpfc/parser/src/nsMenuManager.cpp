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

#include "nsMenuManager.h"
#include "nsxpfcCIID.h"
#include "nsxpfcutil.h"
#include "nsIXPFCMenuBar.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCMenuManagerCID, NS_MENU_MANAGER_CID);
static NS_DEFINE_IID(kIMenuManagerIID, NS_IMENU_MANAGER_IID);

#define MENU_ID_OFFSET 1

nsMenuManager :: nsMenuManager()
{
  NS_INIT_REFCNT();
  mMenuBar = nsnull;
  mMenuContainers = nsnull;
  mValidMenuID = MENU_ID_OFFSET;
}

nsMenuManager :: ~nsMenuManager()
{
  NS_IF_RELEASE(mMenuBar);

  if (mMenuContainers != nsnull) {

	  nsIIterator * iterator;

	  mMenuContainers->CreateIterator(&iterator);
	  iterator->Init();

    nsIXPFCMenuContainer * item;

	  while(!(iterator->IsDone()))
	  {
		  item = (nsIXPFCMenuContainer *) iterator->CurrentItem();
		  NS_RELEASE(item);
		  iterator->Next();
	  }
	  NS_RELEASE(iterator);

    mMenuContainers->RemoveAll();
    NS_RELEASE(mMenuContainers);
  }

}

NS_IMPL_QUERY_INTERFACE(nsMenuManager, kIMenuManagerIID)
NS_IMPL_ADDREF(nsMenuManager)
NS_IMPL_RELEASE(nsMenuManager)

nsresult nsMenuManager :: Init()
{
  static NS_DEFINE_IID(kCVectorCID, NS_ARRAY_CID);
  nsresult res = nsRepository::CreateInstance(kCVectorCID, 
                                     nsnull, 
                                     kCVectorCID, 
                                     (void **)&mMenuContainers);

  if (NS_OK != res)
    return res ;

  mMenuContainers->Init();

  return NS_OK ;
}

nsresult nsMenuManager :: SetMenuBar(nsIXPFCMenuBar * aMenuBar)
{
  NS_IF_RELEASE(mMenuBar);
  mMenuBar = aMenuBar;
  NS_ADDREF(mMenuBar);
  return NS_OK ;
}

nsIXPFCMenuBar * nsMenuManager :: GetMenuBar()
{
  return (mMenuBar) ;
}

nsIXPFCCommandReceiver * nsMenuManager :: GetDefaultReceiver()
{
  return (mDefaultReceiver) ;
}

nsresult nsMenuManager :: SetDefaultReceiver(nsIXPFCCommandReceiver * aReceiver)
{
  mDefaultReceiver = aReceiver;
  return (NS_OK) ;
}

nsresult nsMenuManager::AddMenuContainer(nsIXPFCMenuContainer * aMenuContainer)
{
  mMenuContainers->Append(aMenuContainer);
  NS_ADDREF(aMenuContainer);
  return NS_OK;
}

nsIXPFCMenuItem * nsMenuManager::MenuItemFromID(PRUint32 aID)
{
  nsIXPFCMenuItem * item = nsnull;

  if (mMenuBar)
    item = mMenuBar->MenuItemFromID(aID);

  if (item != nsnull)
    return item;

  return nsnull;
}

PRUint32 nsMenuManager::GetID()
{
  mValidMenuID++;
  return mValidMenuID;
}

