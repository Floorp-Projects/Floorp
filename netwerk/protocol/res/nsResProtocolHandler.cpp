/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/chrome/RegistryMessageUtils.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/unused.h"

#include "nsResProtocolHandler.h"
#include "nsIIOService.h"
#include "nsIFile.h"
#include "nsNetUtil.h"
#include "nsURLHelper.h"
#include "nsEscape.h"

#include "mozilla/Omnijar.h"

using mozilla::dom::ContentParent;
using mozilla::LogLevel;
using mozilla::unused;

static NS_DEFINE_CID(kResURLCID, NS_RESURL_CID);

static nsResProtocolHandler *gResHandler = nullptr;

//
// Log module for Resource Protocol logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsResProtocol:5
//    set NSPR_LOG_FILE=log.txt
//
// this enables LogLevel::Debug level information and places all output in
// the file log.txt
//
static PRLogModuleInfo *gResLog;

#define kAPP           NS_LITERAL_CSTRING("app")
#define kGRE           NS_LITERAL_CSTRING("gre")

//----------------------------------------------------------------------------
// nsResURL : overrides nsStandardURL::GetFile to provide nsIFile resolution
//----------------------------------------------------------------------------

nsresult
nsResURL::EnsureFile()
{
    nsresult rv;

    NS_ENSURE_TRUE(gResHandler, NS_ERROR_NOT_AVAILABLE);

    nsAutoCString spec;
    rv = gResHandler->ResolveURI(this, spec);
    if (NS_FAILED(rv))
        return rv;

    nsAutoCString scheme;
    rv = net_ExtractURLScheme(spec, nullptr, nullptr, &scheme);
    if (NS_FAILED(rv))
        return rv;

    // Bug 585869:
    // In most cases, the scheme is jar if it's not file.
    // Regardless, net_GetFileFromURLSpec should be avoided
    // when the scheme isn't file.
    if (!scheme.EqualsLiteral("file"))
        return NS_ERROR_NO_INTERFACE;

    rv = net_GetFileFromURLSpec(spec, getter_AddRefs(mFile));
#ifdef DEBUG_bsmedberg
    if (NS_SUCCEEDED(rv)) {
        bool exists = true;
        mFile->Exists(&exists);
        if (!exists) {
            printf("resource %s doesn't exist!\n", spec.get());
        }
    }
#endif

    return rv;
}

/* virtual */ nsStandardURL*
nsResURL::StartClone()
{
    nsResURL *clone = new nsResURL();
    return clone;
}

NS_IMETHODIMP 
nsResURL::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    *aClassIDNoAlloc = kResURLCID;
    return NS_OK;
}

//----------------------------------------------------------------------------
// nsResProtocolHandler <public>
//----------------------------------------------------------------------------

nsResProtocolHandler::nsResProtocolHandler()
    : mSubstitutions(16)
{
    gResLog = PR_NewLogModule("nsResProtocol");

    NS_ASSERTION(!gResHandler, "res handler already created!");
    gResHandler = this;
}

nsResProtocolHandler::~nsResProtocolHandler()
{
    gResHandler = nullptr;
}

nsresult
nsResProtocolHandler::Init()
{
    nsresult rv;

    mIOService = do_GetIOService(&rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString appURI, greURI;
    rv = mozilla::Omnijar::GetURIString(mozilla::Omnijar::APP, appURI);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mozilla::Omnijar::GetURIString(mozilla::Omnijar::GRE, greURI);
    NS_ENSURE_SUCCESS(rv, rv);

    //
    // make resource:/// point to the application directory or omnijar
    //
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), appURI.Length() ? appURI : greURI);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = SetSubstitution(EmptyCString(), uri);
    NS_ENSURE_SUCCESS(rv, rv);

    //
    // make resource://app/ point to the application directory or omnijar
    //
    rv = SetSubstitution(kAPP, uri);
    NS_ENSURE_SUCCESS(rv, rv);

    //
    // make resource://gre/ point to the GRE directory
    //
    if (appURI.Length()) { // We already have greURI in uri if appURI.Length() is 0.
        rv = NS_NewURI(getter_AddRefs(uri), greURI);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = SetSubstitution(kGRE, uri);
    NS_ENSURE_SUCCESS(rv, rv);

    //XXXbsmedberg Neil wants a resource://pchrome/ for the profile chrome dir...
    // but once I finish multiple chrome registration I'm not sure that it is needed

    // XXX dveditz: resource://pchrome/ defeats profile directory salting
    // if web content can load it. Tread carefully.

    return rv;
}

static PLDHashOperator
EnumerateSubstitution(const nsACString& aKey,
                      nsIURI* aURI,
                      void* aArg)
{
    nsTArray<ResourceMapping>* resources =
            static_cast<nsTArray<ResourceMapping>*>(aArg);
    SerializedURI uri;
    if (aURI) {
        aURI->GetSpec(uri.spec);
        aURI->GetOriginCharset(uri.charset);
    }

    ResourceMapping resource = {
        nsCString(aKey), uri
    };
    resources->AppendElement(resource);
    return (PLDHashOperator)PL_DHASH_NEXT;
}

void
nsResProtocolHandler::CollectSubstitutions(InfallibleTArray<ResourceMapping>& aResources)
{
    mSubstitutions.EnumerateRead(&EnumerateSubstitution, &aResources);
}

//----------------------------------------------------------------------------
// nsResProtocolHandler::nsISupports
//----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsResProtocolHandler,
                  nsIResProtocolHandler,
                  nsIProtocolHandler,
                  nsISupportsWeakReference)

//----------------------------------------------------------------------------
// nsResProtocolHandler::nsIProtocolHandler
//----------------------------------------------------------------------------

NS_IMETHODIMP
nsResProtocolHandler::GetScheme(nsACString &result)
{
    result.AssignLiteral("resource");
    return NS_OK;
}

NS_IMETHODIMP
nsResProtocolHandler::GetDefaultPort(int32_t *result)
{
    *result = -1;        // no port for res: URLs
    return NS_OK;
}

NS_IMETHODIMP
nsResProtocolHandler::GetProtocolFlags(uint32_t *result)
{
    // XXXbz Is this really true for all resource: URIs?  Could we
    // somehow give different flags to some of them?
    *result = URI_STD | URI_IS_UI_RESOURCE | URI_IS_LOCAL_RESOURCE;
    return NS_OK;
}

NS_IMETHODIMP
nsResProtocolHandler::NewURI(const nsACString &aSpec,
                             const char *aCharset,
                             nsIURI *aBaseURI,
                             nsIURI **result)
{
    nsresult rv;

    nsRefPtr<nsResURL> resURL = new nsResURL();
    if (!resURL)
        return NS_ERROR_OUT_OF_MEMORY;

    // unescape any %2f and %2e to make sure nsStandardURL coalesces them.
    // Later net_GetFileFromURLSpec() will do a full unescape and we want to
    // treat them the same way the file system will. (bugs 380994, 394075)
    nsAutoCString spec;
    const char *src = aSpec.BeginReading();
    const char *end = aSpec.EndReading();
    const char *last = src;

    spec.SetCapacity(aSpec.Length()+1);
    for ( ; src < end; ++src) {
        if (*src == '%' && (src < end-2) && *(src+1) == '2') {
           char ch = '\0';
           if (*(src+2) == 'f' || *(src+2) == 'F')
             ch = '/';
           else if (*(src+2) == 'e' || *(src+2) == 'E')
             ch = '.';

           if (ch) {
             if (last < src)
               spec.Append(last, src-last);
             spec.Append(ch);
             src += 2;
             last = src+1; // src will be incremented by the loop
           }
        }
    }
    if (last < src)
      spec.Append(last, src-last);

    rv = resURL->Init(nsIStandardURL::URLTYPE_STANDARD, -1, spec, aCharset, aBaseURI);
    if (NS_SUCCEEDED(rv)) {
        resURL.forget(result);
    }
    return rv;
}

NS_IMETHODIMP
nsResProtocolHandler::NewChannel2(nsIURI* uri,
                                  nsILoadInfo* aLoadInfo,
                                  nsIChannel** result)
{
    NS_ENSURE_ARG_POINTER(uri);
    nsAutoCString spec;
    nsresult rv = ResolveURI(uri, spec);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> newURI;
    rv = NS_NewURI(getter_AddRefs(newURI), spec);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_NewChannelInternal(result,
                               newURI,
                               aLoadInfo);
    NS_ENSURE_SUCCESS(rv, rv);

    nsLoadFlags loadFlags = 0;
    (*result)->GetLoadFlags(&loadFlags);
    (*result)->SetLoadFlags(loadFlags & ~nsIChannel::LOAD_REPLACE);
    return (*result)->SetOriginalURI(uri);
}

NS_IMETHODIMP
nsResProtocolHandler::NewChannel(nsIURI* uri, nsIChannel* *result)
{
    return NewChannel2(uri, nullptr, result);
}

NS_IMETHODIMP 
nsResProtocolHandler::AllowPort(int32_t port, const char *scheme, bool *_retval)
{
    // don't override anything.  
    *_retval = false;
    return NS_OK;
}

//----------------------------------------------------------------------------
// nsResProtocolHandler::nsIResProtocolHandler
//----------------------------------------------------------------------------

static void
SendResourceSubstitution(const nsACString& root, nsIURI* baseURI)
{
    if (GeckoProcessType_Content == XRE_GetProcessType()) {
        return;
    }

    ResourceMapping resourceMapping;
    resourceMapping.resource = root;
    if (baseURI) {
        baseURI->GetSpec(resourceMapping.resolvedURI.spec);
        baseURI->GetOriginCharset(resourceMapping.resolvedURI.charset);
    }

    nsTArray<ContentParent*> parents;
    ContentParent::GetAll(parents);
    if (!parents.Length()) {
        return;
    }

    for (uint32_t i = 0; i < parents.Length(); i++) {
        unused << parents[i]->SendRegisterChromeItem(resourceMapping);
    }
}

NS_IMETHODIMP
nsResProtocolHandler::SetSubstitution(const nsACString& root, nsIURI *baseURI)
{
    if (!baseURI) {
        mSubstitutions.Remove(root);
        SendResourceSubstitution(root, baseURI);
        return NS_OK;
    }

    // If baseURI isn't a resource URI, we can set the substitution immediately.
    nsAutoCString scheme;
    nsresult rv = baseURI->GetScheme(scheme);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!scheme.EqualsLiteral("resource")) {
        mSubstitutions.Put(root, baseURI);
        SendResourceSubstitution(root, baseURI);
        return NS_OK;
    }

    // baseURI is a resource URI, let's resolve it first.
    nsAutoCString newBase;
    rv = ResolveURI(baseURI, newBase);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> newBaseURI;
    rv = mIOService->NewURI(newBase, nullptr, nullptr,
                            getter_AddRefs(newBaseURI));
    NS_ENSURE_SUCCESS(rv, rv);

    mSubstitutions.Put(root, newBaseURI);
    SendResourceSubstitution(root, newBaseURI);
    return NS_OK;
}

NS_IMETHODIMP
nsResProtocolHandler::GetSubstitution(const nsACString& root, nsIURI **result)
{
    NS_ENSURE_ARG_POINTER(result);

    if (mSubstitutions.Get(root, result))
        return NS_OK;

    // try invoking the directory service for "resource:root"

    nsAutoCString key;
    key.AssignLiteral("resource:");
    key.Append(root);

    nsCOMPtr<nsIFile> file;
    nsresult rv = NS_GetSpecialDirectory(key.get(), getter_AddRefs(file));
    if (NS_FAILED(rv))
        return NS_ERROR_NOT_AVAILABLE;
        
    rv = mIOService->NewFileURI(file, result);
    if (NS_FAILED(rv))
        return NS_ERROR_NOT_AVAILABLE;

    return NS_OK;
}

NS_IMETHODIMP
nsResProtocolHandler::HasSubstitution(const nsACString& root, bool *result)
{
    NS_ENSURE_ARG_POINTER(result);

    *result = mSubstitutions.Get(root, nullptr);
    return NS_OK;
}

NS_IMETHODIMP
nsResProtocolHandler::ResolveURI(nsIURI *uri, nsACString &result)
{
    nsresult rv;

    nsAutoCString host;
    nsAutoCString path;

    rv = uri->GetAsciiHost(host);
    if (NS_FAILED(rv)) return rv;

    rv = uri->GetPath(path);
    if (NS_FAILED(rv)) return rv;

    // Unescape the path so we can perform some checks on it.
    nsAutoCString unescapedPath(path);
    NS_UnescapeURL(unescapedPath);

    // Don't misinterpret the filepath as an absolute URI.
    if (unescapedPath.FindChar(':') != -1)
        return NS_ERROR_MALFORMED_URI;

    if (unescapedPath.FindChar('\\') != -1)
        return NS_ERROR_MALFORMED_URI;

    const char *p = path.get() + 1; // path always starts with a slash
    NS_ASSERTION(*(p-1) == '/', "Path did not begin with a slash!");

    if (*p == '/')
        return NS_ERROR_MALFORMED_URI;

    nsCOMPtr<nsIURI> baseURI;
    rv = GetSubstitution(host, getter_AddRefs(baseURI));
    if (NS_FAILED(rv)) return rv;

    rv = baseURI->Resolve(nsDependentCString(p, path.Length()-1), result);

    if (MOZ_LOG_TEST(gResLog, LogLevel::Debug)) {
        nsAutoCString spec;
        uri->GetAsciiSpec(spec);
        MOZ_LOG(gResLog, LogLevel::Debug,
               ("%s\n -> %s\n", spec.get(), PromiseFlatCString(result).get()));
    }
    return rv;
}
