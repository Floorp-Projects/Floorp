/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsIXULCommand_h___
#define nsIXULCommand_h___

#include "nsString.h"
#include "nsIDOMElement.h"
#include "nsIWebShell.h"
class nsIDOMNode;
class nsIFactory;


// Interface ID for nsIXULCommand
#define NS_IXULCOMMAND_IID \
 { 0xabf645a1, 0xb3d0, 0x11d2, { 0x9a, 0x42, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74}}

// Class ID for an implementation of nsIXULCommand
#define NS_XULCOMMAND_CID \
 { 0xabf645a1, 0xb3d0, 0x11d2, { 0x9a, 0x42, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74}}


//----------------------------------------------------------------------


class nsIXULCommand : public nsISupports {
public:

  /**
   * Adds a source widget/DOMNode to the command
   * @param aNode the DOM node to be added
   * @return NS_OK if there was an node was added otherwise NS_ERROR_FAILURE 
   */
  NS_IMETHOD AddUINode(nsIDOMNode * aNode) = 0;

  /**
   * Removes a source widget/DOMNode from the command
   * @param aNode the DOM node to be remove
   * @return NS_OK if there was an node was remove otherwise NS_ERROR_FAILURE 
   */
  NS_IMETHOD RemoveUINode(nsIDOMNode * aCmd) = 0;

  /**
   * Sets the name of the command
   * @param aName the name of the command
   * @return NS_OK 
   */
  NS_IMETHOD SetName(const nsString &aName) = 0;

  /**
   * Gets the name of the command
   * @param aName the name of the command
   * @return NS_OK 
   */
  NS_IMETHOD GetName(nsString &aName) const = 0;


  /**
   * Sets the command to be enabled/disabled, this then sets all the Source DOMNodes
   * and the corresponding widgets/controls to enabled/disabled
   * @param aIsEnabled the new enabled/disabled state
   * @return NS_OK 
   */
  NS_IMETHOD SetEnabled(PRBool aIsEnabled) = 0;

  /**
   * Returns whether the command is enabled or not
   * @param aIsEnabled the out parameter with the new enabled/disabled state
   * @return NS_OK 
   */
  NS_IMETHOD GetEnabled(PRBool & aIsEnabled) = 0;

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

  /**
   * Sets the Tooltip of the command
   * @param aTip the Tooltip of the command
   * @return NS_OK 
   */
  NS_IMETHOD SetTooltip(const nsString &aTip) = 0;

  /**
   * Gets the Tooltip of the command
   * @param aTip the Tooltip of the command
   * @return NS_OK 
   */
  NS_IMETHOD GetTooltip(nsString &aTip) const = 0;

  /**
   * Sets the description of the command
   * @param aDescription the description of the command
   * @return NS_OK 
   */
  NS_IMETHOD SetDescription(const nsString &aDescription) = 0;

  /**
   * Gets the description of the command
   * @param aDescription the description of the command
   * @return NS_OK 
   */
  NS_IMETHOD GetDescription(nsString &aDescription) const = 0;

  // Some of the originally specific methods
  /*
  NS_IMETHOD Create();
  NS_IMETHOD Destroy();
  NS_IMETHOD AddOriginatingWidget(const nsIFrame *aWidget);
  NS_IMETHOD RemoveOriginatingWidget(nsIFrame *aWidget);
  NS_IMETHOD SetSelected(PRBool aSelected);
  NS_IMETHOD GetSelected(PRBool *aSelected) const;
*/

  /* DO NOT CALL THESE METHODS
   * These are here as a temporary hack - cps
   */
  NS_IMETHOD SetDOMElement(nsIDOMElement * aDOMNode) = 0;
  NS_IMETHOD SetWebShell(nsIWebShell * aWebShell) = 0;
};


extern "C" nsresult
NS_NewXULCommandFactory(nsIFactory** aFactory);

#endif /* nsIXULCommand_h___ */
