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

  // Override some of the native widget methods for scrollbars
  PRBool     OnResize           (nsSizeEvent &event);
  PRBool     DispatchMouseEvent (nsMouseEvent &aEvent);
  NS_IMETHOD Show               (PRBool bState);
  NS_IMETHOD Move               (PRInt32 aX, PRInt32 aY);
  NS_IMETHOD Resize             (PRInt32 aWidth,
                                 PRInt32 aHeight,
                                 PRBool   aRepaint);
  NS_IMETHOD Resize             (PRInt32 aX,
                                 PRInt32 aY,
                                 PRInt32 aWidth,
                                 PRInt32 aHeight,
                                 PRBool   aRepaint);
  
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
  
  PRBool    OnScroll(PRUint32 scrollCode, int cPos);
  void CreateNative(Window aParent, nsRect aRect);
  
private:
  void                 CalcBarBounds(void);
  void                 LayoutBar(void);
  nsresult             NextPage(void);
  nsresult             PrevPage(void);
  PRUint32             mMaxRange;
  PRUint32             mPosition;
  PRUint32             mThumbSize;
  PRUint32             mLineIncrement;
  PRBool               mIsVertical;
  nsIRenderingContext *mRenderingContext;
  Window               mBar;
  nsRect               mBarBounds;
};

#endif // nsButton_h__
