/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StunAddrsRequestParent.h"

#include "../runnable_utils.h"
#include "nsNetUtil.h"

#include "mtransport/nricectx.h"
#include "mtransport/nricemediastream.h"  // needed only for including nricectx.h
#include "mtransport/nricestunaddr.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

StunAddrsRequestParent::StunAddrsRequestParent() : mIPCClosed(false) {
  NS_GetMainThread(getter_AddRefs(mMainThread));

  nsresult res;
  mSTSThread = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &res);
  MOZ_ASSERT(mSTSThread);
}

mozilla::ipc::IPCResult StunAddrsRequestParent::RecvGetStunAddrs() {
  ASSERT_ON_THREAD(mMainThread);

  if (mIPCClosed) {
    return IPC_OK();
  }

  RUN_ON_THREAD(mSTSThread,
                WrapRunnable(RefPtr<StunAddrsRequestParent>(this),
                             &StunAddrsRequestParent::GetStunAddrs_s),
                NS_DISPATCH_NORMAL);

  return IPC_OK();
}

mozilla::ipc::IPCResult StunAddrsRequestParent::Recv__delete__() {
  // see note below in ActorDestroy
  mIPCClosed = true;
  return IPC_OK();
}

void StunAddrsRequestParent::ActorDestroy(ActorDestroyReason why) {
  // We may still have refcount>0 if we haven't made it through
  // GetStunAddrs_s and SendStunAddrs_m yet, but child process
  // has crashed.  We must not send any more msgs to child, or
  // IPDL will kill chrome process, too.
  mIPCClosed = true;
}

void StunAddrsRequestParent::GetStunAddrs_s() {
  ASSERT_ON_THREAD(mSTSThread);

  // get the stun addresses while on STS thread
  NrIceStunAddrArray addrs = NrIceCtx::GetStunAddrs();

  if (mIPCClosed) {
    return;
  }

  // in order to return the result over IPC, we need to be on main thread
  RUN_ON_THREAD(
      mMainThread,
      WrapRunnable(RefPtr<StunAddrsRequestParent>(this),
                   &StunAddrsRequestParent::SendStunAddrs_m, std::move(addrs)),
      NS_DISPATCH_NORMAL);
}

void StunAddrsRequestParent::SendStunAddrs_m(const NrIceStunAddrArray& addrs) {
  ASSERT_ON_THREAD(mMainThread);

  if (mIPCClosed) {
    // nothing to do: child probably crashed
    return;
  }

  mIPCClosed = true;
  // send the new addresses back to the child
  Unused << SendOnStunAddrsAvailable(addrs);
}

NS_IMPL_ADDREF(StunAddrsRequestParent)
NS_IMPL_RELEASE(StunAddrsRequestParent)

}  // namespace net
}  // namespace mozilla
