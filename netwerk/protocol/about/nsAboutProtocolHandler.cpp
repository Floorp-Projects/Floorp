/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#ifdef MOZ_IPC
#include "IPCMessageUtils.h"
#include "mozilla/net/NeckoMessageUtils.h"
#endif

#include "nsAboutProtocolHandler.h"
#include "nsIURI.h"
#include "nsIIOService.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIAboutModule.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsNetCID.h"
#include "nsAboutProtocolUtils.h"
#include "nsNetError.h"
#include "nsNetUtil.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsAutoPtr.h"
#include "nsIWritablePropertyBag2.h"

static NS_DEFINE_CID(kSimpleURICID,     NS_SIMPLEURI_CID);
static NS_DEFINE_CID(kNestedAboutURICID, NS_NESTEDABOUTURI_CID);

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS1(nsAboutProtocolHandler, nsIProtocolHandler)

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsAboutProtocolHandler::GetScheme(nsACString &result)
{
    result.AssignLiteral("about");
    return NS_OK;
}

NS_IMETHODIMP
nsAboutProtocolHandler::GetDefaultPort(PRInt32 *result)
{
    *result = -1;        // no port for about: URLs
    return NS_OK;
}

NS_IMETHODIMP
nsAboutProtocolHandler::GetProtocolFlags(PRUint32 *result)
{
    *result = URI_NORELATIVE | URI_NOAUTH | URI_DANGEROUS_TO_LOAD;
    return NS_OK;
}

NS_IMETHODIMP
nsAboutProtocolHandler::NewURI(const nsACString &aSpec,
                               const char *aCharset, // ignore charset info
                               nsIURI *aBaseURI,
                               nsIURI **result)
{
    *result = nsnull;
    nsresult rv;

    // Use a simple URI to parse out some stuff first
    nsCOMPtr<nsIURI> url = do_CreateInstance(kSimpleURICID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = url->SetSpec(aSpec);
    if (NS_FAILED(rv)) {
        return rv;
    }

    // Unfortunately, people create random about: URIs that don't correspond to
    // about: modules...  Since those URIs will never open a channel, might as
    // well consider them unsafe for better perf, and just in case.
    PRBool isSafe = PR_FALSE;
    
    nsCOMPtr<nsIAboutModule> aboutMod;
    rv = NS_GetAboutModule(url, getter_AddRefs(aboutMod));
    if (NS_SUCCEEDED(rv)) {
        // The standard return case
        PRUint32 flags;
        rv = aboutMod->GetURIFlags(url, &flags);
        NS_ENSURE_SUCCESS(rv, rv);

        isSafe =
            ((flags & nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT) != 0);
    }

    if (isSafe) {
        // We need to indicate that this baby is safe.  Use an inner URI that
        // no one but the security manager will see.  Make sure to preserve our
        // path, in case someone decides to hardcode checks for particular
        // about: URIs somewhere.
        nsCAutoString spec;
        rv = url->GetPath(spec);
        NS_ENSURE_SUCCESS(rv, rv);
        
        spec.Insert("moz-safe-about:", 0);

        nsCOMPtr<nsIURI> inner;
        rv = NS_NewURI(getter_AddRefs(inner), spec);
        NS_ENSURE_SUCCESS(rv, rv);

        nsSimpleNestedURI* outer = new nsNestedAboutURI(inner, aBaseURI);
        NS_ENSURE_TRUE(outer, NS_ERROR_OUT_OF_MEMORY);

        // Take a ref to it in the COMPtr we plan to return
        url = outer;

        rv = outer->SetSpec(aSpec);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    // We don't want to allow mutation, since it would allow safe and
    // unsafe URIs to change into each other...
    NS_TryToSetImmutable(url);
    url.swap(*result);
    return NS_OK;
}

NS_IMETHODIMP
nsAboutProtocolHandler::NewChannel(nsIURI* uri, nsIChannel* *result)
{
    NS_ENSURE_ARG_POINTER(uri);

    // about:what you ask?
    nsCOMPtr<nsIAboutModule> aboutMod;
    nsresult rv = NS_GetAboutModule(uri, getter_AddRefs(aboutMod));
    if (NS_SUCCEEDED(rv)) {
        // The standard return case:
        rv = aboutMod->NewChannel(uri, result);
        if (NS_SUCCEEDED(rv)) {
            nsRefPtr<nsNestedAboutURI> aboutURI;
            nsresult rv2 = uri->QueryInterface(kNestedAboutURICID,
                                               getter_AddRefs(aboutURI));
            if (NS_SUCCEEDED(rv2) && aboutURI->GetBaseURI()) {
                nsCOMPtr<nsIWritablePropertyBag2> writableBag =
                    do_QueryInterface(*result);
                if (writableBag) {
                    writableBag->
                        SetPropertyAsInterface(NS_LITERAL_STRING("baseURI"),
                                               aboutURI->GetBaseURI());
                }
            }
        }
        return rv;
    }

    // mumble...

    if (rv == NS_ERROR_FACTORY_NOT_REGISTERED) {
        // This looks like an about: we don't know about.  Convert
        // this to an invalid URI error.
        rv = NS_ERROR_MALFORMED_URI;
    }

    return rv;
}

NS_IMETHODIMP 
nsAboutProtocolHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Safe about protocol handler impl

NS_IMPL_ISUPPORTS1(nsSafeAboutProtocolHandler, nsIProtocolHandler)

// nsIProtocolHandler methods:

NS_IMETHODIMP
nsSafeAboutProtocolHandler::GetScheme(nsACString &result)
{
    result.AssignLiteral("moz-safe-about");
    return NS_OK;
}

NS_IMETHODIMP
nsSafeAboutProtocolHandler::GetDefaultPort(PRInt32 *result)
{
    *result = -1;        // no port for moz-safe-about: URLs
    return NS_OK;
}

NS_IMETHODIMP
nsSafeAboutProtocolHandler::GetProtocolFlags(PRUint32 *result)
{
    *result = URI_NORELATIVE | URI_NOAUTH | URI_LOADABLE_BY_ANYONE;
    return NS_OK;
}

NS_IMETHODIMP
nsSafeAboutProtocolHandler::NewURI(const nsACString &aSpec,
                                   const char *aCharset, // ignore charset info
                                   nsIURI *aBaseURI,
                                   nsIURI **result)
{
    nsresult rv;

    nsCOMPtr<nsIURI> url = do_CreateInstance(kSimpleURICID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = url->SetSpec(aSpec);
    if (NS_FAILED(rv)) {
        return rv;
    }

    NS_TryToSetImmutable(url);
    
    *result = nsnull;
    url.swap(*result);
    return rv;
}

NS_IMETHODIMP
nsSafeAboutProtocolHandler::NewChannel(nsIURI* uri, nsIChannel* *result)
{
    *result = nsnull;
    return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP 
nsSafeAboutProtocolHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
}

////////////////////////////////////////////////////////////
// nsNestedAboutURI implementation
NS_INTERFACE_MAP_BEGIN(nsNestedAboutURI)
  if (aIID.Equals(kNestedAboutURICID))
      foundInterface = static_cast<nsIURI*>(this);
  else
NS_INTERFACE_MAP_END_INHERITING(nsSimpleNestedURI)

// nsISerializable
NS_IMETHODIMP
nsNestedAboutURI::Read(nsIObjectInputStream* aStream)
{
    nsresult rv = nsSimpleNestedURI::Read(aStream);
    if (NS_FAILED(rv)) return rv;

    PRBool haveBase;
    rv = aStream->ReadBoolean(&haveBase);
    if (NS_FAILED(rv)) return rv;

    if (haveBase) {
        nsCOMPtr<nsISupports> supports;
        rv = aStream->ReadObject(PR_TRUE, getter_AddRefs(supports));
        if (NS_FAILED(rv)) return rv;

        mBaseURI = do_QueryInterface(supports, &rv);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsNestedAboutURI::Write(nsIObjectOutputStream* aStream)
{
    nsresult rv = nsSimpleNestedURI::Write(aStream);
    if (NS_FAILED(rv)) return rv;

    rv = aStream->WriteBoolean(mBaseURI != nsnull);
    if (NS_FAILED(rv)) return rv;

    if (mBaseURI) {
        // A previous iteration of this code wrote out mBaseURI as nsISupports
        // and then read it in as nsIURI, which is non-kosher when mBaseURI
        // implements more than just a single line of interfaces and the
        // canonical nsISupports* isn't the one a static_cast<> of mBaseURI
        // would produce.  For backwards compatibility with existing
        // serializations we continue to write mBaseURI as nsISupports but
        // switch to reading it as nsISupports, with a post-read QI to get to
        // nsIURI.
        rv = aStream->WriteCompoundObject(mBaseURI, NS_GET_IID(nsISupports),
                                          PR_TRUE);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

// nsIIPCSerializable
PRBool
nsNestedAboutURI::Read(const IPC::Message *aMsg, void **aIter)
{
#ifdef MOZ_IPC
    if (!nsSimpleNestedURI::Read(aMsg, aIter))
        return PR_FALSE;

    IPC::URI uri;
    if (!ReadParam(aMsg, aIter, &uri))
        return PR_FALSE;

    mBaseURI = uri;

    return PR_TRUE;
#endif
    return PR_FALSE;
}

void
nsNestedAboutURI::Write(IPC::Message *aMsg)
{
#ifdef MOZ_IPC
    nsSimpleNestedURI::Write(aMsg);

    IPC::URI uri(mBaseURI);
    WriteParam(aMsg, uri);
#endif
}

// nsSimpleURI
/* virtual */ nsSimpleURI*
nsNestedAboutURI::StartClone()
{
    // Sadly, we can't make use of nsSimpleNestedURI::StartClone here.
    NS_ENSURE_TRUE(mInnerURI, nsnull);

    nsCOMPtr<nsIURI> innerClone;
    nsresult rv = mInnerURI->Clone(getter_AddRefs(innerClone));
    if (NS_FAILED(rv)) {
        return nsnull;
    }

    nsNestedAboutURI* url = new nsNestedAboutURI(innerClone, mBaseURI);
    if (url) {
        url->SetMutable(PR_FALSE);
    }

    return url;
}

// nsIClassInfo
NS_IMETHODIMP
nsNestedAboutURI::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    *aClassIDNoAlloc = kNestedAboutURICID;
    return NS_OK;
}
