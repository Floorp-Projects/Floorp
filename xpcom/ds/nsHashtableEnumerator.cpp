/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * Implementation of nsHashEnumerator.
 * Use it to expose nsIEnumerator interfaces around nsHashtable objects.  
 * Contributed by Rob Ginda, rginda@ix.netcom.com
 */

#include "nscore.h"
#include "nsHashtableEnumerator.h"

class nsHashtableEnumerator : public nsIBidirectionalEnumerator
{
  public:    
    NS_DECL_ISUPPORTS

    NS_IMETHOD First ();
    NS_IMETHOD Last ();
    NS_IMETHOD Prev ();
    NS_IMETHOD Next ();
    NS_IMETHOD CurrentItem (nsISupports **retval);
    NS_IMETHOD IsDone ();

  public:    
    virtual ~nsHashtableEnumerator ();
    nsHashtableEnumerator (nsHashtable *aHash, 
                           NS_HASH_ENUMERATOR_CONVERTER aConverter,
                           void *aData);
    nsHashtableEnumerator (); /* no implementation */

  private:
    NS_IMETHOD Reset(nsHashtable *aHash, 
                     NS_HASH_ENUMERATOR_CONVERTER aConverter,
                     void *aData);
    NS_IMETHOD ReleaseElements();

    nsISupports  **mElements;
    PRInt16      mCount, mCurrent;
    PRBool       mDoneFlag;
    
};

struct nsHashEnumClosure
{
    NS_HASH_ENUMERATOR_CONVERTER Converter;
    nsISupports                  **Elements;
    PRInt16                      Current;
    void                         *Data;
};

nsresult
NS_NewHashtableEnumerator (nsHashtable *aHash, 
                           NS_HASH_ENUMERATOR_CONVERTER aConverter,
                           void *aData, nsIEnumerator **retval)
{
    NS_PRECONDITION (retval, "null ptr");

    *retval = nsnull;
    
    nsHashtableEnumerator *hte = new nsHashtableEnumerator (aHash, aConverter,
                                                            aData);
    if (!hte)
        return NS_ERROR_OUT_OF_MEMORY;

    return hte->QueryInterface (nsCOMTypeInfo<nsIEnumerator>::GetIID(),
                                (void **)retval);
}

nsHashtableEnumerator::nsHashtableEnumerator (nsHashtable *aHash,
                                        NS_HASH_ENUMERATOR_CONVERTER aConverter,
                                        void *aData)
        : mElements(nsnull), mCount(0), mDoneFlag(PR_TRUE)
{
    NS_INIT_REFCNT();
    Reset (aHash, aConverter, aData);
    
}

PRBool
hash_enumerator (nsHashKey *aKey, void *aObject, void *closure)
{
    nsresult rv;
    
    nsHashEnumClosure *c = (nsHashEnumClosure *)closure;
    
    rv = c->Converter (aKey, (void *)aObject, (void *)c->Data,
                       &c->Elements[c->Current]);

    if (!NS_FAILED(rv))
        c->Current++;
    
    return PR_TRUE;

}

NS_IMETHODIMP
nsHashtableEnumerator::Reset (nsHashtable *aHash,
                              NS_HASH_ENUMERATOR_CONVERTER aConverter,
                              void *aData)
{
    nsHashEnumClosure c;
    
    ReleaseElements();

    mCurrent = c.Current = 0;
    mCount = aHash->Count();
    if (mCount == 0)
        return NS_ERROR_FAILURE;

    mElements = c.Elements = new nsISupports*[mCount];
    c.Data = aData;
    c.Converter = aConverter;
    aHash->Enumerate (&hash_enumerator, &c);

    mCount = c.Current;  /* some items may not have converted correctly */
    mDoneFlag = PR_FALSE;
    
    return NS_OK;
    
}

NS_IMETHODIMP
nsHashtableEnumerator::ReleaseElements()
{
    
    for (; mCount > 0; mCount--)
        if (mElements[mCount - 1])
            NS_RELEASE(mElements[mCount - 1]);

    delete[] mElements;
    mElements = nsnull;
    
    return NS_OK;
    
}

NS_IMPL_ADDREF(nsHashtableEnumerator);
NS_IMPL_RELEASE(nsHashtableEnumerator);

NS_IMETHODIMP
nsHashtableEnumerator::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if ( !aInstancePtr )
        return NS_ERROR_NULL_POINTER;

    if ( aIID.Equals(nsCOMTypeInfo<nsIBidirectionalEnumerator>::GetIID()) )
        *aInstancePtr = NS_STATIC_CAST(nsIBidirectionalEnumerator*, this);
    else if ( aIID.Equals(nsCOMTypeInfo<nsIEnumerator>::GetIID()) )
        *aInstancePtr = NS_STATIC_CAST(nsIEnumerator*, this);
    else if ( aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()) )
        *aInstancePtr = NS_STATIC_CAST(nsISupports*,
                                       NS_STATIC_CAST(nsISupports*, this));
    else
        *aInstancePtr = nsnull;

    nsresult status;
    if ( !*aInstancePtr )
        status = NS_NOINTERFACE;
    else
    {
        NS_ADDREF(NS_REINTERPRET_CAST(nsISupports*, *aInstancePtr));
        status = NS_OK;
    }

    return status;
    
}

nsHashtableEnumerator::~nsHashtableEnumerator() 
{
    ReleaseElements();
}

NS_IMETHODIMP
nsHashtableEnumerator::First ()
{
    if (!mElements || (mCount == 0))
        return NS_ERROR_FAILURE;

    mCurrent = 0;
    mDoneFlag = PR_FALSE;
    
    return NS_OK;

}

NS_IMETHODIMP
nsHashtableEnumerator::Last ()
{
    if (!mElements || (mCount == 0))
        return NS_ERROR_FAILURE;
    
    mCurrent = mCount - 1;
    mDoneFlag = PR_FALSE;
    
    return NS_OK;

}

NS_IMETHODIMP
nsHashtableEnumerator::Prev ()
{
    if (!mElements || (mCount == 0) || (mCurrent == 0))
        return NS_ERROR_FAILURE;

    mCurrent--;
    if (mCurrent == 0)
        mDoneFlag = PR_TRUE;
    else
        mDoneFlag = PR_FALSE;

    return NS_OK;
    
}

NS_IMETHODIMP
nsHashtableEnumerator::Next ()
{
    if (!mElements || (mCount == 0) || (mCurrent == mCount - 1))
        return NS_ERROR_FAILURE;

    mCurrent++;
    if (mCurrent == mCount - 1)
        mDoneFlag = PR_TRUE;
    else
        mDoneFlag = PR_FALSE;
    
    return NS_OK;
    
}

NS_IMETHODIMP
nsHashtableEnumerator::CurrentItem (nsISupports **retval)
{
    NS_PRECONDITION (retval, "null ptr");
    NS_PRECONDITION (mElements, "invalid state");
    NS_ASSERTION (mCurrent >= 0, "mCurrent less than zero");

    if (mCount == 0)
    {
        *retval = nsnull;
        return NS_ERROR_FAILURE;
    }

    NS_ASSERTION (mCurrent <= mCount - 1, "mCurrent too high");

    *retval = mElements[mCurrent];

    /* who says the item can't be null? */
    if (*retval)
        NS_ADDREF(*retval);

    return NS_OK;
    
}

NS_IMETHODIMP
nsHashtableEnumerator::IsDone ()
{

    if ((!mElements) || (mCount == 0) || (mDoneFlag))
        return NS_ERROR_FAILURE;

    return NS_OK;

}
