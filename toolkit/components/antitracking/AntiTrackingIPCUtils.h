/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_antitrackingipcutils_h
#define mozilla_antitrackingipcutils_h

#include "ipc/IPCMessageUtils.h"

#include "mozilla/AntiTrackingCommon.h"

namespace IPC {

// For allowing passing the enum AntiTrackingCommon::StorageAccessGrantedReason
// over IPC.
template <>
struct ParamTraits<mozilla::AntiTrackingCommon::StorageAccessGrantedReason>
    : public ContiguousEnumSerializerInclusive<
          mozilla::AntiTrackingCommon::StorageAccessGrantedReason,
          mozilla::AntiTrackingCommon::StorageAccessGrantedReason::
              eStorageAccessAPI,
          mozilla::AntiTrackingCommon::StorageAccessGrantedReason::eOpener> {};

}  // namespace IPC

#endif  // mozilla_antitrackingipcutils_h
