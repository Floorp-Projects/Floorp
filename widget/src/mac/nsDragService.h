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
#include <MacWindows.h>

class  nsNativeDragTarget;


class nsDragService : public nsBaseDragService, public nsIDragSessionMac
{
public:
  nsDragService();
  virtual ~nsDragService();

  //nsISupports - can't use inherited because of nsIDragSessionMac
  NS_DECL_ISUPPORTS_INHERITED
  
  //nsIDragService
  NS_IMETHOD InvokeDragSession (nsIDOMNode *aDOMNode, nsISupportsArray * anArrayTransferables, nsIScriptableRegion * aRegion, PRUint32 aActionType);

  //nsIDragSession
  NS_IMETHOD GetData (nsITransferable * aTransferable, PRUint32 aItemIndex);
  NS_IMETHOD IsDataFlavorSupported(const char *aDataFlavor, PRBool *_retval);
  NS_IMETHOD GetNumDropItems (PRUint32 * aNumItems);

  //nsIDragSessionMac
  NS_IMETHOD SetDragReference ( DragReference aDragRef ) ;  

private:

  NS_IMETHOD StartDragSession ( ) ;
  NS_IMETHOD EndDragSession ( ) ;
  
  char* LookupMimeMappingsForItem ( DragReference inDragRef, ItemReference itemRef ) ;

  void RegisterDragItemsAndFlavors ( nsISupportsArray * inArray ) ;
  PRBool BuildDragRegion ( nsIScriptableRegion* inRegion, nsIDOMNode* inNode, RgnHandle ioDragRgn ) ;
  OSErr GetDataForFlavor ( nsISupportsArray* inDragItems, DragReference inDragRef, unsigned int inItemIndex, 
                             FlavorType inFlavor, void** outData, unsigned int * outSize ) ;
  nsresult ExtractDataFromOS ( DragReference inDragRef, ItemReference inItemRef, ResType inFlavor, 
                                 void** outBuffer, PRInt32* outBuffSize ) ;

    // compute a screen rect from the frame associated with the given dom node
  PRBool ComputeGlobalRectFromFrame ( nsIDOMNode* aDOMNode, Rect & outScreenRect ) ;

    // callback for the MacOS DragManager when a drop site asks for data
  static pascal OSErr DragSendDataProc ( FlavorType inFlavor, void* inRefCon,
  										 ItemReference theItemRef, DragReference inDragRef ) ;

  PRBool mImageDraggingSupported;
  DragSendDataUPP mDragSendDataUPP;
  DragReference mDragRef;        // reference to _the_ drag. There can be only one.
  nsISupportsArray* mDataItems;  // cached here for when we start the drag so the 
                                 // DragSendDataProc has access to them. 
                                 // ONLY VALID DURING A DRAG STARTED WITHIN THIS APP.

}; // class nsDragService


#endif // nsDragService_h__

