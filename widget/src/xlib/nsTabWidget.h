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

#ifndef nsTabWidget_h__
#define nsTabWidget_h__

#include "nsWidget.h"
#include "nsITabWidget.h"

class nsTabWidget : public nsWidget,
                    public nsITabWidget
{

public:
  nsTabWidget();
  virtual ~nsTabWidget();
  
  // nsISupports
  NS_IMETHOD_(nsrefcnt) Release(void);          
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);                           
  NS_IMETHOD_(nsrefcnt) AddRef(void);                                       
  
  // nsITabWidget part 
  NS_IMETHOD SetTabs(PRUint32 aNumberOfTabs, const nsString aTabLabels[]);
  NS_IMETHOD GetSelectedTab(PRUint32& aTabNumber);
  
  // nsIWidget overrides
  virtual PRBool OnPaint();
  virtual PRBool OnResize(nsRect &aWindowRect);
  NS_IMETHOD     GetBounds(nsRect &aRect);

};

#endif // nsTabWidget_h__
