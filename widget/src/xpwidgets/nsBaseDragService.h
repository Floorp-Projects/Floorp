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

#ifndef nsBaseDragService_h__
#define nsBaseDragService_h__

#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsITransferable.h"
#include "nsISupportsArray.h"
#include "nsCOMPtr.h"

/**
 * XP DragService wrapper base class
 */

class nsBaseDragService : public nsIDragService, public nsIDragSession
{

public:
  nsBaseDragService();
  virtual ~nsBaseDragService();

  //nsISupports
  NS_DECL_ISUPPORTS
  
  //nsIDragSession and nsIDragService
  NS_DECL_NSIDRAGSERVICE
  NS_DECL_NSIDRAGSESSION

protected:

  static void CreatePrimitiveForData ( const char* aFlavor, void* aDataBuff, PRUint32 aDataLen, nsISupports** aPrimitive );
  static void CreateDataFromPrimitive ( const char* aFlavor, nsISupports* aPrimitive, void** aDataBuff, PRUint32 aDataLen );

  nsCOMPtr<nsISupportsArray> mTransArray;
  PRBool             mCanDrop;
  PRBool             mDoingDrag;
  nsSize             mTargetSize;
  PRUint32           mDragAction;
};

#endif // nsBaseDragService_h__
