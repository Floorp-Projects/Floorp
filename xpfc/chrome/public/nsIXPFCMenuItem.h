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

#ifndef nsIXPFCMenuItem_h___
#define nsIXPFCMenuItem_h___

#include "nsISupports.h"
#include "nsString.h"
#include "nsIXPFCCommandReceiver.h"

// 8a400b00-2cbe-11d2-9246-00805f8a7ab6
#define NS_IXPFCMENUITEM_IID      \
 { 0x8a400b00, 0x2cbe, 0x11d2, \
   {0x92, 0x46, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6} }

class nsIXPFCMenuContainer;

enum nsAlignmentStyle 
{   
  eAlignmentStyle_none,
  eAlignmentStyle_left,
  eAlignmentStyle_right
}; 


class nsIXPFCMenuItem : public nsISupports 
{

public:

  /**
   * Initialize the XPFCMenuBar
   * @result The result of the initialization, NS_Ok if no errors
   */
  NS_IMETHOD Init() = 0;

  NS_IMETHOD SetName(nsString& aName) = 0;
  NS_IMETHOD_(nsString&) GetName() = 0;

  NS_IMETHOD SetLabel(nsString& aLabel) = 0;
  NS_IMETHOD_(nsString&) GetLabel() = 0;

  NS_IMETHOD SetCommand(nsString& aLabel) = 0;
  NS_IMETHOD_(nsString&) GetCommand() = 0;

  NS_IMETHOD_(nsIXPFCMenuContainer *) GetParent() = 0;
  NS_IMETHOD SetParent(nsIXPFCMenuContainer * aMenuContainer) = 0;

  NS_IMETHOD_(PRBool) IsSeparator() = 0;
  NS_IMETHOD_(nsAlignmentStyle) GetAlignmentStyle() = 0;
  NS_IMETHOD SetAlignmentStyle(nsAlignmentStyle aAlignmentStyle) = 0;

  NS_IMETHOD_(PRUint32) GetMenuID() = 0;
  NS_IMETHOD SetMenuID(PRUint32 aID) = 0;

  NS_IMETHOD SendCommand() = 0;

  NS_IMETHOD SetReceiver(nsIXPFCCommandReceiver * aReceiver) = 0;

  NS_IMETHOD_(PRBool) GetEnabled() = 0;
  NS_IMETHOD SetEnabled(PRBool aEnable) = 0;
};

#endif /* nsIXPFCMenuItem_h___ */

