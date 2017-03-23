/* vim: set ts=2 sts=2 et sw=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla__net__ThrottlingService_h
#define mozilla__net__ThrottlingService_h

#include "nsIThrottlingService.h"

#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsInterfaceHashtable.h"
#include "nsIObserver.h"
#include "nsITimer.h"

class nsIHttpChannel;

namespace mozilla {
namespace net {

class ThrottlingService : public nsIThrottlingService
                        , public nsIObserver
                        , public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITHROTTLINGSERVICE
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK

  ThrottlingService();

  nsresult Init();
  void Shutdown();
  static nsresult Create(nsISupports *outer, const nsIID& iid, void **result);

private:
  virtual ~ThrottlingService();

  void MaybeSuspendAll();
  void MaybeResumeAll();

  void HandleExtraAddRemove();

  bool mEnabled;
  bool mInitCalled;
  bool mSuspended;
  uint32_t mPressureCount;
  uint32_t mSuspendPeriod; // How long we should Suspend() channels for
  uint32_t mResumePeriod; // How long we should Resume() channels for
  nsCOMPtr<nsITimer> mTimer;
  typedef nsInterfaceHashtable<nsUint64HashKey, nsIHttpChannel> ChannelHash;
  ChannelHash mChannelHash;

  // Used to avoid inconsistencies in the hash and the suspend/resume count of
  // channels. See comments in AddChannel and RemoveChannel for details.
  void IterateHash(void (* callback)(ChannelHash::Iterator &iter));
  bool mIteratingHash;
  nsCOMArray<nsIHttpChannel> mChannelsToAddRemove;
  nsTArray<bool> mChannelIsAdd;
};

} // ::mozilla::net
} // ::mozilla

#endif // mozilla__net__ThrottlingService_h
