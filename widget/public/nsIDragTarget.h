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

#ifndef nsIDragTarget_h__
#define nsIDragTarget_h__

#include "nsISupports.h"
#include "nsPoint.h"

class nsIDraggedObject;

// {8B5314B8-DB01-11d2-96CE-0060B0FB9956}
#define NS_IDRAGTARGET_IID      \
{ 0x8b5314b8, 0xdb01, 0x11d2,  \
{ 0x96, 0xce, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } }


class nsIDragTarget : public nsISupports {

  public:
 
  /**
    * Called when a draggedObject is dragged PRInt32o this destination. 
    *
    * @param  aDraggedObj the object being dragged
    * @param  aLocation X,Y mouse coord in destination frame
    */
    NS_IMETHOD DragEnter(nsIDraggedObject * aDraggedObj, nsPoint * aLocation) = 0;

  /**
    * Called when a draggedObject is dragged inside this destination. 
    *
    * @param  aDraggedObj the object being dragged
    * @param  aLocation X,Y mouse coord in destination frame
    */
     NS_IMETHOD DragOver(nsIDraggedObject * aDraggedObj, nsPoint * aLocation) = 0;

  /**
    * Called when a draggedObject is dragged outside this destination. 
    *
    * @param  aDraggedObj the object being dragged
    * @param  aLocation X,Y mouse coord in destination frame
    */
     NS_IMETHOD DragExit(nsIDraggedObject * aDraggedObj, nsPoint * aLocation) = 0;

  /**
    * Called when a drop occurs with the current set of actions. 
    *
    * @param  aDraggedObj the object being dragged
    * @param  aActions action to be taken
    * @result Return NS_OK is successful
    */
     NS_IMETHOD DragDrop(nsIDraggedObject * aDraggedObj, PRInt32 aActions) = 0;

};

#endif
