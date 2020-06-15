/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a partial implementation of Chromium's source file
// //sandbox/win/src/sandbox_policy_diagnostic.h

#ifndef SANDBOX_WIN_SRC_SANDBOX_POLICY_DIAGNOSTIC_H_
#define SANDBOX_WIN_SRC_SANDBOX_POLICY_DIAGNOSTIC_H_

#include "mozilla/Assertions.h"

namespace sandbox {

class PolicyBase;

class PolicyDiagnostic final : public PolicyInfo {
 public:
  PolicyDiagnostic(PolicyBase*) {}
  ~PolicyDiagnostic() override = default;
  const char* JsonString() override { MOZ_CRASH(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(PolicyDiagnostic);
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SANDBOX_POLICY_DIAGNOSTIC_H_
