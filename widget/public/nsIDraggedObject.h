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

#ifndef nsIDraggedObject_h__
#define nsIDraggedObject_h__

#include "nsISupports.h"
#include "nsCoord.h"

class nsIDragSource;
class nsIDragTarget;
class nsITransferable;
class nsIFrame;

// {8B5314B9-DB01-11d2-96CE-0060B0FB9956}
#define NS_IDRAGGEDOBJECT_IID      \
{ 0x8b5314b9, 0xdb01, 0x11d2, { 0x96, 0xce, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } }


class nsIDraggedObject : public nsISupports {

  public:

  /**
    * Returns the image that represents the dragged data. This is uses extensively on the NeXT by drag destinations to show the
    *  user what things might look like if the object were dropped. 
    *
    * @param  aDraggedObj the object being dragged
    * @param  aX X mouse coord in destination frame
    * @param  aY Y mouse coord in destination frame
    */
    //NS_IMETHOD GetImage(nsIImage * anImage) = 0;

  /**
    * Returns the x,y offset of the dragged image from the cursor. 
    *
    * @param  aDraggedObj the object being dragged
    * @param  aX X mouse coord in destination frame
    * @param  aY Y mouse coord in destination frame
    */
     NS_IMETHOD GetDragOffset(nscoord * aX, nscoord * aY) = 0;

   /**
    * Gets the transferable object
    *
    * @param  aTransferable The transferable
    * @result NS_Ok if no errors
    */
  
    NS_IMETHOD GetTransferable(nsITransferable ** aTransferable) = 0;

  /**
    * Returns the DragSource if the drag session occured in the same address space. If it source is not in the same address
    * NULL is returned. 
    *
    * @param  nsIDragSource The drag source
    */
    NS_IMETHOD GetSource(nsIDragSource ** aDragSrc) = 0;

  /**
    * Returns the destination the dragged object is currently over. 
    *
    * @param  nsIDragTarget the destination target
    * @result Returns NULL if the destination is not in the same address space. 
    */
    NS_IMETHOD  GetTarget(nsIDragTarget ** aTarget) = 0; 

  /**
    * Returns the nsIFrame we are dragging into
    *
    * @param  aDraggedObj the object being dragged
    * @result Returns NULL if the destination is not in the same address space. 
    */
    NS_IMETHOD GetTargetnFrame(nsIFrame ** aFrame) = 0; 

};

#endif
