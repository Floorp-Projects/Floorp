/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsChannelClassifier_h__
#define nsChannelClassifier_h__

#include "nsIObserver.h"
#include "nsIURIClassifier.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"

#include <functional>

class nsIChannel;

namespace mozilla {
namespace net {

class nsChannelClassifier final : public nsIURIClassifierCallback,
                                  public nsIObserver {
 public:
  explicit nsChannelClassifier(nsIChannel* aChannel);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIURICLASSIFIERCALLBACK
  NS_DECL_NSIOBSERVER

  // Calls nsIURIClassifier.Classify with the principal of the given channel,
  // and cancels the channel on a bad verdict.
  void Start();

 private:
  // True if the channel is on the allow list.
  bool mIsAllowListed;
  // True if the channel has been suspended.
  bool mSuspendedChannel;
  nsCOMPtr<nsIChannel> mChannel;

  ~nsChannelClassifier();
  // Caches good classifications for the channel principal.
  void MarkEntryClassified(nsresult status);
  bool HasBeenClassified(nsIChannel* aChannel);
  // Helper function so that we ensure we call ContinueBeginConnect once
  // Start is called. Returns NS_OK if and only if we will get a callback
  // from the classifier service.
  nsresult StartInternal();
  // Helper function to check a URI against the hostname entitylist
  bool IsHostnameEntitylisted(nsIURI* aUri, const nsACString& aEntitylisted);

  void AddShutdownObserver();
  void RemoveShutdownObserver();
  static nsresult SendThreatHitReport(nsIChannel* aChannel,
                                      const nsACString& aProvider,
                                      const nsACString& aList,
                                      const nsACString& aFullHash);

 public:
  // If we are blocking content, update the corresponding flag in the respective
  // docshell and call nsDocLoader::OnSecurityChange.
  static nsresult SetBlockedContent(nsIChannel* channel, nsresult aErrorCode,
                                    const nsACString& aList,
                                    const nsACString& aProvider,
                                    const nsACString& aFullHash);
};

}  // namespace net
}  // namespace mozilla

#endif
