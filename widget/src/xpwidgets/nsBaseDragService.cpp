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
#include "nsITransferable.h"
#include "nsISupportsArray.h"
#include "nsSize.h"
#include "nsIRegion.h"
#include "nsISupportsPrimitives.h"
#include "nsCOMPtr.h"


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
NS_IMETHODIMP nsBaseDragService::IsDataFlavorSupported(const char *aDataFlavor, PRBool *_retval)
{
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseDragService::InvokeDragSession (nsISupportsArray * anArrayTransferables, nsIRegion * aRegion, PRUint32 aActionType)
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
  return NS_OK;
}


//еее skanky hack until i can correctly re-create primitives from native data. i know this code sucks,
//еее please forgive me.
void
nsBaseDragService :: CreatePrimitiveForData ( const char* aFlavor, void* aDataBuff, PRUint32 aDataLen, nsISupports** aPrimitive )
{
  if ( !aPrimitive )
    return;

  if ( strcmp(aFlavor,kTextMime) == 0 ) {
    nsCOMPtr<nsISupportsString> primitive;
    nsresult rv = nsComponentManager::CreateInstance(NS_SUPPORTS_STRING_PROGID, nsnull, 
                                                      NS_GET_IID(nsISupportsString), getter_AddRefs(primitive));
    if ( primitive ) {
      primitive->SetData ( (char*)aDataBuff );
      nsCOMPtr<nsISupports> genericPrimitive ( do_QueryInterface(primitive) );
      *aPrimitive = genericPrimitive;
      NS_ADDREF(*aPrimitive);
    }
  }
  else {
    nsCOMPtr<nsISupportsWString> primitive;
    nsresult rv = nsComponentManager::CreateInstance(NS_SUPPORTS_WSTRING_PROGID, nsnull, 
                                                      NS_GET_IID(nsISupportsWString), getter_AddRefs(primitive));
    if ( primitive ) {
      primitive->SetData ( (unsigned short*)aDataBuff );
      nsCOMPtr<nsISupports> genericPrimitive ( do_QueryInterface(primitive) );
      *aPrimitive = genericPrimitive;
      NS_ADDREF(*aPrimitive);
    }  
  }

} // CreatePrimitiveForData


void
nsBaseDragService :: CreateDataFromPrimitive ( const char* aFlavor, nsISupports* aPrimitive, void** aDataBuff, PRUint32 aDataLen )
{
  if ( !aDataBuff )
    return;

  if ( strcmp(aFlavor,kTextMime) == 0 ) {
    nsCOMPtr<nsISupportsString> plainText ( do_QueryInterface(aPrimitive) );
    if ( plainText )
      plainText->GetData ( (char**)aDataBuff );
  }
  else {
    nsCOMPtr<nsISupportsWString> doubleByteText ( do_QueryInterface(aPrimitive) );
    if ( doubleByteText )
      doubleByteText->GetData ( (unsigned short**)aDataBuff );
  }

}
