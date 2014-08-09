/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsChannelClassifier_h__
#define nsChannelClassifier_h__

#include "nsIURIClassifier.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"

class nsIChannel;


class nsChannelClassifier MOZ_FINAL : public nsIURIClassifierCallback
{
public:
    nsChannelClassifier();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIURICLASSIFIERCALLBACK

    nsresult Start(nsIChannel *aChannel);

private:
    nsCOMPtr<nsIChannel> mSuspendedChannel;

    ~nsChannelClassifier() {}
    void MarkEntryClassified(nsresult status);
    bool HasBeenClassified(nsIChannel *aChannel);
    // Whether or not tracking protection should be enabled on this channel.
    nsresult ShouldEnableTrackingProtection(nsIChannel *aChannel, bool *result);
};

#endif
