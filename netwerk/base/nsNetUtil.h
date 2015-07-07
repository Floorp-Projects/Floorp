/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNetUtil_h__
#define nsNetUtil_h__

#include "nsCOMPtr.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILoadGroup.h"
#include "nsINetUtil.h"
#include "nsIRequest.h"
#include "nsILoadInfo.h"
#include "nsIIOService.h"
#include "mozilla/Services.h"
#include "nsNetCID.h"

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
class nsINetworkInterface;
class nsIOutputStream;
class nsIParentChannel;
class nsIPersistentProperties;
class nsIProxyInfo;
class nsIRequestObserver;
class nsIStreamListener;
class nsIStreamLoader;
class nsIStreamLoaderObserver;
class nsIUnicharStreamLoader;
class nsIUnicharStreamLoaderObserver;

template <class> class nsCOMPtr;
template <typename> struct already_AddRefed;

#ifdef MOZILLA_INTERNAL_API
#include "nsReadableUtils.h"
#include "nsString.h"
#else
#include "nsStringAPI.h"
#endif

#ifdef MOZILLA_INTERNAL_API
already_AddRefed<nsIIOService> do_GetIOService(nsresult *error = 0);

already_AddRefed<nsINetUtil> do_GetNetUtil(nsresult *error = 0);

#else
// Helper, to simplify getting the I/O service.
const nsGetServiceByContractIDWithError do_GetIOService(nsresult *error = 0);

// An alias to do_GetIOService
const nsGetServiceByContractIDWithError do_GetNetUtil(nsresult *error = 0);

#endif

// private little helper function... don't call this directly!
nsresult net_EnsureIOService(nsIIOService **ios, nsCOMPtr<nsIIOService> &grip);

nsresult NS_NewURI(nsIURI **result,
                   const nsACString &spec,
                   const char *charset = nullptr,
                   nsIURI *baseURI = nullptr,
                   nsIIOService *ioService = nullptr);     // pass in nsIIOService to optimize callers

nsresult NS_NewURI(nsIURI **result,
                   const nsAString &spec,
                   const char *charset = nullptr,
                   nsIURI *baseURI = nullptr,
                   nsIIOService *ioService = nullptr);     // pass in nsIIOService to optimize callers

nsresult NS_NewURI(nsIURI **result,
                  const char *spec,
                  nsIURI *baseURI = nullptr,
                  nsIIOService *ioService = nullptr);     // pass in nsIIOService to optimize callers

nsresult NS_NewFileURI(nsIURI **result,
                       nsIFile *spec,
                       nsIIOService *ioService = nullptr);     // pass in nsIIOService to optimize callers

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
* * If no loading *nsINode* is avaialable, call NS_NewChannel() providing
*   a loading *nsIPrincipal*.
* * Call NS_NewChannelWithTriggeringPrincipal if the triggeringPrincipal
*   is different from the loadingPrincipal.
* * Call NS_NewChannelInternal() providing aLoadInfo object in cases where
*   you already have loadInfo object, e.g in case of a channel redirect.
*
* @param aURI
*        nsIURI from which to make a channel
* @param aLoadingNode
*        The loadingDocument of the channel.
*        The element or document where the result of this request will be
*        used. This is the document/element that will get access to the
*        result of this request. For example for an image load, it's the
*        document in which the image will be loaded. And for a CSS
*        stylesheet it's the document whose rendering will be affected by
*        the stylesheet.
*        If possible, pass in the element which is performing the load. But
*        if the load is coming from a JS API (such as XMLHttpRequest) or if
*        the load might be coalesced across multiple elements (such as
*        for <img>) then pass in the Document node instead.
*        For loads that are not related to any document, such as loads coming
*        from addons or internal browser features, use null here.
* @param aLoadingPrincipal
*        The loadingPrincipal of the channel.
*        The principal of the document where the result of this request will
*        be used.
*        This defaults to the principal of aLoadingNode, so when aLoadingNode
*        is passed this can be left as null. However for loads where
*        aLoadingNode is null this argument must be passed.
*        For example for loads from a WebWorker, pass the principal
*        of that worker. For loads from an addon or from internal browser
*        features, pass the system principal.
*        This principal should almost always be the system principal if
*        aLoadingNode is null. The only exception to this is for loads
*        from WebWorkers since they don't have any nodes to be passed as
*        aLoadingNode.
*        Please note, aLoadingPrincipal is *not* the principal of the
*        resource being loaded. But rather the principal of the context
*        where the resource will be used.
* @param aTriggeringPrincipal
*        The triggeringPrincipal of the load.
*        The triggeringPrincipal is the principal of the resource that caused
*        this particular URL to be loaded.
*        Most likely the triggeringPrincipal and the loadingPrincipal are
*        identical, in which case the triggeringPrincipal can be left out.
*        In some cases the loadingPrincipal and the triggeringPrincipal are
*        different however, e.g. a stylesheet may import a subresource. In
*        that case the principal of the stylesheet which contains the
*        import command is the triggeringPrincipal, and the principal of
*        the document whose rendering is affected is the loadingPrincipal.
* @param aSecurityFlags
*        The securityFlags of the channel.
*        Any of the securityflags defined in nsILoadInfo.idl
* @param aContentPolicyType
*        The contentPolicyType of the channel.
*        Any of the content types defined in nsIContentPolicy.idl
*
* Please note, if you provide both a loadingNode and a loadingPrincipal,
* then loadingPrincipal must be equal to loadingNode->NodePrincipal().
* But less error prone is to just supply a loadingNode.
*
* Keep in mind that URIs coming from a webpage should *never* use the
* systemPrincipal as the loadingPrincipal.
*/
nsresult NS_NewChannelInternal(nsIChannel           **outChannel,
                               nsIURI                *aUri,
                               nsINode               *aLoadingNode,
                               nsIPrincipal          *aLoadingPrincipal,
                               nsIPrincipal          *aTriggeringPrincipal,
                               nsSecurityFlags        aSecurityFlags,
                               nsContentPolicyType    aContentPolicyType,
                               nsILoadGroup          *aLoadGroup = nullptr,
                               nsIInterfaceRequestor *aCallbacks = nullptr,
                               nsLoadFlags            aLoadFlags = nsIRequest::LOAD_NORMAL,
                               nsIIOService          *aIoService = nullptr);

// See NS_NewChannelInternal for usage and argument description
nsresult NS_NewChannelInternal(nsIChannel           **outChannel,
                               nsIURI                *aUri,
                               nsILoadInfo           *aLoadInfo,
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
                                     nsILoadGroup          *aLoadGroup = nullptr,
                                     nsIInterfaceRequestor *aCallbacks = nullptr,
                                     nsLoadFlags            aLoadFlags = nsIRequest::LOAD_NORMAL,
                                     nsIIOService          *aIoService = nullptr);


// See NS_NewChannelInternal for usage and argument description
nsresult /*NS_NewChannelWithPrincipalAndTriggeringPrincipal */
NS_NewChannelWithTriggeringPrincipal(nsIChannel           **outChannel,
                                     nsIURI                *aUri,
                                     nsIPrincipal          *aLoadingPrincipal,
                                     nsIPrincipal          *aTriggeringPrincipal,
                                     nsSecurityFlags        aSecurityFlags,
                                     nsContentPolicyType    aContentPolicyType,
                                     nsILoadGroup          *aLoadGroup = nullptr,
                                     nsIInterfaceRequestor *aCallbacks = nullptr,
                                     nsLoadFlags            aLoadFlags = nsIRequest::LOAD_NORMAL,
                                     nsIIOService          *aIoService = nullptr);

// See NS_NewChannelInternal for usage and argument description
nsresult /* NS_NewChannelNode */
NS_NewChannel(nsIChannel           **outChannel,
              nsIURI                *aUri,
              nsINode               *aLoadingNode,
              nsSecurityFlags        aSecurityFlags,
              nsContentPolicyType    aContentPolicyType,
              nsILoadGroup          *aLoadGroup = nullptr,
              nsIInterfaceRequestor *aCallbacks = nullptr,
              nsLoadFlags            aLoadFlags = nsIRequest::LOAD_NORMAL,
              nsIIOService          *aIoService = nullptr);

// See NS_NewChannelInternal for usage and argument description
nsresult /* NS_NewChannelPrincipal */
NS_NewChannel(nsIChannel           **outChannel,
              nsIURI                *aUri,
              nsIPrincipal          *aLoadingPrincipal,
              nsSecurityFlags        aSecurityFlags,
              nsContentPolicyType    aContentPolicyType,
              nsILoadGroup          *aLoadGroup = nullptr,
              nsIInterfaceRequestor *aCallbacks = nullptr,
              nsLoadFlags            aLoadFlags = nsIRequest::LOAD_NORMAL,
              nsIIOService          *aIoService = nullptr);

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

nsresult /* NS_NewInputStreamChannelWithLoadInfo */
NS_NewInputStreamChannelInternal(nsIChannel        **outChannel,
                                 nsIURI             *aUri,
                                 nsIInputStream     *aStream,
                                 const nsACString   &aContentType,
                                 const nsACString   &aContentCharset,
                                 nsILoadInfo        *aLoadInfo);

nsresult NS_NewInputStreamChannelInternal(nsIChannel        **outChannel,
                                          nsIURI             *aUri,
                                          nsIInputStream     *aStream,
                                          const nsACString   &aContentType,
                                          const nsACString   &aContentCharset,
                                          nsINode            *aLoadingNode,
                                          nsIPrincipal       *aLoadingPrincipal,
                                          nsIPrincipal       *aTriggeringPrincipal,
                                          nsSecurityFlags     aSecurityFlags,
                                          nsContentPolicyType aContentPolicyType,
                                          nsIURI             *aBaseURI = nullptr);


nsresult /* NS_NewInputStreamChannelPrincipal */
NS_NewInputStreamChannel(nsIChannel        **outChannel,
                         nsIURI             *aUri,
                         nsIInputStream     *aStream,
                         nsIPrincipal       *aLoadingPrincipal,
                         nsSecurityFlags     aSecurityFlags,
                         nsContentPolicyType aContentPolicyType,
                         const nsACString   &aContentType    = EmptyCString(),
                         const nsACString   &aContentCharset = EmptyCString());

nsresult NS_NewInputStreamChannelInternal(nsIChannel        **outChannel,
                                          nsIURI             *aUri,
                                          const nsAString    &aData,
                                          const nsACString   &aContentType,
                                          nsINode            *aLoadingNode,
                                          nsIPrincipal       *aLoadingPrincipal,
                                          nsIPrincipal       *aTriggeringPrincipal,
                                          nsSecurityFlags     aSecurityFlags,
                                          nsContentPolicyType aContentPolicyType,
                                          bool                aIsSrcdocChannel = false,
                                          nsIURI             *aBaseURI = nullptr);

nsresult NS_NewInputStreamChannel(nsIChannel        **outChannel,
                                  nsIURI             *aUri,
                                  const nsAString    &aData,
                                  const nsACString   &aContentType,
                                  nsIPrincipal       *aLoadingPrincipal,
                                  nsSecurityFlags     aSecurityFlags,
                                  nsContentPolicyType aContentPolicyType,
                                  bool                aIsSrcdocChannel = false,
                                  nsIURI             *aBaseURI = nullptr);

nsresult NS_NewInputStreamPump(nsIInputStreamPump **result,
                               nsIInputStream      *stream,
                               int64_t              streamPos = int64_t(-1),
                               int64_t              streamLen = int64_t(-1),
                               uint32_t             segsize = 0,
                               uint32_t             segcount = 0,
                               bool                 closeWhenDone = false);

// NOTE: you will need to specify whether or not your streams are buffered
// (i.e., do they implement ReadSegments/WriteSegments).  the default
// assumption of TRUE for both streams might not be right for you!
nsresult NS_NewAsyncStreamCopier(nsIAsyncStreamCopier **result,
                                 nsIInputStream        *source,
                                 nsIOutputStream       *sink,
                                 nsIEventTarget        *target,
                                 bool                   sourceBuffered = true,
                                 bool                   sinkBuffered = true,
                                 uint32_t               chunkSize = 0,
                                 bool                   closeSource = true,
                                 bool                   closeSink = true);

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

nsresult NS_NewStreamLoaderInternal(nsIStreamLoader        **outStream,
                                    nsIURI                  *aUri,
                                    nsIStreamLoaderObserver *aObserver,
                                    nsINode                 *aLoadingNode,
                                    nsIPrincipal            *aLoadingPrincipal,
                                    nsSecurityFlags          aSecurityFlags,
                                    nsContentPolicyType      aContentPolicyType,
                                    nsISupports             *aContext = nullptr,
                                    nsILoadGroup            *aLoadGroup = nullptr,
                                    nsIInterfaceRequestor   *aCallbacks = nullptr,
                                    nsLoadFlags              aLoadFlags = nsIRequest::LOAD_NORMAL,
                                    nsIURI                  *aReferrer = nullptr);

nsresult /* NS_NewStreamLoaderNode */
NS_NewStreamLoader(nsIStreamLoader        **outStream,
                   nsIURI                  *aUri,
                   nsIStreamLoaderObserver *aObserver,
                   nsINode                 *aLoadingNode,
                   nsSecurityFlags          aSecurityFlags,
                   nsContentPolicyType      aContentPolicyType,
                   nsISupports             *aContext = nullptr,
                   nsILoadGroup            *aLoadGroup = nullptr,
                   nsIInterfaceRequestor   *aCallbacks = nullptr,
                   nsLoadFlags              aLoadFlags = nsIRequest::LOAD_NORMAL,
                   nsIURI                  *aReferrer = nullptr);

nsresult /* NS_NewStreamLoaderPrincipal */
NS_NewStreamLoader(nsIStreamLoader        **outStream,
                   nsIURI                  *aUri,
                   nsIStreamLoaderObserver *aObserver,
                   nsIPrincipal            *aLoadingPrincipal,
                   nsSecurityFlags          aSecurityFlags,
                   nsContentPolicyType      aContentPolicyType,
                   nsISupports             *aContext = nullptr,
                   nsILoadGroup            *aLoadGroup = nullptr,
                   nsIInterfaceRequestor   *aCallbacks = nullptr,
                   nsLoadFlags              aLoadFlags = nsIRequest::LOAD_NORMAL,
                   nsIURI                  *aReferrer = nullptr);

nsresult NS_NewUnicharStreamLoader(nsIUnicharStreamLoader        **result,
                                   nsIUnicharStreamLoaderObserver *observer);

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

nsresult NS_ParseContentType(const nsACString &rawContentType,
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

nsresult NS_NewPartialLocalFileInputStream(nsIInputStream **result,
                                           nsIFile         *file,
                                           uint64_t         offset,
                                           uint64_t         length,
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

// returns the input end of a pipe.  the output end of the pipe
// is attached to the original stream.  data from the original
// stream is read into the pipe on a background thread.
nsresult NS_BackgroundInputStream(nsIInputStream **result,
                                  nsIInputStream  *stream,
                                  uint32_t         segmentSize  = 0,
                                  uint32_t         segmentCount = 0);

// returns the output end of a pipe.  the input end of the pipe
// is attached to the original stream.  data written to the pipe
// is copied to the original stream on a background thread.
nsresult NS_BackgroundOutputStream(nsIOutputStream **result,
                                   nsIOutputStream  *stream,
                                   uint32_t          segmentSize  = 0,
                                   uint32_t          segmentCount = 0);

MOZ_WARN_UNUSED_RESULT nsresult
NS_NewBufferedInputStream(nsIInputStream **result,
                          nsIInputStream  *str,
                          uint32_t         bufferSize);

// note: the resulting stream can be QI'ed to nsISafeOutputStream iff the
// provided stream supports it.
nsresult NS_NewBufferedOutputStream(nsIOutputStream **result,
                                    nsIOutputStream  *str,
                                    uint32_t          bufferSize);

/**
 * Attempts to buffer a given output stream.  If this fails, it returns the
 * passed-in output stream.
 *
 * @param aOutputStream
 *        The output stream we want to buffer.  This cannot be null.
 * @param aBufferSize
 *        The size of the buffer for the buffered output stream.
 * @returns an nsIOutputStream that is buffered with the specified buffer size,
 *          or is aOutputStream if creating the new buffered stream failed.
 */
already_AddRefed<nsIOutputStream>
NS_BufferOutputStream(nsIOutputStream *aOutputStream,
                      uint32_t aBufferSize);

// returns an input stream compatible with nsIUploadChannel::SetUploadStream()
nsresult NS_NewPostDataStream(nsIInputStream  **result,
                              bool              isFile,
                              const nsACString &data);

nsresult NS_ReadInputStreamToBuffer(nsIInputStream *aInputStream,
                                    void **aDest,
                                    uint32_t aCount);

// external code can't see fallible_t
#ifdef MOZILLA_INTERNAL_API

nsresult NS_ReadInputStreamToString(nsIInputStream *aInputStream,
                                    nsACString &aDest,
                                    uint32_t aCount);

#endif

nsresult
NS_LoadPersistentPropertiesFromURI(nsIPersistentProperties **outResult,
                                   nsIURI                   *aUri,
                                   nsIPrincipal             *aLoadingPrincipal,
                                   nsContentPolicyType       aContentPolicyType,
                                   nsIIOService             *aIoService = nullptr);

nsresult
NS_LoadPersistentPropertiesFromURISpec(nsIPersistentProperties **outResult,
                                       const nsACString         &aSpec,
                                       nsIPrincipal             *aLoadingPrincipal,
                                       nsContentPolicyType       aContentPolicyType,
                                       const char               *aCharset = nullptr,
                                       nsIURI                   *aBaseURI = nullptr,
                                       nsIIOService             *aIoService = nullptr);

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
    NS_PRECONDITION(channel, "null channel");
    *result = nullptr;

    nsCOMPtr<nsIInterfaceRequestor> cbs;
    channel->GetNotificationCallbacks(getter_AddRefs(cbs));
    if (cbs)
        cbs->GetInterface(iid, result);
    if (!*result) {
        // try load group's notification callbacks...
        nsCOMPtr<nsILoadGroup> loadGroup;
        channel->GetLoadGroup(getter_AddRefs(loadGroup));
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

// Constants duplicated from nsIScriptSecurityManager so we avoid having necko
// know about script security manager.
#define NECKO_NO_APP_ID 0
#define NECKO_UNKNOWN_APP_ID UINT32_MAX
// special app id reserved for separating the safebrowsing cookie
#define NECKO_SAFEBROWSING_APP_ID UINT32_MAX - 1

/**
 * Gets AppId and isInBrowserElement from channel's nsILoadContext.
 * Returns false if error or channel's callbacks don't implement nsILoadContext.
 */
bool NS_GetAppInfo(nsIChannel *aChannel,
                   uint32_t *aAppID,
                   bool *aIsInBrowserElement);

/**
 *  Gets appId and browserOnly parameters from the TOPIC_WEB_APP_CLEAR_DATA
 *  nsIObserverService notification.  Used when clearing user data or
 *  uninstalling web apps.
 */
nsresult NS_GetAppInfoFromClearDataNotification(nsISupports *aSubject,
                                                uint32_t *aAppID,
                                                bool *aBrowserOnly);

/**
 * Determines whether appcache should be checked for a given URI.
 */
bool NS_ShouldCheckAppCache(nsIURI *aURI, bool usePrivateBrowsing);

bool NS_ShouldCheckAppCache(nsIPrincipal *aPrincipal, bool usePrivateBrowsing);

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

bool NS_IsAppOffline(uint32_t appId);

bool NS_IsAppOffline(nsIPrincipal *principal);

/**
 * Helper functions for implementing nsINestedURI::innermostURI.
 *
 * Note that NS_DoImplGetInnermostURI is "private" -- call
 * NS_ImplGetInnermostURI instead.
 */
nsresult NS_DoImplGetInnermostURI(nsINestedURI *nestedURI, nsIURI **result);

nsresult NS_ImplGetInnermostURI(nsINestedURI *nestedURI, nsIURI **result);

/**
 * Helper function that ensures that |result| is a URI that's safe to
 * return.  If |uri| is immutable, just returns it, otherwise returns
 * a clone.  |uri| must not be null.
 */
nsresult NS_EnsureSafeToReturn(nsIURI *uri, nsIURI **result);

/**
 * Helper function that tries to set the argument URI to be immutable
 */
void NS_TryToSetImmutable(nsIURI *uri);

/**
 * Helper function for calling ToImmutableURI.  If all else fails, returns
 * the input URI.  The optional second arg indicates whether we had to fall
 * back to the input URI.  Passing in a null URI is ok.
 */
already_AddRefed<nsIURI> NS_TryToMakeImmutable(nsIURI *uri,
                                               nsresult *outRv = nullptr);

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
 * Get the "final" URI for a channel.  This is either the same as GetURI or
 * GetOriginalURI, depending on whether this channel has
 * nsIChanel::LOAD_REPLACE set.  For channels without that flag set, the final
 * URI is the original URI, while for ones with the flag the final URI is the
 * channel URI.
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
 * Helper function to create a random URL string that's properly formed
 * but guaranteed to be invalid.
 */
nsresult NS_MakeRandomInvalidURLString(nsCString &result);

/**
 * Helper function to determine whether urlString is Java-compatible --
 * whether it can be passed to the Java URL(String) constructor without the
 * latter throwing a MalformedURLException, or without Java otherwise
 * mishandling it.  This function (in effect) implements a scheme whitelist
 * for Java.
 */
nsresult NS_CheckIsJavaCompatibleURLString(nsCString& urlString, bool *result);

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

namespace mozilla {
namespace net {

const static uint64_t kJS_MAX_SAFE_UINTEGER = +9007199254740991ULL;
const static  int64_t kJS_MIN_SAFE_INTEGER  = -9007199254740991LL;
const static  int64_t kJS_MAX_SAFE_INTEGER  = +9007199254740991LL;

// Make sure a 64bit value can be captured by JS MAX_SAFE_INTEGER
bool InScriptableRange(int64_t val);

// Make sure a 64bit value can be captured by JS MAX_SAFE_INTEGER
bool InScriptableRange(uint64_t val);

} // namespace mozilla
} // namespace mozilla::net

// Include some function bodies for callers with external linkage
#ifndef MOZILLA_INTERNAL_API
#include "nsNetUtil.inl"
#endif

#endif // !nsNetUtil_h__
