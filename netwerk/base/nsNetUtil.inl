/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNetUtil_inl
#define nsNetUtil_inl

#include "mozilla/Services.h"

#include "nsComponentManagerUtils.h"
#include "nsIBufferedStreams.h"
#include "nsIChannel.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsIFileURL.h"
#include "nsIHttpChannel.h"
#include "nsIInputStreamChannel.h"
#include "nsIIOService.h"
#include "nsINestedURI.h"
#include "nsINode.h"
#include "nsIProtocolHandler.h"
#include "nsIStandardURL.h"
#include "nsIStreamLoader.h"
#include "nsIURI.h"
#include "nsIURIWithPrincipal.h"
#include "nsIWritablePropertyBag2.h"
#include "nsNetCID.h"
#include "nsStringStream.h"

#ifdef MOZILLA_INTERNAL_API
// Don't allow functions that end up in nsNetUtil.cpp to be inlined out.
#define INLINE_IF_EXTERN MOZ_NEVER_INLINE
#else
// Make sure that functions included via nsNetUtil.h don't get multiply defined.
#define INLINE_IF_EXTERN MOZ_ALWAYS_INLINE
#endif

#ifdef MOZILLA_INTERNAL_API

INLINE_IF_EXTERN already_AddRefed<nsIIOService>
do_GetIOService(nsresult *error /* = 0 */)
{
    nsCOMPtr<nsIIOService> io = mozilla::services::GetIOService();
    if (error)
        *error = io ? NS_OK : NS_ERROR_FAILURE;
    return io.forget();
}

INLINE_IF_EXTERN already_AddRefed<nsINetUtil>
do_GetNetUtil(nsresult *error /* = 0 */)
{
    nsCOMPtr<nsIIOService> io = mozilla::services::GetIOService();
    nsCOMPtr<nsINetUtil> util;
    if (io)
        util = do_QueryInterface(io);

    if (error)
        *error = !!util ? NS_OK : NS_ERROR_FAILURE;
    return util.forget();
}

#else

INLINE_IF_EXTERN const nsGetServiceByContractIDWithError
do_GetIOService(nsresult *error /* = 0 */)
{
    return nsGetServiceByContractIDWithError(NS_IOSERVICE_CONTRACTID, error);
}

INLINE_IF_EXTERN const nsGetServiceByContractIDWithError
do_GetNetUtil(nsresult *error /* = 0 */)
{
    return do_GetIOService(error);
}
#endif

// private little helper function... don't call this directly!
MOZ_ALWAYS_INLINE nsresult
net_EnsureIOService(nsIIOService **ios, nsCOMPtr<nsIIOService> &grip)
{
    nsresult rv = NS_OK;
    if (!*ios) {
        grip = do_GetIOService(&rv);
        *ios = grip;
    }
    return rv;
}

INLINE_IF_EXTERN nsresult
NS_NewURI(nsIURI **result,
          const nsACString &spec,
          const char *charset /* = nullptr */,
          nsIURI *baseURI /* = nullptr */,
          nsIIOService *ioService /* = nullptr */)     // pass in nsIIOService to optimize callers
{
    nsresult rv;
    nsCOMPtr<nsIIOService> grip;
    rv = net_EnsureIOService(&ioService, grip);
    if (ioService)
        rv = ioService->NewURI(spec, charset, baseURI, result);
    return rv;
}

INLINE_IF_EXTERN nsresult
NS_NewURI(nsIURI **result,
          const nsAString &spec,
          const char *charset /* = nullptr */,
          nsIURI *baseURI /* = nullptr */,
          nsIIOService *ioService /* = nullptr */)     // pass in nsIIOService to optimize callers
{
    return NS_NewURI(result, NS_ConvertUTF16toUTF8(spec), charset, baseURI, ioService);
}

INLINE_IF_EXTERN nsresult
NS_NewURI(nsIURI **result,
          const char *spec,
          nsIURI *baseURI /* = nullptr */,
          nsIIOService *ioService /* = nullptr */)     // pass in nsIIOService to optimize callers
{
    return NS_NewURI(result, nsDependentCString(spec), nullptr, baseURI, ioService);
}

INLINE_IF_EXTERN nsresult
NS_NewChannelInternal(nsIChannel           **outChannel,
                      nsIURI                *aUri,
                      nsINode               *aLoadingNode,
                      nsIPrincipal          *aLoadingPrincipal,
                      nsIPrincipal          *aTriggeringPrincipal,
                      nsSecurityFlags        aSecurityFlags,
                      nsContentPolicyType    aContentPolicyType,
                      nsILoadGroup          *aLoadGroup /* = nullptr */,
                      nsIInterfaceRequestor *aCallbacks /* = nullptr */,
                      nsLoadFlags            aLoadFlags /* = nsIRequest::LOAD_NORMAL */,
                      nsIIOService          *aIoService /* = nullptr */)
{
  NS_ENSURE_ARG_POINTER(outChannel);

  nsCOMPtr<nsIIOService> grip;
  nsresult rv = net_EnsureIOService(&aIoService, grip);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> channel;
  rv = aIoService->NewChannelFromURI2(
         aUri,
         aLoadingNode ?
           aLoadingNode->AsDOMNode() : nullptr,
         aLoadingPrincipal,
         aTriggeringPrincipal,
         aSecurityFlags,
         aContentPolicyType,
         getter_AddRefs(channel));
  NS_ENSURE_SUCCESS(rv, rv);

  if (aLoadGroup) {
    rv = channel->SetLoadGroup(aLoadGroup);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aCallbacks) {
    rv = channel->SetNotificationCallbacks(aCallbacks);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aLoadFlags != nsIRequest::LOAD_NORMAL) {
    // Retain the LOAD_REPLACE load flag if set.
    nsLoadFlags normalLoadFlags = 0;
    channel->GetLoadFlags(&normalLoadFlags);
    rv = channel->SetLoadFlags(aLoadFlags | (normalLoadFlags & nsIChannel::LOAD_REPLACE));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  channel.forget(outChannel);
  return NS_OK;
}

INLINE_IF_EXTERN nsresult
NS_NewChannelInternal(nsIChannel           **outChannel,
                      nsIURI                *aUri,
                      nsILoadInfo           *aLoadInfo,
                      nsILoadGroup          *aLoadGroup /* = nullptr */,
                      nsIInterfaceRequestor *aCallbacks /* = nullptr */,
                      nsLoadFlags            aLoadFlags /* = nsIRequest::LOAD_NORMAL */,
                      nsIIOService          *aIoService /* = nullptr */)
{
  // NS_NewChannelInternal is mostly called for channel redirects. We should allow
  // the creation of a channel even if the original channel did not have a loadinfo
  // attached.
  if (!aLoadInfo) {
    return NS_NewChannelInternal(outChannel,
                                 aUri,
                                 nullptr, // aLoadingNode
                                 nullptr, // aLoadingPrincipal
                                 nullptr, // aTriggeringPrincipal
                                 nsILoadInfo::SEC_NORMAL,
                                 nsIContentPolicy::TYPE_OTHER,
                                 aLoadGroup,
                                 aCallbacks,
                                 aLoadFlags,
                                 aIoService);
  }
  nsresult rv = NS_NewChannelInternal(outChannel,
                                      aUri,
                                      aLoadInfo->LoadingNode(),
                                      aLoadInfo->LoadingPrincipal(),
                                      aLoadInfo->TriggeringPrincipal(),
                                      aLoadInfo->GetSecurityFlags(),
                                      aLoadInfo->GetContentPolicyType(),
                                      aLoadGroup,
                                      aCallbacks,
                                      aLoadFlags,
                                      aIoService);
  NS_ENSURE_SUCCESS(rv, rv);
  // Please note that we still call SetLoadInfo on the channel because
  // we want the same instance of the loadInfo to be set on the channel.
  (*outChannel)->SetLoadInfo(aLoadInfo);
  return NS_OK;
}

INLINE_IF_EXTERN nsresult /* NS_NewChannelPrincipal */
NS_NewChannel(nsIChannel           **outChannel,
              nsIURI                *aUri,
              nsIPrincipal          *aLoadingPrincipal,
              nsSecurityFlags        aSecurityFlags,
              nsContentPolicyType    aContentPolicyType,
              nsILoadGroup          *aLoadGroup /* = nullptr */,
              nsIInterfaceRequestor *aCallbacks /* = nullptr */,
              nsLoadFlags            aLoadFlags /* = nsIRequest::LOAD_NORMAL */,
              nsIIOService          *aIoService /* = nullptr */)
{
  return NS_NewChannelInternal(outChannel,
                               aUri,
                               nullptr, // aLoadingNode,
                               aLoadingPrincipal,
                               nullptr, // aTriggeringPrincipal
                               aSecurityFlags,
                               aContentPolicyType,
                               aLoadGroup,
                               aCallbacks,
                               aLoadFlags,
                               aIoService);
}

INLINE_IF_EXTERN nsresult
NS_NewStreamLoader(nsIStreamLoader        **result,
                   nsIStreamLoaderObserver *observer,
                   nsIRequestObserver      *requestObserver /* = nullptr */)
{
    nsresult rv;
    nsCOMPtr<nsIStreamLoader> loader =
        do_CreateInstance(NS_STREAMLOADER_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = loader->Init(observer, requestObserver);
        if (NS_SUCCEEDED(rv)) {
            *result = nullptr;
            loader.swap(*result);
        }
    }
    return rv;
}

INLINE_IF_EXTERN nsresult
NS_NewLocalFileInputStream(nsIInputStream **result,
                           nsIFile         *file,
                           int32_t          ioFlags       /* = -1 */,
                           int32_t          perm          /* = -1 */,
                           int32_t          behaviorFlags /* = 0 */)
{
    nsresult rv;
    nsCOMPtr<nsIFileInputStream> in =
        do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = in->Init(file, ioFlags, perm, behaviorFlags);
        if (NS_SUCCEEDED(rv))
            in.forget(result);
    }
    return rv;
}

INLINE_IF_EXTERN nsresult
NS_NewLocalFileOutputStream(nsIOutputStream **result,
                            nsIFile          *file,
                            int32_t           ioFlags       /* = -1 */,
                            int32_t           perm          /* = -1 */,
                            int32_t           behaviorFlags /* = 0 */)
{
    nsresult rv;
    nsCOMPtr<nsIFileOutputStream> out =
        do_CreateInstance(NS_LOCALFILEOUTPUTSTREAM_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = out->Init(file, ioFlags, perm, behaviorFlags);
        if (NS_SUCCEEDED(rv))
            out.forget(result);
    }
    return rv;
}

INLINE_IF_EXTERN MOZ_WARN_UNUSED_RESULT nsresult
NS_NewBufferedInputStream(nsIInputStream **result,
                          nsIInputStream  *str,
                          uint32_t         bufferSize)
{
    nsresult rv;
    nsCOMPtr<nsIBufferedInputStream> in =
        do_CreateInstance(NS_BUFFEREDINPUTSTREAM_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = in->Init(str, bufferSize);
        if (NS_SUCCEEDED(rv)) {
            in.forget(result);
        }
    }
    return rv;
}

INLINE_IF_EXTERN nsresult
NS_NewPostDataStream(nsIInputStream  **result,
                     bool              isFile,
                     const nsACString &data)
{
    nsresult rv;

    if (isFile) {
        nsCOMPtr<nsIFile> file;
        nsCOMPtr<nsIInputStream> fileStream;

        rv = NS_NewNativeLocalFile(data, false, getter_AddRefs(file));
        if (NS_SUCCEEDED(rv)) {
            rv = NS_NewLocalFileInputStream(getter_AddRefs(fileStream), file);
            if (NS_SUCCEEDED(rv)) {
                // wrap the file stream with a buffered input stream
                rv = NS_NewBufferedInputStream(result, fileStream, 8192);
            }
        }
        return rv;
    }

    // otherwise, create a string stream for the data (copies)
    nsCOMPtr<nsIStringInputStream> stream
        (do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv));
    if (NS_FAILED(rv))
        return rv;

    rv = stream->SetData(data.BeginReading(), data.Length());
    if (NS_FAILED(rv))
        return rv;

    stream.forget(result);
    return NS_OK;
}

#endif // nsNetUtil_inl
