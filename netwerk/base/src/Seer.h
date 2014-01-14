/* vim: set ts=2 sts=2 et sw=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_Seer_h
#define mozilla_net_Seer_h

#include "nsINetworkSeer.h"

#include "nsCOMPtr.h"
#include "nsIDNSListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsIObserver.h"
#include "nsISpeculativeConnect.h"
#include "nsProxyRelease.h"

#include "mozilla/Mutex.h"
#include "mozilla/storage/StatementCache.h"
#include "mozilla/TimeStamp.h"

class nsIDNSService;
class nsINetworkSeerVerifier;
class nsIThread;

class mozIStorageConnection;
class mozIStorageService;
class mozIStorageStatement;

namespace mozilla {
namespace net {

typedef nsMainThreadPtrHandle<nsINetworkSeerVerifier> SeerVerifierHandle;

class SeerPredictionRunner;
struct SeerTelemetryAccumulators;
class SeerDNSListener;

class Seer : public nsINetworkSeer
           , public nsIObserver
           , public nsISpeculativeConnectionOverrider
           , public nsIInterfaceRequestor
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSINETWORKSEER
  NS_DECL_NSIOBSERVER
  NS_DECL_NSISPECULATIVECONNECTIONOVERRIDER
  NS_DECL_NSIINTERFACEREQUESTOR

  Seer();
  virtual ~Seer();

  nsresult Init();
  void Shutdown();
  static nsresult Create(nsISupports *outer, const nsIID& iid, void **result);

private:
  friend class SeerPredictionEvent;
  friend class SeerLearnEvent;
  friend class SeerResetEvent;
  friend class SeerPredictionRunner;
  friend class SeerDBShutdownRunner;

  void CheckForAndDeleteOldDBFile();
  nsresult EnsureInitStorage();

  // This is a proxy for the information we need from an nsIURI
  struct UriInfo {
    nsAutoCString spec;
    nsAutoCString origin;
  };

  void PredictForLink(nsIURI *targetURI,
                      nsIURI *sourceURI,
                      nsINetworkSeerVerifier *verifier);
  void PredictForPageload(const UriInfo &dest,
                          SeerVerifierHandle &verifier,
                          int stackCount,
                          TimeStamp &predictStartTime);
  void PredictForStartup(SeerVerifierHandle &verifier,
                         TimeStamp &predictStartTime);

  // Whether we're working on a page or an origin
  enum QueryType {
    QUERY_PAGE = 0,
    QUERY_ORIGIN
  };

  // Holds info from the db about a top-level page or origin
  struct TopLevelInfo {
    int32_t id;
    int32_t loadCount;
    PRTime lastLoad;
  };

  // Holds info from the db about a subresource
  struct SubresourceInfo {
    int32_t id;
    int32_t hitCount;
    PRTime lastHit;
  };

  nsresult ReserveSpaceInQueue();
  void FreeSpaceInQueue();

  int CalculateGlobalDegradation(PRTime now,
                                 PRTime lastLoad);
  int CalculateConfidence(int baseConfidence,
                          PRTime lastHit,
                          PRTime lastPossible,
                          int globalDegradation);
  void SetupPrediction(int confidence,
                       const nsACString &uri,
                       SeerPredictionRunner *runner);

  bool LookupTopLevel(QueryType queryType,
                      const nsACString &key,
                      TopLevelInfo &info);
  void AddTopLevel(QueryType queryType,
                   const nsACString &key,
                   PRTime now);
  void UpdateTopLevel(QueryType queryType,
                      const TopLevelInfo &info,
                      PRTime now);
  bool TryPredict(QueryType queryType,
                  const TopLevelInfo &info,
                  PRTime now,
                  SeerVerifierHandle &verifier,
                  TimeStamp &predictStartTime);
  bool WouldRedirect(const TopLevelInfo &info,
                     PRTime now,
                     UriInfo &newUri);

  bool LookupSubresource(QueryType queryType,
                         const int32_t parentId,
                         const nsACString &key,
                         SubresourceInfo &info);
  void AddSubresource(QueryType queryType,
                      const int32_t parentId,
                      const nsACString &key, PRTime now);
  void UpdateSubresource(QueryType queryType,
                         const SubresourceInfo &info,
                         PRTime now);

  void MaybeLearnForStartup(const UriInfo &uri, const PRTime now);

  void LearnForToplevel(const UriInfo &uri);
  void LearnForSubresource(const UriInfo &targetURI, const UriInfo &sourceURI);
  void LearnForRedirect(const UriInfo &targetURI, const UriInfo &sourceURI);
  void LearnForStartup(const UriInfo &uri);

  void ResetInternal();

  // Observer-related stuff
  nsresult InstallObserver();
  void RemoveObserver();

  bool mInitialized;

  bool mEnabled;
  bool mEnableHoverOnSSL;

  int mPageDegradationDay;
  int mPageDegradationWeek;
  int mPageDegradationMonth;
  int mPageDegradationYear;
  int mPageDegradationMax;

  int mSubresourceDegradationDay;
  int mSubresourceDegradationWeek;
  int mSubresourceDegradationMonth;
  int mSubresourceDegradationYear;
  int mSubresourceDegradationMax;

  int mPreconnectMinConfidence;
  int mPreresolveMinConfidence;
  int mRedirectLikelyConfidence;

  int32_t mMaxQueueSize;

  nsCOMPtr<nsIThread> mIOThread;

  nsCOMPtr<nsISpeculativeConnect> mSpeculativeService;

  nsCOMPtr<nsIFile> mDBFile;
  nsCOMPtr<mozIStorageService> mStorageService;
  nsCOMPtr<mozIStorageConnection> mDB;
  mozilla::storage::StatementCache<mozIStorageStatement> mStatements;

  PRTime mStartupTime;
  PRTime mLastStartupTime;
  int32_t mStartupCount;

  nsCOMPtr<nsIDNSService> mDnsService;

  int32_t mQueueSize;
  mozilla::Mutex mQueueSizeLock;

  nsAutoPtr<SeerTelemetryAccumulators> mAccumulators;

  nsRefPtr<SeerDNSListener> mDNSListener;
};

} // ::mozilla::net
} // ::mozilla

#endif // mozilla_net_Seer_h
