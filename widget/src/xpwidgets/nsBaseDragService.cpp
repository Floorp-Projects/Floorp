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

#include "nsBaseDragService.h"
#include "nsITransferable.h"

#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsITransferable.h"
#include "nsISupportsArray.h"
#include "nsSize.h"
#include "nsIRegion.h"


NS_IMPL_ADDREF(nsBaseDragService)
NS_IMPL_RELEASE(nsBaseDragService)

//-------------------------------------------------------------------------
//
// DragService constructor
//
//-------------------------------------------------------------------------
nsBaseDragService::nsBaseDragService() :
  mCanDrop(PR_FALSE), 
  mDoingDrag(PR_FALSE),
  mTargetSize(0,0), 
  mDragAction(DRAGDROP_ACTION_NONE)
{
  NS_INIT_REFCNT();
  nsresult result = NS_NewISupportsArray(getter_AddRefs(mTransArray));
  if ( !NS_SUCCEEDED(result) ) {
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

/**
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 * 
*/ 
nsresult nsBaseDragService::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr)
    return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_NOINTERFACE;

  if ( aIID.Equals(nsCOMTypeInfo<nsIDragService>::GetIID()) ) {
    *aInstancePtr = NS_STATIC_CAST(nsIDragService*,this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  else if (aIID.Equals(nsCOMTypeInfo<nsIDragSession>::GetIID())) {
    *aInstancePtr = NS_STATIC_CAST(nsIDragSession*,this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return rv;
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



//-------------------------------------------------------------------------

NS_IMETHODIMP nsBaseDragService::GetData (nsITransferable * aTransferable, PRUint32 aItemIndex)

{
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseDragService::IsDataFlavorSupported(nsString * aDataFlavor)
{
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseDragService::InvokeDragSession (nsISupportsArray * anArrayTransferables, nsIRegion * aRegion, PRUint32 aActionType)
{
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseDragService::InvokeDragSessionSingle (nsITransferable * aTransferable,  nsIRegion * aRegion, PRUint32 aActionType)
{
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseDragService::GetCurrentSession (nsIDragSession ** aSession)
{
  if ( !aSession )
    return NS_ERROR_INVALID_ARG;
  
  // "this" also implements a drag session, so say we are one but only if there
  // is currently a drag going on. 
  if ( mDoingDrag ) {
    NS_ADDREF_THIS();      // addRef because we're a "getter"
    *aSession = this;
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
  return NS_OK;
}
