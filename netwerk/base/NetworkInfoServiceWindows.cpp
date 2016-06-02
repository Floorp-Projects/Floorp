/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <winsock2.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>

#include "mozilla/UniquePtr.h"

#include "NetworkInfoServiceImpl.h"

namespace mozilla {
namespace net {

nsresult
DoListAddresses(AddrMapType& aAddrMap)
{
  UniquePtr<MIB_IPADDRTABLE> ipAddrTable;
  DWORD size = sizeof(MIB_IPADDRTABLE);

  ipAddrTable.reset((MIB_IPADDRTABLE*) malloc(size));
  if (!ipAddrTable) {
    return NS_ERROR_FAILURE;
  }

  DWORD retVal = GetIpAddrTable(ipAddrTable.get(), &size, 0);
  if (retVal == ERROR_INSUFFICIENT_BUFFER) {
    ipAddrTable.reset((MIB_IPADDRTABLE*) malloc(size));
    if (!ipAddrTable) {
      return NS_ERROR_FAILURE;
    }
    retVal = GetIpAddrTable(ipAddrTable.get(), &size, 0);
  }
  if (retVal != NO_ERROR) {
    return NS_ERROR_FAILURE;
  }

  for (DWORD i = 0; i < ipAddrTable->dwNumEntries; i++) {
    int index = ipAddrTable->table[i].dwIndex;
    uint32_t addrVal = (uint32_t) ipAddrTable->table[i].dwAddr;

    nsCString indexString;
    indexString.AppendInt(index, 10);

    nsCString addrString;
    addrString.AppendPrintf("%d.%d.%d.%d",
        (addrVal >> 0) & 0xff, (addrVal >> 8) & 0xff,
        (addrVal >> 16) & 0xff, (addrVal >> 24) & 0xff);

    aAddrMap.Put(indexString, addrString);
  }

  return NS_OK;
}

} // namespace net
} // namespace mozilla
