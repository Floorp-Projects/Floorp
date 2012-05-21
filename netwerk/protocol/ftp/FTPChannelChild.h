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

  NS_OVERRIDE nsresult OpenContentStream(bool async,
                                         nsIInputStream** stream,
                                         nsIChannel** channel);

  bool IsSuspended();

protected:
  NS_OVERRIDE bool RecvOnStartRequest(const PRInt32& aContentLength,
                                      const nsCString& aContentType,
                                      const PRTime& aLastModified,
                                      const nsCString& aEntityID,
                                      const IPC::URI& aURI);
  NS_OVERRIDE bool RecvOnDataAvailable(const nsCString& data,
                                       const PRUint32& offset,
                                       const PRUint32& count);
  NS_OVERRIDE bool RecvOnStopRequest(const nsresult& statusCode);
  NS_OVERRIDE bool RecvFailedAsyncOpen(const nsresult& statusCode);
  NS_OVERRIDE bool RecvDeleteSelf();

  void DoOnStartRequest(const PRInt32& aContentLength,
                        const nsCString& aContentType,
                        const PRTime& aLastModified,
                        const nsCString& aEntityID,
                        const IPC::URI& aURI);
  void DoOnDataAvailable(const nsCString& data,
                         const PRUint32& offset,
                         const PRUint32& count);
  void DoOnStopRequest(const nsresult& statusCode);
  void DoFailedAsyncOpen(const nsresult& statusCode);
  void DoDeleteSelf();

  friend class FTPStartRequestEvent;
  friend class FTPDataAvailableEvent;
  friend class FTPStopRequestEvent;
  friend class FTPFailedAsyncOpenEvent;
  friend class FTPDeleteSelfEvent;

private:
  // Called asynchronously from Resume: continues any pending calls into client.
  void CompleteResume();
  nsresult AsyncCall(void (FTPChannelChild::*funcPtr)(),
                     nsRunnableMethod<FTPChannelChild> **retval = nsnull);

  nsCOMPtr<nsIInputStream> mUploadStream;

  bool mIPCOpen;
  ChannelEventQueue mEventQ;
  bool mCanceled;
  PRUint32 mSuspendCount;
  bool mIsPending;
  bool mWasOpened;
  
  PRTime mLastModifiedTime;
  PRUint64 mStartPos;
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
