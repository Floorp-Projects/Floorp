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
 *   Mats Palmgren <mats.palmgren@bredband.net>
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


//
// Mike Pinkerton
// Netscape Communications
//
// See associated header file for details
//


#include <Gestalt.h>

#include "nsDragService.h"

#include "nsITransferable.h"
#include "nsString.h"
#include "nsMimeMapper.h"
#include "nsClipboard.h"
#include "nsIRegion.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsPrimitiveHelpers.h"
#include "nsLinebreakConverter.h"
#include "nsIMacUtils.h"

#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIFrame.h"
#include "nsIView.h"
#include "nsRect.h"
#include "nsPoint.h"
#include "nsIWidget.h"
#ifndef MOZ_WIDGET_COCOA
#include "nsCarbonHelpers.h"
#endif
// file save stuff
#include "nsNetUtil.h"
#include "nsILocalFileMac.h"

#include "nsIDOMElement.h"
#ifndef MOZ_CAIRO_GFX
#include "nsIImageMac.h"
#endif
#include "nsIImage.h"
#include "nsMacNativeUnicodeConverter.h"
#include "nsICharsetConverterManager.h"
#include "nsStylClipboardUtils.h"

//
// Get the primary frame for a content node in the current document.
//
static nsIFrame*
GetPrimaryFrameFor(nsIContent* aContent)
{
  nsIFrame* result = nsnull;
  nsIDocument* doc = aContent->GetCurrentDoc();
  if (doc) {
    nsIPresShell* presShell = doc->GetShellAt(0);
    if (presShell) {
      result = presShell->GetPrimaryFrameFor(aContent);
    }
  }
  return result;
}

static const PRUint32 kPrivateFlavorMask = 0xffff0000;
static const PRUint32 kPrivateFlavorTag = 'MZ..' & kPrivateFlavorMask;


static void
SetPortToKnownGoodPort()
{
    WindowPtr firstWindow = GetWindowList();
    if (firstWindow)
      ::SetGWorld(::GetWindowPort(firstWindow), ::GetMainDevice());
}


// we need our own stuff for MacOS because of nsIDragSessionMac.
NS_IMPL_ADDREF_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_RELEASE_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_QUERY_INTERFACE3(nsDragService, nsIDragService, nsIDragSession, nsIDragSessionMac)


//
// DragService constructor
//
nsDragService::nsDragService()
  : mDragSendDataUPP(nsnull)
  , mDragRef(0)
  , mDataItems(nsnull)
  , mImageDraggingSupported(PR_FALSE)
{
#if USE_TRANSLUCENT_DRAGS
  // check if the Drag Manager supports image dragging
  // however, it doesn't really work too well, so let's back off (pinkerton)
  long response;
  OSErr err = Gestalt(gestaltDragMgrAttr, &response); 
  if (err == noErr && (response & (1L << gestaltDragMgrHasImageSupport))) {
    mImageDraggingSupported = PR_TRUE;
  }
#endif
  
  mDragSendDataUPP = NewDragSendDataUPP(DragSendDataProc);
}


//
// DragService destructor
//
nsDragService::~nsDragService()
{
  if ( mDragSendDataUPP )
    ::DisposeDragSendDataUPP(mDragSendDataUPP);
}


PRBool
nsDragService::ComputeGlobalRectFromFrame ( nsIDOMNode* aDOMNode, Rect & outScreenRect )
{
  NS_ASSERTION ( aDOMNode, "Oopps, no DOM node" );

  nsCOMPtr<nsIContent> content = do_QueryInterface(aDOMNode);

// this isn't so much of an issue as long as we're just dragging around outlines,
// but it is when we are showing the text being drawn. Comment it out for now
// but leave it around when we turn this all back on (pinkerton).
#if USE_TRANSLUCENT_DRAGGING && defined(MOZ_XUL)
  // until bug 41237 is fixed, only do translucent dragging if the drag is in
  // the chrome or it's a link.
  if (!content || !content->IsNodeOfType(nsINode::eXUL)) {
    // the link node is the parent of the node we have (which is probably text or image).
    nsCOMPtr<nsIDOMNode> parent;
    aDOMNode->GetParentNode ( getter_AddRefs(parent) );
    if ( parent ) {
      nsAutoString localName;
      parent->GetLocalName ( localName );
      if ( ! localName.EqualsLiteral("A") )
        return PR_FALSE;
    }
    else
      return FALSE;
  }
#endif
  
  outScreenRect.left = outScreenRect.right = outScreenRect.top = outScreenRect.bottom = 0;

  if (!content)
    return PR_FALSE;

  // Get the frame for this content node (note: frames are not refcounted)
  nsIFrame *frame = ::GetPrimaryFrameFor(content);
  if (!frame)
    return PR_FALSE;
  
  //
  // Now that we have the frame, we have to convert its coordinates into global screen
  // coordinates.
  //
  
  nsRect rect = frame->GetRect();

  // Find offset from our view
	nsIView *containingView = nsnull;
	nsPoint	viewOffset(0,0);
	frame->GetOffsetFromView(viewOffset, &containingView);
  NS_ASSERTION(containingView, "No containing view!");
  if ( !containingView )
    return PR_FALSE;

  // get the widget associated with the containing view. 
  nsPoint widgetOffset;
  nsIWidget* aWidget = containingView->GetNearestWidget ( &widgetOffset );

  float t2p = frame->GetPresContext()->TwipsToPixels();

  // Shift our offset rect by offset into our view, and
  // the view's offset to the closest widget. Then convert that to global coordinates.
  // Recall that WidgetToScreen() will give us the global coordinates of the rectangle we 
  // give it, but it expects  everything to be in pixels.
  nsRect screenOffset;                                
  screenOffset.MoveBy ( NSTwipsToIntPixels(widgetOffset.x + viewOffset.x, t2p),
                        NSTwipsToIntPixels(widgetOffset.y + viewOffset.y, t2p) );
  aWidget->WidgetToScreen ( screenOffset, screenOffset );

  // stash it all in a mac rect
  outScreenRect.left = screenOffset.x;
  outScreenRect.top = screenOffset.y;
  outScreenRect.right = outScreenRect.left + NSTwipsToIntPixels(rect.width, t2p);
  outScreenRect.bottom = outScreenRect.top + NSTwipsToIntPixels(rect.height, t2p);
            
  return PR_TRUE;
} // ComputeGlobalRectFromFrame


//
// InvokeDragSession
//
// Do all the work to kick it off.
//
NS_IMETHODIMP
nsDragService::InvokeDragSession (nsIDOMNode *aDOMNode, nsISupportsArray * aTransferableArray, nsIScriptableRegion * aDragRgn, PRUint32 aActionType)
{
#ifdef MOZ_WIDGET_COCOA
  SetPortToKnownGoodPort();
  GrafPtr port;
  GDHandle handle;
  ::GetGWorld(&port, &handle);
  if (!IsValidPort(port))
  return NS_ERROR_FAILURE;
#endif

  ::InitCursor();
  nsBaseDragService::InvokeDragSession ( aDOMNode, aTransferableArray, aDragRgn, aActionType );
  
  DragReference theDragRef;
  OSErr result = ::NewDrag(&theDragRef);
  if ( result != noErr )
    return NS_ERROR_FAILURE;
  mDragRef = theDragRef;
#if DEBUG_DD
  printf("**** created drag ref %ld\n", theDragRef);
#endif
  
  Rect frameRect = { 0, 0, 0, 0 };
  RgnHandle theDragRgn = ::NewRgn();
  ::RectRgn(theDragRgn, &frameRect);

  if ( mImageDraggingSupported ) {
    Point	imgOffsetPt;
    imgOffsetPt.v = imgOffsetPt.h = 0;

    PRBool canUseRect = BuildDragRegion ( aDragRgn, aDOMNode, theDragRgn );
    if ( canUseRect ) {
      // passing in null for image's PixMapHandle to SetDragImage() means use bits on screen
      ::SetDragImage (theDragRef, nsnull, theDragRgn, imgOffsetPt, kDragDarkerTranslucency);
    }
  }
  else
    BuildDragRegion ( aDragRgn, aDOMNode, theDragRgn );

  // add the flavors from the transferables. Cache this array for the send data proc
  mDataItems = aTransferableArray;
  RegisterDragItemsAndFlavors(aTransferableArray, theDragRgn);

  // we have to synthesize the native event because we may be called from JavaScript
  // through XPConnect. In that case, we only have a DOM event and no way to
  // get to the native event. As a consequence, we just always fake it.
  EventRecord theEvent;
  theEvent.what = mouseDown;
  theEvent.message = 0L;
  theEvent.when = TickCount();
  theEvent.modifiers = 0L;

  // if we don't have the original mouseDown location, just assume the drag
  // started in the middle of the frame. This prevents us from having the mouse
  // and the region we're dragging separated by a big gap (which could happen if
  // we used the current mouse position). I know this isn't exactly right, and you can
  // see it if you're paying attention, but who pays such close attention?
  if (mScreenX == -1 || mScreenY == -1) {
    Rect dragRect;
    ::GetRegionBounds(theDragRgn, &dragRect);
    theEvent.where.v = dragRect.top + ((dragRect.bottom - dragRect.top) / 2);
    theEvent.where.h = dragRect.left + ((dragRect.right - dragRect.left) / 2);
  }
  else {
    theEvent.where.v = mScreenY;
    theEvent.where.h = mScreenX;
  }

  // register drag send proc which will call us back when asked for the actual
  // flavor data (instead of placing it all into the drag manager)
  ::SetDragSendProc ( theDragRef, mDragSendDataUPP, this );

  // start the drag. Be careful, mDragRef will be invalid AFTER this call (it is
  // reset by the dragTrackingHandler).
  StartDragSession();
  ::TrackDrag ( theDragRef, &theEvent, theDragRgn );
  EndDragSession();
  
  // clean up after ourselves 
  ::DisposeRgn ( theDragRgn );
  result = ::DisposeDrag ( theDragRef );
#if DEBUG_DD
  printf("**** disposing drag ref %ld\n", theDragRef);
#endif
  NS_ASSERTION ( result == noErr, "Error disposing drag" );
  mDragRef = 0L;
  mDataItems = nsnull;

  return NS_OK; 

} // InvokeDragSession


//
// BuildDragRegion
//
// Given the XP region describing the drag rectangles, build up an appropriate drag region. If
// the region we're given is null, try the given DOM node. If that doesn't work fake it as 
// best we can.
//
// Returns PR_TRUE if the region is something that can be used with SetDragImage()
//
PRBool
nsDragService::BuildDragRegion ( nsIScriptableRegion* inRegion, nsIDOMNode* inNode, RgnHandle ioDragRgn )
{
  PRBool retVal = PR_TRUE;
  nsCOMPtr<nsIRegion> geckoRegion;
  if ( inRegion )
    inRegion->GetRegion(getter_AddRefs(geckoRegion));
    
#ifdef MOZ_WIDGET_COCOA
  SetPortToKnownGoodPort();
  GrafPtr port;
  GDHandle handle;
  ::GetGWorld(&port, &handle);
  if (!IsValidPort(port))
  return NS_ERROR_FAILURE;
#endif

  // create the drag region. Pull out the native mac region from the nsIRegion we're
  // given, copy it, inset it one pixel, and subtract them so we're left with just an
  // outline. Too bad we can't do this with gfx api's.
  //
  // At the end, we are left with an outline of the region in global coordinates.
  if ( geckoRegion ) {
    RgnHandle dragRegion = nsnull;
    geckoRegion->GetNativeRegion((void*&)dragRegion);
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
    PRBool useRectFromFrame = PR_FALSE;
    
    // no region provided, let's try to use the dom node to get the frame. Pick a 
    // silly default in case we can't get it.
    Point currMouse;
    ::GetMouse(&currMouse);
    Rect frameRect = { currMouse.v, currMouse.h, currMouse.v + 25, currMouse.h + 100 };
    if ( inNode )
      useRectFromFrame = ComputeGlobalRectFromFrame ( inNode, frameRect );
    else
      NS_WARNING ( "Can't find anything to get a drag rect from. I'm dyin' out here!" );

    if ( ioDragRgn ) {
      RgnHandle frameRgn = ::NewRgn();
      if ( frameRgn ) {
        ::RectRgn ( frameRgn, &frameRect );
        ::CopyRgn ( frameRgn, ioDragRgn );
        ::InsetRgn ( ioDragRgn, 1, 1 );
        ::DiffRgn ( frameRgn, ioDragRgn, ioDragRgn );
        
        ::DisposeRgn ( frameRgn );
      }
    }
    
    // if we couldn't find the exact frame coordinates, then we need to alert people that
    // they shouldn't use this as the basis of SetDragImage()
    retVal = useRectFromFrame;
  }

  return retVal;
  
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
nsDragService::RegisterDragItemsAndFlavors(nsISupportsArray* inArray, RgnHandle inDragRgn)
{
  const FlavorFlags flags = 0;
  
  Rect      dragRgnBounds = {0, 0, 0, 0};
  if (inDragRgn)
    GetRegionBounds(inDragRgn, &dragRgnBounds);

  PRUint32 numDragItems = 0;
  inArray->Count ( &numDragItems ) ;
  for ( PRUint32 itemIndex = 0; itemIndex < numDragItems; ++itemIndex ) {
    nsMimeMapperMac theMapper;
  
    nsCOMPtr<nsISupports> genericItem;
    inArray->GetElementAt ( itemIndex, getter_AddRefs(genericItem) );
    nsCOMPtr<nsITransferable> currItem ( do_QueryInterface(genericItem) );
    if ( currItem ) {   
      nsCOMPtr<nsISupportsArray> flavorList;
      if ( NS_SUCCEEDED(currItem->FlavorsTransferableCanExport(getter_AddRefs(flavorList))) ) {
        PRUint32 numFlavors;
        flavorList->Count ( &numFlavors );
        for ( PRUint32 flavorIndex = 0; flavorIndex < numFlavors; ++flavorIndex ) {
        
          nsCOMPtr<nsISupports> genericWrapper;
          flavorList->GetElementAt ( flavorIndex, getter_AddRefs(genericWrapper) );
          nsCOMPtr<nsISupportsCString> currentFlavor ( do_QueryInterface(genericWrapper) );
  	      if ( currentFlavor ) {
  	        nsCAutoString flavorStr;
  	        currentFlavor->GetData(flavorStr);
  	        FlavorType macOSFlavor = theMapper.MapMimeTypeToMacOSType(flavorStr.get());
              
            if (macOSFlavor == kDragFlavorTypePromiseHFS) {
              // we got kFilePromiseMime
              // kDragFlavorTypePromiseHFS is special. See http://developer.apple.com/technotes/tn/tn1085.html
              PromiseHFSFlavor promiseData;
              promiseData.fileType           = 0;     // let the file extension prevail!
              promiseData.fileCreator        = 0;
              promiseData.fdFlags            = 0;
              promiseData.promisedFlavor     = kDragPromisedFlavor;

              ::AddDragItemFlavor(mDragRef, itemIndex, kDragFlavorTypePromiseHFS, &promiseData, sizeof(promiseData), flavorNotSaved);
              ::AddDragItemFlavor(mDragRef, itemIndex, kDragPromisedFlavor, NULL, 0, flavorNotSaved);
            }
            else
              ::AddDragItemFlavor(mDragRef, itemIndex, macOSFlavor, NULL, 0, flags);
  	        
  	        // If we advertise text/unicode, then make sure we add 'TEXT' to the list
  	        // of flavors supported since we will do the conversion ourselves in GetDataForFlavor()
  	        if ( strcmp(flavorStr.get(), kUnicodeMime) == 0 ) {
  	          theMapper.MapMimeTypeToMacOSType(kTextMime);
  	          ::AddDragItemFlavor ( mDragRef, itemIndex, 'TEXT', NULL, 0, flags );	        
  	          ::AddDragItemFlavor ( mDragRef, itemIndex, 'styl', NULL, 0, flags );	        
  	        }
  	      }
          
        } // foreach flavor in item              
      } // if valid flavor list
    } // if item is a transferable
    
    // put the mime mapping data for this item in a special flavor. Unlike the other data,
    // we have to put the data in now (rather than defer it) or the mappings will go out 
    // of scope by the time they are asked for. Remember that the |mappingLen|
    // includes the null, and we need to maintain that for when we parse it.
    short mappingLen;
    char* mapping = theMapper.ExportMapping(&mappingLen);
    if ( mapping && mappingLen ) {
      ::AddDragItemFlavor ( mDragRef, itemIndex, nsMimeMapperMac::MappingFlavor(), 
                               mapping, mappingLen, flags );
	    nsMemory::Free ( mapping );
    
      ::SetDragItemBounds(mDragRef, itemIndex, &dragRgnBounds);
	  }
    
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
nsDragService::GetData ( nsITransferable * aTransferable, PRUint32 aItemIndex )
{
  nsresult errCode = NS_ERROR_FAILURE;

  // make sure we have a good transferable
  if ( !aTransferable )
    return NS_ERROR_INVALID_ARG;

  // get flavor list that includes all acceptable flavors (including ones obtained through
  // conversion). Flavors are nsISupportsCStrings so that they can be seen from JS.
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
  nsMemory::Free ( mappings );
  
  // Now walk down the list of flavors. When we find one that is actually present,
  // copy out the data into the transferable in that format. SetTransferData()
  // implicitly handles conversions.
  PRUint32 cnt;
  flavorList->Count ( &cnt );
  for (PRUint32 i = 0; i < cnt; ++i) {
    nsCOMPtr<nsISupports> genericWrapper;
    flavorList->GetElementAt ( i, getter_AddRefs(genericWrapper) );
    nsCOMPtr<nsISupportsCString> currentFlavor ( do_QueryInterface(genericWrapper) );
    if ( currentFlavor ) {
      // find MacOS flavor (but don't add it if it's not there)
      nsCAutoString flavorStr;
      currentFlavor->GetData(flavorStr);
      FlavorType macOSFlavor = theMapper.MapMimeTypeToMacOSType(flavorStr.get(), PR_FALSE);
#if DEBUG_DD
      printf("looking for data in type %s, mac flavor %ld\n", flavorStr.get(), macOSFlavor);
#endif

      // check if it is present in the current drag item.
      FlavorFlags unused;
      PRBool dataFound = PR_FALSE;
      void* dataBuff = nsnull;
      PRUint32 dataSize = 0;
      if ( macOSFlavor && ::GetFlavorFlags(mDragRef, itemRef, macOSFlavor, &unused) == noErr ) {	    
        nsresult loadResult = ExtractDataFromOS(mDragRef, itemRef, macOSFlavor, &dataBuff, &dataSize);
  	    if ( NS_SUCCEEDED(loadResult) && dataBuff )
  	      dataFound = PR_TRUE;
      }
      else {
	      // if we are looking for text/unicode and we fail to find it on the clipboard first,
        // try again with text/plain. If that is present, convert it to unicode.
        if ( strcmp(flavorStr.get(), kUnicodeMime) == 0 ) {
          if ( ::GetFlavorFlags(mDragRef, itemRef, 'TEXT', &unused) == noErr ) {	 
            
            // if 'styl' is available, we can get a script of the first run
            // and use it for converting 'TEXT'
            nsresult loadResult = ExtractDataFromOS(mDragRef, itemRef, 'styl', &dataBuff, &dataSize);
            if (NS_SUCCEEDED(loadResult) && 
                dataBuff &&
                (dataSize >= (sizeof(ScrpSTElement) + 2))) {
              StScrpRec *scrpRecP = (StScrpRec *) dataBuff;
              ScrpSTElement *styl = scrpRecP->scrpStyleTab;
              ScriptCode script = styl ? ::FontToScript(styl->scrpFont) : smCurrentScript;
              
              // free 'styl' and get 'TEXT'
              nsMemory::Free(dataBuff);
              loadResult = ExtractDataFromOS(mDragRef, itemRef, 'TEXT', &dataBuff, &dataSize);
              if ( NS_SUCCEEDED(loadResult) && dataBuff ) {
                PRUnichar* convertedText = nsnull;
                PRInt32 convertedTextLen = 0;
                errCode = nsMacNativeUnicodeConverter::ConvertScripttoUnicode(script, 
                                                                              (const char *) dataBuff,
                                                                              dataSize,
                                                                              &convertedText,
                                                                              &convertedTextLen);
                if (NS_SUCCEEDED(errCode) && convertedText) {
                  nsMemory::Free(dataBuff);
                  dataBuff = convertedText;
                  dataSize = convertedTextLen * sizeof(PRUnichar);
                  dataFound = PR_TRUE;
                }
              }
            }          
          
            if (!dataFound) {
              loadResult = ExtractDataFromOS(mDragRef, itemRef, 'TEXT', &dataBuff, &dataSize);
              if ( NS_SUCCEEDED(loadResult) && dataBuff ) {
                const char* castedText = NS_REINTERPRET_CAST(char*, dataBuff);          
                PRUnichar* convertedText = nsnull;
                PRInt32 convertedTextLen = 0;
                nsPrimitiveHelpers::ConvertPlatformPlainTextToUnicode ( castedText, dataSize, 
                                                                          &convertedText, &convertedTextLen );
                if ( convertedText ) {
                  // out with the old, in with the new 
                  nsMemory::Free(dataBuff);
                  dataBuff = convertedText;
                  dataSize = convertedTextLen * 2;
                  dataFound = PR_TRUE;
                }
              } // if plain text data on clipboard
            }
          } // if plain text flavor present
        } // if looking for text/unicode   
      } // else we try one last ditch effort to find our data

	  if ( dataFound ) {
      nsCOMPtr<nsISupports> genericDataWrapper;

	    if ( strcmp(flavorStr.get(), kFileMime) == 0 ) {
	      // we have a HFSFlavor struct in |dataBuff|. Create an nsLocalFileMac object.
	      HFSFlavor* fileData = NS_REINTERPRET_CAST(HFSFlavor*, dataBuff);
	      NS_ASSERTION ( sizeof(HFSFlavor) == dataSize, "Ooops, we really don't have a HFSFlavor" );
	      nsCOMPtr<nsILocalFileMac> file;
	      if ( NS_SUCCEEDED(NS_NewLocalFileWithFSSpec(&fileData->fileSpec, PR_TRUE, getter_AddRefs(file))) )
	        genericDataWrapper = do_QueryInterface(file);
	    }
      else if ((strcmp(flavorStr.get(), kURLDataMime) == 0) || (strcmp(flavorStr.get(), kURLDescriptionMime) == 0)) {
        // need to convert platform data to unicode
        const char* castedText        = NS_REINTERPRET_CAST(char*, dataBuff);          
        PRUnichar*  convertedText     = nsnull;
        PRInt32     convertedTextLen  = 0;
        nsPrimitiveHelpers::ConvertPlatformPlainTextToUnicode(castedText, dataSize, 
                                                                  &convertedText, &convertedTextLen);
        if (convertedText)
        {
          nsMemory::Free(dataBuff);
          dataBuff = convertedText;
          dataSize = convertedTextLen * 2;
          nsPrimitiveHelpers::CreatePrimitiveForData(flavorStr.get(), (void *)dataBuff, dataSize, getter_AddRefs(genericDataWrapper));
        }
      }
	    else {
          // we probably have some form of text. The DOM only wants LF, so convert k
          // from MacOS line endings to DOM line endings.
          nsLinebreakHelpers::ConvertPlatformToDOMLinebreaks(flavorStr.get(), &dataBuff, NS_REINTERPRET_CAST(int*, &dataSize));

          unsigned char *dataPtr = (unsigned char *) dataBuff;
          // skip BOM (Byte Order Mark to distinguish little or big endian) in 'utxt'
          // 10.2 puts BOM for 'utxt', we need to remove it here
          // for little endian case, we also need to convert the data to big endian
          // but we do not do that currently (need this in case 'utxt' is really in little endian)
          if ( (macOSFlavor == 'utxt') &&
               (dataSize > 2) &&
               ((dataPtr[0] == 0xFE && dataPtr[1] == 0xFF) ||
               (dataPtr[0] == 0xFF && dataPtr[1] == 0xFE)) ) {
            dataSize -= sizeof(PRUnichar);
            dataPtr += sizeof(PRUnichar);
          }
          nsPrimitiveHelpers::CreatePrimitiveForData(flavorStr.get(), (void *) dataPtr, dataSize, getter_AddRefs(genericDataWrapper));
        }
        
        // put it into the transferable.
        errCode = aTransferable->SetTransferData(flavorStr.get(), genericDataWrapper, dataSize);
#ifdef NS_DEBUG
         if ( errCode != NS_OK ) printf("nsDragService:: Error setting data into transferable\n");
#endif
        nsMemory::Free ( dataBuff );
        errCode = NS_OK;

        // we found one, get out of this loop!
        break;
      } 
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
// Handle the case where we ask for unicode and it's not there, but plain text is. We 
// say "yes" in that case, knowing that we will have to perform a conversion when we actually
// pull the data out of the drag.
//
// ¥¥¥ this is obviously useless with more than one drag item. Need to specify 
// ¥¥¥Êand index to this API
//
NS_IMETHODIMP
nsDragService::IsDataFlavorSupported(const char *aDataFlavor, PRBool *_retval)
{
  if ( !_retval )
    return NS_ERROR_INVALID_ARG;

#ifdef NS_DEBUG
      if ( strcmp(aDataFlavor, kTextMime) == 0 )
        NS_WARNING ( "DO NOT USE THE text/plain DATA FLAVOR ANY MORE. USE text/unicode INSTEAD" );
#endif

  *_retval = PR_FALSE;

  // search through all drag items looking for something with this flavor. Recall
  // that drag item indices are 1-based.
  unsigned short numDragItems = 0;
  ::CountDragItems ( mDragRef, &numDragItems );
  for ( int i = 1; i <= numDragItems; ++i ) {
    ItemReference currItem;
    OSErr res = ::GetDragItemReferenceNumber ( mDragRef, i, &currItem );
    if ( res != noErr )
      return NS_ERROR_FAILURE;

    // convert to 4 character MacOS type for this item
    char* mappings = LookupMimeMappingsForItem(mDragRef, currItem) ;
    nsMimeMapperMac theMapper ( mappings );
    FlavorType macFlavor = theMapper.MapMimeTypeToMacOSType(aDataFlavor, PR_FALSE);
    nsMemory::Free(mappings);

    FlavorFlags ignored;
    if ( ::GetFlavorFlags(mDragRef, currItem, macFlavor, &ignored) == noErr )
      *_retval = PR_TRUE;
    else {
      // if the client asked for unicode and it wasn't present, check if we have TEXT.
      // We'll handle the actual data substitution in GetDataForFlavor().
      if ( strcmp(aDataFlavor, kUnicodeMime) == 0 ) {
        if ( ::GetFlavorFlags(mDragRef, currItem, 'TEXT', &ignored) == noErr )
          *_retval = PR_TRUE;
      }          
    }
  } // for each item in drag

  return NS_OK;

} // IsDataFlavorSupported


//
// GetNumDropItems
//
// Returns the number of drop items present in the current drag.
//
NS_IMETHODIMP
nsDragService::GetNumDropItems ( PRUint32 * aNumItems )
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
nsDragService::SetDragReference ( DragReference aDragRef )
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
nsDragService::DragSendDataProc(FlavorType inFlavor, void* inRefCon, ItemReference inItemRef,
									DragReference inDragRef)
{
  OSErr retVal = noErr;
  nsDragService* dragService = NS_STATIC_CAST(nsDragService*, inRefCon);
  NS_ASSERTION ( dragService, "Refcon not set correctly for DragSendDataProc" );
  if (dragService) {
    void* data = nsnull;
    PRUint32 dataSize = 0;
    retVal = dragService->GetDataForFlavor(dragService->mDataItems, inDragRef, inItemRef, inFlavor, &data, &dataSize);
    if ( retVal == noErr ) {      
        if ((inFlavor & kPrivateFlavorMask) == kPrivateFlavorTag) {
          // Byte-swap private flavors if running translated
          nsCOMPtr<nsIMacUtils> macUtils =
           do_GetService("@mozilla.org/xpcom/mac-utils;1");
          PRBool isTranslated;
          if (macUtils &&
              NS_SUCCEEDED(macUtils->GetIsTranslated(&isTranslated)) &&
              isTranslated) {
            char* swappedData = (char*) nsMemory::Alloc(dataSize);
            if (!swappedData) {
              nsMemory::Free(data);
              return notEnoughMemoryErr;
            }
            else {
              swab(data, swappedData, dataSize);
              nsMemory::Free(data);
              data = swappedData;
            }
          }
        }
        // make the data accessible to the DragManager
        retVal = ::SetDragItemFlavorData ( inDragRef, inItemRef, inFlavor, data, dataSize, 0 );
        NS_ASSERTION ( retVal == noErr, "SDIFD failed in DragSendDataProc" );
    }
    if ( data )
      nsMemory::Free ( data );
  } // if valid refcon
  
  return retVal;
  
} // DragSendDataProc


//
// GetDataForFlavor
//
// Given a MacOS flavor and an index for which drag item to lookup, get the information from the
// drag item corresponding to this flavor.
// 
// This routine also handles the conversions between text/plain and text/unicode to take platform
// charset encodings into account. If someone asks for text/plain, we convert the unicode (we assume
// it is present because that's how we knew to advertise text/plain in the first place) and give it
// to them. If someone asks for text/unicode, and it isn't there, we need to convert text/plain and
// hand them back the unicode. Again, we can assume that text/plain is there because otherwise we
// wouldn't have been allowed to look for unicode.
//
OSErr
nsDragService::GetDataForFlavor(nsISupportsArray* inDragItems, DragReference inDragRef, unsigned int inItemIndex, 
                                      FlavorType inFlavor, void** outData, unsigned int* outDataSize)
{
  if ( !inDragItems || !inDragRef )
    return paramErr;
    
  *outData = nsnull;
  *outDataSize = 0;

  OSErr retVal = noErr;
  nsresult rv;
  
  // (assumes that the items were placed into the transferable as nsITranferable*'s, not nsISupports*'s.)
  nsCOMPtr<nsISupports> genericItem;
  inDragItems->GetElementAt(inItemIndex, getter_AddRefs(genericItem));
  nsCOMPtr<nsITransferable> item(do_QueryInterface(genericItem));
  if (!item) return cantGetFlavorErr;
    
    // create a mime mapper to help us out based on data in a special flavor for this item.
  char* mappings = LookupMimeMappingsForItem(inDragRef, inItemIndex);
  nsMimeMapperMac theMapper(mappings);

  nsCAutoString mimeFlavor;
  theMapper.MapMacOSTypeToMimeType(inFlavor, mimeFlavor);
  nsMemory::Free(mappings);
  
  // if someone was asking for text/plain, lookup unicode instead so we can convert it.
  PRBool needToDoConversionToPlainText = PR_FALSE;
  const char* actualFlavor = mimeFlavor.get();
  if ( strcmp(mimeFlavor.get(), kTextMime) == 0 || inFlavor == 'styl' )
  {
    actualFlavor = kUnicodeMime;
    needToDoConversionToPlainText = PR_TRUE;
  }
  else if (strcmp(actualFlavor, kURLDataMime) == 0)
  {
    needToDoConversionToPlainText = PR_TRUE;
  }
  else if (strcmp(actualFlavor, kURLDescriptionMime) == 0)
  {
    needToDoConversionToPlainText = PR_TRUE;
  }
  else if (strcmp(actualFlavor, kFilePromiseMime) == 0)
  {
    /*  Here's how file dragging works:
    
        Drag code adds a kFilePromiseMime flavor to the transferable, with an
        nsIFlavorDataProvider.
        It also adds a kFilePromiseURLMime flavor, with a string containing the source
        url.
    
        We map that to the OS drag flavor kDragFlavorTypePromiseHFS, which promises
        the flavor kDragPromisedFlavor. The OS asks for kDragPromisedFlavor, and we
        map that back to kFilePromiseMime.
        
        When asked for kFilePromiseMime data (here), we figure out the drop location from
        the OS, and set that as an nsILocalFile on the kFilePromiseDirectoryMime flavor. We then
        call GetTransferData() for the kFilePromiseMime flavor, which triggers
        the nsIFlavorDataProvider to do the save.
    */

    nsCOMPtr<nsILocalFile> dropDirectory;
    OSErr err = GetHFSPromiseDropDirectory(inDragRef, inItemIndex, inFlavor, getter_AddRefs(dropDirectory));
    if (err != noErr) return err;
    
    // now add a flavor to the transferable
    nsCOMPtr<nsISupports> localFileISupports = do_QueryInterface(dropDirectory);
    item->SetTransferData(kFilePromiseDirectoryMime, localFileISupports, sizeof(nsILocalFile*));
    
    // now request the kFilePromiseMime data, which will invoke the data provider
    // If successful, the returned data is a reference to the resulting file.
    nsCOMPtr<nsISupports> fileDataPrimitive;
    PRUint32 dataSize = 0;
    rv = item->GetTransferData(kFilePromiseMime, getter_AddRefs(fileDataPrimitive), &dataSize);
    if (NS_FAILED(rv)) return cantGetFlavorErr;
    
    // if we successfully saved, tell the Drag Manager
    nsCOMPtr<nsILocalFile> resultFile = do_QueryInterface(fileDataPrimitive);
    if (resultFile)
      return SetDropFileInDrag(inDragRef, inItemIndex, inFlavor, resultFile);
    
    // something went wrong.
    return cantGetFlavorErr;
  }
  else if (strcmp(actualFlavor, kNativeImageMime) == 0)
  {
    PRUint32 dataSize = 0;
    nsCOMPtr<nsISupports> transferSupports;
    rv = item->GetTransferData(actualFlavor, getter_AddRefs(transferSupports), &dataSize);
    if (NS_FAILED(rv)) return cantGetFlavorErr;

    nsCOMPtr<nsISupportsInterfacePointer> ptrPrimitive(do_QueryInterface(transferSupports));
    if (!ptrPrimitive) return cantGetFlavorErr; 
    
    nsCOMPtr<nsISupports> primitiveData;
    ptrPrimitive->GetData(getter_AddRefs(primitiveData));
#ifndef MOZ_CAIRO_GFX
    nsCOMPtr<nsIImageMac> image = do_QueryInterface(primitiveData);
    if (!image) return cantGetFlavorErr; 
      
    PicHandle picture = nsnull;
    image->ConvertToPICT(&picture);
    if (!picture) return cantGetFlavorErr; 

    PRInt32 pictSize = ::GetHandleSize((Handle)picture);
    char* pictData = nsnull;
    if (pictSize > 0)
      pictData = (char*)nsMemory::Alloc(pictSize);
    if (pictData) {
      ::BlockMoveData(*picture, pictData, pictSize);    // doesn't move memory
      *outData = (void*)pictData;
      *outDataSize = pictSize;
      retVal = noErr;
    }
    else
      retVal = cantGetFlavorErr;

    ::KillPicture(picture);
#else
    retVal = cantGetFlavorErr;
#endif
    return retVal;
  }
    
  nsCOMPtr<nsISupports> data;
  if (NS_SUCCEEDED(item->GetTransferData(actualFlavor, getter_AddRefs(data), outDataSize)))
  {
    nsPrimitiveHelpers::CreateDataFromPrimitive ( actualFlavor, data, outData, *outDataSize );
        
    // Convert unix to mac linebreaks, since mac linebreaks are required for clipboard compatibility.
    // I'm making the assumption here that the substitution will be entirely in-place, since both
    // types of line breaks are 1-byte.
    PRUnichar* castedUnicode = NS_REINTERPRET_CAST(PRUnichar*, *outData);
    nsLinebreakConverter::ConvertUnicharLineBreaksInSitu(&castedUnicode,
                                                         nsLinebreakConverter::eLinebreakUnix,
                                                         nsLinebreakConverter::eLinebreakMac,
                                                         *outDataSize / sizeof(PRUnichar), nsnull);

    // if required, do the extra work to convert unicode to plain text and replace the output
    // values with the plain text.
    if ( needToDoConversionToPlainText ) {
      char* plainTextData = nsnull;
      PRInt32 plainTextLen = 0;
      nsresult rv =
      nsPrimitiveHelpers::ConvertUnicodeToPlatformPlainText ( castedUnicode, *outDataSize / 2, &plainTextData, &plainTextLen );

      ScriptCodeRun *scriptCodeRuns = nsnull;
      PRInt32 scriptRunOutLen;

      // if characters are not mapped from Unicode then try native API to convert to 
      // available script
      if (rv == NS_ERROR_UENC_NOMAPPING) {
        if (plainTextData) {
          nsMemory::Free(plainTextData);
          plainTextData = nsnull;
        }
        rv = nsMacNativeUnicodeConverter::ConvertUnicodetoScript(castedUnicode, 
                                                                 *outDataSize / sizeof(PRUnichar),
                                                                 &plainTextData, 
                                                                 &plainTextLen,
                                                                 &scriptCodeRuns,
                                                                 &scriptRunOutLen);
      }
      else if (NS_SUCCEEDED(rv)) {
        // create a single run with the default system script
        scriptCodeRuns = NS_REINTERPRET_CAST(ScriptCodeRun*,
                                             nsMemory::Alloc(sizeof(ScriptCodeRun)));
        if (scriptCodeRuns) {
          scriptCodeRuns[0].offset = 0;
          scriptCodeRuns[0].script = (ScriptCode) ::GetScriptManagerVariable(smSysScript);
          scriptRunOutLen = 1;
        }
      }
      
      if ( plainTextData && *outData ) {
        nsMemory::Free(*outData);
        *outData = nsnull;
        *outDataSize = 0;
        
        if (inFlavor != 'styl') {
          *outData = plainTextData;
          *outDataSize = plainTextLen;
        }
        else {
          nsMemory::Free(plainTextData);  // discard 'TEXT'
          
          char *stylData;
          PRInt32 stylLen;
          // create 'styl' from the script runs
          if (scriptCodeRuns) {
            rv = CreateStylFromScriptRuns(scriptCodeRuns,
                                               scriptRunOutLen,
                                               &stylData,
                                               &stylLen);
            if (NS_SUCCEEDED(rv)) {
              *outData = stylData;
              *outDataSize = stylLen;
            }
          }
        }
      }
      else
        retVal = cantGetFlavorErr;
      if (scriptCodeRuns)
        nsMemory::Free(scriptCodeRuns);
    }
  }

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
nsDragService::LookupMimeMappingsForItem ( DragReference inDragRef, ItemReference inItemRef )
{
  char* mapperData = nsnull;
  PRUint32 mapperSize = 0;
  ExtractDataFromOS(inDragRef, inItemRef, nsMimeMapperMac::MappingFlavor(),  (void**)&mapperData, &mapperSize);

  return mapperData;
  
#if 0 
  OSErr err = ::GetFlavorDataSize ( inDragRef, itemRef, nsMimeMapperMac::MappingFlavor(), &mapperSize );
  if ( !err && mapperSize > 0 ) {
    mapperData = NS_REINTERPRET_CAST(char*, nsMemory::Alloc(mapperSize + 1));
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
#endif
}


//
// ExtractDataFromOS
//
// Handles pulling the data from the DragManager. Will return NS_ERROR_FAILURE if it can't get
// the data for whatever reason.
//
nsresult
nsDragService::ExtractDataFromOS ( DragReference inDragRef, ItemReference inItemRef, ResType inFlavor, 
                                       void** outBuffer, PRUint32* outBuffSize )
{
  if ( !outBuffer || !outBuffSize || !inFlavor )
    return NS_ERROR_FAILURE;

  nsresult retval = NS_OK;
  char* buff = nsnull;
  Size buffSize = 0;
  OSErr err = ::GetFlavorDataSize ( inDragRef, inItemRef, inFlavor, &buffSize );
  if ( !err && buffSize > 0 ) {
    buff = NS_REINTERPRET_CAST(char*, nsMemory::Alloc(buffSize + 1));
    if ( buff ) {	     
      err = ::GetFlavorData ( inDragRef, inItemRef, inFlavor, buff, &buffSize, 0 );
      if (err == noErr) {
        if ((inFlavor & kPrivateFlavorMask) == kPrivateFlavorTag) {
          // Byte-swap private flavors if running translated
          nsCOMPtr<nsIMacUtils> macUtils =
           do_GetService("@mozilla.org/xpcom/mac-utils;1");
          PRBool isTranslated;
          if (macUtils &&
              NS_SUCCEEDED(macUtils->GetIsTranslated(&isTranslated)) &&
              isTranslated) {
            char* swappedData = (char*) nsMemory::Alloc(buffSize);
            if (!swappedData)
              retval = NS_ERROR_OUT_OF_MEMORY;
            else {
              swab(buff, swappedData, buffSize);
              nsMemory::Free(buff);
              buff = swappedData;
            }
          }
        }
      }
      else {
        #ifdef NS_DEBUG
          printf("nsDragService: Error getting data out of drag manager, #%d\n", err);
        #endif
        retval = NS_ERROR_FAILURE;
      }
    }
    else
      retval = NS_ERROR_FAILURE;
  }

  if ( NS_FAILED(retval) ) {
    if ( buff )
      nsMemory::Free(buff);
  }
  else {
    *outBuffer = buff;
    *outBuffSize = buffSize;
  }
  return retval;

} // ExtractDataFromOS


//
// SetDragAction
//
// Override to set the cursor to the appropriate feedback when the
// drag action changes based on modifier keys
//
NS_IMETHODIMP
nsDragService::SetDragAction ( PRUint32 anAction ) 
{
  const PRInt32 kCopyCursorID = 144;
  const PRInt32 kLinkCursorID = 145;
  
  // don't do the work if it's the same.
  if ( anAction == mDragAction )
    return NS_OK;
    
  CursHandle newCursor = nsnull;  
  if ( anAction == DRAGDROP_ACTION_COPY )
    newCursor = ::GetCursor ( kCopyCursorID );
  else if ( anAction == DRAGDROP_ACTION_LINK )
    newCursor = ::GetCursor ( kLinkCursorID );
  if ( newCursor ) {
    ::HLock((Handle)newCursor);
    ::SetCursor ( *newCursor );
    ::HUnlock((Handle)newCursor);
  }
  else
    ::InitCursor();
 
  return nsBaseDragService::SetDragAction(anAction);
}

#pragma mark -
// Utility routines for dragging files to the Finder

static OSErr GetDropDirectory(DragReference dragRef, FSSpecPtr fssOut)
{
  OSErr err;

  AEDesc dropLocAlias = { typeNull, nil };
  err = ::GetDropLocation(dragRef, &dropLocAlias);
  if (err != noErr) return err;
  
  if (dropLocAlias.descriptorType != typeAlias)
    return paramErr;

  AEDesc dropLocFSS = { typeNull, nil };
  if ((err = ::AECoerceDesc(&dropLocAlias, typeFSS, &dropLocFSS)) == noErr)
  {
    err = ::AEGetDescData(&dropLocFSS, fssOut, sizeof(FSSpec));
    (void)::AEDisposeDesc(&dropLocFSS);
  }

  if (dropLocAlias.dataHandle)
    (void)::AEDisposeDesc(&dropLocAlias);

  return err;
}

OSErr nsDragService::GetHFSPromiseDropDirectory(DragReference inDragRef, unsigned int inItemIndex,
                                FlavorType inFlavor, nsILocalFile** outDir)
{
  // get the promise data
  PromiseHFSFlavor promiseData;
  Size dataSize = sizeof(promiseData);
  OSErr err = ::GetFlavorData(inDragRef, inItemIndex, kDragFlavorTypePromiseHFS, &promiseData, &dataSize, 0);
  if (err != noErr) return err;
  
  FSSpec dropLocation;
  err = GetDropDirectory(inDragRef, &dropLocation);
  if (err != noErr) return err;
  
  nsCOMPtr<nsILocalFileMac> dropFolderSpec;
  nsresult rv = NS_NewLocalFileWithFSSpec(&dropLocation, PR_FALSE, getter_AddRefs(dropFolderSpec));
  if (NS_FAILED(rv)) return fnfErr;

  CallQueryInterface(dropFolderSpec, outDir);
  return noErr;
}

OSErr
nsDragService::SetDropFileInDrag(DragReference inDragRef, unsigned int inItemIndex,
                                  FlavorType inFlavor, nsILocalFile* inFile)
{
  nsCOMPtr<nsILocalFileMac> targetFileMac = do_QueryInterface(inFile);
  if (!targetFileMac) return paramErr;
  
  FSSpec targetFileSpec;
  nsresult rv = targetFileMac->GetFSSpec(&targetFileSpec);
  if (NS_FAILED(rv)) return paramErr;
  
  return ::SetDragItemFlavorData(inDragRef, inItemIndex, inFlavor, &targetFileSpec, sizeof(FSSpec), 0);
}

