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

#ifndef nsDragService_h__
#define nsDragService_h__

#include "nsBaseDragService.h"

struct IDropSource;
struct IDataObject;
class  nsNativeDragTarget;

/**
 * Native Win32 DragService wrapper
 */

class nsDragService : public nsBaseDragService
{

public:
  nsDragService();
  virtual ~nsDragService();
  
  // nsIDragService
  NS_IMETHOD InvokeDragSession (nsISupportsArray * anArrayTransferables, nsIScriptableRegion * aRegion, PRUint32 aActionType);
  NS_IMETHOD GetCurrentSession (nsIDragSession ** aSession);

  // nsIDragSession
  NS_IMETHOD GetData (nsITransferable * aTransferable, PRUint32 anItem);
  NS_IMETHOD GetNumDropItems (PRUint32 * aNumItems);
  NS_IMETHOD IsDataFlavorSupported(const char *aDataFlavor, PRBool *_retval);

  // native impl.
  NS_IMETHOD SetIDataObject (IDataObject * aDataObj);
  NS_IMETHOD StartInvokingDragSession(IDataObject * aDataObj, PRUint32 aActionType);

protected:
  IDropSource        * mNativeDragSrc;
  nsNativeDragTarget * mNativeDragTarget;
  IDataObject *        mDataObject;
};

#endif // nsDragService_h__
