/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsDragHelperService_h__
#define nsDragHelperService_h__

#include "nsCOMPtr.h"
#include "nsIDragService.h"

#include <Carbon/Carbon.h>

#include "nsIDragHelperService.h"


//
// nsDragHelperService
//
// Implementation of nsIDragHelperService, a helper for managing
// drag and drop mechanics.
//

// {75993200-f3b2-11d5-a384-c7054d07d6fc}
#define NS_DRAGHELPERSERVICE_CID      \
{ 0x75993200, 0xf3b2, 0x11d5, { 0xa3, 0x84, 0xc7, 0x05, 0x4d, 0x07, 0xd6, 0xfc } }


class nsDragHelperService : public nsIDragHelperService
{
public:
  nsDragHelperService();
  virtual ~nsDragHelperService();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDRAGHELPERSERVICE

protected:

    // Send a drag event into Gecko
  PRBool DoDragAction(PRUint32      aMessage,
                      DragReference aDragRef,
                      nsIEventSink* aSink);

    // handles modifier keys (cmd, option, etc) and sets the appropriate 
    // action (move, copy, etc) based on the given modifiers.
  void SetDragActionBasedOnModifiers ( short inModifiers ) ;

    // Arranges to send NS_DRAGDROP_OVER events at regular intervals
  void SetDragOverTimer(nsIEventSink* aSink, DragRef aDragRef);
  static void DragOverTimerHandler(EventLoopTimerRef aTimer, void* aUserData);
  
    // holds our drag service across multiple calls to this callback. The reference to
    // the service is obtained when the mouse enters the window and is released when
    // the mouse leaves the window (or there is a drop). This prevents us from having
    // to re-establish the connection to the service manager 15 times a second when
    // handling the |kDragTrackingInWindow| message.
  nsCOMPtr<nsIDragService> mDragService;

    // Used to implement the NS_DRAGDROP_OVER timer
  EventLoopTimerRef      mDragOverTimer;
  nsCOMPtr<nsIEventSink> mDragOverSink;
  DragRef                mDragOverDragRef;
}; // nsDragHelperService


#endif // nsDragHelperService_h__
