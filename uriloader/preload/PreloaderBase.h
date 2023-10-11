/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PreloaderBase_h__
#define PreloaderBase_h__

#include "mozilla/Maybe.h"
#include "mozilla/PreloadHashKey.h"
#include "mozilla/WeakPtr.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsITimer.h"
#include "nsIURI.h"
#include "nsIWeakReferenceUtils.h"
#include "nsTArray.h"

class nsIChannel;
class nsINode;
class nsIRequest;
class nsIStreamListener;

namespace mozilla {

namespace dom {

class Document;

}  // namespace dom

/**
 * A half-abstract base class that resource loaders' respective
 * channel-listening classes should derive from.  Provides a unified API to
 * register the load or preload in a document scoped service, links <link
 * rel="preload"> DOM nodes with the load progress and provides API to possibly
 * consume the data by later, dynamically discovered consumers.
 *
 * This class is designed to be used only on the main thread.
 */
class PreloaderBase : public SupportsWeakPtr, public nsISupports {
 public:
  PreloaderBase() = default;

  // Called by resource loaders to register this preload in the document's
  // preload service to provide coalescing, and access to the preload when it
  // should be used for an actual load.
  void NotifyOpen(const PreloadHashKey& aKey, dom::Document* aDocument,
                  bool aIsPreload, bool aIsModule = false);
  void NotifyOpen(const PreloadHashKey& aKey, nsIChannel* aChannel,
                  dom::Document* aDocument, bool aIsPreload,
                  bool aIsModule = false);

  // Called when the load is about to be started all over again and thus this
  // PreloaderBase will be registered again with the same key.  This method
  // taks care to deregister this preload prior to that.
  // @param aNewPreloader: If there is a new request and loader created for the
  // restarted load, we will pass any necessary information into it.
  void NotifyRestart(dom::Document* aDocument,
                     PreloaderBase* aNewPreloader = nullptr);

  // Called by the loading object when the channel started to load
  // (OnStartRequest or equal) and when it finished (OnStopRequest or equal)
  void NotifyStart(nsIRequest* aRequest);
  void NotifyStop(nsIRequest* aRequest, nsresult aStatus);
  // Use this variant only in complement to NotifyOpen without providing a
  // channel.
  void NotifyStop(nsresult aStatus);

  // Called by resource loaders or any suitable component to notify the preload
  // has been used for an actual load.  This is intended to stop any usage
  // timers.
  // @param aDropLoadBackground: If `Keep` then the loading channel, if still in
  // progress, will not be removed the LOAD_BACKGROUND flag, for instance XHR is
  // the user here.
  enum class LoadBackground { Keep, Drop };
  void NotifyUsage(dom::Document* aDocument,
                   LoadBackground aLoadBackground = LoadBackground::Drop);
  // Whether this preloader has been used for a regular/actual load or not.
  bool IsUsed() const { return mIsUsed; }

  // Removes itself from the document's preloads hashtable
  void RemoveSelf(dom::Document* aDocument);

  // When a loader starting an actual load finds a preload, the data can be
  // delivered using this method.  It will deliver stream listener notifications
  // as if it were coming from the resource loading channel.  The |request|
  // argument will be the channel that loaded/loads the resource.
  // This method must keep to the nsIChannel.AsyncOpen contract.  A loader is
  // not obligated to re-implement this method when not necessarily needed.
  virtual nsresult AsyncConsume(nsIStreamListener* aListener);

  // Accessor to the resource loading channel.
  nsIChannel* Channel() const { return mChannel; }

  // Helper function to set the LOAD_BACKGROUND flag on channel initiated by
  // <link rel=preload>.  This MUST be used before the channel is AsyncOpen'ed.
  static void AddLoadBackgroundFlag(nsIChannel* aChannel);

  // These are linking this preload to <link rel="preload"> DOM nodes.  If we
  // are already loaded, immediately notify events on the node, otherwise wait
  // for NotifyStop() call.
  void AddLinkPreloadNode(nsINode* aNode);
  void RemoveLinkPreloadNode(nsINode* aNode);

  // A collection of redirects, the main consumer is fetch.
  class RedirectRecord {
   public:
    RedirectRecord(uint32_t aFlags, already_AddRefed<nsIURI> aURI)
        : mFlags(aFlags), mURI(aURI) {}

    uint32_t Flags() const { return mFlags; }
    nsCString Spec() const;
    nsCString Fragment() const;

   private:
    uint32_t mFlags;
    nsCOMPtr<nsIURI> mURI;
  };

  const nsTArray<RedirectRecord>& Redirects() { return mRedirectRecords; }

  void SetForEarlyHints() { mIsEarlyHintsPreload = true; }

 protected:
  virtual ~PreloaderBase();

  // The loading channel.  This will update when a redirect occurs.
  nsCOMPtr<nsIChannel> mChannel;

 private:
  void NotifyNodeEvent(nsINode* aNode);
  void CancelUsageTimer();

  void ReportUsageTelemetry();

  // A helper class that will update the PreloaderBase.mChannel member when a
  // redirect happens, so that we can reprioritize or cancel when needed.
  // Having a separate class instead of implementing this on PreloaderBase
  // directly is to keep PreloaderBase as simple as possible so that derived
  // classes don't have to deal with calling super when implementing these
  // interfaces from some reason as well.
  class RedirectSink;

  // A timer callback to trigger the unuse warning for this preload
  class UsageTimer final : public nsITimerCallback, public nsINamed {
    NS_DECL_ISUPPORTS
    NS_DECL_NSITIMERCALLBACK
    NS_DECL_NSINAMED

    UsageTimer(PreloaderBase* aPreload, dom::Document* aDocument);

   private:
    ~UsageTimer() = default;

    WeakPtr<dom::Document> mDocument;
    WeakPtr<PreloaderBase> mPreload;
  };

 private:
  // Reference to HTMLLinkElement DOM nodes to deliver onload and onerror
  // notifications to.
  nsTArray<nsWeakPtr> mNodes;

  // History of redirects.
  nsTArray<RedirectRecord> mRedirectRecords;

  // Usage timer, emits warning when the preload is not used in time.  Started
  // in NotifyOpen and stopped in NotifyUsage.
  nsCOMPtr<nsITimer> mUsageTimer;

  // The key this preload has been registered under.  We want to remember it to
  // be able to deregister itself from the document's preloads.
  PreloadHashKey mKey;

  // This overrides the final event we send to DOM nodes to be always 'load'.
  // Modified in NotifyStart based on LoadInfo data of the loading channel.
  bool mShouldFireLoadEvent = false;

  // True after call to NotifyUsage.
  bool mIsUsed = false;

  // True after we have reported the usage telemetry.  Prevent duplicates.
  bool mUsageTelementryReported = false;

  // True when this is used to Early Hints preload.
  bool mIsEarlyHintsPreload = false;

  // Emplaced when the data delivery has finished, in NotifyStop, holds the
  // result of the load.
  Maybe<nsresult> mOnStopStatus;
};

}  // namespace mozilla

#endif  // !PreloaderBase_h__
