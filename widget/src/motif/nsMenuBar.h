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

#ifndef nsMenuBar_h__
#define nsMenuBar_h__

#include "Xm/Xm.h"
#include "nsXtManageWidget.h"

#include "nsIMenuBar.h"
#include "nsIMenuListener.h"
#include "nsVoidArray.h"

class nsIWidget;

/**
 * Native Motif MenuBar wrapper
 */

class nsMenuBar : public nsIMenuBar, public nsIMenuListener
{

public:
  nsMenuBar();
  virtual ~nsMenuBar();

  // nsIMenuListener interface
  nsEventStatus MenuSelected(const nsMenuEvent & aMenuEvent);
  
  NS_DECL_ISUPPORTS

  
  NS_IMETHOD Create(nsIWidget * aParent);

  // nsIMenuBar Methods
  NS_IMETHOD GetParent(nsIWidget *&aParent);
  NS_IMETHOD SetParent(nsIWidget * aParent);
  NS_IMETHOD AddMenu(nsIMenu * aMenu);
  NS_IMETHOD GetMenuCount(PRUint32 &aCount);
  NS_IMETHOD GetMenuAt(const PRUint32 aCount, nsIMenu *& aMenu);
  NS_IMETHOD InsertMenuAt(const PRUint32 aCount, nsIMenu *& aMenu);
  NS_IMETHOD RemoveMenu(const PRUint32 aCount);
  NS_IMETHOD RemoveAll();
  NS_IMETHOD GetNativeData(void*& aData);
  NS_IMETHOD Paint();
  NS_IMETHOD SetNativeData(void* aData);
protected:
  PRUint32    mNumMenus;
  Widget      mMenu;
  nsIWidget  *mParent;
  PRBool      mIsMenuBarAdded;
};

#endif // nsMenuBar_h__

