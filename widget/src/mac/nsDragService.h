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

  //nsISupports - can't use inherited because of nsIDragSessionMac
  NS_DECL_ISUPPORTS_INHERITED
  
  //nsIDragService
  NS_IMETHOD InvokeDragSession (nsISupportsArray * anArrayTransferables, nsIRegion * aRegion, PRUint32 aActionType);

  //nsIDragSession
  NS_IMETHOD GetData (nsITransferable * aTransferable, PRUint32 aItemIndex);
  NS_IMETHOD IsDataFlavorSupported(const char *aDataFlavor, PRBool *_retval);
  NS_IMETHOD GetNumDropItems (PRUint32 * aNumItems);

  //nsIDragSessionMac
  NS_IMETHOD SetDragReference ( DragReference aDragRef ) ;  

private:

  char* LookupMimeMappingsForItem ( DragReference inDragRef, ItemReference itemRef ) ;

  void RegisterDragItemsAndFlavors ( nsISupportsArray * inArray ) ;
  void BuildDragRegion ( nsIRegion* inRegion, Point inGlobalMouseLoc, RgnHandle ioDragRgn ) ;
  OSErr GetDataForFlavor ( nsISupportsArray* inDragItems, DragReference inDragRef, unsigned int inItemIndex, 
                             FlavorType inFlavor, void** outData, unsigned int * outSize ) ;

    // callback for the MacOS DragManager when a drop site asks for data
  static pascal OSErr DragSendDataProc ( FlavorType inFlavor, void* inRefCon,
  										 ItemReference theItemRef, DragReference inDragRef ) ;

  static DragSendDataUPP sDragSendDataUPP;
  DragReference mDragRef;        // reference to _the_ drag. There can be only one.
  nsISupportsArray* mDataItems;  // cached here for when we start the drag so the 
                                 // DragSendDataProc has access to them. 
                                 // ONLY VALID DURING A DRAG STARTED WITHIN THIS APP.

}; // class nsDragService


#endif // nsDragService_h__

