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

#ifndef nsIDragService_h__
#define nsIDragService_h__

#include "nsISupports.h"
#include "nsPoint.h"

class nsIDragSource;
class nsIImage;

// {8B5314BB-DB01-11d2-96CE-0060B0FB9956}
#define NS_IDRAGSERVICE_IID      \
{ 0x8b5314bb, 0xdb01, 0x11d2, { 0x96, 0xce, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } }


class nsIDragService : public nsISupports {

  public:

  /**
    * Starts a modal drag session.  
    *
    * @param  aDragSrc the object being dragged
    * @param  aStartLocation start location of drag
    * @param  aImageOffset the offset the image should be from the cursor 
    * @param  aImage image to be dragged
    * @param  aDoFlyback indicates image flys back
    */
    NS_IMETHOD StartDragSession (nsIDragSource * aDragSrc, 
                                 nsPoint       * aStartLocation, 
                                 nsPoint       * aImageOffset, 
                                 nsIImage      * aImage, 
                                 PRBool          aDoFlyback) = 0;
};

#endif
