/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsURILoader.h"
#include "nsAutoPtr.h"
#include "nsIURIContentListener.h"
#include "nsIContentHandler.h"
#include "nsILoadGroup.h"
#include "nsIDocumentLoader.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"
#include "nsIChannel.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProgressEventSink.h"
#include "nsIInputStream.h"
#include "nsIStreamConverterService.h"
#include "nsWeakReference.h"
#include "nsIHttpChannel.h"
#include "nsIMultiPartChannel.h"
#include "netCore.h"
#include "nsCRT.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"

#include "nsXPIDLString.h"
#include "nsString.h"

#include "nsIDOMWindowInternal.h"
#include "nsReadableUtils.h"
#include "nsDOMError.h"

#include "nsICategoryManager.h"
#include "nsCExternalHandlerService.h" // contains contractids for the helper app service

#include "nsIMIMEHeaderParam.h"
#include "nsNetCID.h"

#include "nsMimeTypes.h"

static NS_DEFINE_CID(kStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);
#ifdef PR_LOGGING
PRLogModuleInfo* nsURILoader::mLog = nsnull;
#endif

#define LOG(args) PR_LOG(nsURILoader::mLog, PR_LOG_DEBUG, args)
#define LOG_ERROR(args) PR_LOG(nsURILoader::mLog, PR_LOG_ERROR, args)
#define LOG_ENABLED() PR_LOG_TEST(nsURILoader::mLog, PR_LOG_DEBUG)

/**
 * The nsDocumentOpenInfo contains the state required when a single
 * document is being opened in order to discover the content type...
 * Each instance remains alive until its target URL has been loaded
 * (or aborted).
 */
class nsDocumentOpenInfo : public nsIStreamListener
{
public:
  // Needed for nsCOMPtr to work right... Don't call this!
  nsDocumentOpenInfo();

  // Real constructor
  nsDocumentOpenInfo(nsIInterfaceRequestor* aWindowContext,
                     PRBool aIsContentPreferred,
                     nsURILoader* aURILoader);

  NS_DECL_ISUPPORTS

  nsresult Open(nsIChannel* channel);

  // Call this (from OnStartRequest) to attempt to find an nsIStreamListener to
  // take the data off our hands.
  nsresult DispatchContent(nsIRequest *request, nsISupports * aCtxt);

  // Call this if we need to insert a stream converter from aSrcContentType to
  // aOutContentType into the StreamListener chain.  DO NOT call it if the two
  // types are the same, since no conversion is needed in that case.
  nsresult ConvertData(nsIRequest *request,
                       nsIURIContentListener *aListener,
                       const nsACString & aSrcContentType,
                       const nsACString & aOutContentType);

  /**
   * Function to attempt to use aListener to handle the load.  If
   * PR_TRUE is returned, nothing else needs to be done; if PR_FALSE
   * is returned, then a different way of handling the load should be
   * tried.
   */
  PRBool TryContentListener(nsIURIContentListener* aListener,
                            nsIChannel* aChannel);

  // nsIRequestObserver methods:
  NS_DECL_NSIREQUESTOBSERVER

  // nsIStreamListener methods:
  NS_DECL_NSISTREAMLISTENER

protected:
  ~nsDocumentOpenInfo();

protected:
  /**
   * The first content listener to try dispatching data to.  Typically
   * the listener associated with the entity that originated the load.
   */
  nsCOMPtr<nsIURIContentListener> m_contentListener;

  /**
   * The stream listener to forward nsIStreamListener notifications
   * to.  This is set once the load is dispatched.
   */
  nsCOMPtr<nsIStreamListener> m_targetStreamListener;

  /**
   * A pointer to the entity that originated the load. We depend on getting
   * things like nsIURIContentListeners, nsIDOMWindows, etc off of it.
   */
  nsCOMPtr<nsIInterfaceRequestor> m_originalContext;

  /**
   * Boolean to pass to CanHandleContent (also determines whether we
   * use CanHandleContent or IsPreferred).
   */
  PRBool mIsContentPreferred;

  /**
   * The type of the data we will be trying to dispatch.
   */
  nsCString mContentType;

  /**
   * Reference to the URILoader service so we can access its list of
   * nsIURIContentListeners.
   */
  nsRefPtr<nsURILoader> mURILoader;
};

NS_IMPL_THREADSAFE_ADDREF(nsDocumentOpenInfo)
NS_IMPL_THREADSAFE_RELEASE(nsDocumentOpenInfo)

NS_INTERFACE_MAP_BEGIN(nsDocumentOpenInfo)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
NS_INTERFACE_MAP_END_THREADSAFE

nsDocumentOpenInfo::nsDocumentOpenInfo()
{
  NS_NOTREACHED("This should never be called\n");
}

nsDocumentOpenInfo::nsDocumentOpenInfo(nsIInterfaceRequestor* aWindowContext,
                                       PRBool aIsContentPreferred,
                                       nsURILoader* aURILoader)
  : m_originalContext(aWindowContext),
    mIsContentPreferred(aIsContentPreferred),
    mURILoader(aURILoader)
{
}

nsDocumentOpenInfo::~nsDocumentOpenInfo()
{
}

nsresult nsDocumentOpenInfo::Open(nsIChannel *aChannel)
{
  LOG(("[0x%p] nsDocumentOpenInfo::Open", this));
    
  // this method is not complete!!! Eventually, we should first go
  // to the content listener and ask them for a protocol handler...
  // if they don't give us one, we need to go to the registry and get
  // the preferred protocol handler. 

  // But for now, I'm going to let necko do the work for us....

  nsresult rv;

  // ask our window context if it has a uri content listener...
  m_contentListener = do_GetInterface(m_originalContext, &rv);
  if (NS_FAILED(rv)) return rv;

  // now just open the channel!
  rv = aChannel->AsyncOpen(this, nsnull);

  // no content from this load - that's OK.
  if (rv == NS_ERROR_DOM_RETVAL_UNDEFINED ||
      rv == NS_ERROR_NO_CONTENT) {
    LOG(("  rv is NS_ERROR_DOM_RETVAL_UNDEFINED or NS_ERROR_NO_CONTENT -- doing nothing"));
    rv = NS_OK;
  }

  return rv;
}

NS_IMETHODIMP nsDocumentOpenInfo::OnStartRequest(nsIRequest *request, nsISupports * aCtxt)
{
  LOG(("[0x%p] nsDocumentOpenInfo::OnStartRequest", this));
  
  nsresult rv = NS_OK;

  //
  // Deal with "special" HTTP responses:
  //
  // - In the case of a 204 (No Content)  or 205 (Reset Content) response, do not try to find a
  //   content handler.  Just return.  This causes the request to be
  //   ignored.
  //
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(request, &rv));

  if (NS_SUCCEEDED(rv)) {
    PRUint32 responseCode = 0;

    rv = httpChannel->GetResponseStatus(&responseCode);

    if (NS_FAILED(rv)) {
      LOG_ERROR(("  Failed to get HTTP response status"));
      
      // behave as in the canceled case
      return NS_OK;
    }

    LOG(("  HTTP response status: %d", responseCode));

    if (204 == responseCode || 205 == responseCode) {
      return NS_OK;
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
    LOG_ERROR(("  Request failed, status: 0x%08X", rv));
  
    //
    // The transaction has already reported an error - so it will be torn
    // down. Therefore, it is not necessary to return an error code...
    //
    return NS_OK;
  }

  if (httpChannel && mContentType.IsEmpty()) {
    // This is our initial dispatch, and this is an HTTP channel.  Check for
    // the text/plain mess.
    nsCAutoString contentType;
    httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("Content-Type"),
                                   contentType);
    // Make sure to do a case-sensitive exact match comparison here.  Apache
    // 1.x just sends text/plain for "unknown", while Apache 2.x sends
    // text/plain with a ISO-8859-1 charset.  Debian's Apache version, just to
    // be different, sends text/plain with iso-8859-1 charset.  Don't do
    // general case-insensitive comparison, since we really want to apply this
    // crap as rarely as we can.
    if (contentType.EqualsLiteral("text/plain") ||
        contentType.Equals(
             NS_LITERAL_CSTRING("text/plain; charset=ISO-8859-1")) ||
        contentType.Equals(
             NS_LITERAL_CSTRING("text/plain; charset=iso-8859-1"))) {
      // Check whether we have content-encoding.  If we do, don't try to detect
      // the type, since that will lead to the content being automatically
      // decompressed....
      nsCAutoString contentEncoding;
      httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("Content-Encoding"),
                                     contentEncoding);
      if (contentEncoding.IsEmpty()) {
        // OK, this is initial dispatch of an HTTP response and its Content-Type
        // header is exactly "text/plain".  We need to check whether this is
        // really text....
        LOG(("  Possibly bogus text/plain; resetting type to " APPLICATION_MAYBE_TEXT));
        httpChannel->SetContentType(NS_LITERAL_CSTRING(APPLICATION_MAYBE_TEXT));
      }
    }
  }
  
  rv = DispatchContent(request, aCtxt);

  LOG(("  After dispatch, m_targetStreamListener: 0x%p", m_targetStreamListener.get()));
  
  if (m_targetStreamListener)
    rv = m_targetStreamListener->OnStartRequest(request, aCtxt);

  LOG(("  OnStartRequest returning: 0x%08X", rv));
  
  return rv;
}

NS_IMETHODIMP nsDocumentOpenInfo::OnDataAvailable(nsIRequest *request, nsISupports * aCtxt,
                                                  nsIInputStream * inStr, PRUint32 sourceOffset, PRUint32 count)
{
  // if we have retarged to the end stream listener, then forward the call....
  // otherwise, don't do anything

  nsresult rv = NS_OK;
  
  if (m_targetStreamListener)
    rv = m_targetStreamListener->OnDataAvailable(request, aCtxt, inStr, sourceOffset, count);
  return rv;
}

NS_IMETHODIMP nsDocumentOpenInfo::OnStopRequest(nsIRequest *request, nsISupports *aCtxt, 
                                                nsresult aStatus)
{
  LOG(("[0x%p] nsDocumentOpenInfo::OnStopRequest", this));
  
  if ( m_targetStreamListener)
  {
    nsCOMPtr<nsIStreamListener> listener(m_targetStreamListener);

    // If this is a multipart stream, we could get another
    // OnStartRequest after this... reset state.
    m_targetStreamListener = 0;
    mContentType.Truncate();
    listener->OnStopRequest(request, aCtxt, aStatus);
  }

  // Remember...
  // In the case of multiplexed streams (such as multipart/x-mixed-replace)
  // these stream listener methods could be called again :-)
  //
  return NS_OK;
}

nsresult nsDocumentOpenInfo::DispatchContent(nsIRequest *request, nsISupports * aCtxt)
{
  LOG(("[0x%p] nsDocumentOpenInfo::DispatchContent for type '%s'", this, mContentType.get()));

  NS_PRECONDITION(!m_targetStreamListener,
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

  // Check whether the data should be forced to be handled externally.  This
  // could happen because the Content-Disposition header is set so, or, in the
  // future, because the user has specified external handling for the MIME
  // type.
  PRBool forceExternalHandling = PR_FALSE;
  nsCAutoString disposition;
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(request));
  if (httpChannel)
  {
    rv = httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("content-disposition"),
                                        disposition);
  }
  else
  {
    nsCOMPtr<nsIMultiPartChannel> multipartChannel(do_QueryInterface(request));
    if (multipartChannel)
    {
      rv = multipartChannel->GetContentDisposition(disposition);
    }
  }

  LOG(("  Disposition header: '%s'", disposition.get()));

  if (NS_SUCCEEDED(rv) && !disposition.IsEmpty())
  {
    nsCOMPtr<nsIMIMEHeaderParam> mimehdrpar = do_GetService(NS_MIMEHEADERPARAM_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
    {
      nsAutoString dispToken;
      // Get the disposition type
      rv = mimehdrpar->GetParameter(disposition, "", EmptyCString(), 
                                    PR_FALSE, nsnull, dispToken);
      // RFC 2183, section 2.8 says that an unknown disposition
      // value should be treated as "attachment"
      if (NS_FAILED(rv) || 
          (!dispToken.LowerCaseEqualsLiteral("inline") &&
          // Broken sites just send
          // Content-Disposition: filename="file"
          // without a disposition token... screen those out.
           !dispToken.EqualsIgnoreCase("filename", 8)) &&
          // Also in use is Content-Disposition: name="file"
           !dispToken.EqualsIgnoreCase("name", 4))
        // We have a content-disposition of "attachment" or unknown
        forceExternalHandling = PR_TRUE;
    }
  }

  LOG(("  forceExternalHandling: %s", forceExternalHandling ? "yes" : "no"));
    
  // We're going to try to find a contentListener that can handle our data
  nsCOMPtr<nsIURIContentListener> contentListener;
  // The type or data the contentListener wants.
  nsXPIDLCString desiredContentType;

  if (!forceExternalHandling)
  {
    //
    // First step: See whether m_contentListener wants to handle this
    // content type.
    //
    if (m_contentListener && TryContentListener(m_contentListener, aChannel)) {
      LOG(("  Success!  Our default listener likes this type"));
      // All done here
      return NS_OK;
    }

    //
    // Second step: See whether some other registered listener wants
    // to handle this content type.
    //
    PRInt32 count = mURILoader->m_listeners.Count();
    nsCOMPtr<nsIURIContentListener> listener;
    for (PRInt32 i = 0; i < count; i++) {
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
      nsXPIDLCString contractidString;
      rv = catman->GetCategoryEntry(NS_CONTENT_LISTENER_CATEGORYMANAGER_ENTRY,
                                    mContentType.get(),
                                    getter_Copies(contractidString));
      if (NS_SUCCEEDED(rv) && !contractidString.IsEmpty()) {
        LOG(("  Listener contractid for '%s' is '%s'",
             mContentType.get(), contractidString.get()));

        listener = do_CreateInstance(contractidString);
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
    nsCAutoString handlerContractID (NS_CONTENT_HANDLER_CONTRACTID_PREFIX);
    handlerContractID += mContentType;

    nsCOMPtr<nsIContentHandler> contentHandler =
      do_CreateInstance(handlerContractID.get());
    if (contentHandler) {
      LOG(("  Content handler found"));
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
        }
#ifdef PR_LOGGING
        else {
          LOG(("  Content handler taking over load"));
        }
#endif

        return rv;
      }
    }
  }

  NS_ASSERTION(!m_targetStreamListener,
               "If we found a listener, why are we not using it?");
  
  //
  // Fifth step:  If no listener prefers this type, see if any stream
  //              converters exist to transform this content type into
  //              some other.
  //

  // We always want to do this, since even content being forced to
  // be handled externally may need decoding (eg via the unknown
  // content decoder).
  // Don't do this if the server sent us a MIME type of "*/*" because they saw
  // it in our Accept header and got confused.
  // XXXbz have to be careful here; may end up in some sort of bizarre infinite
  // decoding loop.
  if (mContentType != anyType) {
    rv = ConvertData(request, m_contentListener, mContentType, anyType);
    if (NS_FAILED(rv)) {
      m_targetStreamListener = nsnull;
    } else if (m_targetStreamListener) {
      // We found a converter for this MIME type.  We'll just pump data into it
      // and let the downstream nsDocumentOpenInfo handle things.
      LOG(("  Converter taking over now"));
      return NS_OK;
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
    request->SetLoadFlags(loadFlags | nsIChannel::LOAD_RETARGETED_DOCUMENT_URI
                                    | nsIChannel::LOAD_TARGETED);

    rv = helperAppService->DoContent(mContentType,
                                     request,
                                     m_originalContext,
                                     getter_AddRefs(m_targetStreamListener));
    if (NS_FAILED(rv)) {
      request->SetLoadFlags(loadFlags);
      m_targetStreamListener = nsnull;
    }
  }
      
  NS_ASSERTION(m_targetStreamListener || NS_FAILED(rv),
               "There is no way we should be successful at this point without a m_targetStreamListener");
  return rv;
}

nsresult
nsDocumentOpenInfo::ConvertData(nsIRequest *request,
                                nsIURIContentListener* aListener,
                                const nsACString& aSrcContentType,
                                const nsACString& aOutContentType)
{
  LOG(("[0x%p] nsDocumentOpenInfo::ConvertData from '%s' to '%s'", this,
       PromiseFlatCString(aSrcContentType).get(),
       PromiseFlatCString(aOutContentType).get()));

  NS_PRECONDITION(aSrcContentType != aOutContentType,
                  "ConvertData called when the two types are the same!");
  nsresult rv = NS_OK;

  nsCOMPtr<nsIStreamConverterService> StreamConvService = 
    do_GetService(kStreamConverterServiceCID, &rv);
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
  nsCOMPtr<nsDocumentOpenInfo> nextLink =
    new nsDocumentOpenInfo(m_originalContext, mIsContentPreferred, mURILoader);
  if (!nextLink) return NS_ERROR_OUT_OF_MEMORY;

  LOG(("  Downstream DocumentOpenInfo would be: 0x%p", nextLink.get()));
  
  // Make sure nextLink starts with the contentListener that said it wanted the
  // results of this decode.
  nextLink->m_contentListener = aListener;
  // Also make sure it has to look for a stream listener to pump data into.
  nextLink->m_targetStreamListener = nsnull;

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
  return StreamConvService->AsyncConvertData(PromiseFlatCString(aSrcContentType).get(), 
                                             PromiseFlatCString(aOutContentType).get(), 
                                             nextLink, 
                                             request,
                                             getter_AddRefs(m_targetStreamListener));
}

PRBool
nsDocumentOpenInfo::TryContentListener(nsIURIContentListener* aListener,
                                       nsIChannel* aChannel)
{
  LOG(("[0x%p] nsDocumentOpenInfo::TryContentListener; mIsContentPreferred = %s",
       this, mIsContentPreferred ? "true" : "false"));

  NS_PRECONDITION(aListener, "Must have a non-null listener");
  NS_PRECONDITION(aChannel, "Must have a channel");
  
  PRBool listenerWantsContent = PR_FALSE;
  nsXPIDLCString typeToUse;
  
  if (mIsContentPreferred) {
    aListener->IsPreferred(mContentType.get(),
                           getter_Copies(typeToUse),
                           &listenerWantsContent);
  } else {
    aListener->CanHandleContent(mContentType.get(), PR_FALSE,
                                getter_Copies(typeToUse),
                                &listenerWantsContent);
  }
  if (!listenerWantsContent) {
    LOG(("  Listener is not interested"));
    return PR_FALSE;
  }

  if (!typeToUse.IsEmpty() && typeToUse != mContentType) {
    // Need to do a conversion here.

    nsresult rv = ConvertData(aChannel, aListener, mContentType, typeToUse);

    if (NS_FAILED(rv)) {
      // No conversion path -- we don't want this listener, if we got one
      m_targetStreamListener = nsnull;
    }

    LOG(("  Found conversion: %s", m_targetStreamListener ? "yes" : "no"));
    
    // m_targetStreamListener is now the input end of the converter, and we can
    // just pump the data in there, if it exists.  If it does not, we need to
    // try other nsIURIContentListeners.
    return m_targetStreamListener != nsnull;
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
  
  PRBool abort = PR_FALSE;
  nsresult rv = aListener->DoContent(mContentType.get(),
                                     mIsContentPreferred,
                                     aChannel,
                                     getter_AddRefs(m_targetStreamListener),
                                     &abort);
    
  if (NS_FAILED(rv)) {
    LOG_ERROR(("  DoContent failed"));
    
    // Unset the RETARGETED_DOCUMENT_URI flag if we set it...
    aChannel->SetLoadFlags(loadFlags);
    m_targetStreamListener = nsnull;
    return PR_FALSE;
  }

  if (abort) {
    // Nothing else to do here -- aListener is handling it all.  Make
    // sure m_targetStreamListener is null so we don't do anything
    // after this point.
    LOG(("  Listener has aborted the load"));
    m_targetStreamListener = nsnull;
  }

  NS_ASSERTION(abort || m_targetStreamListener, "DoContent returned no listener?");

  // aListener is handling the load from this point on.  
  return PR_TRUE;
}


///////////////////////////////////////////////////////////////////////////////////////////////
// Implementation of nsURILoader
///////////////////////////////////////////////////////////////////////////////////////////////

nsURILoader::nsURILoader()
{
#ifdef PR_LOGGING
  if (!mLog) {
    mLog = PR_NewLogModule("URILoader");
  }
#endif
}

nsURILoader::~nsURILoader()
{
}

NS_IMPL_ADDREF(nsURILoader)
NS_IMPL_RELEASE(nsURILoader)

NS_INTERFACE_MAP_BEGIN(nsURILoader)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIURILoader)
   NS_INTERFACE_MAP_ENTRY(nsIURILoader)
NS_INTERFACE_MAP_END

NS_IMETHODIMP nsURILoader::RegisterContentListener(nsIURIContentListener * aContentListener)
{
  nsresult rv = NS_OK;

  nsWeakPtr weakListener = do_GetWeakReference(aContentListener);
  NS_ASSERTION(weakListener, "your URIContentListener must support weak refs!\n");
  
  if (weakListener)
    m_listeners.AppendObject(weakListener);

  return rv;
} 

NS_IMETHODIMP nsURILoader::UnRegisterContentListener(nsIURIContentListener * aContentListener)
{
  nsWeakPtr weakListener = do_GetWeakReference(aContentListener);
  if (weakListener)
    m_listeners.RemoveObject(weakListener);

  return NS_OK;
  
}

NS_IMETHODIMP nsURILoader::OpenURI(nsIChannel *channel, 
                                   PRBool aIsContentPreferred,
                                   nsIInterfaceRequestor *aWindowContext)
{
  NS_ENSURE_ARG_POINTER(channel);

#ifdef PR_LOGGING
  if (LOG_ENABLED()) {
    nsCOMPtr<nsIURI> uri;
    channel->GetURI(getter_AddRefs(uri));
    nsCAutoString spec;
    uri->GetAsciiSpec(spec);
    LOG(("nsURILoader::OpenURI for %s", spec.get()));
  }
#endif
  
  // Let the window context's uriListener know that the open is starting.  This
  // gives that window a chance to abort the load process.
  nsCOMPtr<nsIURIContentListener> winContextListener(do_GetInterface(aWindowContext));
  if (winContextListener) {
    nsCOMPtr<nsIURI> uri;
    channel->GetURI(getter_AddRefs(uri));
    if (uri) {
      PRBool doAbort = PR_FALSE;
      winContextListener->OnStartURIOpen(uri, &doAbort);
         
      if (doAbort) {
        LOG(("  OnStartURIOpen aborted load"));
        return NS_OK;
      }
    }   
  }

  // we need to create a DocumentOpenInfo object which will go ahead and open
  // the url and discover the content type....
  nsCOMPtr<nsDocumentOpenInfo> loader =
    new nsDocumentOpenInfo(aWindowContext, aIsContentPreferred, this);

  if (!loader) return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIInterfaceRequestor> loadCookie;
  SetupLoadCookie(aWindowContext, getter_AddRefs(loadCookie));

  // now instruct the loader to go ahead and open the url
  return loader->Open(channel);
}

NS_IMETHODIMP nsURILoader::Stop(nsISupports* aLoadCookie)
{
  nsresult rv;
  nsCOMPtr<nsIDocumentLoader> docLoader;

  NS_ENSURE_ARG_POINTER(aLoadCookie);

  docLoader = do_GetInterface(aLoadCookie, &rv);
  if (docLoader) {
    rv = docLoader->Stop();
  }
  return rv;
}

NS_IMETHODIMP
nsURILoader::GetLoadGroupForContext(nsIInterfaceRequestor* aWindowContext,
                                    nsILoadGroup ** aLoadGroup)
{
  nsresult rv;
  nsCOMPtr<nsIInterfaceRequestor> loadCookieForWindow;

  // Initialize the [out] parameter...
  *aLoadGroup = nsnull;

  NS_ENSURE_ARG(aWindowContext);

  rv = SetupLoadCookie(aWindowContext, getter_AddRefs(loadCookieForWindow));
  if (NS_FAILED(rv)) return rv;
  
  rv = CallGetInterface(loadCookieForWindow.get(), aLoadGroup);
  return rv;
}

NS_IMETHODIMP
nsURILoader::GetDocumentLoaderForContext(nsIInterfaceRequestor * aWindowContext,
                                         nsIDocumentLoader ** aDocLoader)
{
  nsresult rv;
  nsCOMPtr<nsIInterfaceRequestor> loadCookieForWindow;

  // Initialize the [out] parameter...
  *aDocLoader = nsnull;

  NS_ENSURE_ARG(aWindowContext);

  rv = SetupLoadCookie(aWindowContext, getter_AddRefs(loadCookieForWindow));
  if (NS_FAILED(rv)) return rv;
  
  rv = CallGetInterface(loadCookieForWindow.get(), aDocLoader);
  return rv;
}

nsresult nsURILoader::SetupLoadCookie(nsIInterfaceRequestor * aWindowContext, 
                                      nsIInterfaceRequestor ** aLoadCookie)
{
  // first, see if we have already set a load cookie on the cnt listener..
  // i.e. if this isn't the first time we've tried to run a url through this window
  // context then we don't need to create another load cookie, we can reuse the first one.
  nsresult rv = NS_OK;
  nsCOMPtr<nsISupports> loadCookie;

  // Initialize the [out] parameter...
  *aLoadCookie = nsnull;

  nsCOMPtr<nsIURIContentListener> cntListener (do_GetInterface(aWindowContext));
  if (cntListener) {
    // Get the load cookie for the requested window context...
    rv = cntListener->GetLoadCookie(getter_AddRefs(loadCookie));

    //
    // If we don't have a load cookie for this window context yet, then 
    // go create one! In order to create a load cookie, we need to get
    // the parent's load cookie if there is one...
    //
    if (!loadCookie) {
      nsCOMPtr<nsIURIContentListener> parentListener;
      nsCOMPtr<nsIDocumentLoader>     parentDocLoader;
      nsCOMPtr<nsIDocumentLoader>     newDocLoader;

      // Try to get the parent's load cookie...
      cntListener->GetParentContentListener(getter_AddRefs(parentListener));
      if (parentListener) {
        rv = parentListener->GetLoadCookie(getter_AddRefs(loadCookie));

        // if we had a parent cookie use it to help with the creation process      
        if (loadCookie) {
          parentDocLoader = do_GetInterface(loadCookie);
        }
      }
      // If there is no parent DocLoader, then use the global DocLoader
      // service as the parent...
      if (!parentDocLoader) {
        parentDocLoader = do_GetService(NS_DOCUMENTLOADER_SERVICE_CONTRACTID, &rv);
      }
      if (NS_FAILED(rv)) return rv;

      //
      // Create a new document loader.  The document loader represents
      // the load cookie which the uriloader hands out...
      //
      rv = parentDocLoader->CreateDocumentLoader(getter_AddRefs(newDocLoader));
      if (NS_FAILED(rv)) return rv;

      newDocLoader->QueryInterface(NS_GET_IID(nsIInterfaceRequestor), 
                                   getter_AddRefs(loadCookie)); 
      rv = cntListener->SetLoadCookie(loadCookie);
    } // if we don't have  a load cookie already
  } // if we have a cntListener

  // loadCookie may be null - for example, <a target="popupWin"> if popupWin is
  // not a defined window.  The following prevents a crash (Bug 32898)
  if (loadCookie) {
    rv = CallQueryInterface(loadCookie, aLoadCookie);
  } else {
    rv = NS_ERROR_UNEXPECTED;
  }

  return rv;
}

