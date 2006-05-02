/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chak Nanga <chak@netscape.com>
 *   Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsViewSourceHandler.h"
#include "nsViewSourceChannel.h"
#include "nsNetUtil.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIProgrammingLanguage.h"

#define VIEW_SOURCE "view-source"

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS1(nsViewSourceHandler, nsIProtocolHandler)

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsViewSourceHandler::GetScheme(nsACString &result)
{
    result.AssignLiteral(VIEW_SOURCE);
    return NS_OK;
}

NS_IMETHODIMP
nsViewSourceHandler::GetDefaultPort(PRInt32 *result)
{
    *result = -1;
    return NS_OK;
}

NS_IMETHODIMP
nsViewSourceHandler::GetProtocolFlags(PRUint32 *result)
{
    *result = URI_NORELATIVE | URI_NOAUTH;
    return NS_OK;
}

NS_IMETHODIMP
nsViewSourceHandler::NewURI(const nsACString &aSpec,
                            const char *aCharset,
                            nsIURI *aBaseURI,
                            nsIURI **aResult)
{
    *aResult = nsnull;

    // Extract inner URL and normalize to ASCII.  This is done to properly
    // support IDN in cases like "view-source:http://www.szalagavató.hu/"

    PRInt32 colon = aSpec.FindChar(':');
    if (colon == kNotFound)
        return NS_ERROR_MALFORMED_URI;

    nsCOMPtr<nsIURI> innerURI;
    nsresult rv = NS_NewURI(getter_AddRefs(innerURI),
                            Substring(aSpec, colon + 1), aCharset);
    if (NS_FAILED(rv))
        return rv;

    NS_TryToSetImmutable(innerURI);

    nsCAutoString asciiSpec;
    rv = innerURI->GetAsciiSpec(asciiSpec);
    if (NS_FAILED(rv))
        return rv;

    // put back our scheme and construct a simple-uri wrapper

    asciiSpec.Insert(VIEW_SOURCE ":", 0);

    // We can't swap() from an nsRefPtr<nsViewSourceURI> to an nsIURI**, sadly.
    nsViewSourceURI* ourURI = new nsViewSourceURI(innerURI);
    nsCOMPtr<nsIURI> uri = ourURI;
    if (!uri)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = ourURI->SetSpec(asciiSpec);
    if (NS_FAILED(rv))
        return rv;

    // Make the URI immutable so it's impossible to get it out of sync
    // with mInnerURI.
    ourURI->SetMutable(PR_FALSE);

    uri.swap(*aResult);
    return rv;
}

NS_IMETHODIMP
nsViewSourceHandler::NewChannel(nsIURI* uri, nsIChannel* *result)
{
    NS_ENSURE_ARG_POINTER(uri);
    nsViewSourceChannel *channel = new nsViewSourceChannel();
    if (!channel)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(channel);

    nsresult rv = channel->Init(uri);
    if (NS_FAILED(rv)) {
        NS_RELEASE(channel);
        return rv;
    }

    *result = NS_STATIC_CAST(nsIViewSourceChannel*, channel);
    return NS_OK;
}

NS_IMETHODIMP 
nsViewSourceHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
}

///////////////////////////////////////////////////////////////
// nsViewSourceURI implementation

static NS_DEFINE_CID(kViewSourceURICID, NS_VIEWSOURCEURI_CID);

NS_IMPL_ISUPPORTS_INHERITED1(nsViewSourceURI, nsSimpleURI, nsINestedURI)

// nsISerializable

NS_IMETHODIMP
nsViewSourceURI::Read(nsIObjectInputStream* aStream)
{
    nsresult rv = nsSimpleURI::Read(aStream);
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(!mMutable, "How did that happen?");

    // Our mPath is going to be ASCII; see nsViewSourceHandler::NewURI.  So
    // just using NS_NewURI with no charset is ok.
    rv = NS_NewURI(getter_AddRefs(mInnerURI), mPath);
    if (NS_FAILED(rv)) return rv;

    NS_TryToSetImmutable(mInnerURI);

    return rv;
}

// nsINestedURI

NS_IMETHODIMP
nsViewSourceURI::GetInnerURI(nsIURI** uri)
{
    return NS_EnsureSafeToReturn(mInnerURI, uri);
}

NS_IMETHODIMP
nsViewSourceURI::GetInnermostURI(nsIURI** uri)
{
    return NS_ImplGetInnermostURI(this, uri);
}

// nsIURI overrides

NS_IMETHODIMP
nsViewSourceURI::Equals(nsIURI* other, PRBool *result)
{
    if (other) {
        PRBool correctScheme;
        nsresult rv = other->SchemeIs(VIEW_SOURCE, &correctScheme);
        NS_ENSURE_SUCCESS(rv, rv);

        if (correctScheme) {
            nsCOMPtr<nsINestedURI> nest = do_QueryInterface(other);
            if (nest) {
                nsCOMPtr<nsIURI> otherInner;
                rv = nest->GetInnerURI(getter_AddRefs(otherInner));
                NS_ENSURE_SUCCESS(rv, rv);

                return otherInner->Equals(mInnerURI, result);
            }
        }
    }

    *result = PR_FALSE;
    return NS_OK;
}

/* virtual */ nsSimpleURI*
nsViewSourceURI::StartClone()
{
    nsCOMPtr<nsIURI> innerClone;
    nsresult rv = mInnerURI->Clone(getter_AddRefs(innerClone));
    if (NS_FAILED(rv)) {
        return nsnull;
    }

    NS_TryToSetImmutable(innerClone);
    
    nsViewSourceURI* url = new nsViewSourceURI(innerClone);
    if (url) {
        url->SetMutable(PR_FALSE);
    }

    return url;
}

// nsIClassInfo overrides

NS_IMETHODIMP 
nsViewSourceURI::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    *aClassIDNoAlloc = kViewSourceURICID;
    return NS_OK;
}
