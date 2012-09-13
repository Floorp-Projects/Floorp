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
#include "ProxyAutoConfig.h"
#include "nsICancelable.h"
#include "nsThreadUtils.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "mozilla/Attributes.h"
#include "mozilla/LinkedList.h"
#include "nsIThread.h"
#include "nsAutoPtr.h"
#include "nsISystemProxySettings.h"
#include "mozilla/TimeStamp.h"

class nsPACMan;

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
   * @param newPACURL
   *        This parameter holds the URL of a new PAC file that should be loaded
   *        before the query is evaluated again. At least one of pacString and
   *        newPACURL should be 0 length.
   */
  virtual void OnQueryComplete(nsresult status,
                               const nsCString &pacString,
                               const nsCString &newPACURL) = 0;
};

class PendingPACQuery MOZ_FINAL : public nsRunnable,
                                  public mozilla::LinkedListElement<PendingPACQuery>
{
public:
  PendingPACQuery(nsPACMan *pacMan, nsIURI *uri,
                  nsPACManCallback *callback, bool mainThreadResponse);

  // can be called from either thread
  void Complete(nsresult status, const nsCString &pacString);
  void UseAlternatePACFile(const nsCString &pacURL);

  nsCString                  mSpec;
  nsCString                  mScheme;
  nsCString                  mHost;
  int32_t                    mPort;

  NS_IMETHOD Run(void);     /* nsRunnable */

private:
  nsPACMan                  *mPACMan;  // weak reference
  nsRefPtr<nsPACManCallback> mCallback;
  bool                       mOnMainThreadOnly;
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
   * This method queries a PAC result asynchronously.  The callback runs on the
   * calling thread.  If the PAC file has not yet been loaded, then this method
   * will queue up the request, and complete it once the PAC file has been
   * loaded.
   * 
   * @param uri
   *        The URI to query.
   * @param callback
   *        The callback to run once the PAC result is available.
   * @param mustCallbackOnMainThread
   *        If set to false the callback can be made from the PAC thread
   */
  nsresult AsyncGetProxyForURI(nsIURI *uri, nsPACManCallback *callback,
                               bool mustCallbackOnMainThread);

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
  bool IsLoading() { return mLoader != nullptr; }

  /**
   * Returns true if the given URI matches the URI of our PAC file.
   */
  bool IsPACURI(nsIURI *uri) {
    bool result;
    return mPACURI && NS_SUCCEEDED(mPACURI->Equals(uri, &result)) && result;
  }

  bool IsPACURI(nsACString &spec)
  {
    nsAutoCString tmp;
    return (mPACURI && NS_SUCCEEDED(mPACURI->GetSpec(tmp)) && tmp.Equals(spec));
  }

  NS_HIDDEN_(nsresult) Init(nsISystemProxySettings *);
  static nsPACMan *sInstance;

  // PAC thread operations only
  void ProcessPendingQ();
  void CancelPendingQ(nsresult);

private:
  NS_DECL_NSISTREAMLOADEROBSERVER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK

  friend class PendingPACQuery;
  friend class PACLoadComplete;
  friend class ExecutePACThreadAction;

  ~nsPACMan();

  /**
   * Cancel any existing load if any.
   */
  void CancelExistingLoad();

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

  /**
   * PostQuery() only runs on the PAC thread and it is used to
   * place a pendingPACQuery into the queue and potentially
   * execute the queue if it was otherwise empty
   */
  nsresult PostQuery(PendingPACQuery *query);

  // PAC thread operations only
  void PostProcessPendingQ();
  void PostCancelPendingQ(nsresult);
  bool ProcessPending();
  void NamePACThread();

private:
  mozilla::net::ProxyAutoConfig mPAC;
  nsCOMPtr<nsIThread>           mPACThread;
  nsCOMPtr<nsISystemProxySettings> mSystemProxySettings;

  mozilla::LinkedList<PendingPACQuery> mPendingQ; /* pac thread only */

  nsCOMPtr<nsIURI>             mPACURI;
  nsCString                    mPACURISpec; // for use off main thread
  nsCOMPtr<nsIStreamLoader>    mLoader;
  bool                         mLoadPending;
  bool                         mShutdown;
  mozilla::TimeStamp           mScheduledReload;
  uint32_t                     mLoadFailureCount;

  bool                         mInProgress;
};

#endif  // nsPACMan_h__
