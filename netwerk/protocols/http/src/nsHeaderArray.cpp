/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsHeaderArray.h"
#include "nscore.h"

nsHeaderArray::nsHeaderArray()
{
    NS_INIT_REFCNT();
    m_pArray = new nsVoidArray();
}


nsHeaderArray::~nsHeaderArray()
{
    if (m_pArray)
    {
        delete m_pArray;
        m_pArray = 0;
    }
}

NS_IMPL_ADDREF(nsHeaderArray);
NS_IMPL_RELEASE(nsHeaderArray);

NS_IMETHODIMP
nsHeaderArray::QueryInterface(REFNSIID iid, void** result)
{
    if (result == nsnull)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(nsISupports::GetIID())) {
        *result = NS_STATIC_CAST(nsISupports*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (iid.Equals(nsICollection::GetIID())) {
        *result = NS_STATIC_CAST(nsICollection*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

////////////////////////////////////////////////////////////////////////////////
// nsICollection methods:

NS_IMETHODIMP_(PRUint32) 
nsHeaderArray::Count(void) const
{
    return m_pArray->Count();
}

NS_IMETHODIMP
nsHeaderArray::AppendElement(nsISupports* i_HeaderPair)
{
    return m_pArray->AppendElement(i_HeaderPair);
}

NS_IMETHODIMP
nsHeaderArray::RemoveElement(nsISupports* i_HeaderPair)
{
    return m_pArray->RemoveElement(i_HeaderPair) ? 
        NS_OK : NS_ERROR_FAILURE;
}
NS_IMETHODIMP
nsHeaderArray::Enumerate(nsIEnumerator* *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHeaderArray::Clear(void)
{
    m_pArray->Clear();
    return NS_OK;
}

