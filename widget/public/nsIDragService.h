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

#ifndef nsIDragService_h__
#define nsIDragService_h__

#include "nsISupports.h"

class nsIDragSession;
class nsITransferable;
class nsISupportsArray;
class nsIRegion;

// {8B5314BB-DB01-11d2-96CE-0060B0FB9956}
#define NS_IDRAGSERVICE_IID      \
{ 0x8b5314bb, 0xdb01, 0x11d2, { 0x96, 0xce, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } }


class nsIDragService : public nsISupports {

  public:

    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDRAGSERVICE_IID)

    enum {
      DRAGDROP_ACTION_NONE = 0x0000,
      DRAGDROP_ACTION_COPY = 0x0001,
      DRAGDROP_ACTION_MOVE = 0x0002,
      DRAGDROP_ACTION_LINK = 0x0004
    };
    
  /**
    * Starts a modal drag session with an array of transaferables 
    *
    * @param  anArrayTransferables - an array of transferables to be dragged
    * @param  aRegion - a region containing rectangles for cursor feedback, 
    *            in window coordinates.
    */
    NS_IMETHOD InvokeDragSession (nsISupportsArray * anArrayTransferables, nsIRegion * aRegion, PRUint32 aActionType) = 0;

  /**
    * Starts a modal drag session with a single transferable
    *
    * @param  aTransferable the transferable to be dragged
    * @param  aRegion - a region containing rectangles for cursor feedback, 
    *            in window coordinates.
    */
    NS_IMETHOD InvokeDragSessionSingle (nsITransferable * aTransferable,  nsIRegion * aRegion, PRUint32 aActionType) = 0;

  /**
    * Returns the current Drag Session  
    *
    * @param  aSession the current drag session
    */
    NS_IMETHOD GetCurrentSession (nsIDragSession ** aSession) = 0;

  /**
    * Tells the Drag Service to start a drag sessiojn, this is called when
    * an external drag occurs
    *
    */
    NS_IMETHOD StartDragSession () = 0;

  /**
    * Tells the Drag Service to end a drag sessiojn, this is called when
    * an external drag occurs
    *
    */
    NS_IMETHOD EndDragSession () = 0;

};

#endif
