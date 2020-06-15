/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/process/memory.h"

#include "mozilla/Assertions.h"

namespace base {

void TerminateBecauseOutOfMemory(size_t size) {
  MOZ_CRASH("Hit base::TerminateBecauseOutOfMemory");
}

}  // namespace base
