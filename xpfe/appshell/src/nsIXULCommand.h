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
 * The Original Code is Mozilla Communicator client code.
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
#ifndef nsIXULCommand_h___
#define nsIXULCommand_h___

#include "nsIMenuListener.h"
#include "nsString.h"
#include "nsIDOMElement.h"
#include "nsIDocShell.h"
class nsIDOMNode;
class nsIFactory;
class nsIMenuItem;

// Interface ID for nsIXULCommand
#define NS_IXULCOMMAND_IID \
 { 0xabf645a1, 0xb3d0, 0x11d2, { 0x9a, 0x42, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74}}

// Class ID for an implementation of nsIXULCommand
#define NS_XULCOMMAND_CID \
 { 0xabf645a1, 0xb3d0, 0x11d2, { 0x9a, 0x42, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74}}


//----------------------------------------------------------------------


class nsIXULCommand : public nsIMenuListener {
public:

  static const nsIID& GetIID() { static nsIID iid = NS_IXULCOMMAND_IID; return iid; }

  /**
   * Sets the menu
   * @param aMenuItem the menu
   * @return NS_OK 
   */
  NS_IMETHOD SetMenuItem(nsIMenuItem * aMenuItem) = 0;

  /**
   * Notifies that an attribute has been set 
   * @param aAttr that changed
   * @return NS_OK 
   */
  NS_IMETHOD AttributeHasBeenSet(const nsString & aAttr) = 0;

  /**
   * Sets the JavaScript Command to be invoked when a "gui" event occurs on a source widget
   * @param aStrCmd the JS command to be cached for later execution
   * @return NS_OK 
   */
  NS_IMETHOD SetCommand(const nsString & aStrCmd) = 0;

  /**
   * Executes the "cached" JavaScript Command 
   * @return NS_OK if the command was executed properly, otherwise an error code
   */
  NS_IMETHOD DoCommand() = 0;

  NS_IMETHOD SetDOMElement(nsIDOMElement * aDOMElement) = 0;
  NS_IMETHOD GetDOMElement(nsIDOMElement ** aDOMElement) = 0;
  NS_IMETHOD SetDocShell(nsIDocShell * aDocShell) = 0;
};


extern "C" nsresult
NS_NewXULCommandFactory(nsIFactory** aFactory);

#endif /* nsIXULCommand_h___ */
