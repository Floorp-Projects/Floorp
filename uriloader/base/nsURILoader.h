/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsURILoader_h__
#define nsURILoader_h__

#include "nsCURILoader.h"
#include "nsISupportsUtils.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsString.h"
#include "nsIWeakReference.h"
#include "mozilla/Attributes.h"
#include "nsIStreamListener.h"
#include "nsIThreadRetargetableStreamListener.h"
#include "nsIExternalHelperAppService.h"

#include "mozilla/Logging.h"

class nsDocumentOpenInfo;

class nsURILoader final : public nsIURILoader {
 public:
  NS_DECL_NSIURILOADER
  NS_DECL_ISUPPORTS

  nsURILoader();

 protected:
  ~nsURILoader();

  /**
   * Equivalent to nsIURILoader::openChannel, but allows specifying whether the
   * channel is opened already.
   */
  [[nodiscard]] nsresult OpenChannel(nsIChannel* channel, uint32_t aFlags,
                                     nsIInterfaceRequestor* aWindowContext,
                                     bool aChannelOpen,
                                     nsIStreamListener** aListener);

  /**
   * we shouldn't need to have an owning ref count on registered
   * content listeners because they are supposed to unregister themselves
   * when they go away. This array stores weak references
   */
  nsCOMArray<nsIWeakReference> m_listeners;

  /**
   * Logging.  The module is called "URILoader"
   */
  static mozilla::LazyLogModule mLog;

  friend class nsDocumentOpenInfo;
};

/**
 * The nsDocumentOpenInfo contains the state required when a single
 * document is being opened in order to discover the content type...
 * Each instance remains alive until its target URL has been loaded
 * (or aborted).
 */
class nsDocumentOpenInfo : public nsIThreadRetargetableStreamListener {
 public:
  // Real constructor
  // aFlags is a combination of the flags on nsIURILoader
  nsDocumentOpenInfo(nsIInterfaceRequestor* aWindowContext, uint32_t aFlags,
                     nsURILoader* aURILoader);
  nsDocumentOpenInfo(uint32_t aFlags, bool aAllowListenerConversions);

  NS_DECL_THREADSAFE_ISUPPORTS

  /**
   * Prepares this object for receiving data. The stream
   * listener methods of this class must not be called before calling this
   * method.
   */
  nsresult Prepare();

  // Call this (from OnStartRequest) to attempt to find an nsIStreamListener to
  // take the data off our hands.
  nsresult DispatchContent(nsIRequest* request);

  // Call this if we need to insert a stream converter from aSrcContentType to
  // aOutContentType into the StreamListener chain.  DO NOT call it if the two
  // types are the same, since no conversion is needed in that case.
  nsresult ConvertData(nsIRequest* request, nsIURIContentListener* aListener,
                       const nsACString& aSrcContentType,
                       const nsACString& aOutContentType);

  /**
   * Function to attempt to use aListener to handle the load.  If
   * true is returned, nothing else needs to be done; if false
   * is returned, then a different way of handling the load should be
   * tried.
   */
  bool TryContentListener(nsIURIContentListener* aListener,
                          nsIChannel* aChannel);

  /**
   * Virtual helper functions for content that we expect to be
   * overriden when running in the parent process on behalf of
   * a content process docshell.
   * We also expect nsIStreamListener functions to be overriden
   * to add functionality.
   */

  /**
   * Attempt to create a steam converter converting from the
   * current mContentType into something else.
   * Sets m_targetStreamListener if it succeeds.
   */
  virtual nsresult TryStreamConversion(nsIChannel* aChannel);

  /**
   * Attempt to use the default content listener as our stream
   * listener.
   * Sets m_targetStreamListener if it succeeds.
   */
  virtual bool TryDefaultContentListener(nsIChannel* aChannel);

  /**
   * Attempt to pass aChannel onto the external helper app service.
   * Sets m_targetStreamListener if it succeeds.
   */
  virtual nsresult TryExternalHelperApp(
      nsIExternalHelperAppService* aHelperAppService, nsIChannel* aChannel);

  /**
   * Create another nsDocumentOpenInfo like this one, so that we can chain
   * them together when we use a stream converter and don't know what the
   * converted content type is until the converter outputs OnStartRequest.
   */
  virtual nsDocumentOpenInfo* Clone() {
    return new nsDocumentOpenInfo(m_originalContext, mFlags, mURILoader);
  }

  // nsIRequestObserver methods:
  NS_DECL_NSIREQUESTOBSERVER

  // nsIStreamListener methods:
  NS_DECL_NSISTREAMLISTENER

  // nsIThreadRetargetableStreamListener
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER

 protected:
  virtual ~nsDocumentOpenInfo();

 protected:
  /**
   * The first content listener to try dispatching data to.  Typically
   * the listener associated with the entity that originated the load.
   * This can be nullptr when running in the parent process for a content
   * process docshell.
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
   * This can be nullptr when running in the parent process for a content
   * process docshell.
   */
  nsCOMPtr<nsIInterfaceRequestor> m_originalContext;

  /**
   * IS_CONTENT_PREFERRED is used for the boolean to pass to CanHandleContent
   * (also determines whether we use CanHandleContent or IsPreferred).
   * DONT_RETARGET means that we will only try m_originalContext, no other
   * listeners.
   */
  uint32_t mFlags;

  /**
   * The type of the data we will be trying to dispatch.
   */
  nsCString mContentType;

  /**
   * Reference to the URILoader service so we can access its list of
   * nsIURIContentListeners.
   * This can be nullptr when running in the parent process for a content
   * process docshell.
   */
  RefPtr<nsURILoader> mURILoader;

  /**
   * Limit of data conversion depth to prevent infinite conversion loops
   */
  uint32_t mDataConversionDepthLimit;

  /**
   * Set to true if OnStartRequest handles the content using an
   * nsIContentHandler, and the content is consumed despite
   * m_targetStreamListener being nullptr.
   */
  bool mUsedContentHandler = false;

  /**
   * True if we allow nsIURIContentListeners to return a requested
   * input typeToUse, and attempt to create a matching stream converter.
   * This is false when running in the parent process for a content process
   * docshell
   */
  bool mAllowListenerConversions = true;
};

#endif /* nsURILoader_h__ */
