/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPACMan_h__
#define nsPACMan_h__

#include "nsIStreamLoader.h"
#include "nsIInterfaceRequestor.h"
#include "nsIChannelEventSink.h"
#include "nsIProxyAutoConfig.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "prclist.h"
#include "mozilla/Attributes.h"

/**
 * This class defines a callback interface used by AsyncGetProxyForURI.
 */
class NS_NO_VTABLE nsPACManCallback : public nsISupports
{
public:
  /**
   * This method is invoked on the same thread that called AsyncGetProxyForURI.
   *
   * @param status
   *        This parameter indicates whether or not the PAC query succeeded.
   * @param pacString
   *        This parameter holds the value of the PAC string.  It is empty when
   *        status is a failure code.
   */
  virtual void OnQueryComplete(nsresult status, const nsCString &pacString) = 0;
};

/**
 * This class provides an abstraction layer above the PAC thread.  The methods
 * defined on this class are intended to be called on the main thread only.
 */
class nsPACMan MOZ_FINAL : public nsIStreamLoaderObserver
                         , public nsIInterfaceRequestor
                         , public nsIChannelEventSink
{
public:
  NS_DECL_ISUPPORTS

  nsPACMan();

  /**
   * This method may be called to shutdown the PAC manager.  Any async queries
   * that have not yet completed will either finish normally or be canceled by
   * the time this method returns.
   */
  void Shutdown();

  /**
   * This method queries a PAC result synchronously.
   *
   * @param uri
   *        The URI to query.
   * @param result
   *        Holds the PAC result string upon return.
   *
   * @return NS_ERROR_IN_PROGRESS if the PAC file is not yet loaded.
   * @return NS_ERROR_NOT_AVAILABLE if the PAC file could not be loaded.
   */
  nsresult GetProxyForURI(nsIURI *uri, nsACString &result);

  /**
   * This method queries a PAC result asynchronously.  The callback runs on the
   * calling thread.  If the PAC file has not yet been loaded, then this method
   * will queue up the request, and complete it once the PAC file has been
   * loaded.
   * 
   * @param uri
   *        The URI to query.
   * @param callback
   *        The callback to run once the PAC result is available.
   */
  nsresult AsyncGetProxyForURI(nsIURI *uri, nsPACManCallback *callback);

  /**
   * This method may be called to reload the PAC file.  While we are loading
   * the PAC file, any asynchronous PAC queries will be queued up to be
   * processed once the PAC file finishes loading.
   *
   * @param pacURI
   *        The nsIURI of the PAC file to load.  If this parameter is null,
   *        then the previous PAC URI is simply reloaded.
   */
  nsresult LoadPACFromURI(nsIURI *pacURI);

  /**
   * Returns true if we are currently loading the PAC file.
   */
  bool IsLoading() { return mLoader != nsnull; }

  /**
   * Returns true if the given URI matches the URI of our PAC file.
   */
  bool IsPACURI(nsIURI *uri) {
    bool result;
    return mPACURI && NS_SUCCEEDED(mPACURI->Equals(uri, &result)) && result;
  }

private:
  NS_DECL_NSISTREAMLOADEROBSERVER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK

  ~nsPACMan();

  /**
   * Cancel any existing load if any.
   */
  void CancelExistingLoad();

  /**
   * Process mPendingQ.  If status is a failure code, then the pending queue
   * will be emptied.  If status is a success code, then the pending requests
   * will be processed (i.e., their Start method will be called).
   */
  void ProcessPendingQ(nsresult status);

  /**
   * Start loading the PAC file.
   */
  void StartLoading();

  /**
   * Reload the PAC file if there is reason to.
   */
  void MaybeReloadPAC();

  /**
   * Called when we fail to load the PAC file.
   */
  void OnLoadFailure();

private:
  nsCOMPtr<nsIProxyAutoConfig> mPAC;
  nsCOMPtr<nsIURI>             mPACURI;
  PRCList                      mPendingQ;
  nsCOMPtr<nsIStreamLoader>    mLoader;
  bool                         mLoadPending;
  bool                         mShutdown;
  PRTime                       mScheduledReload;
  PRUint32                     mLoadFailureCount;
};

#endif  // nsPACMan_h__
