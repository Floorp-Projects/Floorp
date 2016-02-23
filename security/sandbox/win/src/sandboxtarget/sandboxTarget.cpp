/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define TARGET_SANDBOX_EXPORTS
#include "sandboxTarget.h"

namespace mozilla {

// We need to define this function out of line so that clang-cl doesn't inline
// it.
/* static */ SandboxTarget*
SandboxTarget::Instance()
{
  static SandboxTarget sb;
  return &sb;
}

}
