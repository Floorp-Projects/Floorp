/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GECKOVIEWHISTORY_H
#define GECKOVIEWHISTORY_H

#include "IHistory.h"
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

struct TrackedURI {
  // Per `IHistory`, these are not owning references.
  nsTObserverArray<mozilla::dom::Link*> mLinks;
  bool mVisited = false;
};

class GeckoViewHistory final : public mozilla::IHistory,
                               public nsITimerCallback,
                               public nsINamed {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_IHISTORY
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  static already_AddRefed<GeckoViewHistory> GetSingleton();

  GeckoViewHistory();

  void QueryVisitedState(nsIWidget* aWidget,
                         const nsTArray<nsCOMPtr<nsIURI>>& aURIs);
  void HandleVisitedState(const nsTArray<VisitedURI>& aVisitedURIs);

 private:
  virtual ~GeckoViewHistory();

  void QueryVisitedStateInContentProcess();
  void QueryVisitedStateInParentProcess();
  void DispatchNotifyVisited(nsIURI* aURI, mozilla::dom::Document* aDocument);

  static mozilla::StaticRefPtr<GeckoViewHistory> sHistory;

  // A map of unvisited links for URIs. If the history delegate reports that
  // the URI is visited, we'll asynchronously notify and remove the links.
  nsDataHashtable<nsURIHashKey, TrackedURI> mTrackedURIs;

  // A set of URIs for which we don't know the visited status, and need to
  // query the history delegate.
  nsTHashtable<nsURIHashKey> mNewURIs;

  nsCOMPtr<nsITimer> mQueryVisitedStateTimer;
};

#endif
