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

#ifndef _nsHeaderArray_h_
#define _nsHeaderArray_h_

#include "nsVoidArray.h"
#include "nsICollection.h"
#include "nsCOMPtr.h"

/* 
    The nsHeaderArray class is the array of elements of nsHeaderPair 
    type.

    This class is internal to the protocol handler implementation and 
    should theroetically not be used by the app or the core netlib.

    -Gagan Saksena 03/29/99
*/
class nsHeaderArray : public nsICollection
{

public:

    // Constructor and destructor
    nsHeaderArray();
    virtual ~nsHeaderArray();

    // Methods from nsISupports
    NS_DECL_ISUPPORTS

    // Methods from nsICollection
    NS_IMETHOD              AppendElement(nsISupports *aItem);
    
    NS_IMETHOD              Clear(void);
    
    NS_IMETHOD_(PRUint32)   Count(void) const;

    NS_IMETHOD              Enumerate(nsIEnumerator* *result);
    
    NS_IMETHOD              RemoveElement(nsISupports *aItem);
    

protected:

    nsVoidArray* m_pArray;
};

#endif /* _nsHeaderArray_h_ */
