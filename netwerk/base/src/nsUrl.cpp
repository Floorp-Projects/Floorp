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

#include "nsUrl.h"

////////////////////////////////////////////////////////////////////////////////
// nsUrl methods:

nsUrl::nsUrl()
{
    NS_INIT_REFCNT();
}

nsUrl::~nsUrl()
{
}

nsresult
nsUrl::Init(const char* aSpec,
            const nsIUrl* aBaseURL,
            nsISupports* aContainer)
{
    return NS_OK;
}

NS_IMPL_ISUPPORTS(nsUrl, nsIUrl::GetIID());

////////////////////////////////////////////////////////////////////////////////
// nsIUrl methods:

NS_IMETHODIMP
nsUrl::GetScheme(const char* *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsUrl::SetScheme(const char* scheme)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsUrl::GetPreHost(const char* *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsUrl::SetPreHost(const char* preHost)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsUrl::GetHost(const char* *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsUrl::SetHost(const char* host)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsUrl::GetPort(PRInt32 *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsUrl::SetPort(PRInt32 port)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsUrl::GetPath(const char* *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsUrl::SetPath(const char* path)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsUrl::Equals(const nsIUrl* other)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsUrl::ToNewCString(const char* *uriString)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
