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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Paul Ashford
 *   Fredrik Holmqvist <thesuckiestemail@yahoo.se>
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

#include <stdio.h>
#include "nsDragService.h"
#include "nsIDocument.h"
#include "nsIRegion.h"
#include "nsITransferable.h"
#include "nsIServiceManager.h"
#include "nsISupportsPrimitives.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "nsPrimitiveHelpers.h"
#include "nsUnitConversion.h"
#include "nsWidgetsCID.h"
#include "nsCRT.h"

// if we want to do Image-dragging, also need to change Makefile.in
// to add
// 		-I$(topsrcdir)/gfx/src/beos \
// in INCLUDES
// and bug 294234 to be done.
// #include "nsIImage.h"
// #include "nsIImageBeOS.h"
//#include <Bitmap.h>

#include <AppDefs.h>
#include <TypeConstants.h>
#include <DataIO.h>
#include <Mime.h>
#include <Rect.h>
#include <Region.h>
#include <String.h>
#include <View.h>

#include "prlog.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIFrame.h"
#include "nsIView.h"
#include "nsIWidget.h"
  
static NS_DEFINE_CID(kCDragServiceCID,   NS_DRAGSERVICE_CID);
  
static PRLogModuleInfo *sDragLm = NULL;

static nsIFrame*
GetPrimaryFrameFor(nsIDOMNode *aDOMNode)
{
    nsCOMPtr<nsIContent> aContent = do_QueryInterface(aDOMNode);
    if (nsnull == aContent)
        return nsnull;

    nsIDocument* doc = aContent->GetCurrentDoc();
    if (nsnull == doc)
        return nsnull;
    nsIPresShell* presShell = doc->GetShellAt(0);
    if ( nsnull == presShell) 
        return nsnull;
    return presShell->GetPrimaryFrameFor(aContent);
}

static bool 
IsInternalDrag(BMessage * aMsg)
{
    BString orig;
    // We started this drag if originater is 'BeZilla'
    return (nsnull != aMsg && B_OK == aMsg->FindString("be:originator", &orig) &&
	    0 == orig.Compare("BeZilla"));
}

NS_IMPL_THREADSAFE_ISUPPORTS3(nsDragService, nsIDragService, nsIDragSession, nsIDragSessionBeOS)
//NS_IMPL_THREADSAFE_ISUPPORTS1(nsBaseDragService, nsIDragSessionBeOS)

//-------------------------------------------------------------------------
//
// DragService constructor
// Enable logging: 'export NSPR_LOG_MODULES=nsDragService:5'
//-------------------------------------------------------------------------
nsDragService::nsDragService()
{
    // set up our logging module
    if (!sDragLm)
        sDragLm = PR_NewLogModule("nsDragService");
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("\n\nnsDragService::nsDragService"));

    mDragMessage = NULL;
    mCanDrop = PR_FALSE;
}

//-------------------------------------------------------------------------
//
// DragService destructor
//
//-------------------------------------------------------------------------
nsDragService::~nsDragService()
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::~nsDragService"));
    ResetDragInfo();
}

//-------------------------------------------------------------------------
//
// nsIDragService : InvokeDragSession
//
// Called when a drag is being initiated from within mozilla 
// The code here gets the BView, the dragRect, builds the DragMessage and 
// starts the drag.
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsDragService::InvokeDragSession (nsIDOMNode *aDOMNode,
                                  nsISupportsArray * aArrayTransferables,
                                  nsIScriptableRegion * aRegion,
                                  PRUint32 aActionType)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::InvokeDragSession"));
    nsBaseDragService::InvokeDragSession (aDOMNode, aArrayTransferables,
                                         aRegion, aActionType);
    ResetDragInfo();       
    // make sure that we have an array of transferables to use
    if (nsnull == aArrayTransferables)
        return NS_ERROR_INVALID_ARG;

    // set our reference to the transferables.  this will also addref
    // the transferables since we're going to hang onto this beyond the
    // length of this call
    mSourceDataItems = aArrayTransferables;

    // Get a box or a bitmap to drag
    bool haveRect = false;
    BRect dragRect;
	
    if (nsnull != aRegion)
    {
        PRInt32 aX, aY, aWidth, aHeight;
        // TODO. Region may represent multiple rects - when dragging multiple items.
        aRegion->GetBoundingBox(&aX, &aY, &aWidth, &aHeight);
        dragRect.Set( aX, aY, aX + aWidth, aY + aHeight);
        haveRect = true;
        // does this need to be offset?
    } 
    
    // Get the frame for this content node (note: frames are not refcounted)
    nsIFrame *aFrame = GetPrimaryFrameFor(aDOMNode);
    if (nsnull == aFrame)
        return PR_FALSE;
    
    // Now that we have the frame, we have to convert its coordinates into global screen
    // coordinates.
    nsRect aRect = aFrame->GetRect();

    // Find offset from our view
    nsIView *containingView = nsnull;
    nsPoint viewOffset(0,0);
    aFrame->GetOffsetFromView(viewOffset, &containingView);
    NS_ASSERTION(containingView, "No containing view!");
    if (nsnull == containingView)
        return PR_FALSE;

    // get the widget associated with the containing view. 
    nsPoint aWidgetOffset;
    nsCOMPtr<nsIWidget> aWidget = containingView->GetNearestWidget(&aWidgetOffset);
    if (nsnull == aWidget)
        return PR_FALSE;

    BView *view = (BView *) aWidget->GetNativeData(NS_NATIVE_WIDGET);
    // Don't have the rect yet, try to get it from the dom node
    if (nsnull==haveRect)
    {
        float t2p =  aFrame->GetPresContext()->TwipsToPixels(); 
        // GetOffsetFromWidget() actually returns the _parent's_ offset from its widget, so we
        // still have to add in the offset to |containingView|'s parent ourselves.
        nsPoint aViewPos = containingView->GetPosition();
    
        // Shift our offset rect by offset into our view, the view's offset to its parent, and
        // the parent's offset to the closest widget. Then convert that to global coordinates. 
        // Recall that WidgetToScreen() will give us the global coordinates of the rectangle we 
        // give it, but it expects  everything to be in pixels.
        nsRect screenOffset;
        screenOffset.MoveBy ( NSTwipsToIntPixels(aWidgetOffset.x + aViewPos.x + viewOffset.x, t2p),
                            NSTwipsToIntPixels(aWidgetOffset.y + aViewPos.y + viewOffset.y, t2p));
        aWidget->WidgetToScreen ( screenOffset, screenOffset );

        dragRect.Set(screenOffset.x, screenOffset.y, 
		             screenOffset.x + NSTwipsToIntPixels(aRect.width, t2p),
		             screenOffset.y + NSTwipsToIntPixels(aRect.height, t2p));
        haveRect = true;
    }

    mDragAction = aActionType;
    mDragMessage = CreateDragMessage();

    if (!view || !mDragMessage)
        return PR_FALSE;
        
    // Set the original click location, how to get this or is it needed ?
    // sourceMessage->AddPoint("click_location", mPoint);
    
    if (!view->LockLooper())
        return PR_FALSE;
        
    // Well, let's just use the view frame, maybe?
    if (!haveRect) 
    {
        dragRect = view->Frame();
        // do we need to offset?
    }
        
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("invoking mDragView->DragMessage"));
    bool noBitmap = true;

//This is the code for image-dragging, currently disabled. See comments in beginning of file.
# ifdef 0
    do
    {
        PRUint32 dataSize;
        PRUint32 noItems;
        mSourceDataItems->Count(&noItems);
        if (noItems!=1) 
        {
            PR_LOG(sDragLm, PR_LOG_DEBUG, ("Transferables are not ==1, no drag bitmap!"));
            break;
        }
        
        nsCOMPtr<nsISupports> genericItem;
        aArrayTransferables->GetElementAt(0, getter_AddRefs(genericItem));
        nsCOMPtr<nsITransferable> aTransferable (do_QueryInterface(genericItem));
        
        nsCOMPtr<nsISupports> genericDataWrapper;
        nsresult rv = aTransferable->GetTransferData(kNativeImageMime, getter_AddRefs(genericDataWrapper), &dataSize);
        if (NS_FAILED(rv))
        {
            PR_LOG(sDragLm, PR_LOG_DEBUG, ("Could not get nativeimage, no drag bitmap!"));
            break;
        }

        nsCOMPtr<nsISupportsInterfacePointer> ptrPrimitive (do_QueryInterface(genericDataWrapper));
        if (ptrPrimitive == NULL) 
        {
            PR_LOG(sDragLm, PR_LOG_DEBUG, ("Could not get ptrPrimitive, no drag bitmap!"));
            break;
        }

        nsCOMPtr<nsISupports> genericData;
        ptrPrimitive->GetData(getter_AddRefs(genericData));
        if (genericData == NULL) 
        {
            PR_LOG(sDragLm, PR_LOG_DEBUG, ("Could not get data, no drag bitmap!"));
            break;
        }

        //dependent on bug 294234 and how it's implemented. This code was for attachment 183634.
        nsCOMPtr<nsIImageBeOS> image (do_QueryInterface(genericData));
        if (image == NULL)
        {
            PR_LOG(sDragLm, PR_LOG_DEBUG, ("Could not get nsImage, no drag bitmap!"));
            break;
        }

        BBitmap *aBitmap;
        image->GetBitmap(&aBitmap);
        if (aBitmap==NULL || !aBitmap->IsValid())
        {
            PR_LOG(sDragLm, PR_LOG_DEBUG, ("Could not get BBitmap, no drag bitmap %s!", aBitmap==NULL?"(null)":"(not valid)" ));
            break;        
        }

        view->DragMessage(mDragMessage, aBitmap, B_OP_OVER, BPoint(-4,-4), view); 
        noBitmap = false;
    } while(false);
# endif    
    
    if (noBitmap) 
        view->DragMessage(mDragMessage, dragRect, view);
    
    StartDragSession();
    view->UnlockLooper();
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsIDragService : StartDragSession
//
// We overwrite this so we can log it
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsDragService::StartDragSession()
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::StartDragSession()"));
    return nsBaseDragService::StartDragSession();
}

//-------------------------------------------------------------------------
//
// nsIDragService : EndDragSession
//
// We overwrite this so we can log it
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsDragService::EndDragSession(PRBool aDoneDrag)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::EndDragSession()"));
    //Don't reset drag info, keep it until there is a new drag, in case a negotiated drag'n'drop wants the info.
    //We need the draginfo as we are ending starting the dragsession
    //on entering/exiting different views (nsWindows) now.
    //That way the dragsession is always ended when we go outside mozilla windows, but we do throw away the 
    // mSourceDocument and mSourceNode. We do hold on to the nsTransferable if it was a internal drag. 
    //ResetDragInfo();
    return nsBaseDragService::EndDragSession(aDoneDrag);
}

//-------------------------------------------------------------------------
//
// nsIDragSession : SetCanDrop
//
// We overwrite this so we can log it
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsDragService::SetCanDrop(PRBool aCanDrop)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::SetCanDrop(%s)",
                                  aCanDrop == PR_TRUE?"TRUE":"FALSE"));
    return nsBaseDragService::SetCanDrop(aCanDrop);
}


//-------------------------------------------------------------------------
//
// nsIDragSession : GetCanDrop
//
// We overwrite this so we can log it
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsDragService::GetCanDrop(PRBool *aCanDrop)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::GetCanDrop()"));
    return nsBaseDragService::GetCanDrop(aCanDrop);
}

//-------------------------------------------------------------------------
//
// nsIDragSession : GetNumDropItems
//
// Gets the number of items currently being dragged
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsDragService::GetNumDropItems(PRUint32 * aNumItems)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::GetNumDropItems()"));
    if (nsnull == mDragMessage)
    {
        *aNumItems = 0;
        PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::GetNumDropItems(): WARNING! No dragmessage"));
        return NS_OK;
    } 
    // Did we start this drag?
    if (IsInternalDrag(mDragMessage))
        mSourceDataItems->Count(aNumItems);
    else
        // The only thing native that I can think of that may have multiple items
        // would be file references, DND-docs don't say anything about multiple items.
        *aNumItems = 1;


    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::GetNumDropItems():%d", *aNumItems));
    return NS_OK;
}


//-------------------------------------------------------------------------
//
// nsIDragSession : GetData
//
// Copies the data at the given index into the given nsITransferable
//
// This is usually called on Drop, but can be called before that
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsDragService::GetData(nsITransferable * aTransferable, PRUint32 aItemIndex)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::GetData %d", aItemIndex));

    if (nsnull==mDragMessage)
        return NS_ERROR_INVALID_ARG;

    // get flavor list that includes all acceptable flavors (including
    // ones obtained through conversion). Flavors are nsISupportsStrings
    // so that they can be seen from JS.
    nsresult rv = NS_ERROR_FAILURE;
    nsCOMPtr<nsISupportsArray> flavorList;
    rv = aTransferable->FlavorsTransferableCanImport(getter_AddRefs(flavorList));
    if (NS_FAILED(rv))
        return rv;

    // count the number of flavors
    PRUint32 cnt;
    flavorList->Count (&cnt);

    nsCOMPtr<nsISupports> genericWrapper;
    nsCOMPtr<nsISupportsCString> currentFlavor;
    nsXPIDLCString flavorStr;
    nsCOMPtr<nsISupports> genericItem;   
    nsCOMPtr<nsISupports> data;
    PRUint32 tmpDataLen = 0;
    for (unsigned int i= 0; i < cnt; ++i )
    {
        flavorList->GetElementAt(i, getter_AddRefs(genericWrapper));
        currentFlavor = do_QueryInterface(genericWrapper);
        if (!currentFlavor)
            continue;
        currentFlavor->ToString(getter_Copies(flavorStr));
        
        PR_LOG(sDragLm, PR_LOG_DEBUG, ("tnsDragService::GetData trying to get transfer data for %s",
                        (const char *)flavorStr));
                        
        if (IsInternalDrag(mDragMessage))
        {
            mSourceDataItems->GetElementAt(aItemIndex, getter_AddRefs(genericItem));
            nsCOMPtr<nsITransferable> item (do_QueryInterface(genericItem));
            if (!item)
                continue;
            rv = item->GetTransferData(flavorStr, getter_AddRefs(data), &tmpDataLen);
            if (NS_FAILED(rv))
                continue;
            PR_LOG(sDragLm, PR_LOG_DEBUG, ("tnsDragService::GetData setting data."));
            return aTransferable->SetTransferData(flavorStr, data, tmpDataLen);
        } 
        else
        {
            //Check if transfer message is simple_data or older type of DND
            //Check if message has data else continue
            //Negotiate for data (if possible) or get data
        	 //set and return
        }
    }
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("tnsDragService::GetData failed"));
    return NS_ERROR_FAILURE;
}


//-------------------------------------------------------------------------
//
// nsIDragSession : IsDataFlavorSupported
//
// Tells whether the given flavor is supported by the current drag object
//
// Called on MouseOver events
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsDragService::IsDataFlavorSupported (const char *aDataFlavor,
                                      PRBool *_retval)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::IsDataFlavorSupported %s", aDataFlavor));
    if (!_retval)
        return NS_ERROR_INVALID_ARG;

    // set this to no by default
    *_retval = PR_FALSE;

    // check to make sure that we have a drag object set, here
    if (nsnull == mDragMessage)
    {
        PR_LOG(sDragLm, PR_LOG_DEBUG, ("*** warning: IsDataFlavorSupported called without a valid drag context!"));
        return NS_OK;
    }

    if (IsInternalDrag(mDragMessage))
    {
        PRUint32 numDragItems = 0;
        // if we don't have mDataItems we didn't start this drag so it's
        // an external client trying to fool us.
        if (nsnull == mSourceDataItems)
            return NS_OK;
        mSourceDataItems->Count(&numDragItems);
        if (0 == numDragItems)
            PR_LOG(sDragLm, PR_LOG_DEBUG, ("*** warning: Number of dragged items is zero!"));

        // For all dragged items compare their flavors to the one wanted. If there is a match DataFlavor is supported.
        nsCOMPtr<nsISupports> genericItem;
        nsCOMPtr <nsISupportsArray> flavorList;
        PRUint32 numFlavors;
        for (PRUint32 itemIndex = 0; itemIndex < numDragItems; ++itemIndex)
        {
            mSourceDataItems->GetElementAt(itemIndex, getter_AddRefs(genericItem));
            nsCOMPtr<nsITransferable> currItem (do_QueryInterface(genericItem));
            if (nsnull == currItem)
                continue;
            currItem->FlavorsTransferableCanExport(getter_AddRefs(flavorList));
            if (nsnull == flavorList)
                continue;
            flavorList->Count( &numFlavors );
            
            nsCOMPtr<nsISupports> genericWrapper;
            nsXPIDLCString flavorStr;
            for ( PRUint32 flavorIndex = 0; flavorIndex < numFlavors ; ++flavorIndex )
            {
                flavorList->GetElementAt (flavorIndex, getter_AddRefs(genericWrapper));
                nsCOMPtr<nsISupportsCString> currentFlavor = do_QueryInterface(genericWrapper);
                if (nsnull == currentFlavor)
                    continue;
                currentFlavor->ToString ( getter_Copies(flavorStr) );
                PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::IsDataFlavorSupported checking %s against %s", (const char *)flavorStr, aDataFlavor));
                if (0 != strcmp(flavorStr, aDataFlavor))
                    continue;
                PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::IsDataFlavorSupported Got the flavor!"));
                *_retval = PR_TRUE;
                return NS_OK;
            }
        }
    }
    else 
    {
        PR_LOG(sDragLm, PR_LOG_DEBUG, ("*** warning: Native drag not implemented."));
        // TODO: implement native checking
    }
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::IsDataFlavorSupported FALSE"));
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsDragServoce : CreateDragMessage
//
// Builds the drag message needed for BeOS negotiated DND.
//
//-------------------------------------------------------------------------
BMessage *
nsDragService::CreateDragMessage()
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::GetInitialDragMessage"));
    if (nsnull == mSourceDataItems)
        return NULL;
    
    unsigned int numDragItems = 0;
    mSourceDataItems->Count(&numDragItems);
    
    BMessage * returnMsg = new BMessage(B_SIMPLE_DATA);
    
    returnMsg->AddString("be:originator", "BeZilla");
    returnMsg->AddString("be:clip_name","BeZilla Drag Item");
  
    if (mDragAction & DRAGDROP_ACTION_COPY)
        returnMsg->AddInt32("be:actions",B_COPY_TARGET);
    if (mDragAction & DRAGDROP_ACTION_MOVE)
        returnMsg->AddInt32("be:actions",B_MOVE_TARGET);
    if (mDragAction & DRAGDROP_ACTION_LINK)
        returnMsg->AddInt32("be:actions",B_LINK_TARGET);
  
    // Check to see if we're dragging > 1 item.  If we are then we use
    // an internal only type.
    if (numDragItems > 1)
    {
        PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService:: Dragging a list of items ..."));
//        returnMsg->AddString("be:types", gMimeListType);
//        returnMsg->AddString("be:types", B_FILE_MIME_TYPE);
//        returnMsg->AddString("be:filetypes", gMimeListType);
//        returnMsg->AddString("be:type_descriptions", gMimeListType);        
        return returnMsg;
    }

    PRBool addedType = PR_FALSE;

    nsCOMPtr<nsISupports> genericItem;
    nsCOMPtr <nsISupportsArray> flavorList;
    PRUint32 numFlavors;
    nsCOMPtr<nsISupports> genericWrapper;
    nsXPIDLCString flavorStr;

    for (unsigned int itemIndex = 0; itemIndex < numDragItems; ++itemIndex)
    {
        mSourceDataItems->GetElementAt(itemIndex, getter_AddRefs(genericItem));
        nsCOMPtr<nsITransferable> currItem (do_QueryInterface(genericItem));
        if (nsnull == currItem) 
            continue;

        currItem->FlavorsTransferableCanExport(getter_AddRefs(flavorList));
        if (nsnull == flavorList)
            continue;
        flavorList->Count( &numFlavors );
        for (PRUint32 flavorIndex = 0; flavorIndex < numFlavors ; ++flavorIndex )
        {
            flavorList->GetElementAt(flavorIndex, getter_AddRefs(genericWrapper));
            nsCOMPtr<nsISupportsCString> currentFlavor = do_QueryInterface(genericWrapper);
            if (nsnull == currentFlavor)
                continue;
            currentFlavor->ToString ( getter_Copies(flavorStr) );
            
            PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService:: Adding a flavor to our message: %s",flavorStr.get()));
            
            type_code aCode;
            if (B_OK == returnMsg->GetInfo(flavorStr.get(), &aCode))
                continue;
            returnMsg->AddString("be:types",flavorStr.get());
            
            //returnMsg->AddString("be:types", B_FILE_MIME_TYPE);
            returnMsg->AddString("be:filetypes",flavorStr.get());
            returnMsg->AddString("be:type_descriptions",flavorStr.get());
            
            addedType = PR_TRUE;            
            // Check to see if this is text/unicode.  If it is, add
            // text/plain since we automatically support text/plain if
            // we support text/unicode.
            //tqh: but this may cause duplicates?
            if (0 == strcmp(flavorStr, kUnicodeMime))
            {
                PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService:: Adding a TextMime for the UnicodeMime"));
                returnMsg->AddString("be:types",kTextMime);
                //returnMsg->AddString("be:types", B_FILE_MIME_TYPE);
                returnMsg->AddString("be:filetypes",kTextMime);
                returnMsg->AddString("be:type_descriptions",kTextMime);
            }
        }
    }
    
    if (addedType)
    {
        returnMsg->AddString("be:types", B_FILE_MIME_TYPE);
    }
    returnMsg->PrintToStream();
    // If we did not add a type, we can't drag
    NS_ASSERTION(addedType == PR_TRUE, "No flavor/mime in the drag message!");
    return returnMsg;
}

//-------------------------------------------------------------------------
//
// nsIDragSessionBeOS : UpdateDragMessageIfNeeded
//
// Updates the drag message from the old one if we enter a mozilla view with
// a dragmessage from outside. IE one where "be:originator"-key != "BeZilla"
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsDragService::UpdateDragMessageIfNeeded(BMessage *aDragMessage)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::UpdateDragMessageIfNeeded()"));
    if (aDragMessage == mDragMessage) 
        return NS_OK;
    
    // Did we start this drag?
    //It's already set properly by InvokeDragSession, so don't do anything and avoid throwing away
    //mSourceDataItems
    if (IsInternalDrag(aDragMessage))
        return NS_OK;

    PR_LOG(sDragLm, PR_LOG_DEBUG, ("updating."));
    ResetDragInfo();
    mDragMessage = aDragMessage;
    return NS_OK;
}


//-------------------------------------------------------------------------
//
// nsIDragSessionBeOS : TransmitData
//
// When a negotiated drag'n'drop to another app occurs nsWindow
// calls this method with the other apps message which contains
// the info on what data the app wants. This function checks the 
// message and transmits the data to the app, thereby ending the
// drag'n'drop to another app.
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsDragService::TransmitData(BMessage *aNegotiationReply)
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::TransmitData()"));
/*
    unsigned int numDragItems = 0;
    mSourceDataItems->Count(&numDragItems);
    
    returnMsg->AddString("be:originator", "BeZilla");
    returnMsg->AddString("be:clip_name","BeZilla Drag Item");
  
    // Check to see if we're dragging > 1 item.  If we are then we use
    // an internal only type.
    if (numDragItems > 1)
    {
        PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService:: Dragging a list of items ..."));
        delete aNegotiationReply;
        return;
    }
    BMessage aReply = new BMessage(B_MIME_DATA);
    char * aMimeType;
    aNegotiationReply->FindString("be:types", &aMimeType);
     nsCOMPtr<nsITransferable> item;
     item->addDataFlavor(aMimeType);
    GetData(item, 0);
    aReply->AddData(aMimeType, item->);
  */  
    aNegotiationReply->PrintToStream();
    delete aNegotiationReply;
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsIDragService : ResetDragInfo
//
// Resets the stored drag information.
//
//-------------------------------------------------------------------------
void
nsDragService::ResetDragInfo()
{
    PR_LOG(sDragLm, PR_LOG_DEBUG, ("nsDragService::ResetDragInfo()"));
    if (nsnull != mDragMessage) delete mDragMessage;
    mDragMessage = NULL;
    mSourceDataItems = NULL;
}

const char *
nsDragService::FlavorToBeMime(const char * flavor)
{
    //text/plain ok
    //text/unicode -> text/plain
    if (0 == strcmp(flavor,kUnicodeMime)) return kTextMime;    
    //text/html ok
    //AOLMAIL ignore!!     
    //image/png ok
    //image/jpg
    if (0 == strcmp(flavor,kJPEGImageMime)) return "image/jpeg";
    //image/gif ok
    //application/x-moz-file
    if (0 == strcmp(flavor,kFileMime)) return "application/octet-stream";
    //text/x-moz-url (only support this as a filetype (Be bookmark))
    if (0 == strcmp(flavor,kURLMime)) return "application/x-vnd.Be-bookmark";
    //text/x-moz-url-data - we need to read data to find out what type of URL.
    //text/x-moz-url-desc - a url-description (same as title?)
    //kNativeImageMime - don't support as BeOS image
    //kNativeHTMLMime - don't support on BeOS side
    //kFilePromiseURLMime
    //kFilePromiseDestFilename
    //kFilePromiseMime
    //kFilePromiseDirectoryMime
    
//    if (0==strcmp(flavor,kUnicodeMime))
}
