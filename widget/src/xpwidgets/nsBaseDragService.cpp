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

#include "nsBaseDragService.h"
#include "nsITransferable.h"

#include "nsIServiceManager.h"
#include "nsITransferable.h"
#include "nsISupportsArray.h"
#include "nsSize.h"
#include "nsIRegion.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsCOMPtr.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsIDOMNode.h"
#include "nsPresContext.h"


NS_IMPL_ADDREF(nsBaseDragService)
NS_IMPL_RELEASE(nsBaseDragService)
NS_IMPL_QUERY_INTERFACE2(nsBaseDragService, nsIDragService, nsIDragSession)


//-------------------------------------------------------------------------
//
// DragService constructor
//
//-------------------------------------------------------------------------
nsBaseDragService::nsBaseDragService() :
  mCanDrop(PR_FALSE), mDoingDrag(PR_FALSE), mTargetSize(0,0), mDragAction(DRAGDROP_ACTION_NONE)
{
  nsresult result = NS_NewISupportsArray(getter_AddRefs(mTransArray));
  if ( NS_FAILED(result) ) {
    //what do we do? we can't throw!
    ;
  }
}

//-------------------------------------------------------------------------
//
// DragService destructor
//
//-------------------------------------------------------------------------
nsBaseDragService::~nsBaseDragService()
{
}


//---------------------------------------------------------
NS_IMETHODIMP nsBaseDragService::SetCanDrop (PRBool aCanDrop) 
{
 mCanDrop = aCanDrop;
 return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP nsBaseDragService::GetCanDrop (PRBool * aCanDrop)
{
  *aCanDrop = mCanDrop;
  return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP nsBaseDragService::SetDragAction (PRUint32 anAction) 
{
 mDragAction = anAction;
 return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP nsBaseDragService::GetDragAction (PRUint32 * anAction)
{
  *anAction = mDragAction;
  return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP nsBaseDragService::SetTargetSize (nsSize aDragTargetSize) 
{
 mTargetSize = aDragTargetSize;
 return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP nsBaseDragService::GetTargetSize (nsSize * aDragTargetSize)
{
  *aDragTargetSize = mTargetSize;
  return NS_OK;
}

//-------------------------------------------------------------------------

NS_IMETHODIMP nsBaseDragService::GetNumDropItems (PRUint32 * aNumItems)
{
  *aNumItems = 0;
  return NS_ERROR_FAILURE;
}


//
// GetSourceDocument
//
// Returns the DOM document where the drag was initiated. This will be
// nsnull if the drag began outside of our application.
//
NS_IMETHODIMP
nsBaseDragService :: GetSourceDocument ( nsIDOMDocument** aSourceDocument )
{
  *aSourceDocument = mSourceDocument.get();
  NS_IF_ADDREF ( *aSourceDocument );
  
  return NS_OK;
}

//
// GetSourceNode
//
// Returns the DOM node where the drag was initiated. This will be
// nsnull if the drag began outside of our application.
//
NS_IMETHODIMP
nsBaseDragService :: GetSourceNode ( nsIDOMNode** aSourceNode )
{
  *aSourceNode = mSourceNode.get();
  NS_IF_ADDREF ( *aSourceNode );
  
  return NS_OK;
}


//-------------------------------------------------------------------------

NS_IMETHODIMP nsBaseDragService::GetData (nsITransferable * aTransferable, PRUint32 aItemIndex)
{
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseDragService::IsDataFlavorSupported(const char *aDataFlavor, PRBool *_retval)
{
  return NS_ERROR_FAILURE;
}


//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseDragService::InvokeDragSession (nsIDOMNode *aDOMNode, nsISupportsArray * anArrayTransferables, nsIScriptableRegion * aRegion, PRUint32 aActionType)
{
  NS_WARN_IF_FALSE ( aDOMNode, "No node provided to InvokeDragSession, you should provide one" );
  if ( aDOMNode ) {
    // stash the document of the dom node
    aDOMNode->GetOwnerDocument ( getter_AddRefs(mSourceDocument) );
    mSourceNode = aDOMNode;
    
    // When the mouse goes down, the selection code starts a mouse capture. However,
    // this gets in the way of determining drag feedback for things like trees because
    // the event coordinates are in the wrong coord system. Turn off capture by 
    // getting the frame associated with the DOM Node.
    nsIFrame* dragFrame = nsnull;
    nsCOMPtr<nsPresContext> context;
    GetFrameFromNode ( aDOMNode, &dragFrame, getter_AddRefs(context) );
    if ( dragFrame && context )
      dragFrame->CaptureMouse ( context, PR_FALSE );
  }
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseDragService::GetCurrentSession (nsIDragSession ** aSession)
{
  if ( !aSession )
    return NS_ERROR_INVALID_ARG;
  
  // "this" also implements a drag session, so say we are one but only if there
  // is currently a drag going on. 
  if ( mDoingDrag ) {
    *aSession = this;
    NS_ADDREF(*aSession);      // addRef because we're a "getter"
  }
  else
    *aSession = nsnull;
    
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseDragService::StartDragSession ()
{
  if (mDoingDrag) {
    return NS_ERROR_FAILURE;
  }
  mDoingDrag = PR_TRUE;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseDragService::EndDragSession ()
{
  if (!mDoingDrag) {
    return NS_ERROR_FAILURE;
  }
  mDoingDrag = PR_FALSE;
  mSourceDocument = nsnull;     // release the source document we've been holding
  
  return NS_OK;
}


//
// GetFrameFromNode
//
// Get the frame for this content node (note: frames are not refcounted).
//
void
nsBaseDragService :: GetFrameFromNode ( nsIDOMNode* inNode, nsIFrame** outFrame,
                                           nsPresContext** outContext )
{
  *outFrame = nsnull;
  *outContext = nsnull;
  if ( !inNode || !outContext )
    return;

  nsCOMPtr<nsIContent> contentNode = do_QueryInterface(inNode);
  if (contentNode) {
    nsIDocument* doc = contentNode->GetDocument();
    if (doc) {
      nsIPresShell *presShell = doc->GetShellAt(0);
      if (presShell) 	{
      	presShell->GetPresContext(outContext);
      	presShell->GetPrimaryFrameFor(contentNode, outFrame);
        NS_ASSERTION ( *outFrame, "Can't get frame for this dom node" );
      }
    }
  }
  
} // GetFrameFromNode
