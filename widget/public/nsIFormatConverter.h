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

#ifndef nsIFormatConverter_h__
#define nsIFormatConverter_h__

#include "nsISupports.h"

class nsIDataFlavor;
class nsISupportsArray;

// {948A0023-E3A7-11d2-96CF-0060B0FB9956}
#define NS_IFORMATCONVERTER_IID      \
{ 0x948a0023, 0xe3a7, 0x11d2, { 0x96, 0xcf, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } };

class nsIFormatConverter : public nsISupports {

  public:

    static const nsIID& GetIID() { static nsIID iid = NS_IFORMATCONVERTER_IID; return iid; }

  /**
    * Get the list of the "input" data flavors, in otherwords, the flavors  
    * that this converter can convert "from" (the incoming data to the converter)
    *
    * @param  aDataFlavorList fills list with supported flavors
    */
    NS_IMETHOD GetInputDataFlavors(nsISupportsArray ** aDataFlavorList) = 0;

  /**
    * Get the list of the "output" data flavors, in otherwords, the flavors  
    * that this converter can convert "to" (the outgoing data of the converter)
    *
    * @param  aDataFlavorList fills list with supported flavors
    */
    NS_IMETHOD GetOutputDataFlavors(nsISupportsArray ** aDataFlavorList) = 0;

  /**
    * Determines whether a converion from one flavor to another is supported
    *
    * @param  aFromFormatConverter flavor to convert from
    * @param  aFromFormatConverter flavor to convert to
    * @returns returns NS_OK if it can be converted
    */
    NS_IMETHOD CanConvert(nsIDataFlavor * aFromDataFlavor, nsIDataFlavor * aToDataFlavor) = 0;


  /**
    * Determines whether a converion from one flavor to another
    *
    * @param  aFromFormatConverter flavor to convert from
    * @param  aFromFormatConverter flavor to convert to (destination own the memory)
    * @returns returns NS_OK if it was converted
    */
    NS_IMETHOD Convert(nsIDataFlavor * aFromDataFlavor, void * aFromData, PRUint32 aDataLen,
                       nsIDataFlavor * aToDataFlavor,   void ** aToData, PRUint32 * aDataToLen) = 0;



};

#endif

