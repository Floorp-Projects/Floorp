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
#include "nsClipboard.h"
#include "nsIRegion.h"
#include "nsVoidArray.h"
#include "nsISupportsPrimitives.h"
#include "nsCOMPtr.h"


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
  DragReference theDragRef;
  OSErr result = ::NewDrag(&theDragRef);
  if ( result )
    return NS_ERROR_FAILURE;
  mDragRef = theDragRef;
  
  // add the flavors from the transferables. Cache this array for the send data proc
  mDataItems = aTransferableArray;
  RegisterDragItemsAndFlavors ( aTransferableArray ) ;
  
  // we have to synthesize the native event because we may be called from JavaScript
  // through XPConnect. In that case, we only have a DOM event and no way to
  // get to the native event. As a consequence, we just always fake it.
  Point globalMouseLoc;
  ::GetMouse(&globalMouseLoc);
  ::LocalToGlobal(&globalMouseLoc);
  WindowPtr theWindow = nsnull;
  if ( ::FindWindow(globalMouseLoc, &theWindow) != inContent ) {
    // debugging sanity check
    #ifdef NS_DEBUG
    DebugStr("\pAbout to start drag, but FindWindow() != inContent; g");
    #endif
  }  
  EventRecord theEvent;
  theEvent.what = mouseDown;
  theEvent.message = reinterpret_cast<UInt32>(theWindow);
  theEvent.when = 0;
  theEvent.where = globalMouseLoc;
  theEvent.modifiers = 0;
  
  RgnHandle theDragRgn = ::NewRgn();
  BuildDragRegion ( aDragRgn, globalMouseLoc, theDragRgn );

  // register drag send proc which will call us back when asked for the actual
  // flavor data (instead of placing it all into the drag manager)
  ::SetDragSendProc ( theDragRef, sDragSendDataUPP, this );

  // start the drag. Be careful, mDragRef will be invalid AFTER this call (it is
  // reset by the dragTrackingHandler).
  ::TrackDrag ( theDragRef, &theEvent, theDragRgn );

  // clean up after ourselves 
  ::DisposeRgn ( theDragRgn );
  result = ::DisposeDrag ( theDragRef );
  NS_ASSERTION ( result == noErr, "Error disposing drag" );
  mDragRef = 0L;
  mDataItems = nsnull;

  return NS_OK; 

} // StartDragSession


//
// BuildDragRegion
//
// Given the XP region describing the drag rectangles, build up an appropriate drag region. If
// the region we're given is null, create our own placeholder.
//
void
nsDragService :: BuildDragRegion ( nsIRegion* inRegion, Point inGlobalMouseLoc, RgnHandle ioDragRgn )
{
  // create the drag region. Pull out the native mac region from the nsIRegion we're
  // given, copy it, inset it one pixel, and subtract them so we're left with just an
  // outline. Too bad we can't do this with gfx api's.
  //
  // At the end, we are left with an outline of the region in global coordinates.
  if ( inRegion ) {
    RgnHandle dragRegion = nsnull;
    inRegion->GetNativeRegion(dragRegion);
    if ( dragRegion && ioDragRgn ) {
      ::CopyRgn ( dragRegion, ioDragRgn );
      ::InsetRgn ( ioDragRgn, 1, 1 );
      ::DiffRgn ( dragRegion, ioDragRgn, ioDragRgn ); 

      // now shift the region into global coordinates.
      Point offsetFromLocalToGlobal = { 0, 0 };
      ::LocalToGlobal ( &offsetFromLocalToGlobal );
      ::OffsetRgn ( ioDragRgn, offsetFromLocalToGlobal.h, offsetFromLocalToGlobal.v );
    }
  }
  else {
    // no region provided, so we create a default one that is 50 x 50, with the topLeft
    // being at the cursor location.
    NS_WARNING ( "you MUST pass a drag region for MacOS. This is a warning" );    
    if ( ioDragRgn ) {
      Rect placeHolderRect = { inGlobalMouseLoc.v, inGlobalMouseLoc.h, inGlobalMouseLoc.v + 50, 
                                inGlobalMouseLoc.h + 50 };
      RgnHandle placeHolderRgn = ::NewRgn();
      if ( placeHolderRgn ) {
        ::RectRgn ( placeHolderRgn, &placeHolderRect );
        ::CopyRgn ( placeHolderRgn, ioDragRgn );
        ::InsetRgn ( ioDragRgn, 1, 1 );
        ::DiffRgn ( placeHolderRgn, ioDragRgn, ioDragRgn );
        
        ::DisposeRgn ( placeHolderRgn );
      }
    }
  }

} // BuildDragRegion


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
    nsMimeMapperMac theMapper;
  
    nsCOMPtr<nsISupports> genericItem;
    inArray->GetElementAt ( itemIndex, getter_AddRefs(genericItem) );
    nsCOMPtr<nsITransferable> currItem ( do_QueryInterface(genericItem) );
    if ( currItem ) {   
      nsCOMPtr<nsISupportsArray> flavorList;
      if ( NS_SUCCEEDED(currItem->FlavorsTransferableCanExport(getter_AddRefs(flavorList))) ) {
        PRUint32 numFlavors;
        flavorList->Count ( &numFlavors );
        for ( int flavorIndex = 0; flavorIndex < numFlavors; ++flavorIndex ) { 
        
          nsCOMPtr<nsISupports> genericWrapper;
          flavorList->GetElementAt ( flavorIndex, getter_AddRefs(genericWrapper) );
          nsCOMPtr<nsISupportsString> currentFlavor ( do_QueryInterface(genericWrapper) );
	      if ( currentFlavor ) {
	        char* flavorStr = nsnull;
	        currentFlavor->ToString ( &flavorStr );
	        FlavorType macOSFlavor = theMapper.MapMimeTypeToMacOSType(flavorStr);
	        ::AddDragItemFlavor ( mDragRef, itemIndex, macOSFlavor, NULL, 0, flags );
	        delete [] flavorStr;
	      }
          
        } // foreach flavor in item              
      } // if valid flavor list
    } // if item is a transferable
    
    // put the mime mapping data for this item in a special flavor. Unlike the other data,
    // we have to put the data in now (rather than defer it) or the mappings will go out 
    // of scope by the time they are asked for.
    const char* mapping = theMapper.ExportMapping(nsnull);
    if ( strlen(mapping) )
      ::AddDragItemFlavor ( mDragRef, itemIndex, nsMimeMapperMac::MappingFlavor(), 
                               mapping, strlen(mapping), flags );
	delete [] mapping;
    
  } // foreach drag item
  

} // RegisterDragItemsAndFlavors


//
// GetData
//
// Pull data out of the  OS drag item at the requested index and stash it into the 
// given transferable. Only put in the data with the highest fidelity asked for and
// stop as soon as we find a match.
//
NS_IMETHODIMP
nsDragService :: GetData ( nsITransferable * aTransferable, PRUint32 aItemIndex )
{
  nsresult errCode = NS_ERROR_FAILURE;

  // make sure we have a good transferable
  if ( !aTransferable )
    return NS_ERROR_INVALID_ARG;

  // get flavor list that includes all acceptable flavors (including ones obtained through
  // conversion). Flavors are nsISupportsStrings so that they can be seen from JS.
  nsCOMPtr<nsISupportsArray> flavorList;
  errCode = aTransferable->FlavorsTransferableCanImport ( getter_AddRefs(flavorList) );
  if ( NS_FAILED(errCode) )
    return errCode;

  // get the data for the requested drag item. Remember that GetDragItemReferenceNumber()
  // is one-based NOT zero-based like |aItemIndex| is.   
  ItemReference itemRef;
  ::GetDragItemReferenceNumber ( mDragRef, aItemIndex + 1, &itemRef );
 
  // create a mime mapper to help us out based on data in a special flavor for this item
  char* mappings = LookupMimeMappingsForItem(mDragRef, itemRef);
  nsMimeMapperMac theMapper ( mappings );
  delete [] mappings;
  
  // Now walk down the list of flavors. When we find one that is actually present,
  // copy out the data into the transferable in that format. SetTransferData()
  // implicitly handles conversions.
  PRUint32 cnt;
  flavorList->Count ( &cnt );
  for ( int i = 0; i < cnt; ++i ) {
    nsCOMPtr<nsISupports> genericWrapper;
    flavorList->GetElementAt ( i, getter_AddRefs(genericWrapper) );
    nsCOMPtr<nsISupportsString> currentFlavor ( do_QueryInterface(genericWrapper) );
    if ( currentFlavor ) {
      // find MacOS flavor
      char* flavorStr;
      currentFlavor->ToString ( &flavorStr );
      FlavorType macOSFlavor = theMapper.MapMimeTypeToMacOSType(flavorStr);
printf("looking for data in type %s, mac flavor %ld\n", flavorStr, macOSFlavor);
	    
      // check if it is present in the current drag item.
      FlavorFlags unused;
      if ( ::GetFlavorFlags(mDragRef, itemRef, macOSFlavor, &unused) == noErr ) {
	      
printf("flavor found\n");
        // we have it, pull it out of the drag manager. Put it into memory that we allocate
        // with new[] so that the tranferable can own it (and then later use delete[]
        // on it).
        Size dataSize = 0;
        OSErr err = ::GetFlavorDataSize ( mDragRef, itemRef, macOSFlavor, &dataSize );
printf("flavor data size is %ld\n", dataSize);
        if ( !err && dataSize > 0 ) {
          char* dataBuff = new char[dataSize];
          if ( !dataBuff )
            return NS_ERROR_OUT_OF_MEMORY;
	          
          err = ::GetFlavorData ( mDragRef, itemRef, macOSFlavor, dataBuff, &dataSize, 0 );
          if ( err ) {
            #ifdef NS_DEBUG
              printf("nsClipboard: Error getting data out of drag manager, #%ld\n", err);
            #endif
            return NS_ERROR_FAILURE;
          }
	          
          // put it into the transferable.
          nsCOMPtr<nsISupports> genericDataWrapper;
          CreatePrimitiveForData ( flavorStr, dataBuff, dataSize, getter_AddRefs(genericDataWrapper) );
          errCode = aTransferable->SetTransferData ( flavorStr, genericDataWrapper, dataSize );
          #ifdef NS_DEBUG
            if ( errCode != NS_OK ) printf("nsDragService:: Error setting data into transferable\n");
          #endif
          
          delete [] dataBuff;
        } 
        else {
          #ifdef NS_DEBUG
            printf("nsDragService: Error getting data out of drag manager, #%ld\n", err);
          #endif
          errCode = NS_ERROR_FAILURE;
        }
	        
        // we found one, get out of this loop!
        break;
	        
      } // if a flavor found
        
      delete [] flavorStr;
    }
  } // foreach flavor
  
  return errCode;
}


//
// IsDataFlavorSupported
//
// Check the OS to see if the given drag flavor is in the list. Oddly returns
// NS_OK for success and NS_ERROR_FAILURE if flavor is not present.
//
// еее this is obviously useless with more than one drag item. Need to specify 
// еее╩and index to this API
//
NS_IMETHODIMP
nsDragService :: IsDataFlavorSupported(const char *aDataFlavor, PRBool *_retval)
{
  if ( !_retval )
    return NS_ERROR_INVALID_ARG;

  *_retval = PR_FALSE;

  // convert to 4 character MacOS type
  //еее this is wrong because it doesn't take the mime mappings present in the
  //еее drag item flavor into account. FIX ME!
  nsMimeMapperMac theMapper;
  FlavorType macFlavor = theMapper.MapMimeTypeToMacOSType(aDataFlavor);

  // search through all drag items looking for something with this flavor. Recall
  // that drag item indices are 1-based.
  unsigned short numDragItems = 0;
  ::CountDragItems ( mDragRef, &numDragItems );
  for ( int i = 1; i <= numDragItems; ++i ) {
    ItemReference currItem;
    OSErr res = ::GetDragItemReferenceNumber ( mDragRef, i, &currItem );
    if ( res != noErr )
      return NS_ERROR_FAILURE;

    FlavorFlags ignored;
    char foundFlavor = ::GetFlavorFlags(mDragRef, currItem, macFlavor, &ignored) == noErr;
    if ( foundFlavor )
      *_retval = PR_TRUE;
  } // for each item in drag

  return NS_OK;

} // IsDataFlavorSupported


//
// GetNumDropItems
//
// Returns the number of drop items present in the current drag.
//
NS_IMETHODIMP
nsDragService :: GetNumDropItems ( PRUint32 * aNumItems )
{
  // we have to put it into a short first because that's what the MacOS API's expect.
  // After it's in a short, getting it into a long is no problem. Oh well.
  unsigned short numDragItems = 0;
  OSErr result = ::CountDragItems ( mDragRef, &numDragItems );
  *aNumItems = numDragItems;
  
  return ( result == noErr ? NS_OK : NS_ERROR_FAILURE );

} // GetNumDropItems


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
// We cannot rely on |mDragRef| at this point so we have to use what was passed in
// and pass that along.
// 
pascal OSErr
nsDragService :: DragSendDataProc ( FlavorType inFlavor, void* inRefCon, ItemReference inItemRef,
									DragReference inDragRef )
{
  OSErr retVal = noErr;
  nsDragService* self = NS_STATIC_CAST(nsDragService*, inRefCon);
  NS_ASSERTION ( self, "Refcon not set correctly for DragSendDataProc" );
  if ( self ) {
    void* data = nsnull;
    PRUint32 dataSize = 0;
    retVal = self->GetDataForFlavor ( self->mDataItems, inDragRef, inItemRef, inFlavor, &data, &dataSize );
    if ( retVal == noErr ) {      
        // make the data accessable to the DragManager
        retVal = ::SetDragItemFlavorData ( inDragRef, inItemRef, inFlavor, data, dataSize, 0 );
        NS_ASSERTION ( retVal == noErr, "SDIFD failed in DragSendDataProc" );
    }
    delete [] data;
  } // if valid refcon
  
  return retVal;
  
} // DragSendDataProc


//
// GetDataForFlavor
//
// Given a MacOS flavor and an index for which drag item to lookup, get the information from the
// drag item corresponding to this flavor.
//
OSErr
nsDragService :: GetDataForFlavor ( nsISupportsArray* inDragItems, DragReference inDragRef, unsigned int inItemIndex, 
                                      FlavorType inFlavor, void** outData, unsigned int* outDataSize )
{
  if ( !inDragItems || !inDragRef )
    return paramErr;
    
  OSErr retVal = noErr;
  
    // (assumes that the items were placed into the transferable as nsITranferable*'s, not
    // nsISupports*'s.  Don't forget ElementAt() addRefs for us.)
  nsCOMPtr<nsISupports> genericItem;
  inDragItems->GetElementAt ( inItemIndex, getter_AddRefs(genericItem) );
  nsCOMPtr<nsITransferable> item ( do_QueryInterface(genericItem) );
  if ( item ) {
    nsString mimeFlavor;

    // create a mime mapper to help us out based on data in a special flavor for this item.
    char* mappings = LookupMimeMappingsForItem(inDragRef, inItemIndex) ;
    nsMimeMapperMac theMapper ( mappings );
    theMapper.MapMacOSTypeToMimeType ( inFlavor, mimeFlavor );
    delete [] mappings;
    
    *outDataSize = 0;
    char* mimeStr = mimeFlavor.ToNewCString();
    nsCOMPtr<nsISupports> data;
    if ( NS_SUCCEEDED(item->GetTransferData(mimeStr, getter_AddRefs(data), outDataSize)) )
      CreateDataFromPrimitive ( mimeStr, data, outData, *outDataSize );
    else
      retVal = cantGetFlavorErr;
    delete [] mimeStr;     
  } // if valid item

  return retVal;

} // GetDataForFlavor


//
// LookupMimeMappingsForItem
//
// load up the flavor data for the mime mapper from the drag item and create a
// mime mapper to help us go between mime types and macOS flavors. It may not be
// there (such as when the drag originated from another app), but that's ok.
//
// Caller is responsible for deleting the memory.
//
char*
nsDragService :: LookupMimeMappingsForItem ( DragReference inDragRef, ItemReference itemRef )
{
  char* mapperData = nsnull;
  Size mapperSize = 0;
  OSErr err = ::GetFlavorDataSize ( inDragRef, itemRef, nsMimeMapperMac::MappingFlavor(), &mapperSize );
  if ( !err && mapperSize > 0 ) {
    mapperData = new char[mapperSize + 1];
    if ( !mapperData )
      return nsnull;
	          
    err = ::GetFlavorData ( inDragRef, itemRef, nsMimeMapperMac::MappingFlavor(), mapperData, &mapperSize, 0 );
    if ( err ) {
      #ifdef NS_DEBUG
        printf("nsDragService: Error getting data out of drag manager for mime mapper, #%ld\n", err);
      #endif
      return nsnull;
    }
    else
      mapperData[mapperSize] = '\0';    // null terminate the data
  }

  return mapperData;
}

