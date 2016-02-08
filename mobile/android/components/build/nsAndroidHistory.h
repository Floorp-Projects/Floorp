/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_ANDROIDHISTORY_H
#define NS_ANDROIDHISTORY_H

#include "IHistory.h"
#include "nsDataHashtable.h"
#include "nsTPriorityQueue.h"
#include "nsIRunnable.h"
#include "nsIURI.h"
#include "nsITimer.h"


#define NS_ANDROIDHISTORY_CID \
    {0xCCAA4880, 0x44DD, 0x40A7, {0xA1, 0x3F, 0x61, 0x56, 0xFC, 0x88, 0x2C, 0x0B}}

// Max size of History::mRecentlyVisitedURIs
#define RECENTLY_VISITED_URI_SIZE 8

// Max size of History::mEmbedURIs
#define EMBED_URI_SIZE 128

class nsAndroidHistory final : public mozilla::IHistory,
                               public nsIRunnable,
                               public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IHISTORY
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSITIMERCALLBACK

  /**
   * Obtains a pointer that has had AddRef called on it.  Used by the service
   * manager only.
   */
  static nsAndroidHistory* GetSingleton();

  nsAndroidHistory();

private:
  ~nsAndroidHistory() {}

  static nsAndroidHistory* sHistory;

  // Will mimic the value of the places.history.enabled preference.
  bool mHistoryEnabled;

  void LoadPrefs();
  bool ShouldRecordHistory();
  nsresult CanAddURI(nsIURI* aURI, bool* canAdd);

  /**
   * We need to manage data used to determine a:visited status.
   */
  nsDataHashtable<nsStringHashKey, nsTArray<mozilla::dom::Link *> *> mListeners;
  nsTPriorityQueue<nsString> mPendingLinkURIs;

  /**
   * Redirection (temporary and permanent) flags are sent with the redirected
   * URI, not the original URI. Since we want to ignore the original URI, we
   * need to cache the pending visit and make sure it doesn't redirect.
   */
  RefPtr<nsITimer> mTimer;
  typedef AutoTArray<nsCOMPtr<nsIURI>, RECENTLY_VISITED_URI_SIZE> PendingVisitArray;
  PendingVisitArray mPendingVisitURIs;

  bool RemovePendingVisitURI(nsIURI* aURI);
  void SaveVisitURI(nsIURI* aURI);

  /**
   * mRecentlyVisitedURIs remembers URIs which are recently added to the DB,
   * to avoid saving these locations repeatedly in a short period.
   */
  typedef AutoTArray<nsCOMPtr<nsIURI>, RECENTLY_VISITED_URI_SIZE> RecentlyVisitedArray;
  RecentlyVisitedArray mRecentlyVisitedURIs;
  RecentlyVisitedArray::index_type mRecentlyVisitedURIsNextIndex;

  void AppendToRecentlyVisitedURIs(nsIURI* aURI);
  bool IsRecentlyVisitedURI(nsIURI* aURI);

  /**
   * mEmbedURIs remembers URIs which are explicitly not added to the DB,
   * to avoid wasting time on these locations.
   */
  typedef AutoTArray<nsCOMPtr<nsIURI>, EMBED_URI_SIZE> EmbedArray;
  EmbedArray::index_type mEmbedURIsNextIndex;
  EmbedArray mEmbedURIs;

  void AppendToEmbedURIs(nsIURI* aURI);
  bool IsEmbedURI(nsIURI* aURI);
};

#endif
