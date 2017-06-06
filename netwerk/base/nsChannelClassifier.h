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
    nsresult ShouldEnableTrackingProtection(bool *result);

    // Called once we actually classified an URI. (An additional whitelist
    // check will be done if the classifier reports the URI is a tracker.)
    nsresult OnClassifyCompleteInternal(nsresult aErrorCode,
                                        const nsACString& aList,
                                        const nsACString& aProvider,
                                        const nsACString& aPrefix);

private:
    // True if the channel is on the allow list.
    bool mIsAllowListed;
    // True if the channel has been suspended.
    bool mSuspendedChannel;
    nsCOMPtr<nsIChannel> mChannel;
    Maybe<bool> mTrackingProtectionEnabled;

    ~nsChannelClassifier() {}
    // Caches good classifications for the channel principal.
    void MarkEntryClassified(nsresult status);
    bool HasBeenClassified(nsIChannel *aChannel);
    // Helper function so that we ensure we call ContinueBeginConnect once
    // Start is called. Returns NS_OK if and only if we will get a callback
    // from the classifier service.
    nsresult StartInternal();
    // Helper function to check a tracking URI against the whitelist
    nsresult IsTrackerWhitelisted(const nsACString& aList,
                                  const nsACString& aProvider,
                                  const nsACString& aPrefix);
    // Helper function to check a URI against the hostname whitelist
    bool IsHostnameWhitelisted(nsIURI *aUri, const nsACString &aWhitelisted);
    // Checks that the channel was loaded by the URI currently loaded in aDoc
    static bool SameLoadingURI(nsIDocument *aDoc, nsIChannel *aChannel);

    nsresult ShouldEnableTrackingProtectionInternal(nsIChannel *aChannel,
                                                    bool *result);

    bool AddonMayLoad(nsIChannel *aChannel, nsIURI *aUri);
    void AddShutdownObserver();
    void RemoveShutdownObserver();
public:
    // If we are blocking content, update the corresponding flag in the respective
    // docshell and call nsISecurityEventSink::onSecurityChange.
    static nsresult SetBlockedContent(nsIChannel *channel,
                                      nsresult aErrorCode,
                                      const nsACString& aList,
                                      const nsACString& aProvider,
                                      const nsACString& aPrefix);
    static nsresult NotifyTrackingProtectionDisabled(nsIChannel *aChannel);
};

} // namespace net
} // namespace mozilla

#endif
