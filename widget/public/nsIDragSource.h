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

#ifndef nsIDragSource_h__
#define nsIDragSource_h__

#include "nsISupports.h"
 
class nsITransferable;
class nsIDraggedObject;

// {8B5314B7-DB01-11d2-96CE-0060B0FB9956}
#define NS_IDRAGSOURCE_IID      \
{ 0x8b5314b7, 0xdb01, 0x11d2,  { 0x96, 0xce, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } }


class nsIDragSource : public nsISupports {

  public:
 
   /**
    * Gets the transferable object
    *
    * @param  aTransferable The transferable
    * @result NS_Ok if no errors
    */
  
    NS_IMETHOD GetTransferable(nsITransferable ** aTransferable) = 0;

   /**
    * Called when a drag session has finished with the action that occured. 
    *
    * @param aDraggedObj the object being dragged
    * @param anAction tthe action of the drag
    * @result NS_Ok if no errors
    */
 
    NS_IMETHOD DragStopped(nsIDraggedObject * aDraggedObj, PRInt32 anAction) = 0;
};

#endif
