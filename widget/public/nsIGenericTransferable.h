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

#ifndef nsIGenericTransferable_h__
#define nsIGenericTransferable_h__

#include "nsISupports.h"
#include "nsString.h"

class nsIDataFlavor;
class nsISupportsArray;
class nsIFormatConverter;

// {867AF703-F360-11d2-96D2-0060B0FB9956}
#define NS_IGENERICTRANSFERABLE_IID      \
{ 0x867af703, 0xf360, 0x11d2, { 0x96, 0xd2, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } }

class nsIGenericTransferable : public nsISupports {

  public:

    static const nsIID& GetIID() { static nsIID iid = NS_IGENERICTRANSFERABLE_IID; return iid; }

  /**
    * Computes a list of flavors that the transferable can accept into it, either through
    * intrinsic knowledge or input data converters.
    *
    * @param  outFlavorList fills list with supported flavors. This is a copy of
    *          the internal list, so it may be edited w/out affecting the transferable.
    */
  NS_IMETHOD FlavorsTransferableCanImport ( nsISupportsArray** outFlavorList ) = 0;

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
