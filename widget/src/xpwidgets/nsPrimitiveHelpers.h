/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Communicator.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corp.  Portions created by Netscape are Copyright (C) 1999 Netscape 
 * Communications Corp.  All Rights Reserved.
 * 
 * Contributor(s): 
 *   Mike Pinkerton
 */

#ifndef nsPrimitiveHelpers_h___
#define nsPrimitiveHelpers_h___

#include "prtypes.h"

class nsISupports;


class nsPrimitiveHelpers
{
public:

    // Given some data and the flavor it corresponds to, creates the appropriate
    // nsISupports* wrapper for passing across IDL boundaries. The length parameter
    // should not include the null if the data is null terminated.
  static void CreatePrimitiveForData ( const char* aFlavor, void* aDataBuff, 
                                         PRUint32 aDataLen, nsISupports** aPrimitive ) ;

    // Given a nsISupports* primitive and the flavor it represents, creates a new data
    // buffer with the data in it. This data will be null terminated, but the length
    // parameter does not reflect that.
  static void CreateDataFromPrimitive ( const char* aFlavor, nsISupports* aPrimitive, 
                                         void** aDataBuff, PRUint32 aDataLen ) ;

}; // class nsPrimitiveHelpers


#endif // nsPrimitiveHelpers_h___
