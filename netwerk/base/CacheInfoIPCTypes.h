/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CacheInfoIPCTypes_h_
#define mozilla_net_CacheInfoIPCTypes_h_

#include "ipc/IPCMessageUtils.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "nsICacheInfoChannel.h"

namespace IPC {

template <>
struct ParamTraits<nsICacheInfoChannel::PreferredAlternativeDataDeliveryType>
    : public ContiguousEnumSerializerInclusive<
          nsICacheInfoChannel::PreferredAlternativeDataDeliveryType,
          nsICacheInfoChannel::PreferredAlternativeDataDeliveryType::NONE,
          nsICacheInfoChannel::PreferredAlternativeDataDeliveryType::
              SERIALIZE> {};

}  // namespace IPC

#endif  // mozilla_net_CacheInfoIPCTypes_h_
