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

#ifndef nsDragSource_h__
#define nsDragSource_h__

#include "nsIDragSource.h"

//#include <OLE2.h>
//#include "OLEIDL.H"

class nsITransferable;
struct IDropSource;

/**
 * Native Win32 DragSource wrapper
 */

class nsDragSource : public nsIDragSource
{

public:
  nsDragSource();
  virtual ~nsDragSource();

  //nsISupports
  NS_DECL_ISUPPORTS
  
  //nsIDragSource
  NS_IMETHOD GetTransferable(nsITransferable ** aTransferable);
  NS_IMETHOD SetTransferable(nsITransferable * aTransferable);
  NS_IMETHOD DragStopped(nsIDraggedObject * aDraggedObj, PRInt32 anAction);


  IDropSource * GetNativeDragSrc() { return mNativeDragSrc; } // XXX this needs to be moved into impl

protected:

  nsITransferable * mTransferable;

  IDropSource * mNativeDragSrc;

};

#endif // nsDragSource_h__
