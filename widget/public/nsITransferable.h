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

#ifndef nsITransferable_h__
#define nsITransferable_h__

#include "nsISupports.h"
#include "nsString.h"

class nsIDataFlavor;
class nsISupportsArray;

// {8B5314BC-DB01-11d2-96CE-0060B0FB9956}
#define NS_ITRANSFERABLE_IID      \
{ 0x8b5314bc, 0xdb01, 0x11d2, { 0x96, 0xce, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } }

class nsITransferable : public nsISupports {

  public:

  /**
    * Get the list of data flavors that this transferable supports.  
    *
    * @param  aDataFlavorList fills list with supported flavors
    */
    NS_IMETHOD GetTransferDataFlavors(nsISupportsArray * aDataFlavorList) = 0;

  /**
    * See if the given flavor is supported  
    *
    * @param  aFlavor the flavor to check
    * @result Returns NS_OK if supported, return NS_ERROR_FAILURE if not
    */
    NS_IMETHOD IsDataFlavorSupported(nsIDataFlavor * aFlavor) = 0;

  /**
    * Given a flavor retrieve the data. 
    *
    * @param  aFlavor the flavor of data to retrieve
    * @param  aData the data
    */
    NS_IMETHOD GetTransferData(nsIDataFlavor * aFlavor, nsISupports ** aData) = 0;

  /**
    * Sets data
    *
    * @param  aFlavor the flavor of data to retrieve
    * @param  aData the data
    */
    NS_IMETHOD SetTransferData(nsString & aStr) = 0;

  /**
    * Initializes the data flavor 
    *
    * @param  aMimeType mime string
    * @param  aHumanPresentableName human readable string for mime
    */
    NS_IMETHOD AddDataFlavor(const nsString & aMimeType, const nsString & aHumanPresentableName) = 0;

  /**
    * Returns the Platform specific data object
    *
    * @param  aData the native data to be put on the clipboard
    */
    NS_IMETHOD GetNativeData(void ** aData) = 0;

  /**
    * Sets Native data 
    *
    * @param  aData the native data
    */
    NS_IMETHOD SetNativeData(void * aData) = 0;
};

#endif
