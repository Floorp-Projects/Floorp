/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef __NS_XPFCMENU_ITEM
#define __NS_XPFCMENU_ITEM

#include "nsIXPFCMenuItem.h"
#include "nsIXMLParserObject.h"
#include "nsIXPFCCommandReceiver.h"

class nsXPFCMenuItem : public nsIXPFCMenuItem,
                   public nsIXMLParserObject
{

public:

  /**
   * Constructor and Destructor
   */

  nsXPFCMenuItem();
  ~nsXPFCMenuItem();

  /**
   * ISupports Interface
   */
  NS_DECL_ISUPPORTS

  /**
   * Initialize Method
   * @result The result of the initialization, NS_OK if no errors
   */
  NS_IMETHOD Init();

  // nsIXMLParserObject methods
  NS_IMETHOD SetParameter(nsString& aKey, nsString& aValue) ;

  NS_IMETHOD SetName(nsString& aName) ;
  NS_IMETHOD_(nsString&) GetName() ;
  NS_IMETHOD SetLabel(nsString& aLabel) ;
  NS_IMETHOD_(nsString&) GetLabel() ;
  NS_IMETHOD SetCommand(nsString& aLabel);
  NS_IMETHOD_(nsString&) GetCommand();

  NS_IMETHOD_(nsIXPFCMenuContainer *) GetParent();
  NS_IMETHOD SetParent(nsIXPFCMenuContainer * aMenuContainer);

  NS_IMETHOD_(PRBool) IsSeparator() ;

  NS_IMETHOD_(nsAlignmentStyle) GetAlignmentStyle();
  NS_IMETHOD SetAlignmentStyle(nsAlignmentStyle aAlignmentStyle);

  NS_IMETHOD_(PRUint32) GetMenuID() ;
  NS_IMETHOD SetMenuID(PRUint32 aID) ;

  NS_IMETHOD SendCommand() ;
  NS_IMETHOD SetReceiver(nsIXPFCCommandReceiver * aReceiver) ;

  NS_IMETHOD_(PRBool) GetEnabled();
  NS_IMETHOD SetEnabled(PRBool aEnable);

private:
  nsString mName;
  nsString mLabel;
  nsString mCommand;
  nsIXPFCMenuContainer * mParent;
  PRBool mSeparator;
  nsAlignmentStyle mAlignmentStyle;
  PRUint32 mID;
  nsIXPFCCommandReceiver * mReceiver;
  PRBool mEnabled;
};


#endif
