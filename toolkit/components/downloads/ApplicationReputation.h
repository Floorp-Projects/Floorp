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
#include "mozilla/Logging.h"

class nsIRequest;
class PendingDBLookup;
class PendingLookup;

class ApplicationReputationService final :
  public nsIApplicationReputationService {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIAPPLICATIONREPUTATIONSERVICE

public:
  static ApplicationReputationService* GetSingleton();

private:
  friend class PendingLookup;
  friend class PendingDBLookup;
  /**
   * Global singleton object for holding this factory service.
   */
  static ApplicationReputationService* gApplicationReputationService;
  /**
   * MOZ_LOG=ApplicationReputation:5
   */
  static mozilla::LazyLogModule prlog;
  /**
   * This is a singleton, so disallow construction.
   */
  ApplicationReputationService();
  ~ApplicationReputationService();
  /**
   * Wrapper function for QueryReputation that makes it easier to ensure the
   * callback is called.
   */
  nsresult QueryReputationInternal(nsIApplicationReputationQuery* aQuery,
                                   nsIApplicationReputationCallback* aCallback);
};
#endif /* ApplicationReputation_h__ */
