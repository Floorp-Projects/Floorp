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
// See associated header file for details
//


#include "nsDragService.h"

#include "nsITransferable.h"
#include "nsString.h"
#include "nsMimeMapper.h"
#include "nsWidgetsCID.h"
#include "nsClipboard.h"
#include "nsIRegion.h"
#include "nsVoidArray.h"


DragSendDataUPP nsDragService::sDragSendDataUPP = NewDragSendDataProc(DragSendDataProc);


//
// DragService constructor
//
nsDragService::nsDragService()
  : mDragRef(0), mDataItems(nsnull)
{
}


//
// DragService destructor
//
nsDragService::~nsDragService()
{
}


//
// AddRef
// Release
// QueryInterface
//
// handle the QI for nsIDragSessionMac and farm off anything else to the parent
// class.
//
NS_IMPL_ISUPPORTS_INHERITED(nsDragService,nsBaseDragService,nsIDragSessionMac);


//
// StartDragSession
//
// Do all the work to kick it off.
//
NS_IMETHODIMP
nsDragService :: InvokeDragSession (nsISupportsArray * aTransferableArray, nsIRegion * aDragRgn, PRUint32 aActionType)

{
  OSErr result = ::NewDrag(&mDragRef);
  if ( !result )
    return NS_ERROR_FAILURE;

  // add the flavors from the transferables. Cache this array for the send data proc
  RegisterDragItemsAndFlavors ( aTransferableArray ) ;
  mDataItems = aTransferableArray;
  
  // create the drag region. Pull out the native mac region from the nsIRegion we're
  // given, copy it, inset it one pixel, and subtract them so we're left with just an
  // outline. Too bad we can't do this with gfx api's.
  //
  // At the end, we are left with an outline of the region in global coordinates.
  RgnHandle dragRegion = nsnull;
  aDragRgn->GetNativeRegion(dragRegion);
  RgnHandle insetDragRegion = ::NewRgn();
  if ( dragRegion && insetDragRegion ) {
    ::CopyRgn ( dragRegion, insetDragRegion );
    ::InsetRgn ( insetDragRegion, 1, 1 );
    ::DiffRgn ( dragRegion, insetDragRegion, insetDragRegion ); 

    // now shift the region into global coordinates.
    Point offsetFromLocalToGlobal = { 0, 0 };
    ::LocalToGlobal ( &offsetFromLocalToGlobal );
    ::OffsetRgn ( insetDragRegion, offsetFromLocalToGlobal.h, offsetFromLocalToGlobal.v );
  }

  // register drag send proc which will call us back when asked for the actual
  // flavor data (instead of placing it all into the drag manager)
  ::SetDragSendProc ( mDragRef, sDragSendDataUPP, this );

  // we have to synthesize the native event because we may be called from JavaScript
  // through XPConnect. In that case, we only have a DOM event and no way to
  // get to the native event. As a consequence, we just always fake it.
  Point globalMouseLoc;
  ::GetMouse(&globalMouseLoc);
  ::LocalToGlobal(&globalMouseLoc);
  WindowPtr theWindow = nsnull;;
  if ( ::FindWindow(globalMouseLoc, &theWindow) != inContent ) {
    // debugging sanity check
    #ifdef NS_DEBUG
    DebugStr("\pAbout to start drag, but FindWindow() != inContent");
    #endif
  }

  EventRecord theEvent;
  theEvent.what = mouseDown;
  theEvent.message = reinterpret_cast<UInt32>(theWindow);
  theEvent.when = 0;
  theEvent.where = globalMouseLoc;
  theEvent.modifiers = 0;

  // start the drag
  result = ::TrackDrag ( mDragRef, &theEvent, insetDragRegion );

  // clean up after ourselves 
  ::DisposeRgn ( insetDragRegion );
  ::DisposeDrag ( mDragRef );
  mDataItems = nsnull;

  return NS_OK; 

} // StartDragSession


//
// RegisterDragItemsAndFlavors
//
// Takes the multiple drag items from an array of transferables and registers them
// and their flavors with the MacOS DragManager. Note that we don't actually place
// any of the data there yet, but will rely on a sendDataProc to get the data as
// requested.
//
void
nsDragService :: RegisterDragItemsAndFlavors ( nsISupportsArray * inArray )
{
  const FlavorFlags flags = 0;
  
  unsigned int numDragItems = 0;
  inArray->Count ( &numDragItems ) ;
  for ( int itemIndex = 0; itemIndex < numDragItems; ++itemIndex ) {  
    nsCOMPtr<nsITransferable> currItem ( do_QueryInterface((*inArray)[itemIndex]) );
    if ( currItem ) {   
      nsVoidArray* flavorList = nsnull;
      if ( NS_SUCCEEDED(currItem->FlavorsTransferableCanExport(&flavorList)) ) {
        for ( int flavorIndex = 0; flavorIndex < flavorList->Count(); ++flavorIndex ) {
        
	      nsString* currentFlavor = NS_STATIC_CAST(nsString*, (*flavorList)[flavorIndex]);
	      if ( nsnull != currentFlavor ) {
	        FlavorType macOSFlavor = nsMimeMapperMac::MapMimeTypeToMacOSType(*currentFlavor);
	        ::AddDragItemFlavor ( mDragRef, itemIndex, macOSFlavor, NULL, 0, flags );
          }
          
        } // foreach flavor in item              
      } // if valid flavor list
    } // if item is a transferable  
  } // foreach drag item

} // RegisterDragItemsAndFlavors


//
// GetData
//
// Pull data out of the OS drag items and stash it into the given transferable.
// Only put in the data with the highest fidelity asked for.
//
// NOTE: THIS API NEEDS TO CHANGE TO RETURN A LIST OF TRANSFERABLES. IT IS 
// UNSUITABLE FOR MULTIPLE DRAG ITEMS. AS A HACK, THE INITIAL IMPLEMENTATION WILL
// ONLY LOOK AT THE FIRST DRAG ITEM.
//
// NOTE: I ALSO NEED TO COME BACK IN AND DO BETTER ERROR CHECKING ON DRAGMANAGER ROUTINES
// BUT NOT UNTIL THIS IS CLEANED UP TO WORK WITH MULTIPLE TRANSFERABLES.
//
NS_IMETHODIMP
nsDragService :: GetData (nsITransferable * aTransferable)
{
printf("------nsDragService :: getData\n");
  nsresult errCode = NS_ERROR_FAILURE;

  // make sure we have a good transferable
  if ( !aTransferable )
    return NS_ERROR_INVALID_ARG;

  // get flavor list that includes all acceptable flavors (including ones obtained through
  // conversion)
  nsVoidArray * flavorList;
  errCode = aTransferable->FlavorsTransferableCanImport ( &flavorList );
  if ( errCode != NS_OK )
    return NS_ERROR_FAILURE;

  // get the data for each drag item. Remember that GetDragItemReferenceNumber()
  // is one-based NOT zero-based.
  unsigned short numDragItems = 0;
  ::CountDragItems ( mDragRef, &numDragItems );
printf("CountDragItems says :: %ld\n", numDragItems);
  for ( int item = 1; item <= numDragItems; ++item ) {
  
    ItemReference itemRef;
    ::GetDragItemReferenceNumber ( mDragRef, item, &itemRef );
printf("item ref = %ld\n", itemRef );
 
	  // Now walk down the list of flavors. When we find one that is actually present,
	  // copy out the data into the transferable in that format. SetTransferData()
	  // implicitly handles conversions.
	  PRUint32 cnt = flavorList->Count();
	  for ( int i = 0; i < cnt; ++i ) {
	    nsString * currentFlavor = NS_STATIC_CAST(nsString*, (*flavorList)[i]);
	    if ( nsnull != currentFlavor ) {
	      // find MacOS flavor
	      FlavorType macOSFlavor = nsMimeMapperMac::MapMimeTypeToMacOSType(*currentFlavor);
printf("looking for data in type %s, mac flavor %ld\n", currentFlavor->ToNewCString(), macOSFlavor);
	    
	      // check if it is present in the current drag item.
	      FlavorFlags unused;
	      if ( ::GetFlavorFlags(mDragRef, itemRef, macOSFlavor, &unused) == noErr ) {
	      
printf("flavor found\n");
	        // we have it, pull it out of the drag manager. Put it into memory that we allocate
	        // with new[] so that the tranferable can own it (and then later use delete[]
	        // on it).
	        Size dataSize = 0;
	        OSErr err = ::GetFlavorDataSize ( mDragRef, itemRef, macOSFlavor, &dataSize );
printf("flavor data size is %ld, err is %ld\n", dataSize, err);
	        if ( !err && dataSize > 0 ) {
	          char* dataBuff = new char[dataSize];
	          if ( !dataBuff )
	            return NS_ERROR_OUT_OF_MEMORY;
	          
	          err = ::GetFlavorData ( mDragRef, itemRef, macOSFlavor, dataBuff, &dataSize, 0 );
	          if ( err ) {
	            #ifdef NS_DEBUG
	               printf("nsClipboard: Error getting data off the clipboard, #%d\n", dataSize);
	            #endif
	            return NS_ERROR_FAILURE;
	          }
	          
	          // put it into the transferable
	          errCode = aTransferable->SetTransferData ( currentFlavor, dataBuff, dataSize );
	          #ifdef NS_DEBUG
	            if ( errCode != NS_OK ) printf("nsClipboard:: Error setting data into transferable\n");
	          #endif
	        } 
	        else {
	           #ifdef NS_DEBUG
	             printf("nsClipboard: Error getting data off the clipboard, #%d\n", dataSize);
	           #endif
	           errCode = NS_ERROR_FAILURE;
	        }
	        
	        // we found one, get out of this loop!
	        break;
	        
	      } // if a flavor found
	    }
	  } // foreach flavor
  
  	  // SMARMY HACK UNTIL API IS UPDATED. ONLY GET THE FIRST DRAG ITEM
      break;
      
  } // foreach drag item
  
  delete flavorList;
  
printf("------nsDragService :: getData\n");
  return errCode;
}


//
// IsDataFlavorSupported
//
// Check the OS to see if the given drag flavor is in the list. Oddly returns
// NS_OK for success and NS_ERROR_FAILURE if flavor is not present.
//
NS_IMETHODIMP
nsDragService :: IsDataFlavorSupported(nsString * aDataFlavor)
{
  nsresult flavorSupported = NS_ERROR_FAILURE;

  // convert to 4 character MacOS type
  FlavorType macFlavor = nsMimeMapperMac::MapMimeTypeToMacOSType(*aDataFlavor);

  // search through all drag items looking for something with this flavor
  unsigned short numDragItems = 0;
  ::CountDragItems ( mDragRef, &numDragItems );
  for ( int i = 0; i < numDragItems; ++i ) {
    ItemReference currItem;
    OSErr res = ::GetDragItemReferenceNumber ( mDragRef, i, &currItem );
    if ( res != noErr )
      return NS_ERROR_FAILURE;

    FlavorFlags ignored;
    char foundFlavor = ::GetFlavorFlags(mDragRef, currItem, macFlavor, &ignored) == noErr;
    if ( foundFlavor )
      flavorSupported = NS_OK;
  } // for each item in drag

  return flavorSupported;

} // IsDataFlavorSupported


//
// SetDragReference
//
// An API to allow the drag manager callback functions to tell the session about the
// current dragRef w/out resorting to knowing the internals of the implementation
//
NS_IMETHODIMP
nsDragService :: SetDragReference ( DragReference aDragRef )
{
  mDragRef = aDragRef;
  return NS_OK;
  
} // SetDragReference


//
// DragSendDataProc
//
// Called when a drop occurs and the drop site asks for the data.
//
// This will only get called when Mozilla is the originator of the drag, so we can
// make certain assumptions. One is that we've cached the list of transferables in the
// drag session. The other is that the item ref is the index into this list so we
// can easily pull out the item asked for.
// 
pascal OSErr
nsDragService :: DragSendDataProc ( FlavorType inFlavor, void* inRefCon, ItemReference inItemRef,
									DragReference inDragRef )
{
  OSErr retVal = noErr;
  nsDragService* self = NS_STATIC_CAST(nsDragService*, inRefCon);
  NS_ASSERTION ( self, "Refcon not set correctly for DragSendDataProc" );
  if ( self ) {
    nsCOMPtr<nsITransferable> item ( do_QueryInterface(self->mDataItems->ElementAt(inItemRef)) );
    if ( item ) {
    
      nsString mimeFlavor;
      nsMimeMapperMac::MapMacOSTypeToMimeType ( inFlavor, mimeFlavor ); 
      
      void* data = nsnull;
      PRUint32 dataSize = 0;
      if ( NS_SUCCEEDED(item->GetTransferData(&mimeFlavor, &data, &dataSize)) ) {      
        // make the data accessable to the DragManager
        retVal = ::SetDragItemFlavorData ( inDragRef, inItemRef, inFlavor, data, dataSize, 0 );
      }
      else
        retVal = cantGetFlavorErr;
        
    } // if valid item
  } // if valid refcon
  
  return retVal;
  
} // DragSendDataProc
