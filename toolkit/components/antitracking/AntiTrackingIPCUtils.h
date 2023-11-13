/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_antitrackingipcutils_h
#define mozilla_antitrackingipcutils_h

#include "ipc/EnumSerializer.h"

#include "mozilla/ContentBlockingNotifier.h"
#include "mozilla/StorageAccessAPIHelper.h"

#include "nsILoadInfo.h"

namespace IPC {

// For allowing passing the enum
// ContentBlockingNotifier::StorageAccessPermissionGrantedReason over IPC.
template <>
struct ParamTraits<
    mozilla::ContentBlockingNotifier::StorageAccessPermissionGrantedReason>
    : public ContiguousEnumSerializerInclusive<
          mozilla::ContentBlockingNotifier::
              StorageAccessPermissionGrantedReason,
          mozilla::ContentBlockingNotifier::
              StorageAccessPermissionGrantedReason::eStorageAccessAPI,
          mozilla::ContentBlockingNotifier::
              StorageAccessPermissionGrantedReason::
                  ePrivilegeStorageAccessForOriginAPI> {};

// ContentBlockingNotifier::BlockingDecision over IPC.
template <>
struct ParamTraits<mozilla::ContentBlockingNotifier::BlockingDecision>
    : public ContiguousEnumSerializerInclusive<
          mozilla::ContentBlockingNotifier::BlockingDecision,
          mozilla::ContentBlockingNotifier::BlockingDecision::eBlock,
          mozilla::ContentBlockingNotifier::BlockingDecision::eAllow> {};

// StorageAccessAPIHelper::StorageAccessPromptChoices over IPC.
template <>
struct ParamTraits<mozilla::StorageAccessAPIHelper::StorageAccessPromptChoices>
    : public ContiguousEnumSerializerInclusive<
          mozilla::StorageAccessAPIHelper::StorageAccessPromptChoices,
          mozilla::StorageAccessAPIHelper::StorageAccessPromptChoices::eAllow,
          mozilla::StorageAccessAPIHelper::StorageAccessPromptChoices::
              eAllowAutoGrant> {};

// nsILoadInfo::StoragePermissionState over IPC.
template <>
struct ParamTraits<nsILoadInfo::StoragePermissionState>
    : public ContiguousEnumSerializerInclusive<
          nsILoadInfo::StoragePermissionState,
          nsILoadInfo::StoragePermissionState::NoStoragePermission,
          nsILoadInfo::StoragePermissionState::StoragePermissionAllowListed> {};

// ContentBlockingNotifier::CanvasFingerprinter over IPC.
template <>
struct ParamTraits<mozilla::ContentBlockingNotifier::CanvasFingerprinter>
    : public ContiguousEnumSerializerInclusive<
          mozilla::ContentBlockingNotifier::CanvasFingerprinter,
          mozilla::ContentBlockingNotifier::CanvasFingerprinter::eFingerprintJS,
          mozilla::ContentBlockingNotifier::CanvasFingerprinter::eMaybe> {};
}  // namespace IPC

#endif  // mozilla_antitrackingipcutils_h
