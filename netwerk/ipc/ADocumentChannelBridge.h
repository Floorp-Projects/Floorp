/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ADocumentChannelBridge_h
#define mozilla_net_ADocumentChannelBridge_h

#include "mozilla/net/PDocumentChannelParent.h"

namespace mozilla {
namespace net {

/**
 * ADocumentChannelBridge is the interface for DocumentLoadListener to
 * communicate with the nsIChannel placeholder in the docshell. It may be
 * implemented over IPDL.
 */
class ADocumentChannelBridge {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  // Notify the destination docshell that we're not going to send
  // a response to it (usually because we've redirected to a different
  // process), and drop any references to the parent DocumentLoadListener.
  // This should remove the nsIChannel from the loadgroup, and
  // fire OnStart/StopRequest with aStatus.
  // aLoadGroupStatus is used as mStatus when we remove the child channel
  // from the loadgroup (but aStatus is passed as the parameter to
  // RemoveRequest).
  // We do this so we can remove using NS_BINDING_RETARGETED, but still have
  // the channel not be in an error state.
  virtual void DisconnectChildListeners(nsresult aStatus,
                                        nsresult aLoadGroupStatus) = 0;

  // Delete the bridge, and drop any refs to the DocumentLoadListener
  virtual void Delete() = 0;

  // Checks if we should allow a redirect to aNewURI.
  // We need this because we currently can only do CSP checks in the content
  // process in order to get the right events fired.
  virtual RefPtr<PDocumentChannelParent::ConfirmRedirectPromise>
  ConfirmRedirect(const LoadInfoArgs& aLoadInfo, nsIURI* aNewURI) = 0;

  // Initate a switch from the DocumentChannel to the protocol-specific
  // real channel.
  virtual RefPtr<PDocumentChannelParent::RedirectToRealChannelPromise>
  RedirectToRealChannel(uint32_t aRedirectFlags, uint32_t aLoadFlags) = 0;

  // Returns the process id that this bridge is connected to.
  virtual base::ProcessId OtherPid() const = 0;

  // Attach a StreamFilterParent to the remote-side nsIChannel of this bridge.
  virtual bool AttachStreamFilter(
      ipc::Endpoint<mozilla::extensions::PStreamFilterParent>&& aEndpoint) = 0;

 protected:
  virtual ~ADocumentChannelBridge() = default;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_ADocumentChannelBridge_h
