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
#include "nsIDataFlavor.h"
#include "nsMimeMapper.h"
#include "nsWidgetsCID.h"
#include "nsClipboard.h"
#include "nsIRegion.h"


//
// DragService constructor
//
nsDragService::nsDragService()
  : mDragRef(0)
{
}


//
// DragService destructor
//
nsDragService::~nsDragService()
{
}


//
// StartDragSession
//
// Do all the work to kick it off.
//
NS_IMETHODIMP
nsDragService :: InvokeDragSession (nsISupportsArray * anArrayTransferables, nsIRegion * aDragRgn, PRUint32 aActionType)

{
  //еее what should we do about |mDragRef| from the previous drag?? Does anyone
  //еее outside of this method need it? Should it really be a member variable?

  OSErr result = ::NewDrag(&mDragRef);
  if ( !result )
    return NS_ERROR_FAILURE;


  //еее add the flavors from the transferables


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

  
  //еее register drag send procs


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

  return NS_OK; 

} // StartDragSession


//
// GetData
//
// Pull data out of the OS drag items and stash it into the given transferable
//
NS_IMETHODIMP
nsDragService :: GetData (nsITransferable * aTransferable)
{
  printf("nsDragService::GetData -- unimplemented on MacOS\n");
  return NS_ERROR_FAILURE;
}


//
// IsDataFlavorSupported
//
// Check the OS to see if the given drag flavor is in the list. Oddly returns
// NS_OK for success and NS_ERROR_FAILURE if flavor is not present.
//
NS_IMETHODIMP
nsDragService :: IsDataFlavorSupported(nsIDataFlavor * aDataFlavor)
{
  nsresult flavorSupported = NS_ERROR_FAILURE;

  // convert to 4 character MacOS type
  nsAutoString mimeType;
  aDataFlavor->GetMimeType(mimeType);
  FlavorType macFlavor = nsMimeMapperMac::MapMimeTypeToMacOSType(mimeType);

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

