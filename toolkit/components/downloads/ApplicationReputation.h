/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ApplicationReputation_h__
#define ApplicationReputation_h__

#include "nsIApplicationReputation.h"
#include "nsIRequestObserver.h"
#include "nsIStreamListener.h"
#include "nsISupports.h"

#include "nsCOMPtr.h"
#include "nsString.h"

class nsIApplicationReputationListener;
class nsIChannel;
class nsIObserverService;
class nsIRequest;
class PRLogModuleInfo;

class ApplicationReputationService MOZ_FINAL :
  public nsIApplicationReputationService {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIAPPLICATIONREPUTATIONSERVICE

public:
  static ApplicationReputationService* GetSingleton();

private:
  /**
   * Global singleton object for holding this factory service.
   */
  static ApplicationReputationService* gApplicationReputationService;

  ApplicationReputationService();
  ~ApplicationReputationService();

  /**
   * Wrapper function for QueryReputation that makes it easier to ensure the
   * callback is called.
   */
  nsresult QueryReputationInternal(nsIApplicationReputationQuery* aQuery,
                                   nsIApplicationReputationCallback* aCallback);
};

/**
 * This class implements nsIApplicationReputation. See the
 * nsIApplicationReputation.idl for details. ApplicationReputation also
 * implements nsIStreamListener because it calls nsIChannel->AsyncOpen.
 */
class ApplicationReputationQuery MOZ_FINAL :
  public nsIApplicationReputationQuery,
  public nsIStreamListener {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIAPPLICATIONREPUTATIONQUERY

  ApplicationReputationQuery();
  ~ApplicationReputationQuery();

private:
  /**
   * Corresponding member variables for the attributes in the IDL.
   */
  nsString mSuggestedFileName;
  nsCOMPtr<nsIURI> mURI;
  uint32_t mFileSize;
  nsCString mSha256Hash;

  /**
   * The callback for the request.
   */
  nsCOMPtr<nsIApplicationReputationCallback> mCallback;

  /**
   * The response from the application reputation query. This is read in chunks
   * as part of our nsIStreamListener implementation and may contain embedded
   * NULLs.
   */
  nsCString mResponse;

  /**
   * Wrapper function for nsIStreamListener.onStopRequest to make it easy to
   * guarantee calling the callback
   */
  nsresult OnStopRequestInternal(nsIRequest *aRequest,
                                 nsISupports *aContext,
                                 nsresult aResult,
                                 bool* aShouldBlock);
};

#endif /* ApplicationReputation_h__ */
