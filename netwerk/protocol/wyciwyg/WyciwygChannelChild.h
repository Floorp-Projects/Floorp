/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WyciwygChannelChild_h
#define mozilla_net_WyciwygChannelChild_h

#include "mozilla/net/PWyciwygChannelChild.h"
#include "nsIWyciwygChannel.h"
#include "nsIChannel.h"
#include "PrivateBrowsingChannel.h"

class nsIProgressEventSink;

namespace mozilla {
namespace net {

class ChannelEventQueue;

// TODO: replace with IPDL states
enum WyciwygChannelChildState {
  WCC_NEW,
  WCC_INIT,

  // States when reading from the channel
  WCC_OPENED,
  WCC_ONSTART,
  WCC_ONDATA,
  WCC_ONSTOP,

  // States when writing to the cache
  WCC_ONWRITE,
  WCC_ONCLOSED
};


// Header file contents
class WyciwygChannelChild : public PWyciwygChannelChild
                          , public nsIWyciwygChannel
                          , public PrivateBrowsingChannel<WyciwygChannelChild>
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUEST
  NS_DECL_NSICHANNEL
  NS_DECL_NSIWYCIWYGCHANNEL

  WyciwygChannelChild();

  void AddIPDLReference();
  void ReleaseIPDLReference();

  nsresult Init(nsIURI *uri);

  bool IsSuspended();

protected:
  virtual ~WyciwygChannelChild();

  bool RecvOnStartRequest(const nsresult& statusCode,
                          const int64_t& contentLength,
                          const int32_t& source,
                          const nsCString& charset,
                          const nsCString& securityInfo) MOZ_OVERRIDE;
  bool RecvOnDataAvailable(const nsCString& data,
                           const uint64_t& offset) MOZ_OVERRIDE;
  bool RecvOnStopRequest(const nsresult& statusCode) MOZ_OVERRIDE;
  bool RecvCancelEarly(const nsresult& statusCode) MOZ_OVERRIDE;

  void OnStartRequest(const nsresult& statusCode,
                      const int64_t& contentLength,
                      const int32_t& source,
                      const nsCString& charset,
                      const nsCString& securityInfo);
  void OnDataAvailable(const nsCString& data,
                       const uint64_t& offset);
  void OnStopRequest(const nsresult& statusCode);
  void CancelEarly(const nsresult& statusCode);

  friend class PrivateBrowsingChannel<WyciwygChannelChild>;

private:
  nsresult                          mStatus;
  bool                              mIsPending;
  bool                              mCanceled;
  uint32_t                          mLoadFlags;
  int64_t                           mContentLength;
  int32_t                           mCharsetSource;
  nsCString                         mCharset;
  nsCOMPtr<nsIURI>                  mURI;
  nsCOMPtr<nsIURI>                  mOriginalURI;
  nsCOMPtr<nsISupports>             mOwner;
  nsCOMPtr<nsIInterfaceRequestor>   mCallbacks;
  nsCOMPtr<nsIProgressEventSink>    mProgressSink;
  nsCOMPtr<nsILoadGroup>            mLoadGroup;
  nsCOMPtr<nsIStreamListener>       mListener;
  nsCOMPtr<nsISupports>             mListenerContext;
  nsCOMPtr<nsISupports>             mSecurityInfo;

  // FIXME: replace with IPDL states (bug 536319)
  enum WyciwygChannelChildState mState;

  bool mIPCOpen;
  bool mSentAppData;
  nsRefPtr<ChannelEventQueue> mEventQ;

  friend class WyciwygStartRequestEvent;
  friend class WyciwygDataAvailableEvent;
  friend class WyciwygStopRequestEvent;
  friend class WyciwygCancelEvent;
};

inline bool
WyciwygChannelChild::IsSuspended()
{
  return false;
}

} // namespace net
} // namespace mozilla

#endif // mozilla_net_WyciwygChannelChild_h
