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

#ifndef nsDraggedObject_h__
#define nsDraggedObject_h__

#include "nsIDraggedObject.h"
#include "nsPoint.h"

#include <OLE2.h>
#include "OLEIDL.H"

class nsIDraggedObject;

/**
 * Native Win32 DraggedObject wrapper
 */

class nsDraggedObject : public nsIDraggedObject
{

public:
  nsDraggedObject();
  virtual ~nsDraggedObject();

  //nsISupports
  NS_DECL_ISUPPORTS
  
  //nsIDraggedObject
  NS_IMETHOD GetDragOffset(nscoord * aX, nscoord * aY);
  NS_IMETHOD GetTransferable(nsITransferable ** aTransferable);
  NS_IMETHOD GetSource(nsIDragSource ** aDragSrc);
  NS_IMETHOD GetTarget(nsIDragTarget ** aTarget); 
  NS_IMETHOD GetTargetnFrame(nsIFrame ** aFrame); 



protected:


};

#endif // nsDraggedObject_h__
