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
#include "nsITransferable.h"
#include "nsSize.h"
#include "nsCOMPtr.h"

/**
 * XP DragService wrapper base class
 */

class nsBaseDragService : public nsIDragService
{

public:
  nsBaseDragService();
  virtual ~nsBaseDragService();

  //nsISupports
  NS_DECL_ISUPPORTS
  
  //nsIDragService
  NS_IMETHOD SetCanDrop (PRBool aCanDrop); 
  NS_IMETHOD GetCanDrop (PRBool * aCanDrop); 

  NS_IMETHOD SetDragAction (PRUint32 anAction); 
  NS_IMETHOD GetDragAction (PRUint32 * anAction); 

  NS_IMETHOD SetTargetSize (nsSize aDragTargetSize); 
  NS_IMETHOD GetTargetSize (nsSize * aDragTargetSize); 

  NS_IMETHOD StartDragSession (nsITransferable * aTransferable, PRUint32 aActionType);
  NS_IMETHOD GetData (nsITransferable * aTransferable);

protected:

  nsCOMPtr<nsITransferable> mTransferable;
  PRBool            mCanDrop;
  nsSize            mTargetSize;
  PRUint32          mDragAction;
};

#endif // nsBaseDragService_h__
