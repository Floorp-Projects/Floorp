/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ipc_DataStorageIPCUtils_h
#define ipc_DataStorageIPCUtils_h

#include "ipc/IPCMessageUtils.h"
#include "mozilla/DataStorage.h"

namespace IPC {
  template<>
  struct ParamTraits<mozilla::DataStorageType> :
    public ContiguousEnumSerializer<mozilla::DataStorageType,
                                    mozilla::DataStorage_Persistent,
                                    mozilla::DataStorageType(mozilla::DataStorage_Private + 1)> {};
} // namespace IPC

#endif // mozilla_DataStorageIPCUtils_hh
