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
class nsIFormatConverter;

// {8B5314BC-DB01-11d2-96CE-0060B0FB9956}
#define NS_ITRANSFERABLE_IID      \
{ 0x8b5314bc, 0xdb01, 0x11d2, { 0x96, 0xce, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } }

class nsITransferable : public nsISupports {

  public:

    static const nsIID& GetIID() { static nsIID iid = NS_ITRANSFERABLE_IID; return iid; }

  /**
    * Computes a list of flavors that the transferable can accept into it, either through
    * intrinsic knowledge or input data converters.
    *
    * @param  outFlavorList fills list with supported flavors. This is a copy of
    *          the internal list, so it may be edited w/out affecting the transferable.
    */
  NS_IMETHOD FlavorsTransferableCanImport ( nsISupportsArray** outFlavorList ) = 0;

  /**
    * Computes a list of flavors that the transferable can export, either through
    * intrinsic knowledge or output data converters.
    *
    * @param  aDataFlavorList fills list with supported flavors. This is a copy of
    *          the internal list, so it may be edited w/out affecting the transferable.
    */
  NS_IMETHOD FlavorsTransferableCanExport ( nsISupportsArray** outFlavorList ) = 0;

  /**
    * Get the list of data flavors that this transferable supports (w/out conversion). 
    * (NOTE: We're not sure how useful this is in the presence of the above two methods,
    * but figured we'd keep it around just in case). 
    *
    * @param  aDataFlavorList fills list with supported flavors. This is a copy of
    *          the internal list, so it may be edited w/out affecting the transferable.
    */
    NS_IMETHOD GetTransferDataFlavors(nsISupportsArray ** aDataFlavorList) = 0;

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
    * @param  aData the data. This is NOT a copy, so the caller MUST NOT DELETE it.
    * @param  aDataLen the length of the data
    */
    NS_IMETHOD GetTransferData(nsIDataFlavor * aFlavor, void ** aData, PRUint32 * aDataLen) = 0;

  /**
    * Set Data into the transferable as a specified DataFlavor. The transferable now
    * owns the data, so the caller must not delete it.
    *
    * @param  aFlavor the flavor of data that is being set
    * @param  aData the data 
    * @param  aDataLen the length of the data (it may or may not be meaningful)
    */
    NS_IMETHOD SetTransferData(nsIDataFlavor * aFlavor, void * aData, PRUint32 aDataLen) = 0;

  /**
    * Add the data flavor, indicating that this transferable 
    * can receive this type of flavor
    *
    * @param  aDataFlavor a new data flavor to handle
    * @param  aHumanPresentableName human readable string for mime
    */
    NS_IMETHOD AddDataFlavor(nsIDataFlavor * aDataFlavor) = 0;

  /**
    * Returns whether the data is large
    *
    * @return NS_OK is data set is larg, NS_ERROR_FAILURE if data set is small
    */
    NS_IMETHOD IsLargeDataSet() = 0;

  /**
    * Sets the converter for this transferable
    *
    */
    NS_IMETHOD SetConverter(nsIFormatConverter * aConverter) = 0;

  /**
    * Gets the converter for this transferable
    *
    */
    NS_IMETHOD GetConverter(nsIFormatConverter ** aConverter) = 0;

};

#endif
