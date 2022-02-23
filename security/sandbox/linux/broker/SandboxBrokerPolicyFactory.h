/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxBrokerPolicyFactory_h
#define mozilla_SandboxBrokerPolicyFactory_h

#include "mozilla/SandboxBroker.h"

#include <mutex>

namespace mozilla {

class SandboxBrokerPolicyFactory {
 public:
  SandboxBrokerPolicyFactory() = default;

  UniquePtr<SandboxBroker::Policy> GetContentPolicy(int aPid,
                                                    bool aFileProcess);

  static UniquePtr<SandboxBroker::Policy> GetRDDPolicy(int aPid);
  static UniquePtr<SandboxBroker::Policy> GetSocketProcessPolicy(int aPid);
  static UniquePtr<SandboxBroker::Policy> GetUtilityProcessPolicy(int aPid);

 private:
  UniquePtr<const SandboxBroker::Policy> mCommonContentPolicy;
  std::once_flag mContentInited;

  void InitContentPolicy();
};

}  // namespace mozilla

#endif  // mozilla_SandboxBrokerPolicyFactory_h
