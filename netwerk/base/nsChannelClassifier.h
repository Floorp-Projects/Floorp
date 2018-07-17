/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsChannelClassifier_h__
#define nsChannelClassifier_h__

#include "nsIObserver.h"
#include "nsIURIClassifier.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"

#include <functional>

class nsIChannel;
class nsIHttpChannelInternal;
class nsIDocument;

namespace mozilla {
namespace net {

class nsChannelClassifier final : public nsIURIClassifierCallback,
                                  public nsIObserver
{
public:
    explicit nsChannelClassifier(nsIChannel* aChannel);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIURICLASSIFIERCALLBACK
    NS_DECL_NSIOBSERVER

    // Calls nsIURIClassifier.Classify with the principal of the given channel,
    // and cancels the channel on a bad verdict.
    void Start();
    // Whether or not tracking protection should be enabled on this channel.
    bool ShouldEnableTrackingProtection();
    // Whether or not to annotate the channel with tracking protection list.
    bool ShouldEnableTrackingAnnotation();

    // Helper function to check a tracking URI against the whitelist
    nsresult IsTrackerWhitelisted(nsIURI* aWhiteListURI,
                                  nsIURIClassifierCallback* aCallback);

    // Called once we actually classified an URI. (An additional whitelist
    // check will be done if the classifier reports the URI is a tracker.)
    nsresult OnClassifyCompleteInternal(nsresult aErrorCode,
                                        const nsACString& aList,
                                        const nsACString& aProvider,
                                        const nsACString& aFullHash);

    // Check a tracking URI against the local blacklist and whitelist.
    // Returning NS_OK means the check will be processed
    // and the caller should wait for the result.
    nsresult CheckIsTrackerWithLocalTable(std::function<void()>&& aCallback);

    // Helper function to create a whitelist URL.
    already_AddRefed<nsIURI> CreateWhiteListURI() const;

    already_AddRefed<nsIChannel> GetChannel();

private:
    // True if the channel is on the allow list.
    bool mIsAllowListed;
    // True if the channel has been suspended.
    bool mSuspendedChannel;
    nsCOMPtr<nsIChannel> mChannel;
    Maybe<bool> mTrackingProtectionEnabled;
    Maybe<bool> mTrackingAnnotationEnabled;

    ~nsChannelClassifier();
    // Caches good classifications for the channel principal.
    void MarkEntryClassified(nsresult status);
    bool HasBeenClassified(nsIChannel *aChannel);
    // Helper function so that we ensure we call ContinueBeginConnect once
    // Start is called. Returns NS_OK if and only if we will get a callback
    // from the classifier service.
    nsresult StartInternal();
    // Helper function to check a URI against the hostname whitelist
    bool IsHostnameWhitelisted(nsIURI *aUri, const nsACString &aWhitelisted);
    // Note this function will be also used to decide whether or not to enable
    // channel annotation. When |aAnnotationsOnly| is true, this function
    // is called by ShouldEnableTrackingAnnotation(). Otherwise, this is called
    // by ShouldEnableTrackingProtection().
    nsresult ShouldEnableTrackingProtectionInternal(nsIChannel *aChannel,
                                                    bool aAnnotationsOnly,
                                                    bool *result);

    bool AddonMayLoad(nsIChannel *aChannel, nsIURI *aUri);
    void AddShutdownObserver();
    void RemoveShutdownObserver();
    static nsresult SendThreatHitReport(nsIChannel *aChannel,
                                        const nsACString& aProvider,
                                        const nsACString& aList,
                                        const nsACString& aFullHash);
public:
    // If we are blocking content, update the corresponding flag in the respective
    // docshell and call nsISecurityEventSink::onSecurityChange.
    static nsresult SetBlockedContent(nsIChannel *channel,
                                      nsresult aErrorCode,
                                      const nsACString& aList,
                                      const nsACString& aProvider,
                                      const nsACString& aFullHash);
    static nsresult NotifyTrackingProtectionDisabled(nsIChannel *aChannel);
};

} // namespace net
} // namespace mozilla

#endif
