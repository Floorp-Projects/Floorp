/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxInitialization.h"

#include "sandbox/win/src/sandbox_factory.h"

namespace mozilla {
namespace sandboxing {

static sandbox::TargetServices*
InitializeTargetServices()
{
  sandbox::TargetServices* targetServices =
    sandbox::SandboxFactory::GetTargetServices();
  if (!targetServices) {
    return nullptr;
  }

  if (targetServices->Init() != sandbox::SBOX_ALL_OK) {
    return nullptr;
  }

  return targetServices;
}

sandbox::TargetServices*
GetInitializedTargetServices()
{
  static sandbox::TargetServices* sInitializedTargetServices =
    InitializeTargetServices();

  return sInitializedTargetServices;
}

void
LowerSandbox()
{
  GetInitializedTargetServices()->LowerToken();
}

} // sandboxing
} // mozilla
