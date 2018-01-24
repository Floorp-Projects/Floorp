/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxFilter_h
#define mozilla_SandboxFilter_h

#include <vector>
#include "mozilla/Atomics.h"
#include "mozilla/Range.h"
#include "mozilla/UniquePtr.h"

namespace sandbox {
namespace bpf_dsl {
class Policy;
}
}

namespace mozilla {

#ifdef MOZ_CONTENT_SANDBOX
class SandboxBrokerClient;
struct ContentProcessSandboxParams;

UniquePtr<sandbox::bpf_dsl::Policy> GetContentSandboxPolicy(SandboxBrokerClient* aMaybeBroker,
                                                            ContentProcessSandboxParams&& aParams);
#endif

#ifdef MOZ_GMP_SANDBOX
class SandboxOpenedFiles;

// The SandboxOpenedFiles object must live until the process exits.
UniquePtr<sandbox::bpf_dsl::Policy> GetMediaSandboxPolicy(const SandboxOpenedFiles* aFiles);
#endif

} // namespace mozilla

#endif
