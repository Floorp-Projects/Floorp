/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rob Ginda <rginda@ix.netcom.com>
 *   Alec Flett <alecf@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Implementation of nsHashEnumerator.
 * Use it to expose nsISimpleEnumerator interfaces around nsHashtable objects. 
 */

#include "nscore.h"
#include "nsHashtableEnumerator.h"
#include "nsISimpleEnumerator.h"

struct nsHashEnumClosure
{
    nsHashEnumeratorConverterFunc Converter;
    nsISupports                  **Elements;
    PRUint32                     Current;
    void                         *Data;
};

PRBool PR_CALLBACK 
hash_enumerator(nsHashKey *aKey, void *aObject, void *closure)
{
    nsresult rv;
    
    nsHashEnumClosure *c = (nsHashEnumClosure *)closure;
    
    rv = c->Converter(aKey, (void *)aObject, (void *)c->Data,
                      &c->Elements[c->Current]);

    if (NS_SUCCEEDED(rv))
        c->Current++;
    
    return PR_TRUE;

}

class nsHashtableEnumerator : public nsISimpleEnumerator
{
public:
    nsHashtableEnumerator(nsHashtable *aHash,
                          nsHashEnumeratorConverterFunc aConverter,
                          void* aData);
    virtual ~nsHashtableEnumerator();

    NS_DECL_ISUPPORTS
    NS_DECL_NSISIMPLEENUMERATOR

    void* operator new (size_t size, nsHashtable* aArray) CPP_THROW_NEW;
    void operator delete(void* ptr) {
        ::operator delete(ptr);
    }
    
private:
    // not to be implemented
    void* operator new (size_t size);
    
    PRUint32 mCurrent;
    PRUint32 mCount;
    nsISupports* mElements[1];
};

NS_COM nsresult
NS_NewHashtableEnumerator(nsHashtable *aHash,
                          nsHashEnumeratorConverterFunc aConverter,
                          void* aData, nsISimpleEnumerator **retval)
{
    *retval = nsnull;

    nsHashtableEnumerator *hte =
        new (aHash) nsHashtableEnumerator(aHash, aConverter, aData);

    if (!hte)
        return NS_ERROR_OUT_OF_MEMORY;

    *retval = NS_STATIC_CAST(nsISimpleEnumerator*, hte);
	NS_ADDREF(*retval);

    return NS_OK;
}

void*
nsHashtableEnumerator::operator new(size_t size, nsHashtable* aHash)
    CPP_THROW_NEW
{
    // create enough space such that mValueArray points to a large
    // enough value. Note that the initial value of size gives us
    // space for mValueArray[0], so we must subtract
    size += (aHash->Count() - 1) * sizeof(nsISupports*);

    // do the actual allocation
    nsHashtableEnumerator * result =
        NS_STATIC_CAST(nsHashtableEnumerator*, ::operator new(size));

    return result;
}

NS_IMPL_ISUPPORTS1(nsHashtableEnumerator, nsISimpleEnumerator)

nsHashtableEnumerator::nsHashtableEnumerator(nsHashtable *aHash,
                                             nsHashEnumeratorConverterFunc
                                             aConverter,
                                             void* aData)
{
    NS_INIT_ISUPPORTS();

    nsHashEnumClosure c;
    c.Current = mCurrent = 0;
    c.Elements = mElements;
    c.Data = aData;
    c.Converter = aConverter;
    aHash->Enumerate(&hash_enumerator, &c);

    mCount = c.Current; /* some items may not have converted correctly */

}

nsHashtableEnumerator::~nsHashtableEnumerator()
{
    for (;mCurrent<mCount; mCurrent++)
        NS_RELEASE(mElements[mCurrent]);
}

NS_IMETHODIMP
nsHashtableEnumerator::HasMoreElements(PRBool *aResult)
{
    *aResult = (mCurrent < mCount);
    return NS_OK;
}

NS_IMETHODIMP
nsHashtableEnumerator::GetNext(nsISupports** aResult)
{
    if (mCurrent >= mCount) return NS_ERROR_UNEXPECTED;
    
    *aResult = mElements[mCurrent++];
    // no need to addref - we'll just steal the refcount from the
    // array of elements.. this is safe because the enumerator can
    // only advance through the list once.
    return NS_OK;
}
