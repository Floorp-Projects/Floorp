/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_psm_PSMCOntentListener_h_
#define mozilla_psm_PSMCOntentListener_h_

#include "nsCOMPtr.h"
#include "nsIStreamListener.h"
#include "nsIURIContentListener.h"
#include "nsIStreamListener.h"
#include "nsWeakReference.h"
#include "mozilla/psm/PPSMContentDownloaderChild.h"
#include "mozilla/psm/PPSMContentDownloaderParent.h"

#define NS_PSMCONTENTLISTEN_CID                      \
  {                                                  \
    0xc94f4a30, 0x64d7, 0x11d4, {                    \
      0x99, 0x60, 0x00, 0xb0, 0xd0, 0x23, 0x54, 0xa0 \
    }                                                \
  }
#define NS_PSMCONTENTLISTEN_CONTRACTID "@mozilla.org/security/psmdownload;1"

namespace mozilla {
namespace net {

class PChannelDiverterParent;

}  // namespace net
}  // namespace mozilla

namespace mozilla {
namespace psm {

// PSMContentStreamListener for parent-process downloading.
class PSMContentStreamListener : public nsIStreamListener {
 public:
  explicit PSMContentStreamListener(uint32_t type);
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  void ImportCertificate();

 protected:
  virtual ~PSMContentStreamListener();

  nsCString mByteData;
  uint32_t mType;
};

// Parent actor for importing a remote cert when the load was started by the
// child.
class PSMContentDownloaderParent : public PPSMContentDownloaderParent,
                                   public PSMContentStreamListener {
 public:
  explicit PSMContentDownloaderParent(uint32_t type);

  mozilla::ipc::IPCResult RecvOnStartRequest(const uint32_t& contentLength);
  mozilla::ipc::IPCResult RecvOnDataAvailable(const nsCString& data,
                                              const uint64_t& offset,
                                              const uint32_t& count);
  mozilla::ipc::IPCResult RecvOnStopRequest(const nsresult& code);

  // We inherit most of nsIStreamListener from PSMContentStreamListener, but
  // we have to override OnStopRequest to know when we're done with our IPC
  // ref.
  NS_IMETHOD OnStopRequest(nsIRequest* request, nsresult code) override;

  mozilla::ipc::IPCResult RecvDivertToParentUsing(
      mozilla::net::PChannelDiverterParent* diverter);

 protected:
  virtual ~PSMContentDownloaderParent();

  virtual void ActorDestroy(ActorDestroyReason why) override;
  bool mIPCOpen;
};

// Child actor for importing a cert.
class PSMContentDownloaderChild : public nsIStreamListener,
                                  public PPSMContentDownloaderChild {
 public:
  PSMContentDownloaderChild();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

 private:
  ~PSMContentDownloaderChild();
};

class PSMContentListener : public nsIURIContentListener,
                           public nsSupportsWeakReference {
 public:
  PSMContentListener();
  nsresult init();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIURICONTENTLISTENER

 protected:
  virtual ~PSMContentListener();

 private:
  nsCOMPtr<nsISupports> mLoadCookie;
  nsCOMPtr<nsIURIContentListener> mParentContentListener;
};

}  // namespace psm
}  // namespace mozilla

#endif  // mozilla_psm_PSMCOntentListener_h
