/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTransferableIPC_h__
#define nsTransferableIPC_h__

#include "ipc/EnumSerializer.h"

enum class TransferableDataType : uint8_t {
  String,
  CString,
  InputStream,
  ImageContainer,
  Blob,
};

namespace IPC {

template <>
struct ParamTraits<TransferableDataType>
    : public ContiguousEnumSerializerInclusive<TransferableDataType,
                                               TransferableDataType::String,
                                               TransferableDataType::Blob> {};

}  // namespace IPC

#endif  // nsTransferableIPC_h__
