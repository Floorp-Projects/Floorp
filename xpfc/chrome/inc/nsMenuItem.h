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

#ifndef __NS_MENU_ITEM
#define __NS_MENU_ITEM

#include "nsIMenuItem.h"
#include "nsIXMLParserObject.h"
#include "nsIXPFCCommandReceiver.h"

class nsMenuItem : public nsIMenuItem,
                   public nsIXMLParserObject
{

public:

  /**
   * Constructor and Destructor
   */

  nsMenuItem();
  ~nsMenuItem();

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

  NS_IMETHOD_(nsIMenuContainer *) GetParent();
  NS_IMETHOD SetParent(nsIMenuContainer * aMenuContainer);

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
  nsIMenuContainer * mParent;
  PRBool mSeparator;
  nsAlignmentStyle mAlignmentStyle;
  PRUint32 mID;
  nsIXPFCCommandReceiver * mReceiver;
  PRBool mEnabled;
};


#endif
