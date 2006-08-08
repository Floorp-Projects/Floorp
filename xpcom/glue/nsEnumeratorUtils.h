/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsEnumeratorUtils_h__
#define nsEnumeratorUtils_h__

#include "nsIEnumerator.h"
#include "nsISupportsArray.h"

class NS_COM nsArrayEnumerator : public nsISimpleEnumerator
{
public:
    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator interface
    NS_IMETHOD HasMoreElements(PRBool* aResult);
    NS_IMETHOD GetNext(nsISupports** aResult);

    // nsRDFArrayEnumerator methods
    nsArrayEnumerator(nsISupportsArray* aValueArray);
    virtual ~nsArrayEnumerator(void);

protected:
    nsISupportsArray* mValueArray;
    PRInt32 mIndex;
};

extern "C" NS_COM nsresult
NS_NewArrayEnumerator(nsISimpleEnumerator* *result,
                      nsISupportsArray* array);

////////////////////////////////////////////////////////////////////////////////

class NS_COM nsSingletonEnumerator : public nsISimpleEnumerator
{
public:
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator methods
    NS_IMETHOD HasMoreElements(PRBool* aResult);
    NS_IMETHOD GetNext(nsISupports** aResult);

    nsSingletonEnumerator(nsISupports* aValue);
    virtual ~nsSingletonEnumerator();

protected:
    nsISupports* mValue;
    PRBool mConsumed;
};

extern "C" NS_COM nsresult
NS_NewSingletonEnumerator(nsISimpleEnumerator* *result,
                          nsISupports* singleton);

////////////////////////////////////////////////////////////////////////////////

class NS_COM nsAdapterEnumerator : public nsISimpleEnumerator
{
public:
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator methods
    NS_IMETHOD HasMoreElements(PRBool* aResult);
    NS_IMETHOD GetNext(nsISupports** aResult);

    nsAdapterEnumerator(nsIEnumerator* aEnum);
    virtual ~nsAdapterEnumerator();

protected:
    nsIEnumerator* mEnum;
    nsISupports*   mCurrent;
    PRBool mStarted;
};

extern "C" NS_COM nsresult
NS_NewAdapterEnumerator(nsISimpleEnumerator* *result,
                        nsIEnumerator* enumerator);

////////////////////////////////////////////////////////////////////////

#endif /* nsEnumeratorUtils_h__ */
