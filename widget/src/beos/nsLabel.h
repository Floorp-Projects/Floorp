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

#ifndef nsLabel_h__
#define nsLabel_h__

#include "nsdefs.h"
#include "nsWindow.h"
#include "nsSwitchToUIThread.h"

#include "nsILabel.h"

#include <StringView.h>

/**
 * Native Win32 Label wrapper
 */

class nsLabel :  public nsWindow,
                 public nsILabel
{

public:
  nsLabel();
  virtual ~nsLabel();

   // nsISupports
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);                           
  NS_IMETHOD_(nsrefcnt) AddRef(void);                                       
  NS_IMETHOD_(nsrefcnt) Release(void);          

  // nsILabel part
  NS_IMETHOD SetLabel(const nsString &aText);
  NS_IMETHOD GetLabel(nsString &aBuffer);
  NS_IMETHOD SetAlignment(nsLabelAlignment aAlignment);

  virtual PRBool OnMove(PRInt32 aX, PRInt32 aY);
  virtual PRBool OnPaint(nsRect &r);
  virtual PRBool OnResize(nsRect &aWindowRect);

  NS_IMETHOD   GetBounds(nsRect &aRect);
  NS_IMETHOD   PreCreateWidget(nsWidgetInitData *aInitData);

  NS_IMETHOD GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight);
  NS_IMETHOD SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight);

protected:
  nsLabelAlignment mAlignment;
  BStringView *mStringView;
  BView *CreateBeOSView();
};

//
// A BStringView subclass
//
class nsStringViewBeOS : public BStringView, public nsIWidgetStore {
  public:
    nsStringViewBeOS( nsIWidget *aWidgetWindow, BRect aFrame, const char *aName,
        const char *text, uint32 aResizingMode = B_FOLLOW_LEFT | B_FOLLOW_TOP, 
        uint32 aFlags = B_WILL_DRAW | B_NAVIGABLE );
};

#endif // nsLabel_h__
