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

#include "nsJARURI.h"
#include "nsNetUtil.h"
#include "nsIIOService.h"
#include "nsFileSpec.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIZipReader.h"
#include "nsReadableUtils.h"
#include "nsURLHelper.h"

////////////////////////////////////////////////////////////////////////////////
 
nsJARURI::nsJARURI()
    : mJAREntry(nsnull)
{
}
 
nsJARURI::~nsJARURI()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS3(nsJARURI, nsIJARURI, nsIURI, nsISerializable)

nsresult
nsJARURI::Init(const char *charsetHint)
{
    mCharsetHint = charsetHint;
    return NS_OK;
}

#define NS_JAR_SCHEME           NS_LITERAL_CSTRING("jar:")
#define NS_JAR_DELIMITER        NS_LITERAL_CSTRING("!/")

nsresult
nsJARURI::FormatSpec(const nsACString &entryPath, nsACString &result)
{
    nsCAutoString fileSpec;
    nsresult rv = mJARFile->GetSpec(fileSpec);
    if (NS_FAILED(rv)) return rv;

    result = NS_JAR_SCHEME + fileSpec + NS_JAR_DELIMITER + entryPath;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsISerializable methods:

NS_IMETHODIMP
nsJARURI::Read(nsIObjectInputStream* aStream)
{
    NS_NOTREACHED("nsJARURI::Read");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJARURI::Write(nsIObjectOutputStream* aStream)
{
    NS_NOTREACHED("nsJARURI::Write");
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsIURI methods:

NS_IMETHODIMP
nsJARURI::GetSpec(nsACString &aSpec)
{
    return FormatSpec(mJAREntry, aSpec);
}

NS_IMETHODIMP
nsJARURI::SetSpec(const nsACString &aSpec)
{
    nsresult rv;
    nsCOMPtr<nsIIOService> ioServ(do_GetIOService(&rv));
    if (NS_FAILED(rv)) return rv;

    nsCAutoString scheme;
    rv = net_ExtractURLScheme(aSpec, nsnull, nsnull, &scheme);
    if (NS_FAILED(rv)) return rv;

    if (strcmp("jar", scheme.get()) != 0)
        return NS_ERROR_MALFORMED_URI;

    // Search backward from the end for the "!/" delimiter. Remember, jar URLs
    // can nest, e.g.:
    //    jar:jar:http://www.foo.com/bar.jar!/a.jar!/b.html
    // This gets the b.html document from out of the a.jar file, that's 
    // contained within the bar.jar file.

    nsACString::const_iterator begin, end, delim_begin, delim_end;
    aSpec.BeginReading(begin);
    aSpec.EndReading(end);

    delim_begin = begin;
    delim_end = end;

    if (!RFindInReadable(NS_JAR_DELIMITER, delim_begin, delim_end))
        return NS_ERROR_MALFORMED_URI;

    begin.advance(4);

    rv = ioServ->NewURI(Substring(begin, delim_begin), mCharsetHint.get(), nsnull, getter_AddRefs(mJARFile));
    if (NS_FAILED(rv)) return rv;

    // skip over any extra '/' chars
    while (*delim_end == '/')
        ++delim_end;

    rv = net_ResolveRelativePath(Substring(delim_end, end),
                                           NS_LITERAL_CSTRING(""),
                                           mJAREntry);
    return rv;
}

NS_IMETHODIMP
nsJARURI::GetPrePath(nsACString &prePath)
{
    prePath = "jar:";
    return NS_OK;
}

NS_IMETHODIMP
nsJARURI::GetScheme(nsACString &aScheme)
{
    aScheme = "jar";
    return NS_OK;
}

NS_IMETHODIMP
nsJARURI::SetScheme(const nsACString &aScheme)
{
    // doesn't make sense to set the scheme of a jar: URL
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJARURI::GetUserPass(nsACString &aUserPass)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJARURI::SetUserPass(const nsACString &aUserPass)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJARURI::GetUsername(nsACString &aUsername)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJARURI::SetUsername(const nsACString &aUsername)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJARURI::GetPassword(nsACString &aPassword)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJARURI::SetPassword(const nsACString &aPassword)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJARURI::GetHostPort(nsACString &aHostPort)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJARURI::SetHostPort(const nsACString &aHostPort)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJARURI::GetHost(nsACString &aHost)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJARURI::SetHost(const nsACString &aHost)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJARURI::GetPort(PRInt32 *aPort)
{
    return NS_ERROR_FAILURE;
}
 
NS_IMETHODIMP
nsJARURI::SetPort(PRInt32 aPort)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJARURI::GetPath(nsACString &aPath)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJARURI::SetPath(const nsACString &aPath)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJARURI::GetAsciiSpec(nsACString &aSpec)
{
    return GetSpec(aSpec);
}

NS_IMETHODIMP
nsJARURI::GetAsciiHost(nsACString &aHost)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJARURI::GetOriginCharset(nsACString &aOriginCharset)
{
    aOriginCharset.Truncate();
    return NS_OK;
}

NS_IMETHODIMP
nsJARURI::Equals(nsIURI *other, PRBool *result)
{
    nsresult rv;
    *result = PR_FALSE;

    if (other == nsnull)
        return NS_OK;	// not equal

    nsCOMPtr<nsIJARURI> otherJAR(do_QueryInterface(other, &rv));
    if (NS_FAILED(rv))
        return NS_OK;   // not equal

    nsCOMPtr<nsIURI> otherJARFile;
    rv = otherJAR->GetJARFile(getter_AddRefs(otherJARFile));
    if (NS_FAILED(rv)) return rv;

    PRBool equal;
    rv = mJARFile->Equals(otherJARFile, &equal);
    if (NS_FAILED(rv)) return rv;
    if (!equal)
        return NS_OK;   // not equal

    nsCAutoString otherJAREntry;
    rv = otherJAR->GetJAREntry(otherJAREntry);
    if (NS_FAILED(rv)) return rv;

    *result = (strcmp(mJAREntry.get(), otherJAREntry.get()) == 0);
    return NS_OK;
}

NS_IMETHODIMP
nsJARURI::SchemeIs(const char *i_Scheme, PRBool *o_Equals)
{
    NS_ENSURE_ARG_POINTER(o_Equals);
    if (!i_Scheme) return NS_ERROR_INVALID_ARG;

    if (*i_Scheme == 'j' || *i_Scheme == 'J') {
        *o_Equals = PL_strcasecmp("jar", i_Scheme) ? PR_FALSE : PR_TRUE;
    } else {
        *o_Equals = PR_FALSE;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsJARURI::Clone(nsIURI **result)
{
    nsresult rv;

    nsCOMPtr<nsIURI> newJARFile;
    rv = mJARFile->Clone(getter_AddRefs(newJARFile));
    if (NS_FAILED(rv)) return rv;

    nsJARURI* uri = new nsJARURI();
    if (uri == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(uri);
    uri->mJARFile = newJARFile;
    uri->mJAREntry = mJAREntry;
    *result = uri;

    return NS_OK;
}

NS_IMETHODIMP
nsJARURI::Resolve(const nsACString &relativePath, nsACString &result)
{
    nsresult rv;

    nsCAutoString scheme;
    rv = net_ExtractURLScheme(relativePath, nsnull, nsnull, &scheme);
    if (NS_SUCCEEDED(rv)) {
        // then aSpec is absolute
        result = relativePath;
        return NS_OK;
    }

    nsCAutoString path(mJAREntry);
    PRInt32 pos = path.RFind("/");
    if (pos >= 0)
        path.Truncate(pos + 1);
    else
        path = "";

    nsCAutoString resolvedEntry;
    rv = net_ResolveRelativePath(relativePath, path,
                                 resolvedEntry);
    if (NS_FAILED(rv)) return rv;

    return FormatSpec(resolvedEntry, result);
}

////////////////////////////////////////////////////////////////////////////////
// nsIJARURI methods:

NS_IMETHODIMP
nsJARURI::GetJARFile(nsIURI* *jarFile)
{
    *jarFile = mJARFile;
    NS_ADDREF(*jarFile);
    return NS_OK;
}

NS_IMETHODIMP
nsJARURI::SetJARFile(nsIURI* jarFile)
{
    mJARFile = jarFile;
    return NS_OK;
}

NS_IMETHODIMP
nsJARURI::GetJAREntry(nsACString &entryPath)
{
    // trim off any trailing ref, query, or param
    PRInt32 pos = mJAREntry.RFindCharInSet("#?;");
    if (pos < 0)
        pos = mJAREntry.Length();
    entryPath = Substring(mJAREntry, 0, pos);
    return NS_OK;
}

NS_IMETHODIMP
nsJARURI::SetJAREntry(const nsACString &entryPath)
{
    mJAREntry.Truncate();

    return net_ResolveRelativePath(entryPath,
                                   NS_LITERAL_CSTRING(""),
                                   mJAREntry);
}

////////////////////////////////////////////////////////////////////////////////
