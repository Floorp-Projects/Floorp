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

namespace IPC {
class URI;
}

namespace mozilla {

namespace ipc {
class OptionalURIParams;
} // namespace ipc

namespace net {
class PChannelDiverterParent;
} // namespace net

namespace dom {

class ContentParent;
class PBrowserParent;

class ExternalHelperAppParent : public PExternalHelperAppParent
                              , public nsHashPropertyBag
                              , public nsIChannel
                              , public nsIMultiPartChannel
                              , public nsIResumableChannel
                              , public nsIStreamListener
{
    typedef mozilla::ipc::OptionalURIParams OptionalURIParams;

public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIMULTIPARTCHANNEL
    NS_DECL_NSIRESUMABLECHANNEL
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER

    bool RecvOnStartRequest(const nsCString& entityID) MOZ_OVERRIDE;
    bool RecvOnDataAvailable(const nsCString& data,
                             const uint64_t& offset,
                             const uint32_t& count) MOZ_OVERRIDE;
    bool RecvOnStopRequest(const nsresult& code) MOZ_OVERRIDE;

    bool RecvDivertToParentUsing(PChannelDiverterParent* diverter) MOZ_OVERRIDE;

    ExternalHelperAppParent(const OptionalURIParams& uri, const int64_t& contentLength);
    void Init(ContentParent *parent,
              const nsCString& aMimeContentType,
              const nsCString& aContentDisposition,
              const uint32_t& aContentDispositionHint,
              const nsString& aContentDispositionFilename,
              const bool& aForceSave,
              const OptionalURIParams& aReferrer,
              PBrowserParent* aBrowser);
    virtual ~ExternalHelperAppParent();

protected:
  virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;
  void Delete();

private:
  nsCOMPtr<nsIStreamListener> mListener;
  nsCOMPtr<nsIURI> mURI;
  bool mPending;
  DebugOnly<bool> mDiverted;
  bool mIPCClosed;
  nsLoadFlags mLoadFlags;
  nsresult mStatus;
  int64_t mContentLength;
  uint32_t mContentDisposition;
  nsString mContentDispositionFilename;
  nsCString mContentDispositionHeader;
  nsCString mEntityID;
};

} // namespace dom
} // namespace mozilla
