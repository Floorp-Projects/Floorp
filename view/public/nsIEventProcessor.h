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

#ifndef nsIEventProcessor_h___
#define nsIEventProcessor_h___

#include "nsISupports.h"
#include "nsEvent.h"

struct nsGUIEvent;

// IID for the nsIEventProcessor interface
// {DCC4C0F0-6D61-496f-96B1-F29226A62332}
#define NS_IEVENTPROCESSOR_IID    \
{ 0xdcc4c0f0, 0x6d61, 0x496f, \
{ 0x96, 0xb1, 0xf2, 0x92, 0x26, 0xa6, 0x23, 0x32 } }

//----------------------------------------------------------------------

/**
 * Event Processor interface
 *
 * Enables others to plug into the ViewManager so they can 
 * discard events of their choosing
 */
class nsIEventProcessor : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IEVENTPROCESSOR_IID)

  /**
   * Returns NS_OK if event should be processed, return NS_ERROR_FAILURE if
   * the event should be discarded
   * @param aEvent an event to be looked at
   * @param aStatus status of event
   */
  NS_IMETHOD ProcessEvent(nsGUIEvent *aEvent, PRBool aIsInContentArea, nsEventStatus *aStatus) const = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIEVENTPROCESSOR \
  NS_IMETHOD ProcessEvent(nsGUIEvent *aEvent, PRBool aIsInContentArea, nsEventStatus *aStatus) const;

#endif
