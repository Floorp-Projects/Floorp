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

#ifndef nsScrollbar_h__
#define nsScrollbar_h__

#include "nsWidget.h"
#include "nsIScrollbar.h"

class nsScrollbar : public nsWidget,
                    public nsIScrollbar
{

public:
  nsScrollbar(PRBool aIsVertical);
  virtual                 ~nsScrollbar();

  // nsISupports
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);                           
  NS_IMETHOD_(nsrefcnt) AddRef(void);                                       
  NS_IMETHOD_(nsrefcnt) Release(void);          
  
  // nsIScrollBar implementation
  NS_IMETHOD SetMaxRange(PRUint32 aEndRange);
  NS_IMETHOD GetMaxRange(PRUint32& aMaxRange);
  NS_IMETHOD SetPosition(PRUint32 aPos);
  NS_IMETHOD GetPosition(PRUint32& aPos);
  NS_IMETHOD SetThumbSize(PRUint32 aSize);
  NS_IMETHOD GetThumbSize(PRUint32& aSize);
  NS_IMETHOD SetLineIncrement(PRUint32 aSize);
  NS_IMETHOD GetLineIncrement(PRUint32& aSize);
  NS_IMETHOD SetParameters(PRUint32 aMaxRange, PRUint32 aThumbSize,
			   PRUint32 aPosition, PRUint32 aLineIncrement);
  
  virtual PRBool    OnPaint();
  virtual PRBool    OnScroll(PRUint32 scrollCode, int cPos);
  virtual PRBool    OnResize(nsRect &aWindowRect);
  NS_IMETHOD        GetBounds(nsRect &aRect);

};

#endif // nsButton_h__
