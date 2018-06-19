/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PExternalHelperAppParent.h"
#include "nsIChannel.h"
#include "nsIMultiPartChannel.h"
#include "nsIResumableChannel.h"
#include "nsIStreamListener.h"
#include "nsHashPropertyBag.h"
#include "PrivateBrowsingChannel.h"

namespace IPC {
class URI;
} // namespace IPC

namespace mozilla {

namespace ipc {
class OptionalURIParams;
} // namespace ipc

namespace net {
class PChannelDiverterParent;
} // namespace net

namespace dom {

#define NS_IEXTERNALHELPERAPPPARENT_IID \
{ 0x127a01bc, 0x2a49, 0x46a8, \
  { 0x8c, 0x63, 0x4b, 0x5d, 0x3c, 0xa4, 0x07, 0x9c } }

class nsIExternalHelperAppParent : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IEXTERNALHELPERAPPPARENT_IID)

  /**
   * Returns true if this fake channel represented a file channel in the child.
   */
  virtual bool WasFileChannel() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIExternalHelperAppParent, NS_IEXTERNALHELPERAPPPARENT_IID)

class ContentParent;
class PBrowserParent;

class ExternalHelperAppParent : public PExternalHelperAppParent
                              , public nsHashPropertyBag
                              , public nsIChannel
                              , public nsIMultiPartChannel
                              , public nsIResumableChannel
                              , public nsIStreamListener
                              , public net::PrivateBrowsingChannel<ExternalHelperAppParent>
                              , public nsIExternalHelperAppParent
{
    typedef mozilla::ipc::OptionalURIParams OptionalURIParams;

public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIMULTIPARTCHANNEL
    NS_DECL_NSIRESUMABLECHANNEL
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER

    mozilla::ipc::IPCResult RecvOnStartRequest(const nsCString& entityID,
                                               PBrowserParent* aBrowser) override;
    mozilla::ipc::IPCResult RecvOnDataAvailable(const nsCString& data,
                                                const uint64_t& offset,
                                                const uint32_t& count) override;
    mozilla::ipc::IPCResult RecvOnStopRequest(const nsresult& code) override;

    mozilla::ipc::IPCResult RecvDivertToParentUsing(PChannelDiverterParent* diverter,
                                                    PBrowserParent* aBrowser) override;

    bool WasFileChannel() override {
      return mWasFileChannel;
    }

    ExternalHelperAppParent(const OptionalURIParams& uri, const int64_t& contentLength,
                            const bool& wasFileChannel,
                            const nsCString& aContentDispositionHeader,
                            const uint32_t& aContentDispositionHint,
                            const nsString& aContentDispositionFilename);
    void Init(ContentParent *parent,
              const nsCString& aMimeContentType,
              const bool& aForceSave,
              const OptionalURIParams& aReferrer,
              PBrowserParent* aBrowser);

protected:
  virtual ~ExternalHelperAppParent();

  virtual void ActorDestroy(ActorDestroyReason why) override;
  void Delete();

private:
  nsCOMPtr<nsIStreamListener> mListener;
  nsCOMPtr<nsIURI> mURI;
  bool mPending;
#ifdef DEBUG
  bool mDiverted;
#endif
  bool mIPCClosed;
  nsLoadFlags mLoadFlags;
  nsresult mStatus;
  int64_t mContentLength;
  bool mWasFileChannel;
  uint32_t mContentDisposition;
  nsString mContentDispositionFilename;
  nsCString mContentDispositionHeader;
  nsCString mEntityID;
};

} // namespace dom
} // namespace mozilla
