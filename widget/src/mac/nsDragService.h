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


//
// Mike Pinkerton
// Netscape Communications
//
// The MacOS native implementation of nsIDragService, nsIDragSession, and
// nsIDragSessionMac.
//


#ifndef nsDragService_h__
#define nsDragService_h__


#include "nsBaseDragService.h"
#include "nsIDragSessionMac.h"
#include <Drag.h>


class  nsNativeDragTarget;


class nsDragService : public nsBaseDragService, public nsIDragSessionMac
{
public:
  nsDragService();
  virtual ~nsDragService();

  //nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  //nsIDragService
  NS_IMETHOD InvokeDragSession (nsISupportsArray * anArrayTransferables, nsIRegion * aRegion, PRUint32 aActionType);
  //NS_IMETHOD InvokeDragSessionSingle (nsITransferable * aTransferable,  nsIRegion * aRegion, PRUint32 aActionType);

  //nsIDragSession
  NS_IMETHOD GetData (nsITransferable * aTransferable);
  NS_IMETHOD IsDataFlavorSupported(nsString * aDataFlavor);

  //nsIDragSessionMac
  NS_IMETHOD SetDragReference ( DragReference aDragRef ) ;  

private:

  DragReference mDragRef;    // reference to _the_ drag. There can be only one.

}; // class nsDragService


#endif // nsDragService_h__

