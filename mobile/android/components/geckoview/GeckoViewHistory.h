/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GECKOVIEWHISTORY_H
#define GECKOVIEWHISTORY_H

#include "mozilla/BaseHistory.h"
#include "nsDataHashtable.h"
#include "nsTObserverArray.h"
#include "nsURIHashKey.h"
#include "nsINamed.h"
#include "nsITimer.h"
#include "nsIURI.h"

#include "mozilla/StaticPtr.h"

class nsIWidget;

namespace mozilla {
namespace dom {
class Document;
}
}  // namespace mozilla

struct VisitedURI {
  nsCOMPtr<nsIURI> mURI;
  bool mVisited = false;
};

class GeckoViewHistory final : public mozilla::BaseHistory,
                               public nsITimerCallback,
                               public nsINamed {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  // IHistory
  NS_IMETHOD VisitURI(nsIWidget*, nsIURI*, nsIURI* aLastVisitedURI,
                      uint32_t aFlags) final;
  NS_IMETHOD SetURITitle(nsIURI*, const nsAString&) final;

  // BaseHistory
  mozilla::Result<mozilla::Ok, nsresult> StartVisitedQuery(nsIURI*) final;
  void CancelVisitedQueryIfPossible(nsIURI*) final;

  static already_AddRefed<GeckoViewHistory> GetSingleton();

  GeckoViewHistory();

  void QueryVisitedState(nsIWidget* aWidget,
                         const nsTArray<nsCOMPtr<nsIURI>>& aURIs);
  void HandleVisitedState(const nsTArray<VisitedURI>& aVisitedURIs);

 private:
  virtual ~GeckoViewHistory();

  void QueryVisitedStateInContentProcess();
  void QueryVisitedStateInParentProcess();

  static mozilla::StaticRefPtr<GeckoViewHistory> sHistory;

  // A set of URIs for which we don't know the visited status, and need to
  // query the history delegate.
  nsTHashtable<nsURIHashKey> mNewURIs;

  nsCOMPtr<nsITimer> mQueryVisitedStateTimer;

  // Whether mQueryVisitedStateTimer is armed.
  //
  // We could just null out the timer, but this way we don't have to re-allocate
  // a timer over and over.
  bool mQueryVisitedStateTimerPending = false;
};

#endif
