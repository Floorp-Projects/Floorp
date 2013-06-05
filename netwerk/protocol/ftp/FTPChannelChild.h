/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_FTPChannelChild_h
#define mozilla_net_FTPChannelChild_h

#include "mozilla/net/PFTPChannelChild.h"
#include "mozilla/net/ChannelEventQueue.h"
#include "nsBaseChannel.h"
#include "nsIFTPChannel.h"
#include "nsIUploadChannel.h"
#include "nsIProxiedChannel.h"
#include "nsIResumableChannel.h"
#include "nsIChildChannel.h"

#include "nsIStreamListener.h"
#include "PrivateBrowsingChannel.h"

namespace mozilla {
namespace net {

// This class inherits logic from nsBaseChannel that is not needed for an
// e10s child channel, but it works.  At some point we could slice up
// nsBaseChannel and have a new class that has only the common logic for
// nsFTPChannel/FTPChannelChild.

class FTPChannelChild : public PFTPChannelChild
                      , public nsBaseChannel
                      , public nsIFTPChannel
                      , public nsIUploadChannel
                      , public nsIResumableChannel
                      , public nsIProxiedChannel
                      , public nsIChildChannel
{
public:
  typedef ::nsIStreamListener nsIStreamListener;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIFTPCHANNEL
  NS_DECL_NSIUPLOADCHANNEL
  NS_DECL_NSIRESUMABLECHANNEL
  NS_DECL_NSIPROXIEDCHANNEL
  NS_DECL_NSICHILDCHANNEL

  NS_IMETHOD Cancel(nsresult status);
  NS_IMETHOD Suspend();
  NS_IMETHOD Resume();

  FTPChannelChild(nsIURI* uri);
  virtual ~FTPChannelChild();

  void AddIPDLReference();
  void ReleaseIPDLReference();

  NS_IMETHOD AsyncOpen(nsIStreamListener* listener, nsISupports* aContext);

  // Note that we handle this ourselves, overriding the nsBaseChannel
  // default behavior, in order to be e10s-friendly.
  NS_IMETHOD IsPending(bool* result);

  nsresult OpenContentStream(bool async,
                             nsIInputStream** stream,
                             nsIChannel** channel) MOZ_OVERRIDE;

  bool IsSuspended();

protected:
  bool RecvOnStartRequest(const int64_t& aContentLength,
                          const nsCString& aContentType,
                          const PRTime& aLastModified,
                          const nsCString& aEntityID,
                          const URIParams& aURI) MOZ_OVERRIDE;
  bool RecvOnDataAvailable(const nsCString& data,
                           const uint64_t& offset,
                           const uint32_t& count) MOZ_OVERRIDE;
  bool RecvOnStopRequest(const nsresult& statusCode) MOZ_OVERRIDE;
  bool RecvFailedAsyncOpen(const nsresult& statusCode) MOZ_OVERRIDE;
  bool RecvDeleteSelf() MOZ_OVERRIDE;

  void DoOnStartRequest(const int64_t& aContentLength,
                        const nsCString& aContentType,
                        const PRTime& aLastModified,
                        const nsCString& aEntityID,
                        const URIParams& aURI);
  void DoOnDataAvailable(const nsCString& data,
                         const uint64_t& offset,
                         const uint32_t& count);
  void DoOnStopRequest(const nsresult& statusCode);
  void DoFailedAsyncOpen(const nsresult& statusCode);
  void DoDeleteSelf();

  friend class FTPStartRequestEvent;
  friend class FTPDataAvailableEvent;
  friend class FTPStopRequestEvent;
  friend class FTPFailedAsyncOpenEvent;
  friend class FTPDeleteSelfEvent;

private:
  nsCOMPtr<nsIInputStream> mUploadStream;

  bool mIPCOpen;
  nsRefPtr<ChannelEventQueue> mEventQ;
  bool mCanceled;
  uint32_t mSuspendCount;
  bool mIsPending;
  bool mWasOpened;
  
  PRTime mLastModifiedTime;
  uint64_t mStartPos;
  nsCString mEntityID;
};

inline bool
FTPChannelChild::IsSuspended()
{
  return mSuspendCount != 0;
}

} // namespace net
} // namespace mozilla

#endif // mozilla_net_FTPChannelChild_h
