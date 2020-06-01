/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_antitrackingipcutils_h
#define mozilla_antitrackingipcutils_h

#include "ipc/IPCMessageUtils.h"

#include "mozilla/ContentBlockingNotifier.h"
#include "mozilla/ContentBlocking.h"

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
              StorageAccessPermissionGrantedReason::eOpener> {};

// ContentBlockingNotifier::BlockingDecision over IPC.
template <>
struct ParamTraits<mozilla::ContentBlockingNotifier::BlockingDecision>
    : public ContiguousEnumSerializerInclusive<
          mozilla::ContentBlockingNotifier::BlockingDecision,
          mozilla::ContentBlockingNotifier::BlockingDecision::eBlock,
          mozilla::ContentBlockingNotifier::BlockingDecision::eAllow> {};

// ContentBlocking::StorageAccessPromptChoices over IPC.
template <>
struct ParamTraits<mozilla::ContentBlocking::StorageAccessPromptChoices>
    : public ContiguousEnumSerializerInclusive<
          mozilla::ContentBlocking::StorageAccessPromptChoices,
          mozilla::ContentBlocking::StorageAccessPromptChoices::eAllow,
          mozilla::ContentBlocking::StorageAccessPromptChoices::
              eAllowAutoGrant> {};
}  // namespace IPC

#endif  // mozilla_antitrackingipcutils_h
