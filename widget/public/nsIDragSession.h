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

#ifndef nsIDragSession_h__
#define nsIDragSession_h__

#include "nsISupports.h"
#include "nsString.h"

class  nsITransferable;
struct nsSize;

// {CBA22C53-FCCE-11d2-96D4-0060B0FB9956}

#define NS_IDRAGSESSION_IID      \
{ 0xcba22c53, 0xfcce, 0x11d2, { 0x96, 0xd4, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } };

class nsIDragSession : public nsISupports {

  public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDRAGSESSION_IID)

  /**
    * Set the current state of the drag whether it can be dropped or not.
    * usually the target "frame" sets this so the native system can render the correct feedback
    *
    * @param  aCanDrop indicates whether it can be dropped here
    */

   NS_IMETHOD SetCanDrop (PRBool aCanDrop) = 0; 

   /**
    * Retrieves whether the drag can be dropped at this location 
    *
    * @param  aCanDrop indicates whether it can be dropped here
    */

    NS_IMETHOD GetCanDrop (PRBool * aCanDrop) = 0; 

  /**
    * Sets the action (copy, move, link, et.c) for the current drag 
    *
    * @param  anAction the current action
    */

    NS_IMETHOD SetDragAction (PRUint32 anAction) = 0; 

   /**
    * Gets the action (copy, move, link, et.c) for the current drag 
    *
    * @param  anAction the current action
    */

    NS_IMETHOD GetDragAction (PRUint32 * anAction) = 0; 

  /**
    * Sets the current width and height if the drag target area. 
    * It will contain the current size of the Frame that the drag is currently in
    * 
    * @param  aDragTargetSize contains width/height of the current target
    */

   NS_IMETHOD SetTargetSize (nsSize aDragTargetSize) = 0; 

   /**
    * Gets the current width and height if the drag target area. 
    * It will contain the current size of the Frame that the drag is currently in
    *
    * @param  aCanDrop indicates whether it can be dropped here
    */

    NS_IMETHOD GetTargetSize (nsSize * aDragTargetSize) = 0; 

  /**
    * Get data from a Drag->Drop   
    *
    * @param  aTransferable the transferable for the data to be put into
    * @param  aItemIndex which of multiple drag items, zero-based
    */

    NS_IMETHOD GetData (nsITransferable * aTransferable, PRUint32 aItemIndex) = 0;

  /**
    * Get the number items that were dropped
    *
    * @param  aNumItems the number of dropped items
    */

    NS_IMETHOD GetNumDropItems (PRUint32 * aNumItems) = 0;


   /**
    * Check to set if ant of the native data on the clipboard matches this data flavor
    *
    * @result NS_OK if if the data flavor is supported and, NS_ERROR_FAILURE is it is not
    */

    NS_IMETHOD IsDataFlavorSupported(nsString * aDataFlavor) = 0;
};

#endif
