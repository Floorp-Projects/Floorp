/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
  virtual void DestroyNative();  // override since we have 2 native widgets
  
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
  PRBool    SendEvent(PRUint32 message);
  
private:
  void                 CalcBarBounds(void);
  void                 LayoutBar(void);
  nsresult             NextPage(void);
  nsresult             PrevPage(void);
  PRUint32             mMaxRange;
  PRUint32             mPosition;
  PRUint32             mThumbSize;
  float                mXScale;
  float                mYScale;
  PRUint32             mLineIncrement;
  PRBool               mIsVertical;
  nsIRenderingContext *mRenderingContext;
  Window               mBar;
  nsRect               mBarBounds;
};

#endif // nsButton_h__
