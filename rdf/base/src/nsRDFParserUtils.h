/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */


/*

  Some useful parsing routines.

 */

#ifndef nsRDFParserUtils_h__
#define nsRDFParserUtils_h__

#include "nscore.h"
class nsIURL;
class nsString;

class nsRDFParserUtils {
public:
    static PRUnichar
    EntityToUnicode(const char* buf);

    static void
    StripAndConvert(nsString& aResult);

    static nsresult
    GetQuotedAttributeValue(const nsString& aSource, 
                            const nsString& aAttribute,
                            nsString& aValue);


    static void
    FullyQualifyURI(const nsIURL* base, nsString& spec);

};


#endif // nsRDFPasrserUtils_h__

