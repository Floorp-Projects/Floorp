/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsURILoader.h"
#include "nsAutoPtr.h"
#include "nsIURIContentListener.h"
#include "nsIContentHandler.h"
#include "nsILoadGroup.h"
#include "nsIDocumentLoader.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"
#include "nsIChannel.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIInputStream.h"
#include "nsIStreamConverterService.h"
#include "nsIWeakReferenceUtils.h"
#include "nsIHttpChannel.h"
#include "netCore.h"
#include "nsCRT.h"
#include "nsIDocShell.h"
#include "nsIThreadRetargetableStreamListener.h"
#include "nsIChildChannel.h"
#include "nsExternalHelperAppService.h"

#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsReadableUtils.h"
#include "nsError.h"

#include "nsICategoryManager.h"
#include "nsCExternalHandlerService.h"

#include "nsNetCID.h"

#include "nsMimeTypes.h"

#include "nsDocLoader.h"
#include "mozilla/Attributes.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "mozilla/StaticPrefs_dom.h"
#include "nsContentUtils.h"

mozilla::LazyLogModule nsURILoader::mLog("URILoader");

#define LOG(args) MOZ_LOG(nsURILoader::mLog, mozilla::LogLevel::Debug, args)
#define LOG_ERROR(args) \
  MOZ_LOG(nsURILoader::mLog, mozilla::LogLevel::Error, args)
#define LOG_ENABLED() MOZ_LOG_TEST(nsURILoader::mLog, mozilla::LogLevel::Debug)

static uint32_t sConvertDataLimit = 20;

static bool InitPreferences() {
  mozilla::Preferences::AddUintVarCache(
      &sConvertDataLimit, "general.document_open_conversion_depth_limit", 20);
  return true;
}

NS_IMPL_ADDREF(nsDocumentOpenInfo)
NS_IMPL_RELEASE(nsDocumentOpenInfo)

NS_INTERFACE_MAP_BEGIN(nsDocumentOpenInfo)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIThreadRetargetableStreamListener)
NS_INTERFACE_MAP_END

nsDocumentOpenInfo::nsDocumentOpenInfo(nsIInterfaceRequestor* aWindowContext,
                                       uint32_t aFlags, nsURILoader* aURILoader)
    : m_originalContext(aWindowContext),
      mFlags(aFlags),
      mURILoader(aURILoader),
      mDataConversionDepthLimit(sConvertDataLimit) {}

nsDocumentOpenInfo::nsDocumentOpenInfo(uint32_t aFlags,
                                       bool aAllowListenerConversions)
    : m_originalContext(nullptr),
      mFlags(aFlags),
      mURILoader(nullptr),
      mDataConversionDepthLimit(sConvertDataLimit),
      mAllowListenerConversions(aAllowListenerConversions) {}

nsDocumentOpenInfo::~nsDocumentOpenInfo() {}

nsresult nsDocumentOpenInfo::Prepare() {
  LOG(("[0x%p] nsDocumentOpenInfo::Prepare", this));

  nsresult rv;

  // ask our window context if it has a uri content listener...
  m_contentListener = do_GetInterface(m_originalContext, &rv);
  return rv;
}

NS_IMETHODIMP nsDocumentOpenInfo::OnStartRequest(nsIRequest* request) {
  LOG(("[0x%p] nsDocumentOpenInfo::OnStartRequest", this));
  MOZ_ASSERT(request);
  if (!request) {
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv = NS_OK;

  //
  // Deal with "special" HTTP responses:
  //
  // - In the case of a 204 (No Content) or 205 (Reset Content) response, do
  //   not try to find a content handler.  Return NS_BINDING_ABORTED to cancel
  //   the request.  This has the effect of ensuring that the DocLoader does
  //   not try to interpret this as a real request.
  //
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(request, &rv));

  if (NS_SUCCEEDED(rv)) {
    uint32_t responseCode = 0;

    rv = httpChannel->GetResponseStatus(&responseCode);

    if (NS_FAILED(rv)) {
      LOG_ERROR(("  Failed to get HTTP response status"));

      // behave as in the canceled case
      return NS_OK;
    }

    LOG(("  HTTP response status: %d", responseCode));

    if (204 == responseCode || 205 == responseCode) {
      return NS_BINDING_ABORTED;
    }

    static bool sLargeAllocationHeaderEnabled = false;
    static bool sCachedLargeAllocationPref = false;
    if (!sCachedLargeAllocationPref) {
      sCachedLargeAllocationPref = true;
      mozilla::Preferences::AddBoolVarCache(
          &sLargeAllocationHeaderEnabled, "dom.largeAllocationHeader.enabled");
    }

    if (sLargeAllocationHeaderEnabled) {
      if (StaticPrefs::dom_largeAllocation_testing_allHttpLoads()) {
        nsCOMPtr<nsIURI> uri;
        rv = httpChannel->GetURI(getter_AddRefs(uri));
        if (NS_SUCCEEDED(rv) && uri) {
          if ((uri->SchemeIs("http") || uri->SchemeIs("https")) &&
              nsContentUtils::AttemptLargeAllocationLoad(httpChannel)) {
            return NS_BINDING_ABORTED;
          }
        }
      }

      // If we have a Large-Allocation header, let's check if we should perform
      // a process switch.
      nsAutoCString largeAllocationHeader;
      rv = httpChannel->GetResponseHeader(
          NS_LITERAL_CSTRING("Large-Allocation"), largeAllocationHeader);
      if (NS_SUCCEEDED(rv) &&
          nsContentUtils::AttemptLargeAllocationLoad(httpChannel)) {
        return NS_BINDING_ABORTED;
      }
    }
  }

  //
  // Make sure that the transaction has succeeded, so far...
  //
  nsresult status;

  rv = request->GetStatus(&status);

  NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to get request status!");
  if (NS_FAILED(rv)) return rv;

  if (NS_FAILED(status)) {
    LOG_ERROR(("  Request failed, status: 0x%08" PRIX32,
               static_cast<uint32_t>(status)));

    //
    // The transaction has already reported an error - so it will be torn
    // down. Therefore, it is not necessary to return an error code...
    //
    return NS_OK;
  }

  rv = DispatchContent(request, nullptr);

  LOG(("  After dispatch, m_targetStreamListener: 0x%p, rv: 0x%08" PRIX32,
       m_targetStreamListener.get(), static_cast<uint32_t>(rv)));

  NS_ASSERTION(
      NS_SUCCEEDED(rv) || !m_targetStreamListener,
      "Must not have an m_targetStreamListener with a failure return!");

  NS_ENSURE_SUCCESS(rv, rv);

  if (m_targetStreamListener)
    rv = m_targetStreamListener->OnStartRequest(request);

  LOG(("  OnStartRequest returning: 0x%08" PRIX32, static_cast<uint32_t>(rv)));

  return rv;
}

NS_IMETHODIMP
nsDocumentOpenInfo::CheckListenerChain() {
  NS_ASSERTION(NS_IsMainThread(), "Should be on the main thread!");
  nsresult rv = NS_OK;
  nsCOMPtr<nsIThreadRetargetableStreamListener> retargetableListener =
      do_QueryInterface(m_targetStreamListener, &rv);
  if (retargetableListener) {
    rv = retargetableListener->CheckListenerChain();
  }
  LOG(
      ("[0x%p] nsDocumentOpenInfo::CheckListenerChain %s listener %p rv "
       "%" PRIx32,
       this, (NS_SUCCEEDED(rv) ? "success" : "failure"),
       (nsIStreamListener*)m_targetStreamListener, static_cast<uint32_t>(rv)));
  return rv;
}

NS_IMETHODIMP
nsDocumentOpenInfo::OnDataAvailable(nsIRequest* request, nsIInputStream* inStr,
                                    uint64_t sourceOffset, uint32_t count) {
  // if we have retarged to the end stream listener, then forward the call....
  // otherwise, don't do anything

  nsresult rv = NS_OK;

  if (m_targetStreamListener)
    rv = m_targetStreamListener->OnDataAvailable(request, inStr, sourceOffset,
                                                 count);
  return rv;
}

NS_IMETHODIMP nsDocumentOpenInfo::OnStopRequest(nsIRequest* request,
                                                nsresult aStatus) {
  LOG(("[0x%p] nsDocumentOpenInfo::OnStopRequest", this));

  if (m_targetStreamListener) {
    nsCOMPtr<nsIStreamListener> listener(m_targetStreamListener);

    // If this is a multipart stream, we could get another
    // OnStartRequest after this... reset state.
    m_targetStreamListener = nullptr;
    mContentType.Truncate();
    listener->OnStopRequest(request, aStatus);
  }
  mUsedContentHandler = false;

  // Remember...
  // In the case of multiplexed streams (such as multipart/x-mixed-replace)
  // these stream listener methods could be called again :-)
  //
  return NS_OK;
}

nsresult nsDocumentOpenInfo::DispatchContent(nsIRequest* request,
                                             nsISupports* aCtxt) {
  LOG(("[0x%p] nsDocumentOpenInfo::DispatchContent for type '%s'", this,
       mContentType.get()));

  MOZ_ASSERT(!m_targetStreamListener,
             "Why do we already have a target stream listener?");

  nsresult rv;
  nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(request);
  if (!aChannel) {
    LOG_ERROR(("  Request is not a channel.  Bailing."));
    return NS_ERROR_FAILURE;
  }

  NS_NAMED_LITERAL_CSTRING(anyType, "*/*");
  if (mContentType.IsEmpty() || mContentType == anyType) {
    rv = aChannel->GetContentType(mContentType);
    if (NS_FAILED(rv)) return rv;
    LOG(("  Got type from channel: '%s'", mContentType.get()));
  }

  bool isGuessFromExt =
      mContentType.LowerCaseEqualsASCII(APPLICATION_GUESS_FROM_EXT);
  if (isGuessFromExt) {
    // Reset to application/octet-stream for now; no one other than the
    // external helper app service should see APPLICATION_GUESS_FROM_EXT.
    mContentType = APPLICATION_OCTET_STREAM;
    aChannel->SetContentType(NS_LITERAL_CSTRING(APPLICATION_OCTET_STREAM));
  }

  // Check whether the data should be forced to be handled externally.  This
  // could happen because the Content-Disposition header is set so, or, in the
  // future, because the user has specified external handling for the MIME
  // type.
  bool forceExternalHandling = false;
  uint32_t disposition;
  rv = aChannel->GetContentDisposition(&disposition);

  if (NS_SUCCEEDED(rv) && disposition == nsIChannel::DISPOSITION_ATTACHMENT) {
    forceExternalHandling = true;
  }

  LOG(("  forceExternalHandling: %s", forceExternalHandling ? "yes" : "no"));

  if (!forceExternalHandling) {
    //
    // First step: See whether m_contentListener wants to handle this
    // content type.
    //
    if (TryDefaultContentListener(aChannel)) {
      LOG(("  Success!  Our default listener likes this type"));
      // All done here
      return NS_OK;
    }

    // If we aren't allowed to try other listeners, just skip through to
    // trying to convert the data.
    if (!(mFlags & nsIURILoader::DONT_RETARGET)) {
      //
      // Second step: See whether some other registered listener wants
      // to handle this content type.
      //
      int32_t count = mURILoader ? mURILoader->m_listeners.Count() : 0;
      nsCOMPtr<nsIURIContentListener> listener;
      for (int32_t i = 0; i < count; i++) {
        listener = do_QueryReferent(mURILoader->m_listeners[i]);
        if (listener) {
          if (TryContentListener(listener, aChannel)) {
            LOG(("  Found listener registered on the URILoader"));
            return NS_OK;
          }
        } else {
          // remove from the listener list, reset i and update count
          mURILoader->m_listeners.RemoveObjectAt(i--);
          --count;
        }
      }

      //
      // Third step: Try to find a content listener that has not yet had
      // the chance to register, as it is contained in a not-yet-loaded
      // module, but which has registered a contract ID.
      //
      nsCOMPtr<nsICategoryManager> catman =
          do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
      if (catman) {
        nsCString contractidString;
        rv = catman->GetCategoryEntry(NS_CONTENT_LISTENER_CATEGORYMANAGER_ENTRY,
                                      mContentType, contractidString);
        if (NS_SUCCEEDED(rv) && !contractidString.IsEmpty()) {
          LOG(("  Listener contractid for '%s' is '%s'", mContentType.get(),
               contractidString.get()));

          listener = do_CreateInstance(contractidString.get());
          LOG(("  Listener from category manager: 0x%p", listener.get()));

          if (listener && TryContentListener(listener, aChannel)) {
            LOG(("  Listener from category manager likes this type"));
            return NS_OK;
          }
        }
      }

      //
      // Fourth step: try to find an nsIContentHandler for our type.
      //
      nsAutoCString handlerContractID(NS_CONTENT_HANDLER_CONTRACTID_PREFIX);
      handlerContractID += mContentType;

      nsCOMPtr<nsIContentHandler> contentHandler =
          do_CreateInstance(handlerContractID.get());
      if (contentHandler) {
        LOG(("  Content handler found"));
        // Note that m_originalContext can be nullptr when running this in
        // the parent process on behalf on a docshell in the content process,
        // and in that case we only support content handlers that don't need
        // the context.
        rv = contentHandler->HandleContent(mContentType.get(),
                                           m_originalContext, request);
        // XXXbz returning an error code to represent handling the
        // content is just bizarre!
        if (rv != NS_ERROR_WONT_HANDLE_CONTENT) {
          if (NS_FAILED(rv)) {
            // The content handler has unexpectedly failed.  Cancel the request
            // just in case the handler didn't...
            LOG(("  Content handler failed.  Aborting load"));
            request->Cancel(rv);
          } else {
            LOG(("  Content handler taking over load"));
            mUsedContentHandler = true;
          }

          return rv;
        }
      }
    } else {
      LOG(
          ("  DONT_RETARGET flag set, so skipped over random other content "
           "listeners and content handlers"));
    }

    //
    // Fifth step:  If no listener prefers this type, see if any stream
    //              converters exist to transform this content type into
    //              some other.
    //
    // Don't do this if the server sent us a MIME type of "*/*" because they saw
    // it in our Accept header and got confused.
    // XXXbz have to be careful here; may end up in some sort of bizarre
    // infinite decoding loop.
    if (mContentType != anyType) {
      rv = TryStreamConversion(aChannel);
      if (NS_SUCCEEDED(rv)) {
        return NS_OK;
      }
    }
  }

  NS_ASSERTION(!m_targetStreamListener,
               "If we found a listener, why are we not using it?");

  if (mFlags & nsIURILoader::DONT_RETARGET) {
    LOG(
        ("  External handling forced or (listener not interested and no "
         "stream converter exists), and retargeting disallowed -> aborting"));
    return NS_ERROR_WONT_HANDLE_CONTENT;
  }

  // Before dispatching to the external helper app service, check for an HTTP
  // error page.  If we got one, we don't want to handle it with a helper app,
  // really.
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(request));
  if (httpChannel) {
    bool requestSucceeded;
    rv = httpChannel->GetRequestSucceeded(&requestSucceeded);
    if (NS_FAILED(rv) || !requestSucceeded) {
      // returning error from OnStartRequest will cancel the channel
      return NS_ERROR_FILE_NOT_FOUND;
    }
  }

  // Sixth step:
  //
  // All attempts to dispatch this content have failed.  Just pass it off to
  // the helper app service.
  //

  nsCOMPtr<nsIExternalHelperAppService> helperAppService =
      do_GetService(NS_EXTERNALHELPERAPPSERVICE_CONTRACTID, &rv);
  if (helperAppService) {
    LOG(("  Passing load off to helper app service"));

    // Set these flags to indicate that the channel has been targeted and that
    // we are not using the original consumer.
    nsLoadFlags loadFlags = 0;
    request->GetLoadFlags(&loadFlags);
    request->SetLoadFlags(loadFlags | nsIChannel::LOAD_RETARGETED_DOCUMENT_URI |
                          nsIChannel::LOAD_TARGETED);

    if (isGuessFromExt) {
      mContentType = APPLICATION_GUESS_FROM_EXT;
      aChannel->SetContentType(NS_LITERAL_CSTRING(APPLICATION_GUESS_FROM_EXT));
    }

    rv = TryExternalHelperApp(helperAppService, aChannel);
    if (NS_FAILED(rv)) {
      request->SetLoadFlags(loadFlags);
      m_targetStreamListener = nullptr;
    }
  }

  NS_ASSERTION(m_targetStreamListener || NS_FAILED(rv),
               "There is no way we should be successful at this point without "
               "a m_targetStreamListener");
  return rv;
}

nsresult nsDocumentOpenInfo::TryExternalHelperApp(
    nsIExternalHelperAppService* aHelperAppService, nsIChannel* aChannel) {
  return aHelperAppService->DoContent(mContentType, aChannel, m_originalContext,
                                      false, nullptr,
                                      getter_AddRefs(m_targetStreamListener));
}

nsresult nsDocumentOpenInfo::ConvertData(nsIRequest* request,
                                         nsIURIContentListener* aListener,
                                         const nsACString& aSrcContentType,
                                         const nsACString& aOutContentType) {
  LOG(("[0x%p] nsDocumentOpenInfo::ConvertData from '%s' to '%s'", this,
       PromiseFlatCString(aSrcContentType).get(),
       PromiseFlatCString(aOutContentType).get()));

  if (mDataConversionDepthLimit == 0) {
    LOG(
        ("[0x%p] nsDocumentOpenInfo::ConvertData - reached the recursion "
         "limit!",
         this));
    // This will fall back to external helper app handling.
    return NS_ERROR_ABORT;
  }

  MOZ_ASSERT(aSrcContentType != aOutContentType,
             "ConvertData called when the two types are the same!");

  nsresult rv = NS_OK;

  nsCOMPtr<nsIStreamConverterService> StreamConvService =
      do_GetService(NS_STREAMCONVERTERSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  LOG(("  Got converter service"));

  // When applying stream decoders, it is necessary to "insert" an
  // intermediate nsDocumentOpenInfo instance to handle the targeting of
  // the "final" stream or streams.
  //
  // For certain content types (ie. multi-part/x-mixed-replace) the input
  // stream is split up into multiple destination streams.  This
  // intermediate instance is used to target these "decoded" streams...
  //
  RefPtr<nsDocumentOpenInfo> nextLink = Clone();

  LOG(("  Downstream DocumentOpenInfo would be: 0x%p", nextLink.get()));

  // Decrease the conversion recursion limit by one to prevent infinite loops.
  nextLink->mDataConversionDepthLimit = mDataConversionDepthLimit - 1;

  // Make sure nextLink starts with the contentListener that said it wanted
  // the results of this decode.
  nextLink->m_contentListener = aListener;
  // Also make sure it has to look for a stream listener to pump data into.
  nextLink->m_targetStreamListener = nullptr;

  // Make sure that nextLink treats the data as aOutContentType when
  // dispatching; that way even if the stream converters don't change the type
  // on the channel we will still do the right thing.  If aOutContentType is
  // */*, that's OK -- that will just indicate to nextLink that it should get
  // the type off the channel.
  nextLink->mContentType = aOutContentType;

  // The following call sets m_targetStreamListener to the input end of the
  // stream converter and sets the output end of the stream converter to
  // nextLink.  As we pump data into m_targetStreamListener the stream
  // converter will convert it and pass the converted data to nextLink.
  return StreamConvService->AsyncConvertData(
      PromiseFlatCString(aSrcContentType).get(),
      PromiseFlatCString(aOutContentType).get(), nextLink, request,
      getter_AddRefs(m_targetStreamListener));
}

nsresult nsDocumentOpenInfo::TryStreamConversion(nsIChannel* aChannel) {
  NS_NAMED_LITERAL_CSTRING(anyType, "*/*");
  nsresult rv = ConvertData(aChannel, m_contentListener, mContentType, anyType);
  if (NS_FAILED(rv)) {
    m_targetStreamListener = nullptr;
  } else if (m_targetStreamListener) {
    // We found a converter for this MIME type.  We'll just pump data into
    // it and let the downstream nsDocumentOpenInfo handle things.
    LOG(("  Converter taking over now"));
  }
  return rv;
}

bool nsDocumentOpenInfo::TryContentListener(nsIURIContentListener* aListener,
                                            nsIChannel* aChannel) {
  LOG(("[0x%p] nsDocumentOpenInfo::TryContentListener; mFlags = 0x%x", this,
       mFlags));

  MOZ_ASSERT(aListener, "Must have a non-null listener");
  MOZ_ASSERT(aChannel, "Must have a channel");

  bool listenerWantsContent = false;
  nsCString typeToUse;

  if (mFlags & nsIURILoader::IS_CONTENT_PREFERRED) {
    aListener->IsPreferred(mContentType.get(), getter_Copies(typeToUse),
                           &listenerWantsContent);
  } else {
    aListener->CanHandleContent(mContentType.get(), false,
                                getter_Copies(typeToUse),
                                &listenerWantsContent);
  }
  if (!listenerWantsContent) {
    LOG(("  Listener is not interested"));
    return false;
  }

  if (!typeToUse.IsEmpty() && typeToUse != mContentType) {
    // Need to do a conversion here.

    nsresult rv = NS_ERROR_NOT_AVAILABLE;
    if (mAllowListenerConversions) {
      rv = ConvertData(aChannel, aListener, mContentType, typeToUse);
    }

    if (NS_FAILED(rv)) {
      // No conversion path -- we don't want this listener, if we got one
      m_targetStreamListener = nullptr;
    }

    LOG(("  Found conversion: %s", m_targetStreamListener ? "yes" : "no"));

    // m_targetStreamListener is now the input end of the converter, and we can
    // just pump the data in there, if it exists.  If it does not, we need to
    // try other nsIURIContentListeners.
    return m_targetStreamListener != nullptr;
  }

  // At this point, aListener wants data of type mContentType.  Let 'em have
  // it.  But first, if we are retargeting, set an appropriate flag on the
  // channel
  nsLoadFlags loadFlags = 0;
  aChannel->GetLoadFlags(&loadFlags);

  // Set this flag to indicate that the channel has been targeted at a final
  // consumer.  This load flag is tested in nsDocLoader::OnProgress.
  nsLoadFlags newLoadFlags = nsIChannel::LOAD_TARGETED;

  nsCOMPtr<nsIURIContentListener> originalListener =
      do_GetInterface(m_originalContext);
  if (originalListener != aListener) {
    newLoadFlags |= nsIChannel::LOAD_RETARGETED_DOCUMENT_URI;
  }
  aChannel->SetLoadFlags(loadFlags | newLoadFlags);

  bool abort = false;
  bool isPreferred = (mFlags & nsIURILoader::IS_CONTENT_PREFERRED) != 0;
  nsresult rv =
      aListener->DoContent(mContentType, isPreferred, aChannel,
                           getter_AddRefs(m_targetStreamListener), &abort);

  if (NS_FAILED(rv)) {
    LOG_ERROR(("  DoContent failed"));

    // Unset the RETARGETED_DOCUMENT_URI flag if we set it...
    aChannel->SetLoadFlags(loadFlags);
    m_targetStreamListener = nullptr;
    return false;
  }

  if (abort) {
    // Nothing else to do here -- aListener is handling it all.  Make
    // sure m_targetStreamListener is null so we don't do anything
    // after this point.
    LOG(("  Listener has aborted the load"));
    m_targetStreamListener = nullptr;
  }

  NS_ASSERTION(abort || m_targetStreamListener,
               "DoContent returned no listener?");

  // aListener is handling the load from this point on.
  return true;
}

bool nsDocumentOpenInfo::TryDefaultContentListener(nsIChannel* aChannel) {
  if (m_contentListener) {
    return TryContentListener(m_contentListener, aChannel);
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Implementation of nsURILoader
///////////////////////////////////////////////////////////////////////////////////////////////

nsURILoader::nsURILoader() {}

nsURILoader::~nsURILoader() {}

NS_IMPL_ADDREF(nsURILoader)
NS_IMPL_RELEASE(nsURILoader)

NS_INTERFACE_MAP_BEGIN(nsURILoader)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIURILoader)
  NS_INTERFACE_MAP_ENTRY(nsIURILoader)
NS_INTERFACE_MAP_END

NS_IMETHODIMP nsURILoader::RegisterContentListener(
    nsIURIContentListener* aContentListener) {
  nsresult rv = NS_OK;

  nsWeakPtr weakListener = do_GetWeakReference(aContentListener);
  NS_ASSERTION(weakListener,
               "your URIContentListener must support weak refs!\n");

  if (weakListener) m_listeners.AppendObject(weakListener);

  return rv;
}

NS_IMETHODIMP nsURILoader::UnRegisterContentListener(
    nsIURIContentListener* aContentListener) {
  nsWeakPtr weakListener = do_GetWeakReference(aContentListener);
  if (weakListener) m_listeners.RemoveObject(weakListener);

  return NS_OK;
}

NS_IMETHODIMP nsURILoader::OpenURI(nsIChannel* channel, uint32_t aFlags,
                                   nsIInterfaceRequestor* aWindowContext) {
  NS_ENSURE_ARG_POINTER(channel);

  if (LOG_ENABLED()) {
    nsCOMPtr<nsIURI> uri;
    channel->GetURI(getter_AddRefs(uri));
    nsAutoCString spec;
    uri->GetAsciiSpec(spec);
    LOG(("nsURILoader::OpenURI for %s", spec.get()));
  }

  nsCOMPtr<nsIStreamListener> loader;
  nsresult rv = OpenChannel(channel, aFlags, aWindowContext, false,
                            getter_AddRefs(loader));
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_WONT_HANDLE_CONTENT) {
      // Not really an error, from this method's point of view
      return NS_OK;
    }
  }

  if (aFlags & nsIURILoader::REDIRECTED_CHANNEL) {
    // Our channel was redirected from another process, so doesn't need to
    // be opened again. However, it does need its listener hooked up
    // correctly.
    if (nsCOMPtr<nsIChildChannel> childChannel = do_QueryInterface(channel)) {
      return childChannel->CompleteRedirectSetup(loader);
    }

    // It's possible for the redirected channel to not implement
    // nsIChildChannel and be entirely local (like srcdoc). In that case we
    // can just open the local instance and it will work.
  }

  // This method is not complete. Eventually, we should first go
  // to the content listener and ask them for a protocol handler...
  // if they don't give us one, we need to go to the registry and get
  // the preferred protocol handler.

  // But for now, I'm going to let necko do the work for us....
  rv = channel->AsyncOpen(loader);

  // no content from this load - that's OK.
  if (rv == NS_ERROR_NO_CONTENT) {
    LOG(("  rv is NS_ERROR_NO_CONTENT -- doing nothing"));
    return NS_OK;
  }
  return rv;
}

nsresult nsURILoader::OpenChannel(nsIChannel* channel, uint32_t aFlags,
                                  nsIInterfaceRequestor* aWindowContext,
                                  bool aChannelIsOpen,
                                  nsIStreamListener** aListener) {
  NS_ASSERTION(channel, "Trying to open a null channel!");
  NS_ASSERTION(aWindowContext, "Window context must not be null");

  if (LOG_ENABLED()) {
    nsCOMPtr<nsIURI> uri;
    channel->GetURI(getter_AddRefs(uri));
    nsAutoCString spec;
    uri->GetAsciiSpec(spec);
    LOG(("nsURILoader::OpenChannel for %s", spec.get()));
  }

  // Let the window context's uriListener know that the open is starting. This
  // gives that window a chance to abort the load process.
  nsCOMPtr<nsIURIContentListener> winContextListener(
      do_GetInterface(aWindowContext));
  if (winContextListener) {
    nsCOMPtr<nsIURI> uri;
    channel->GetURI(getter_AddRefs(uri));
    if (uri) {
      bool doAbort = false;
      winContextListener->OnStartURIOpen(uri, &doAbort);

      if (doAbort) {
        LOG(("  OnStartURIOpen aborted load"));
        return NS_ERROR_WONT_HANDLE_CONTENT;
      }
    }
  }

  static bool once = InitPreferences();
  mozilla::Unused << once;

  // we need to create a DocumentOpenInfo object which will go ahead and open
  // the url and discover the content type....
  RefPtr<nsDocumentOpenInfo> loader =
      new nsDocumentOpenInfo(aWindowContext, aFlags, this);

  // Set the correct loadgroup on the channel
  nsCOMPtr<nsILoadGroup> loadGroup(do_GetInterface(aWindowContext));

  if (!loadGroup) {
    // XXXbz This context is violating what we'd like to be the new uriloader
    // api.... Set up a nsDocLoader to handle the loadgroup for this context.
    // This really needs to go away!
    nsCOMPtr<nsIURIContentListener> listener(do_GetInterface(aWindowContext));
    if (listener) {
      nsCOMPtr<nsISupports> cookie;
      listener->GetLoadCookie(getter_AddRefs(cookie));
      if (!cookie) {
        RefPtr<nsDocLoader> newDocLoader = new nsDocLoader();
        nsresult rv = newDocLoader->Init();
        if (NS_FAILED(rv)) return rv;
        rv = nsDocLoader::AddDocLoaderAsChildOfRoot(newDocLoader);
        if (NS_FAILED(rv)) return rv;
        cookie = nsDocLoader::GetAsSupports(newDocLoader);
        listener->SetLoadCookie(cookie);
      }
      loadGroup = do_GetInterface(cookie);
    }
  }

  // If the channel is pending, then we need to remove it from its current
  // loadgroup
  nsCOMPtr<nsILoadGroup> oldGroup;
  channel->GetLoadGroup(getter_AddRefs(oldGroup));
  if (aChannelIsOpen && !SameCOMIdentity(oldGroup, loadGroup)) {
    // It is important to add the channel to the new group before
    // removing it from the old one, so that the load isn't considered
    // done as soon as the request is removed.
    loadGroup->AddRequest(channel, nullptr);

    if (oldGroup) {
      oldGroup->RemoveRequest(channel, nullptr, NS_BINDING_RETARGETED);
    }
  }

  channel->SetLoadGroup(loadGroup);

  // prepare the loader for receiving data
  nsresult rv = loader->Prepare();
  if (NS_SUCCEEDED(rv)) NS_ADDREF(*aListener = loader);
  return rv;
}

NS_IMETHODIMP nsURILoader::OpenChannel(nsIChannel* channel, uint32_t aFlags,
                                       nsIInterfaceRequestor* aWindowContext,
                                       nsIStreamListener** aListener) {
  bool pending;
  if (NS_FAILED(channel->IsPending(&pending))) {
    pending = false;
  }

  return OpenChannel(channel, aFlags, aWindowContext, pending, aListener);
}

NS_IMETHODIMP nsURILoader::Stop(nsISupports* aLoadCookie) {
  nsresult rv;
  nsCOMPtr<nsIDocumentLoader> docLoader;

  NS_ENSURE_ARG_POINTER(aLoadCookie);

  docLoader = do_GetInterface(aLoadCookie, &rv);
  if (docLoader) {
    rv = docLoader->Stop();
  }
  return rv;
}
