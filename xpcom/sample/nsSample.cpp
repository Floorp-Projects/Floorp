/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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


/*

  A sample of XPConnect. This file contains an implementation of
  nsISample.

*/
#include "nscore.h"
#include "nsISample.h"
#include "nsIAllocator.h"
#include "plstr.h"
#include "stdio.h"

class SampleImpl : public nsISample
{
public:
    SampleImpl();
    virtual ~SampleImpl();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsISample interface
    NS_IMETHOD GetValue(char * *aValue);
    NS_IMETHOD SetValue(char * aValue);

    NS_IMETHOD WriteValue(const char *aPrefix);

    NS_IMETHOD Poke(const char* aValue);

private:
    char* mValue;
};

////////////////////////////////////////////////////////////////////////

nsresult
NS_NewSample(nsISample** aSample)
{
    NS_PRECONDITION(aSample != nsnull, "null ptr");
    if (! aSample)
        return NS_ERROR_NULL_POINTER;

    *aSample = new SampleImpl();
    if (! *aSample)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aSample);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////


SampleImpl::SampleImpl() : mValue(nsnull)
{
    NS_INIT_REFCNT();
    mValue = PL_strdup("initial value");
}


SampleImpl::~SampleImpl()
{
    if (mValue)
        PL_strfree(mValue);
}



NS_IMPL_ISUPPORTS(SampleImpl, nsISample::GetIID());


NS_IMETHODIMP
SampleImpl::GetValue(char** aValue)
{
    NS_PRECONDITION(aValue != nsnull, "null ptr");
    if (! aValue)
        return NS_ERROR_NULL_POINTER;

    if (mValue) {
        *aValue = (char*) nsAllocator::Alloc(PL_strlen(mValue) + 1);
        if (! *aValue)
            return NS_ERROR_NULL_POINTER;

        PL_strcpy(*aValue, mValue);
    }
    else {
        *aValue = nsnull;
    }
    return NS_OK;
}


NS_IMETHODIMP
SampleImpl::SetValue(char* aValue)
{
    NS_PRECONDITION(aValue != nsnull, "null ptr");
    if (! aValue)
        return NS_ERROR_NULL_POINTER;

    if (mValue) {
        PL_strfree(mValue);
    }

    mValue = PL_strdup(aValue);
    return NS_OK;
}

NS_IMETHODIMP
SampleImpl::Poke(const char* aValue)
{
    return SetValue((char*) aValue);
}

NS_IMETHODIMP
SampleImpl::WriteValue(const char* aPrefix)
{
    NS_PRECONDITION(aPrefix != nsnull, "null ptr");
    if (! aPrefix)
        return NS_ERROR_NULL_POINTER;

    printf("%s %s\n", aPrefix, mValue);
    return NS_OK;
}
