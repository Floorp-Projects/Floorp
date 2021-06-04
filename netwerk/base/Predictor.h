/* vim: set ts=2 sts=2 et sw=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_Predictor_h
#define mozilla_net_Predictor_h

#include "nsINetworkPredictor.h"
#include "nsINetworkPredictorVerifier.h"

#include "nsCOMPtr.h"
#include "nsICacheEntry.h"
#include "nsICacheEntryOpenCallback.h"
#include "nsICacheStorageService.h"
#include "nsICacheStorageVisitor.h"
#include "nsIDNSListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsIObserver.h"
#include "nsISpeculativeConnect.h"
#include "nsIStreamListener.h"
#include "mozilla/RefPtr.h"
#include "nsString.h"
#include "nsTArray.h"

#include "mozilla/TimeStamp.h"

class nsICacheStorage;
class nsIDNSService;
class nsIIOService;
class nsILoadContextInfo;
class nsITimer;

namespace mozilla {
namespace net {

class nsHttpRequestHead;
class nsHttpResponseHead;

class Predictor final : public nsINetworkPredictor,
                        public nsIObserver,
                        public nsISpeculativeConnectionOverrider,
                        public nsIInterfaceRequestor,
                        public nsICacheEntryMetaDataVisitor,
                        public nsINetworkPredictorVerifier {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSINETWORKPREDICTOR
  NS_DECL_NSIOBSERVER
  NS_DECL_NSISPECULATIVECONNECTIONOVERRIDER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICACHEENTRYMETADATAVISITOR
  NS_DECL_NSINETWORKPREDICTORVERIFIER

  Predictor();

  nsresult Init();
  void Shutdown();
  static nsresult Create(nsISupports* outer, const nsIID& iid, void** result);

  // Used to update whether a particular URI was cacheable or not.
  // sourceURI and targetURI are the same as the arguments to Learn
  // and httpStatus is the status code we got while loading targetURI.
  static void UpdateCacheability(nsIURI* sourceURI, nsIURI* targetURI,
                                 uint32_t httpStatus,
                                 nsHttpRequestHead& requestHead,
                                 nsHttpResponseHead* reqponseHead,
                                 nsILoadContextInfo* lci, bool isTracking);

 private:
  virtual ~Predictor();

  // Stores callbacks for a child process predictor (for test purposes)
  nsCOMPtr<nsINetworkPredictorVerifier> mChildVerifier;

  union Reason {
    PredictorLearnReason mLearn;
    PredictorPredictReason mPredict;
  };

  class DNSListener : public nsIDNSListener {
   public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIDNSLISTENER

    DNSListener() = default;

   private:
    virtual ~DNSListener() = default;
  };

  class Action : public nsICacheEntryOpenCallback {
   public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSICACHEENTRYOPENCALLBACK

    Action(bool fullUri, bool predict, Reason reason, nsIURI* targetURI,
           nsIURI* sourceURI, nsINetworkPredictorVerifier* verifier,
           Predictor* predictor);
    Action(bool fullUri, bool predict, Reason reason, nsIURI* targetURI,
           nsIURI* sourceURI, nsINetworkPredictorVerifier* verifier,
           Predictor* predictor, uint8_t stackCount);

    static const bool IS_FULL_URI = true;
    static const bool IS_ORIGIN = false;

    static const bool DO_PREDICT = true;
    static const bool DO_LEARN = false;

   private:
    virtual ~Action() = default;

    bool mFullUri : 1;
    bool mPredict : 1;
    union {
      PredictorPredictReason mPredictReason;
      PredictorLearnReason mLearnReason;
    };
    nsCOMPtr<nsIURI> mTargetURI;
    nsCOMPtr<nsIURI> mSourceURI;
    nsCOMPtr<nsINetworkPredictorVerifier> mVerifier;
    TimeStamp mStartTime;
    uint8_t mStackCount;
    RefPtr<Predictor> mPredictor;
  };

  class CacheabilityAction : public nsICacheEntryOpenCallback,
                             public nsICacheEntryMetaDataVisitor {
   public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSICACHEENTRYOPENCALLBACK
    NS_DECL_NSICACHEENTRYMETADATAVISITOR

    CacheabilityAction(nsIURI* targetURI, uint32_t httpStatus,
                       const nsCString& method, bool isTracking, bool couldVary,
                       bool isNoStore, Predictor* predictor)
        : mTargetURI(targetURI),
          mHttpStatus(httpStatus),
          mMethod(method),
          mIsTracking(isTracking),
          mCouldVary(couldVary),
          mIsNoStore(isNoStore),
          mPredictor(predictor) {}

   private:
    virtual ~CacheabilityAction() = default;

    nsCOMPtr<nsIURI> mTargetURI;
    uint32_t mHttpStatus;
    nsCString mMethod;
    bool mIsTracking;
    bool mCouldVary;
    bool mIsNoStore;
    RefPtr<Predictor> mPredictor;
    nsTArray<nsCString> mKeysToCheck;
    nsTArray<nsCString> mValuesToCheck;
  };

  class Resetter : public nsICacheEntryOpenCallback,
                   public nsICacheEntryMetaDataVisitor,
                   public nsICacheStorageVisitor {
   public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSICACHEENTRYOPENCALLBACK
    NS_DECL_NSICACHEENTRYMETADATAVISITOR
    NS_DECL_NSICACHESTORAGEVISITOR

    explicit Resetter(Predictor* predictor);

   private:
    virtual ~Resetter() = default;

    void Complete();

    uint32_t mEntriesToVisit;
    nsTArray<nsCString> mKeysToDelete;
    RefPtr<Predictor> mPredictor;
    nsTArray<nsCOMPtr<nsIURI>> mURIsToVisit;
    nsTArray<nsCOMPtr<nsILoadContextInfo>> mInfosToVisit;
  };

  class SpaceCleaner : public nsICacheEntryMetaDataVisitor {
   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICACHEENTRYMETADATAVISITOR

    explicit SpaceCleaner(Predictor* predictor)
        : mLRUStamp(0), mLRUKeyToDelete(nullptr), mPredictor(predictor) {}

    void Finalize(nsICacheEntry* entry);

   private:
    virtual ~SpaceCleaner() = default;
    uint32_t mLRUStamp;
    const char* mLRUKeyToDelete;
    nsTArray<nsCString> mLongKeysToDelete;
    RefPtr<Predictor> mPredictor;
  };

  class PrefetchListener : public nsIStreamListener {
   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER

    PrefetchListener(nsINetworkPredictorVerifier* verifier, nsIURI* uri,
                     Predictor* predictor)
        : mVerifier(verifier), mURI(uri), mPredictor(predictor) {}

   private:
    virtual ~PrefetchListener() = default;

    nsCOMPtr<nsINetworkPredictorVerifier> mVerifier;
    nsCOMPtr<nsIURI> mURI;
    RefPtr<Predictor> mPredictor;
    TimeStamp mStartTime;
  };

  // Observer-related stuff
  nsresult InstallObserver();
  void RemoveObserver();

  // Service startup utilities
  void MaybeCleanupOldDBFiles();

  // The guts of prediction

  // This is the top-level driver for doing any prediction that needs
  // information from the cache. Returns true if any predictions were queued up
  //   * reason - What kind of prediction this is/why this prediction is
  //              happening (pageload, startup)
  //   * entry - the cache entry with the information we need
  //   * isNew - whether or not the cache entry is brand new and empty
  //   * fullUri - whether we are doing predictions based on a full page URI, or
  //               just the origin of the page
  //   * targetURI - the URI that we are predicting based upon - IOW, the URI
  //                 that is being loaded or being redirected to
  //   * verifier - used for testing to verify the expected predictions happen
  //   * stackCount - used to ensure we don't recurse too far trying to find the
  //                  final redirection in a redirect chain
  bool PredictInternal(PredictorPredictReason reason, nsICacheEntry* entry,
                       bool isNew, bool fullUri, nsIURI* targetURI,
                       nsINetworkPredictorVerifier* verifier,
                       uint8_t stackCount);

  // Used when predicting because the user's mouse hovered over a link
  //   * targetURI - the URI target of the link
  //   * sourceURI - the URI of the page on which the link appears
  //   * originAttributes - the originAttributes for this prediction
  //   * verifier - used for testing to verify the expected predictions happen
  void PredictForLink(nsIURI* targetURI, nsIURI* sourceURI,
                      const OriginAttributes& originAttributes,
                      nsINetworkPredictorVerifier* verifier);

  // Used when predicting because a page is being loaded (which may include
  // being the target of a redirect). All arguments are the same as for
  // PredictInternal. Returns true if any predictions were queued up.
  bool PredictForPageload(nsICacheEntry* entry, nsIURI* targetURI,
                          uint8_t stackCount, bool fullUri,
                          nsINetworkPredictorVerifier* verifier);

  // Used when predicting pages that will be used near browser startup. All
  // arguments are the same as for PredictInternal. Returns true if any
  // predictions were queued up.
  bool PredictForStartup(nsICacheEntry* entry, bool fullUri,
                         nsINetworkPredictorVerifier* verifier);

  // Utilities related to prediction

  // Used to update our rolling load count (how many of the last n loads was a
  // partular resource loaded on?)
  //   * entry - cache entry of page we're loading
  //   * flags - value that contains our rolling count as the top 20 bits (but
  //             we may use fewer than those 20 bits for calculations)
  //   * key - metadata key that we will update on entry
  //   * hitCount - part of the metadata we need to preserve
  //   * lastHit - part of the metadata we need to preserve
  void UpdateRollingLoadCount(nsICacheEntry* entry, const uint32_t flags,
                              const char* key, const uint32_t hitCount,
                              const uint32_t lastHit);

  // Used to calculate how much to degrade our confidence for all resources
  // on a particular page, because of how long ago the most recent load of that
  // page was. Returns a value between 0 (very recent most recent load) and 100
  // (very distant most recent load)
  //   * lastLoad - time stamp of most recent load of a page
  int32_t CalculateGlobalDegradation(uint32_t lastLoad);

  // Used to calculate how confident we are that a particular resource will be
  // used. Returns a value between 0 (no confidence) and 100 (very confident)
  //   * hitCount - number of times this resource has been seen when loading
  //                this page
  //   * hitsPossible - number of times this page has been loaded
  //   * lastHit - timestamp of the last time this resource was seen when
  //               loading this page
  //   * lastPossible - timestamp of the last time this page was loaded
  //   * globalDegradation - value calculated by CalculateGlobalDegradation for
  //                         this page
  int32_t CalculateConfidence(uint32_t hitCount, uint32_t hitsPossible,
                              uint32_t lastHit, uint32_t lastPossible,
                              int32_t globalDegradation);

  // Used to calculate all confidence values for all resources associated with a
  // page.
  //   * entry - the cache entry with all necessary information about this page
  //   * referrer - the URI that we are loading (may be null)
  //   * lastLoad - timestamp of the last time this page was loaded
  //   * loadCount - number of times this page has been loaded
  //   * gloablDegradation - value calculated by CalculateGlobalDegradation for
  //                         this page
  //   * fullUri - whether we're predicting for a full URI or origin-only
  void CalculatePredictions(nsICacheEntry* entry, nsIURI* referrer,
                            uint32_t lastLoad, uint32_t loadCount,
                            int32_t globalDegradation, bool fullUri);

  enum PrefetchIgnoreReason {
    PREFETCH_OK,
    NOT_FULL_URI,
    NO_REFERRER,
    MISSED_A_LOAD,
    PREFETCH_DISABLED,
    PREFETCH_DISABLED_VIA_COUNT,
    CONFIDENCE_TOO_LOW
  };

  // Used to prepare any necessary prediction for a resource on a page
  //   * confidence - value calculated by CalculateConfidence for this resource
  //   * flags - the flags taken from the resource
  //   * uri - the ascii spec of the URI of the resource
  void SetupPrediction(int32_t confidence, uint32_t flags, const nsCString& uri,
                       PrefetchIgnoreReason reason);

  // Used to kick off a prefetch from RunPredictions if necessary
  //   * uri - the URI to prefetch
  //   * referrer - the URI of the referring page
  //   * originAttributes - the originAttributes of this prefetch
  //   * verifier - used for testing to ensure the expected prefetch happens
  nsresult Prefetch(nsIURI* uri, nsIURI* referrer,
                    const OriginAttributes& originAttributes,
                    nsINetworkPredictorVerifier* verifier);

  // Used to actually perform any predictions set up via SetupPrediction.
  // Returns true if any predictions were performed.
  //   * referrer - the URI we are predicting from
  //   * originAttributs - the originAttributes we are predicting from
  //   * verifier - used for testing to ensure the expected predictions happen
  bool RunPredictions(nsIURI* referrer,
                      const OriginAttributes& originAttributes,
                      nsINetworkPredictorVerifier* verifier);

  // Used to guess whether a page will redirect to another page or not. Returns
  // true if a redirection is likely.
  //   * entry - cache entry with all necessary information about this page
  //   * loadCount - number of times this page has been loaded
  //   * lastLoad - timestamp of the last time this page was loaded
  //   * globalDegradation - value calculated by CalculateGlobalDegradation for
  //                         this page
  //   * redirectURI - if this returns true, the URI that is likely to be
  //                   redirected to, otherwise null
  bool WouldRedirect(nsICacheEntry* entry, uint32_t loadCount,
                     uint32_t lastLoad, int32_t globalDegradation,
                     nsIURI** redirectURI);

  // The guts of learning information

  // This is the top-level driver for doing any updating of our information in
  // the cache
  //   * reason - why this learn is happening (pageload, startup, redirect)
  //   * entry - the cache entry with the information we need
  //   * isNew - whether or not the cache entry is brand new and empty
  //   * fullUri - whether we are doing predictions based on a full page URI, or
  //               just the origin of the page
  //   * targetURI - the URI that we are adding to our data - most often a
  //                 resource loaded by a page the user navigated to
  //   * sourceURI - the URI that caused targetURI to be loaded, usually the
  //                 page the user navigated to
  void LearnInternal(PredictorLearnReason reason, nsICacheEntry* entry,
                     bool isNew, bool fullUri, nsIURI* targetURI,
                     nsIURI* sourceURI);

  // Used when learning about a resource loaded by a page
  //   * entry - the cache entry with information that needs updating
  //   * targetURI - the URI of the resource that was loaded by the page
  void LearnForSubresource(nsICacheEntry* entry, nsIURI* targetURI);

  // Used when learning about a redirect from one page to another
  //   * entry - the cache entry of the page that was redirected from
  //   * targetURI - the URI of the redirect target
  void LearnForRedirect(nsICacheEntry* entry, nsIURI* targetURI);

  // Used to learn about pages loaded close to browser startup. This results in
  // LearnForStartup being called if we are, in fact, near browser startup
  //   * uri - the URI of a page that has been loaded (may not have been near
  //           browser startup)
  //   * fullUri - true if this is a full page uri, false if it's an origin
  //   * originAttributes - the originAttributes for this learning.
  void MaybeLearnForStartup(nsIURI* uri, bool fullUri,
                            const OriginAttributes& originAttributes);

  // Used in conjunction with MaybeLearnForStartup to learn about pages loaded
  // close to browser startup
  //   * entry - the cache entry that stores the startup page list
  //   * targetURI - the URI of a page that was loaded near browser startup
  void LearnForStartup(nsICacheEntry* entry, nsIURI* targetURI);

  // Used to parse the data we store in cache metadata
  //   * key - the cache metadata key
  //   * value - the cache metadata value
  //   * uri - (out) the ascii spec of the URI this metadata entry was about
  //   * hitCount - (out) the number of times this URI has been seen
  //   * lastHit - (out) timestamp of the last time this URI was seen
  //   * flags - (out) flags for this metadata entry
  bool ParseMetaDataEntry(const char* key, const char* value, nsCString& uri,
                          uint32_t& hitCount, uint32_t& lastHit,
                          uint32_t& flags);

  // Used to update whether a particular URI was cacheable or not.
  // sourceURI and targetURI are the same as the arguments to Learn
  // and httpStatus is the status code we got while loading targetURI.
  void UpdateCacheabilityInternal(nsIURI* sourceURI, nsIURI* targetURI,
                                  uint32_t httpStatus, const nsCString& method,
                                  const OriginAttributes& originAttributes,
                                  bool isTracking, bool couldVary,
                                  bool isNoStore);

  // Gets the pref value and clamps it within the acceptable range.
  uint32_t ClampedPrefetchRollingLoadCount();

  // Our state
  bool mInitialized;

  nsTArray<nsCString> mKeysToOperateOn;
  nsTArray<nsCString> mValuesToOperateOn;

  nsCOMPtr<nsICacheStorageService> mCacheStorageService;

  nsCOMPtr<nsISpeculativeConnect> mSpeculativeService;

  nsCOMPtr<nsIURI> mStartupURI;
  uint32_t mStartupTime;
  uint32_t mLastStartupTime;
  int32_t mStartupCount;

  nsCOMPtr<nsIDNSService> mDnsService;

  RefPtr<DNSListener> mDNSListener;

  nsTArray<nsCOMPtr<nsIURI>> mPrefetches;
  nsTArray<nsCOMPtr<nsIURI>> mPreconnects;
  nsTArray<nsCOMPtr<nsIURI>> mPreresolves;

  static Predictor* sSelf;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_Predictor_h
