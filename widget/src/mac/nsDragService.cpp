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



#include "nsWidgetsCID.h"

#include "nsClipboard.h"





static NS_DEFINE_IID(kIDragServiceIID,   NS_IDRAGSERVICE_IID);



NS_IMPL_ADDREF_INHERITED(nsDragService, nsBaseDragService)

NS_IMPL_RELEASE_INHERITED(nsDragService, nsBaseDragService)





//

// DragService constructor

//

nsDragService::nsDragService()

{

  NS_INIT_REFCNT();

}





//

// DragService destructor

//

nsDragService::~nsDragService()

{

}





/**

 * @param aIID The name of the class implementing the method

 * @param _classiiddef The name of the #define symbol that defines the IID

 * for the class (e.g. NS_ISUPPORTS_IID)

 * 

*/ 

nsresult

nsDragService::QueryInterface(const nsIID& aIID, void** aInstancePtr)

{



  if (NULL == aInstancePtr) {

    return NS_ERROR_NULL_POINTER;

  }



  nsresult rv = NS_NOINTERFACE;



  if (aIID.Equals(kIDragServiceIID)) {

    *aInstancePtr = (void*) ((nsIDragService*)this);

    NS_ADDREF_THIS();

    return NS_OK;

  }



  return rv;

}





//

// StartDragSession

//

// Do all the work to kick it off.

//

NS_IMETHODIMP

nsDragService::StartDragSession (nsITransferable * aTransferable, PRUint32 aActionType)



{

  return NS_ERROR_FAILURE;

  

} // StartDragSession



