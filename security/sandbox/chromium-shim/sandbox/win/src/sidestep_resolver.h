/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// This is a dummy version of the Chromium source file
// sandbox/win/src/sidestep_resolver.h, which contains classes that are never
// actually used. We crash in the member functions to ensure this.
// Formatting and guards closely match original file for easy comparison.

#ifndef SANDBOX_SRC_SIDESTEP_RESOLVER_H__
#define SANDBOX_SRC_SIDESTEP_RESOLVER_H__

#include <stddef.h>

#include "base/macros.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/resolver.h"

#include "mozilla/Assertions.h"

namespace sandbox {

class SidestepResolverThunk : public ResolverThunk {
 public:
  SidestepResolverThunk() {}
  ~SidestepResolverThunk() override {}

  // Implementation of Resolver::Setup.
  NTSTATUS Setup(const void* target_module,
                 const void* interceptor_module,
                 const char* target_name,
                 const char* interceptor_name,
                 const void* interceptor_entry_point,
                 void* thunk_storage,
                 size_t storage_bytes,
                 size_t* storage_used) override { MOZ_CRASH(); }

  size_t GetThunkSize() const override { MOZ_CRASH(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(SidestepResolverThunk);
};

class SmartSidestepResolverThunk : public SidestepResolverThunk {
 public:
  SmartSidestepResolverThunk() {}
  ~SmartSidestepResolverThunk() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SmartSidestepResolverThunk);
};

}  // namespace sandbox


#endif  // SANDBOX_SRC_SIDESTEP_RESOLVER_H__
