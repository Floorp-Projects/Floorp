/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StunAddrsRequestParent.h"

#include "../runnable_utils.h"
#include "nsNetUtil.h"

#include "mtransport/nricectx.h"
#include "mtransport/nricemediastream.h" // needed only for including nricectx.h
#include "mtransport/nricestunaddr.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

StunAddrsRequestParent::StunAddrsRequestParent()
{
  NS_GetMainThread(getter_AddRefs(mMainThread));

  nsresult res;
  mSTSThread = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &res);
  MOZ_ASSERT(mSTSThread);
}

mozilla::ipc::IPCResult
StunAddrsRequestParent::RecvGetStunAddrs()
{
  ASSERT_ON_THREAD(mMainThread);

  RUN_ON_THREAD(mSTSThread,
                WrapRunnable(this, &StunAddrsRequestParent::GetStunAddrs_s),
                NS_DISPATCH_NORMAL);

  return IPC_OK();
}

void
StunAddrsRequestParent::ActorDestroy(ActorDestroyReason why)
{
  // nothing to do here
}

void
StunAddrsRequestParent::GetStunAddrs_s()
{
  ASSERT_ON_THREAD(mSTSThread);

  // get the stun addresses while on STS thread
  NrIceStunAddrArray addrs; // = NrIceCtx::GetStunAddrs();

  // in order to return the result over IPC, we need to be on main thread
  RUN_ON_THREAD(mMainThread,
                WrapRunnable(this,
                             &StunAddrsRequestParent::SendStunAddrs_m,
                             std::move(addrs)),
                NS_DISPATCH_NORMAL);
}

void
StunAddrsRequestParent::SendStunAddrs_m(const NrIceStunAddrArray& addrs)
{
  ASSERT_ON_THREAD(mMainThread);

  // send the new addresses back to the child
  Unused << SendOnStunAddrsAvailable(addrs);
}

NS_IMPL_ADDREF(StunAddrsRequestParent)
NS_IMPL_RELEASE(StunAddrsRequestParent)

} // namespace net
} // namespace mozilla
