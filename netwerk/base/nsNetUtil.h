/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNetUtil_h__
#define nsNetUtil_h__

#include "mozilla/Maybe.h"
#include "nsCOMPtr.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILoadGroup.h"
#include "nsINetUtil.h"
#include "nsIRequest.h"
#include "nsILoadInfo.h"
#include "nsIIOService.h"
#include "mozilla/NotNull.h"
#include "mozilla/Services.h"
#include "mozilla/Unused.h"
#include "nsNetCID.h"
#include "nsReadableUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"

class nsIURI;
class nsIPrincipal;
class nsIAsyncStreamCopier;
class nsIAuthPrompt;
class nsIAuthPrompt2;
class nsIChannel;
class nsIChannelPolicy;
class nsIDownloadObserver;
class nsIEventTarget;
class nsIFileProtocolHandler;
class nsIFileStream;
class nsIInputStream;
class nsIInputStreamPump;
class nsIInterfaceRequestor;
class nsINestedURI;
class nsIOutputStream;
class nsIParentChannel;
class nsIPersistentProperties;
class nsIProxyInfo;
class nsIRequestObserver;
class nsIStreamListener;
class nsIStreamLoader;
class nsIStreamLoaderObserver;
class nsIIncrementalStreamLoader;
class nsIIncrementalStreamLoaderObserver;

namespace mozilla {
class Encoding;
class OriginAttributes;
namespace dom {
class ClientInfo;
class PerformanceStorage;
class ServiceWorkerDescriptor;
} // namespace dom
} // namespace mozilla

template <class> class nsCOMPtr;
template <typename> struct already_AddRefed;

already_AddRefed<nsIIOService> do_GetIOService(nsresult *error = nullptr);

already_AddRefed<nsINetUtil> do_GetNetUtil(nsresult *error = nullptr);

// private little helper function... don't call this directly!
nsresult net_EnsureIOService(nsIIOService **ios, nsCOMPtr<nsIIOService> &grip);

nsresult NS_NewURI(nsIURI **result,
                   const nsACString &spec,
                   const char *charset = nullptr,
                   nsIURI *baseURI = nullptr,
                   nsIIOService *ioService = nullptr);     // pass in nsIIOService to optimize callers

nsresult NS_NewURI(nsIURI **result,
                   const nsACString &spec,
                   mozilla::NotNull<const mozilla::Encoding*> encoding,
                   nsIURI *baseURI = nullptr,
                   nsIIOService *ioService = nullptr);     // pass in nsIIOService to optimize callers

nsresult NS_NewURI(nsIURI **result,
                   const nsAString &spec,
                   const char *charset = nullptr,
                   nsIURI *baseURI = nullptr,
                   nsIIOService *ioService = nullptr);     // pass in nsIIOService to optimize callers

nsresult NS_NewURI(nsIURI **result,
                   const nsAString &spec,
                   mozilla::NotNull<const mozilla::Encoding*> encoding,
                   nsIURI *baseURI = nullptr,
                   nsIIOService *ioService = nullptr);     // pass in nsIIOService to optimize callers

nsresult NS_NewURI(nsIURI **result,
                  const char *spec,
                  nsIURI *baseURI = nullptr,
                  nsIIOService *ioService = nullptr);     // pass in nsIIOService to optimize callers

nsresult NS_NewFileURI(nsIURI **result,
                       nsIFile *spec,
                       nsIIOService *ioService = nullptr);     // pass in nsIIOService to optimize callers

// These methods will only mutate the URI if the ref of aInput doesn't already
// match the ref we are trying to set.
// If aInput has no ref, and we are calling NS_GetURIWithoutRef, or
// NS_GetURIWithNewRef with an empty string, then aOutput will be the same
// as aInput. The same is true if aRef is already equal to the ref of aInput.
// This is OK because URIs are immutable and threadsafe.
// If the URI doesn't support ref fragments aOutput will be the same as aInput.
nsresult NS_GetURIWithNewRef(nsIURI* aInput,
                             const nsACString& aRef,
                             nsIURI** aOutput);
nsresult NS_GetURIWithoutRef(nsIURI* aInput,
                             nsIURI** aOutput);

nsresult NS_GetSanitizedURIStringFromURI(nsIURI *aUri,
                                         nsAString &aSanitizedSpec);

/*
* How to create a new Channel, using NS_NewChannel,
* NS_NewChannelWithTriggeringPrincipal,
* NS_NewInputStreamChannel, NS_NewChannelInternal
* and it's variations:
*
* What specific API function to use:
* * The NS_NewChannelInternal functions should almost never be directly
*   called outside of necko code.
* * If possible, use NS_NewChannel() providing a loading *nsINode*
* * If no loading *nsINode* is available, try calling NS_NewChannel() providing
*   a loading *ClientInfo*.
* * If no loading *nsINode* or *ClientInfo* are available, call NS_NewChannel()
*   providing a loading *nsIPrincipal*.
* * Call NS_NewChannelWithTriggeringPrincipal if the triggeringPrincipal
*   is different from the loadingPrincipal.
* * Call NS_NewChannelInternal() providing aLoadInfo object in cases where
*   you already have loadInfo object, e.g in case of a channel redirect.
*
* @param aURI
*        nsIURI from which to make a channel
* @param aLoadingNode
* @param aLoadingPrincipal
* @param aTriggeringPrincipal
* @param aSecurityFlags
* @param aContentPolicyType
*        These will be used as values for the nsILoadInfo object on the
*        created channel. For details, see nsILoadInfo in nsILoadInfo.idl
*
* Please note, if you provide both a loadingNode and a loadingPrincipal,
* then loadingPrincipal must be equal to loadingNode->NodePrincipal().
* But less error prone is to just supply a loadingNode.
*
* Note, if you provide a loading ClientInfo its principal must match the
* loading principal.  Currently you must pass both as the loading principal
* may have additional mutable values like CSP on it.  In the future these
* will be removed from nsIPrincipal and the API can be changed to take just
* the loading ClientInfo.
*
* Keep in mind that URIs coming from a webpage should *never* use the
* systemPrincipal as the loadingPrincipal.
*/
nsresult NS_NewChannelInternal(nsIChannel           **outChannel,
                               nsIURI                *aUri,
                               nsINode               *aLoadingNode,
                               nsIPrincipal          *aLoadingPrincipal,
                               nsIPrincipal          *aTriggeringPrincipal,
                               const mozilla::Maybe<mozilla::dom::ClientInfo>& aLoadingClientInfo,
                               const mozilla::Maybe<mozilla::dom::ServiceWorkerDescriptor>& aController,
                               nsSecurityFlags        aSecurityFlags,
                               nsContentPolicyType    aContentPolicyType,
                               mozilla::dom::PerformanceStorage* aPerformanceStorage = nullptr,
                               nsILoadGroup          *aLoadGroup = nullptr,
                               nsIInterfaceRequestor *aCallbacks = nullptr,
                               nsLoadFlags            aLoadFlags = nsIRequest::LOAD_NORMAL,
                               nsIIOService          *aIoService = nullptr);

// See NS_NewChannelInternal for usage and argument description
nsresult NS_NewChannelInternal(nsIChannel           **outChannel,
                               nsIURI                *aUri,
                               nsILoadInfo           *aLoadInfo,
                               mozilla::dom::PerformanceStorage* aPerformanceStorage = nullptr,
                               nsILoadGroup          *aLoadGroup = nullptr,
                               nsIInterfaceRequestor *aCallbacks = nullptr,
                               nsLoadFlags            aLoadFlags = nsIRequest::LOAD_NORMAL,
                               nsIIOService          *aIoService = nullptr);

// See NS_NewChannelInternal for usage and argument description
nsresult /*NS_NewChannelWithNodeAndTriggeringPrincipal */
NS_NewChannelWithTriggeringPrincipal(nsIChannel           **outChannel,
                                     nsIURI                *aUri,
                                     nsINode               *aLoadingNode,
                                     nsIPrincipal          *aTriggeringPrincipal,
                                     nsSecurityFlags        aSecurityFlags,
                                     nsContentPolicyType    aContentPolicyType,
                                     mozilla::dom::PerformanceStorage* aPerformanceStorage = nullptr,
                                     nsILoadGroup          *aLoadGroup = nullptr,
                                     nsIInterfaceRequestor *aCallbacks = nullptr,
                                     nsLoadFlags            aLoadFlags = nsIRequest::LOAD_NORMAL,
                                     nsIIOService          *aIoService = nullptr);

// See NS_NewChannelInternal for usage and argument description
nsresult
NS_NewChannelWithTriggeringPrincipal(nsIChannel           **outChannel,
                                     nsIURI                *aUri,
                                     nsIPrincipal          *aLoadingPrincipal,
                                     nsIPrincipal          *aTriggeringPrincipal,
                                     nsSecurityFlags        aSecurityFlags,
                                     nsContentPolicyType    aContentPolicyType,
                                     mozilla::dom::PerformanceStorage* aPerformanceStorage = nullptr,
                                     nsILoadGroup          *aLoadGroup = nullptr,
                                     nsIInterfaceRequestor *aCallbacks = nullptr,
                                     nsLoadFlags            aLoadFlags = nsIRequest::LOAD_NORMAL,
                                     nsIIOService          *aIoService = nullptr);

// See NS_NewChannelInternal for usage and argument description
nsresult
NS_NewChannelWithTriggeringPrincipal(nsIChannel           **outChannel,
                                     nsIURI                *aUri,
                                     nsIPrincipal          *aLoadingPrincipal,
                                     nsIPrincipal          *aTriggeringPrincipal,
                                     const mozilla::dom::ClientInfo& aLoadingClientInfo,
                                     const mozilla::Maybe<mozilla::dom::ServiceWorkerDescriptor>& aController,
                                     nsSecurityFlags        aSecurityFlags,
                                     nsContentPolicyType    aContentPolicyType,
                                     mozilla::dom::PerformanceStorage* aPerformanceStorage = nullptr,
                                     nsILoadGroup          *aLoadGroup = nullptr,
                                     nsIInterfaceRequestor *aCallbacks = nullptr,
                                     nsLoadFlags            aLoadFlags = nsIRequest::LOAD_NORMAL,
                                     nsIIOService          *aIoService = nullptr);


// See NS_NewChannelInternal for usage and argument description
nsresult
NS_NewChannel(nsIChannel           **outChannel,
              nsIURI                *aUri,
              nsINode               *aLoadingNode,
              nsSecurityFlags        aSecurityFlags,
              nsContentPolicyType    aContentPolicyType,
              mozilla::dom::PerformanceStorage* aPerformanceStorage = nullptr,
              nsILoadGroup          *aLoadGroup = nullptr,
              nsIInterfaceRequestor *aCallbacks = nullptr,
              nsLoadFlags            aLoadFlags = nsIRequest::LOAD_NORMAL,
              nsIIOService          *aIoService = nullptr);

// See NS_NewChannelInternal for usage and argument description
nsresult
NS_NewChannel(nsIChannel           **outChannel,
              nsIURI                *aUri,
              nsIPrincipal          *aLoadingPrincipal,
              nsSecurityFlags        aSecurityFlags,
              nsContentPolicyType    aContentPolicyType,
              mozilla::dom::PerformanceStorage* aPerformanceStorage = nullptr,
              nsILoadGroup          *aLoadGroup = nullptr,
              nsIInterfaceRequestor *aCallbacks = nullptr,
              nsLoadFlags            aLoadFlags = nsIRequest::LOAD_NORMAL,
              nsIIOService          *aIoService = nullptr);

// See NS_NewChannelInternal for usage and argument description
nsresult
NS_NewChannel(nsIChannel** outChannel,
              nsIURI* aUri,
              nsIPrincipal* aLoadingPrincipal,
              const mozilla::dom::ClientInfo& aLoadingClientInfo,
              const mozilla::Maybe<mozilla::dom::ServiceWorkerDescriptor>& aController,
              nsSecurityFlags aSecurityFlags,
              nsContentPolicyType aContentPolicyType,
              mozilla::dom::PerformanceStorage* aPerformanceStorage = nullptr,
              nsILoadGroup* aLoadGroup = nullptr,
              nsIInterfaceRequestor* aCallbacks = nullptr,
              nsLoadFlags aLoadFlags = nsIRequest::LOAD_NORMAL,
              nsIIOService* aIoService = nullptr);

nsresult NS_GetIsDocumentChannel(nsIChannel * aChannel, bool *aIsDocument);

nsresult NS_MakeAbsoluteURI(nsACString       &result,
                            const nsACString &spec,
                            nsIURI           *baseURI);

nsresult NS_MakeAbsoluteURI(char        **result,
                            const char   *spec,
                            nsIURI       *baseURI);

nsresult NS_MakeAbsoluteURI(nsAString       &result,
                            const nsAString &spec,
                            nsIURI          *baseURI);

/**
 * This function is a helper function to get a scheme's default port.
 */
int32_t NS_GetDefaultPort(const char *scheme,
                          nsIIOService *ioService = nullptr);

/**
 * This function is a helper function to apply the ToAscii conversion
 * to a string
 */
bool NS_StringToACE(const nsACString &idn, nsACString &result);

/**
 * This function is a helper function to get a protocol's default port if the
 * URI does not specify a port explicitly. Returns -1 if this protocol has no
 * concept of ports or if there was an error getting the port.
 */
int32_t NS_GetRealPort(nsIURI *aURI);

nsresult
NS_NewInputStreamChannelInternal(nsIChannel** outChannel,
                                 nsIURI* aUri,
                                 already_AddRefed<nsIInputStream> aStream,
                                 const nsACString& aContentType,
                                 const nsACString& aContentCharset,
                                 nsILoadInfo* aLoadInfo);

nsresult
NS_NewInputStreamChannelInternal(nsIChannel** outChannel,
                                 nsIURI* aUri,
                                 already_AddRefed<nsIInputStream> aStream,
                                 const nsACString& aContentType,
                                 const nsACString& aContentCharset,
                                 nsINode* aLoadingNode,
                                 nsIPrincipal* aLoadingPrincipal,
                                 nsIPrincipal* aTriggeringPrincipal,
                                 nsSecurityFlags aSecurityFlags,
                                 nsContentPolicyType aContentPolicyType);


nsresult
NS_NewInputStreamChannel(nsIChannel* *outChannel,
                         nsIURI* aUri,
                         already_AddRefed<nsIInputStream> aStream,
                         nsIPrincipal* aLoadingPrincipal,
                         nsSecurityFlags aSecurityFlags,
                         nsContentPolicyType aContentPolicyType,
                         const nsACString& aContentType    = EmptyCString(),
                         const nsACString& aContentCharset = EmptyCString());

nsresult NS_NewInputStreamChannelInternal(nsIChannel        **outChannel,
                                          nsIURI             *aUri,
                                          const nsAString    &aData,
                                          const nsACString   &aContentType,
                                          nsINode            *aLoadingNode,
                                          nsIPrincipal       *aLoadingPrincipal,
                                          nsIPrincipal       *aTriggeringPrincipal,
                                          nsSecurityFlags     aSecurityFlags,
                                          nsContentPolicyType aContentPolicyType,
                                          bool                aIsSrcdocChannel = false);

nsresult
NS_NewInputStreamChannelInternal(nsIChannel        **outChannel,
                                 nsIURI             *aUri,
                                 const nsAString    &aData,
                                 const nsACString   &aContentType,
                                 nsILoadInfo        *aLoadInfo,
                                 bool                aIsSrcdocChannel = false);

nsresult NS_NewInputStreamChannel(nsIChannel        **outChannel,
                                  nsIURI             *aUri,
                                  const nsAString    &aData,
                                  const nsACString   &aContentType,
                                  nsIPrincipal       *aLoadingPrincipal,
                                  nsSecurityFlags     aSecurityFlags,
                                  nsContentPolicyType aContentPolicyType,
                                  bool                aIsSrcdocChannel = false);

nsresult
NS_NewInputStreamPump(nsIInputStreamPump** aResult,
                      already_AddRefed<nsIInputStream> aStream,
                      uint32_t aSegsize = 0,
                      uint32_t aSegcount = 0,
                      bool aCloseWhenDone = false,
                      nsIEventTarget *aMainThreadTarget = nullptr);

nsresult NS_NewLoadGroup(nsILoadGroup      **result,
                         nsIRequestObserver *obs);

// Create a new nsILoadGroup that will match the given principal.
nsresult
NS_NewLoadGroup(nsILoadGroup **aResult, nsIPrincipal* aPrincipal);

// Determine if the given loadGroup/principal pair will produce a principal
// with similar permissions when passed to NS_NewChannel().  This checks for
// things like making sure the appId and browser element flags match.  Without
// an appropriate load group these values can be lost when getting the result
// principal back out of the channel.  Null principals are also always allowed
// as they do not have permissions to actually use the load group.
bool
NS_LoadGroupMatchesPrincipal(nsILoadGroup *aLoadGroup,
                             nsIPrincipal *aPrincipal);

nsresult NS_NewDownloader(nsIStreamListener   **result,
                          nsIDownloadObserver  *observer,
                          nsIFile              *downloadLocation = nullptr);

nsresult NS_NewStreamLoader(nsIStreamLoader        **result,
                            nsIStreamLoaderObserver *observer,
                            nsIRequestObserver      *requestObserver = nullptr);

nsresult NS_NewIncrementalStreamLoader(nsIIncrementalStreamLoader        **result,
                                       nsIIncrementalStreamLoaderObserver *observer);

nsresult NS_NewStreamLoaderInternal(nsIStreamLoader        **outStream,
                                    nsIURI                  *aUri,
                                    nsIStreamLoaderObserver *aObserver,
                                    nsINode                 *aLoadingNode,
                                    nsIPrincipal            *aLoadingPrincipal,
                                    nsSecurityFlags          aSecurityFlags,
                                    nsContentPolicyType      aContentPolicyType,
                                    nsILoadGroup            *aLoadGroup = nullptr,
                                    nsIInterfaceRequestor   *aCallbacks = nullptr,
                                    nsLoadFlags              aLoadFlags = nsIRequest::LOAD_NORMAL,
                                    nsIURI                  *aReferrer = nullptr);

nsresult
NS_NewStreamLoader(nsIStreamLoader        **outStream,
                   nsIURI                  *aUri,
                   nsIStreamLoaderObserver *aObserver,
                   nsINode                 *aLoadingNode,
                   nsSecurityFlags          aSecurityFlags,
                   nsContentPolicyType      aContentPolicyType,
                   nsILoadGroup            *aLoadGroup = nullptr,
                   nsIInterfaceRequestor   *aCallbacks = nullptr,
                   nsLoadFlags              aLoadFlags = nsIRequest::LOAD_NORMAL,
                   nsIURI                  *aReferrer = nullptr);

nsresult
NS_NewStreamLoader(nsIStreamLoader        **outStream,
                   nsIURI                  *aUri,
                   nsIStreamLoaderObserver *aObserver,
                   nsIPrincipal            *aLoadingPrincipal,
                   nsSecurityFlags          aSecurityFlags,
                   nsContentPolicyType      aContentPolicyType,
                   nsILoadGroup            *aLoadGroup = nullptr,
                   nsIInterfaceRequestor   *aCallbacks = nullptr,
                   nsLoadFlags              aLoadFlags = nsIRequest::LOAD_NORMAL,
                   nsIURI                  *aReferrer = nullptr);

nsresult NS_NewSyncStreamListener(nsIStreamListener **result,
                                  nsIInputStream    **stream);

/**
 * Implement the nsIChannel::Open(nsIInputStream**) method using the channel's
 * AsyncOpen method.
 *
 * NOTE: Reading from the returned nsIInputStream may spin the current
 * thread's event queue, which could result in any event being processed.
 */
nsresult NS_ImplementChannelOpen(nsIChannel      *channel,
                                 nsIInputStream **result);

nsresult NS_NewRequestObserverProxy(nsIRequestObserver **result,
                                    nsIRequestObserver  *observer,
                                    nsISupports         *context);

nsresult NS_NewSimpleStreamListener(nsIStreamListener **result,
                                    nsIOutputStream    *sink,
                                    nsIRequestObserver *observer = nullptr);

nsresult NS_CheckPortSafety(int32_t       port,
                            const char   *scheme,
                            nsIIOService *ioService = nullptr);

// Determine if this URI is using a safe port.
nsresult NS_CheckPortSafety(nsIURI *uri);

nsresult NS_NewProxyInfo(const nsACString &type,
                         const nsACString &host,
                         int32_t           port,
                         uint32_t          flags,
                         nsIProxyInfo    **result);

nsresult NS_GetFileProtocolHandler(nsIFileProtocolHandler **result,
                                   nsIIOService            *ioService = nullptr);

nsresult NS_GetFileFromURLSpec(const nsACString  &inURL,
                               nsIFile          **result,
                               nsIIOService      *ioService = nullptr);

nsresult NS_GetURLSpecFromFile(nsIFile      *file,
                               nsACString   &url,
                               nsIIOService *ioService = nullptr);

/**
 * Converts the nsIFile to the corresponding URL string.
 * Should only be called on files which are not directories,
 * is otherwise identical to NS_GetURLSpecFromFile, but is
 * usually more efficient.
 * Warning: this restriction may not be enforced at runtime!
 */
nsresult NS_GetURLSpecFromActualFile(nsIFile      *file,
                                     nsACString   &url,
                                     nsIIOService *ioService = nullptr);

/**
 * Converts the nsIFile to the corresponding URL string.
 * Should only be called on files which are directories,
 * is otherwise identical to NS_GetURLSpecFromFile, but is
 * usually more efficient.
 * Warning: this restriction may not be enforced at runtime!
 */
nsresult NS_GetURLSpecFromDir(nsIFile      *file,
                              nsACString   &url,
                              nsIIOService *ioService = nullptr);

/**
 * Obtains the referrer for a given channel.  This first tries to obtain the
 * referrer from the property docshell.internalReferrer, and if that doesn't
 * work and the channel is an nsIHTTPChannel, we check it's referrer property.
 *
 * @returns NS_ERROR_NOT_AVAILABLE if no referrer is available.
 */
nsresult NS_GetReferrerFromChannel(nsIChannel *channel,
                                   nsIURI **referrer);

nsresult NS_ParseRequestContentType(const nsACString &rawContentType,
                                    nsCString        &contentType,
                                    nsCString        &contentCharset);

nsresult NS_ParseResponseContentType(const nsACString &rawContentType,
                                     nsCString        &contentType,
                                     nsCString        &contentCharset);

nsresult NS_ExtractCharsetFromContentType(const nsACString &rawContentType,
                                          nsCString        &contentCharset,
                                          bool             *hadCharset,
                                          int32_t          *charsetStart,
                                          int32_t          *charsetEnd);

nsresult NS_NewLocalFileInputStream(nsIInputStream **result,
                                    nsIFile         *file,
                                    int32_t          ioFlags       = -1,
                                    int32_t          perm          = -1,
                                    int32_t          behaviorFlags = 0);

nsresult NS_NewLocalFileOutputStream(nsIOutputStream **result,
                                     nsIFile          *file,
                                     int32_t           ioFlags       = -1,
                                     int32_t           perm          = -1,
                                     int32_t           behaviorFlags = 0);

// returns a file output stream which can be QI'ed to nsISafeOutputStream.
nsresult NS_NewAtomicFileOutputStream(nsIOutputStream **result,
                                      nsIFile          *file,
                                      int32_t           ioFlags       = -1,
                                      int32_t           perm          = -1,
                                      int32_t           behaviorFlags = 0);

// returns a file output stream which can be QI'ed to nsISafeOutputStream.
nsresult NS_NewSafeLocalFileOutputStream(nsIOutputStream **result,
                                         nsIFile          *file,
                                         int32_t           ioFlags       = -1,
                                         int32_t           perm          = -1,
                                         int32_t           behaviorFlags = 0);

nsresult NS_NewLocalFileStream(nsIFileStream **result,
                               nsIFile        *file,
                               int32_t         ioFlags       = -1,
                               int32_t         perm          = -1,
                               int32_t         behaviorFlags = 0);

MOZ_MUST_USE nsresult
NS_NewBufferedInputStream(nsIInputStream** aResult,
                          already_AddRefed<nsIInputStream> aInputStream,
                          uint32_t aBufferSize);

// note: the resulting stream can be QI'ed to nsISafeOutputStream iff the
// provided stream supports it.
nsresult NS_NewBufferedOutputStream(nsIOutputStream** aResult,
                                    already_AddRefed<nsIOutputStream> aOutputStream,
                                    uint32_t aBufferSize);

/**
 * This function reads an inputStream and stores its content into a buffer. In
 * general, you should avoid using this function because, it blocks the current
 * thread until the operation is done.
 * If the inputStream is async, the reading happens on an I/O thread.
 *
 * @param aInputStream the inputStream.
 * @param aDest the destination buffer. if *aDest is null, it will be allocated
 *              with the size of the written data. if aDest is not null, aCount
 *              must greater than 0.
 * @param aCount the amount of data to read. Use -1 if you want that all the
 *               stream is read.
 * @param aWritten this pointer will be used to store the number of data
 *                 written in the buffer. If you don't need, pass nullptr.
 */
nsresult NS_ReadInputStreamToBuffer(nsIInputStream *aInputStream,
                                    void **aDest,
                                    int64_t aCount,
                                    uint64_t* aWritten = nullptr);

/**
 * See the comment for NS_ReadInputStreamToBuffer
 */
nsresult NS_ReadInputStreamToString(nsIInputStream *aInputStream,
                                    nsACString &aDest,
                                    int64_t aCount,
                                    uint64_t* aWritten = nullptr);

nsresult
NS_LoadPersistentPropertiesFromURISpec(nsIPersistentProperties **outResult,
                                       const nsACString         &aSpec);

/**
 * NS_QueryNotificationCallbacks implements the canonical algorithm for
 * querying interfaces from a channel's notification callbacks.  It first
 * searches the channel's notificationCallbacks attribute, and if the interface
 * is not found there, then it inspects the notificationCallbacks attribute of
 * the channel's loadGroup.
 *
 * Note: templatized only because nsIWebSocketChannel is currently not an
 * nsIChannel.
 */
template <class T> inline void
NS_QueryNotificationCallbacks(T            *channel,
                              const nsIID  &iid,
                              void        **result)
{
    MOZ_ASSERT(channel, "null channel");
    *result = nullptr;

    nsCOMPtr<nsIInterfaceRequestor> cbs;
    mozilla::Unused << channel->GetNotificationCallbacks(getter_AddRefs(cbs));
    if (cbs)
        cbs->GetInterface(iid, result);
    if (!*result) {
        // try load group's notification callbacks...
        nsCOMPtr<nsILoadGroup> loadGroup;
        mozilla::Unused << channel->GetLoadGroup(getter_AddRefs(loadGroup));
        if (loadGroup) {
            loadGroup->GetNotificationCallbacks(getter_AddRefs(cbs));
            if (cbs)
                cbs->GetInterface(iid, result);
        }
    }
}

// template helper:
// Note: "class C" templatized only because nsIWebSocketChannel is currently not
// an nsIChannel.

template <class C, class T> inline void
NS_QueryNotificationCallbacks(C           *channel,
                              nsCOMPtr<T> &result)
{
    NS_QueryNotificationCallbacks(channel, NS_GET_TEMPLATE_IID(T),
                                  getter_AddRefs(result));
}

/**
 * Alternate form of NS_QueryNotificationCallbacks designed for use by
 * nsIChannel implementations.
 */
inline void
NS_QueryNotificationCallbacks(nsIInterfaceRequestor  *callbacks,
                              nsILoadGroup           *loadGroup,
                              const nsIID            &iid,
                              void                  **result)
{
    *result = nullptr;

    if (callbacks)
        callbacks->GetInterface(iid, result);
    if (!*result) {
        // try load group's notification callbacks...
        if (loadGroup) {
            nsCOMPtr<nsIInterfaceRequestor> cbs;
            loadGroup->GetNotificationCallbacks(getter_AddRefs(cbs));
            if (cbs)
                cbs->GetInterface(iid, result);
        }
    }
}

/**
 * Returns true if channel is using Private Browsing, or false if not.
 * Returns false if channel's callbacks don't implement nsILoadContext.
 */
bool NS_UsePrivateBrowsing(nsIChannel *channel);

/**
 * Extract the OriginAttributes from the channel's triggering principal.
 */
bool NS_GetOriginAttributes(nsIChannel *aChannel,
                            mozilla::OriginAttributes &aAttributes);

/**
 * Returns true if the channel has visited any cross-origin URLs on any
 * URLs that it was redirected through.
 */
bool NS_HasBeenCrossOrigin(nsIChannel* aChannel, bool aReport = false);

/**
 * Returns true if the channel is a safe top-level navigation.
 */
bool NS_IsSafeTopLevelNav(nsIChannel* aChannel);

/**
 * Returns true if the channel is a foreign with respect to the host-uri.
 * For loads of TYPE_DOCUMENT, this function returns true if it's a
 * cross origin navigation.
 */
bool NS_IsSameSiteForeign(nsIChannel* aChannel, nsIURI* aHostURI);

// Constants duplicated from nsIScriptSecurityManager so we avoid having necko
// know about script security manager.
#define NECKO_NO_APP_ID 0
#define NECKO_UNKNOWN_APP_ID UINT32_MAX

// Unique first-party domain for separating the safebrowsing cookie.
// Note if this value is changed, code in test_cookiejars_safebrowsing.js and
// nsUrlClassifierHashCompleter.js should also be changed.
#define NECKO_SAFEBROWSING_FIRST_PARTY_DOMAIN \
  "safebrowsing.86868755-6b82-4842-b301-72671a0db32e.mozilla"

// Unique first-party domain for separating about uri.
#define ABOUT_URI_FIRST_PARTY_DOMAIN \
  "about.ef2a7dd5-93bc-417f-a698-142c3116864f.mozilla"

/**
 * Determines whether appcache should be checked for a given principal.
 */
bool NS_ShouldCheckAppCache(nsIPrincipal *aPrincipal);

/**
 * Wraps an nsIAuthPrompt so that it can be used as an nsIAuthPrompt2. This
 * method is provided mainly for use by other methods in this file.
 *
 * *aAuthPrompt2 should be set to null before calling this function.
 */
void NS_WrapAuthPrompt(nsIAuthPrompt *aAuthPrompt,
                       nsIAuthPrompt2 **aAuthPrompt2);

/**
 * Gets an auth prompt from an interface requestor. This takes care of wrapping
 * an nsIAuthPrompt so that it can be used as an nsIAuthPrompt2.
 */
void NS_QueryAuthPrompt2(nsIInterfaceRequestor  *aCallbacks,
                         nsIAuthPrompt2        **aAuthPrompt);

/**
 * Gets an nsIAuthPrompt2 from a channel. Use this instead of
 * NS_QueryNotificationCallbacks for better backwards compatibility.
 */
void NS_QueryAuthPrompt2(nsIChannel      *aChannel,
                         nsIAuthPrompt2 **aAuthPrompt);

/* template helper */
template <class T> inline void
NS_QueryNotificationCallbacks(nsIInterfaceRequestor *callbacks,
                              nsILoadGroup          *loadGroup,
                              nsCOMPtr<T>           &result)
{
    NS_QueryNotificationCallbacks(callbacks, loadGroup,
                                  NS_GET_TEMPLATE_IID(T),
                                  getter_AddRefs(result));
}

/* template helper */
template <class T> inline void
NS_QueryNotificationCallbacks(const nsCOMPtr<nsIInterfaceRequestor> &aCallbacks,
                              const nsCOMPtr<nsILoadGroup>          &aLoadGroup,
                              nsCOMPtr<T>                           &aResult)
{
    NS_QueryNotificationCallbacks(aCallbacks.get(), aLoadGroup.get(), aResult);
}

/* template helper */
template <class T> inline void
NS_QueryNotificationCallbacks(const nsCOMPtr<nsIChannel> &aChannel,
                              nsCOMPtr<T>                &aResult)
{
    NS_QueryNotificationCallbacks(aChannel.get(), aResult);
}

/**
 * This function returns a nsIInterfaceRequestor instance that returns the
 * same result as NS_QueryNotificationCallbacks when queried.  It is useful
 * as the value for nsISocketTransport::securityCallbacks.
 */
nsresult
NS_NewNotificationCallbacksAggregation(nsIInterfaceRequestor  *callbacks,
                                       nsILoadGroup           *loadGroup,
                                       nsIEventTarget         *target,
                                       nsIInterfaceRequestor **result);

nsresult
NS_NewNotificationCallbacksAggregation(nsIInterfaceRequestor  *callbacks,
                                       nsILoadGroup           *loadGroup,
                                       nsIInterfaceRequestor **result);

/**
 * Helper function for testing online/offline state of the browser.
 */
bool NS_IsOffline();

/**
 * Helper functions for implementing nsINestedURI::innermostURI.
 *
 * Note that NS_DoImplGetInnermostURI is "private" -- call
 * NS_ImplGetInnermostURI instead.
 */
nsresult NS_DoImplGetInnermostURI(nsINestedURI *nestedURI, nsIURI **result);

nsresult NS_ImplGetInnermostURI(nsINestedURI *nestedURI, nsIURI **result);

/**
 * Helper function for testing whether the given URI, or any of its
 * inner URIs, has all the given protocol flags.
 */
nsresult NS_URIChainHasFlags(nsIURI   *uri,
                             uint32_t  flags,
                             bool     *result);

/**
 * Helper function for getting the innermost URI for a given URI.  The return
 * value could be just the object passed in if it's not a nested URI.
 */
already_AddRefed<nsIURI> NS_GetInnermostURI(nsIURI *aURI);

/**
 * Get the "final" URI for a channel.  This is either channel's load info
 * resultPrincipalURI, if set, or GetOriginalURI.  In most cases (but not all) load
 * info resultPrincipalURI, if set, corresponds to URI of the channel if it's required
 * to represent the actual principal for the channel.
 */
nsresult NS_GetFinalChannelURI(nsIChannel *channel, nsIURI **uri);

// NS_SecurityHashURI must return the same hash value for any two URIs that
// compare equal according to NS_SecurityCompareURIs.  Unfortunately, in the
// case of files, it's not clear we can do anything better than returning
// the schemeHash, so hashing files degenerates to storing them in a list.
uint32_t NS_SecurityHashURI(nsIURI *aURI);

bool NS_SecurityCompareURIs(nsIURI *aSourceURI,
                            nsIURI *aTargetURI,
                            bool aStrictFileOriginPolicy);

bool NS_URIIsLocalFile(nsIURI *aURI);

// When strict file origin policy is enabled, SecurityCompareURIs will fail for
// file URIs that do not point to the same local file. This call provides an
// alternate file-specific origin check that allows target files that are
// contained in the same directory as the source.
//
// https://developer.mozilla.org/en-US/docs/Same-origin_policy_for_file:_URIs
bool NS_RelaxStrictFileOriginPolicy(nsIURI *aTargetURI,
                                    nsIURI *aSourceURI,
                                    bool aAllowDirectoryTarget = false);

bool NS_IsInternalSameURIRedirect(nsIChannel *aOldChannel,
                                  nsIChannel *aNewChannel,
                                  uint32_t aFlags);

bool NS_IsHSTSUpgradeRedirect(nsIChannel *aOldChannel,
                              nsIChannel *aNewChannel,
                              uint32_t aFlags);

nsresult NS_LinkRedirectChannels(uint32_t channelId,
                                 nsIParentChannel *parentChannel,
                                 nsIChannel **_result);

/**
 * Helper function which checks whether the channel can be
 * openend using Open2() or has to fall back to opening
 * the channel using Open().
 */
nsresult NS_MaybeOpenChannelUsingOpen2(nsIChannel* aChannel,
                                       nsIInputStream **aStream);

/**
 * Helper function which checks whether the channel can be
 * openend using AsyncOpen2() or has to fall back to opening
 * the channel using AsyncOpen().
 */
nsresult NS_MaybeOpenChannelUsingAsyncOpen2(nsIChannel* aChannel,
                                            nsIStreamListener *aListener);

/** Given the first (disposition) token from a Content-Disposition header,
 * tell whether it indicates the content is inline or attachment
 * @param aDispToken the disposition token from the content-disposition header
 */
uint32_t NS_GetContentDispositionFromToken(const nsAString &aDispToken);

/** Determine the disposition (inline/attachment) of the content based on the
 * Content-Disposition header
 * @param aHeader the content-disposition header (full value)
 * @param aChan the channel the header came from
 */
uint32_t NS_GetContentDispositionFromHeader(const nsACString &aHeader,
                                            nsIChannel *aChan = nullptr);

/** Extracts the filename out of a content-disposition header
 * @param aFilename [out] The filename. Can be empty on error.
 * @param aDisposition Value of a Content-Disposition header
 * @param aURI Optional. Will be used to get a fallback charset for the
 *        filename, if it is QI'able to nsIURL
 */
nsresult NS_GetFilenameFromDisposition(nsAString &aFilename,
                                       const nsACString &aDisposition,
                                       nsIURI *aURI = nullptr);

/**
 * Make sure Personal Security Manager is initialized
 */
void net_EnsurePSMInit();

/**
 * Test whether a URI is "about:blank".  |uri| must not be null
 */
bool NS_IsAboutBlank(nsIURI *uri);

nsresult NS_GenerateHostPort(const nsCString &host, int32_t port,
                             nsACString &hostLine);

/**
 * Sniff the content type for a given request or a given buffer.
 *
 * aSnifferType can be either NS_CONTENT_SNIFFER_CATEGORY or
 * NS_DATA_SNIFFER_CATEGORY.  The function returns the sniffed content type
 * in the aSniffedType argument.  This argument will not be modified if the
 * content type could not be sniffed.
 */
void NS_SniffContent(const char *aSnifferType, nsIRequest *aRequest,
                     const uint8_t *aData, uint32_t aLength,
                     nsACString &aSniffedType);

/**
 * Whether the channel was created to load a srcdoc document.
 * Note that view-source:about:srcdoc is classified as a srcdoc document by
 * this function, which may not be applicable everywhere.
 */
bool NS_IsSrcdocChannel(nsIChannel *aChannel);

/**
 * Return true if the given string is a reasonable HTTP header value given the
 * definition in RFC 2616 section 4.2.  Currently we don't pay the cost to do
 * full, sctrict validation here since it would require fulling parsing the
 * value.
 */
bool NS_IsReasonableHTTPHeaderValue(const nsACString &aValue);

/**
 * Return true if the given string is a valid HTTP token per RFC 2616 section
 * 2.2.
 */
bool NS_IsValidHTTPToken(const nsACString &aToken);

/**
 * Strip the leading or trailing HTTP whitespace per fetch spec section 2.2.
 */
void NS_TrimHTTPWhitespace(const nsACString& aSource, nsACString& aDest);

/**
 * Return true if the given request must be upgraded to HTTPS.
 */
nsresult NS_ShouldSecureUpgrade(nsIURI* aURI,
                                nsILoadInfo* aLoadInfo,
                                nsIPrincipal* aChannelResultPrincipal,
                                bool aPrivateBrowsing,
                                bool aAllowSTS,
                                const mozilla::OriginAttributes& aOriginAttributes,
                                bool& aShouldUpgrade);

/**
 * Returns an https URI for channels that need to go through secure upgrades.
 */
nsresult NS_GetSecureUpgradedURI(nsIURI* aURI, nsIURI** aUpgradedURI);

nsresult NS_CompareLoadInfoAndLoadContext(nsIChannel *aChannel);

/**
 * Return default referrer policy which is controlled by user
 * prefs:
 * network.http.referer.defaultPolicy for regular mode
 * network.http.referer.defaultPolicy.pbmode for private mode
 */
uint32_t NS_GetDefaultReferrerPolicy(bool privateBrowsing = false);

namespace mozilla {
namespace net {

const static uint64_t kJS_MAX_SAFE_UINTEGER = +9007199254740991ULL;
const static  int64_t kJS_MIN_SAFE_INTEGER  = -9007199254740991LL;
const static  int64_t kJS_MAX_SAFE_INTEGER  = +9007199254740991LL;

// Make sure a 64bit value can be captured by JS MAX_SAFE_INTEGER
bool InScriptableRange(int64_t val);

// Make sure a 64bit value can be captured by JS MAX_SAFE_INTEGER
bool InScriptableRange(uint64_t val);

} // namespace net
} // namespace mozilla

#endif // !nsNetUtil_h__
