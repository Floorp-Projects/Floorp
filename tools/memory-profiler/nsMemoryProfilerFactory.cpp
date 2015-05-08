/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsCOMPtr.h"
#include "MemoryProfiler.h"

using mozilla::MemoryProfiler;

NS_GENERIC_FACTORY_CONSTRUCTOR(MemoryProfiler)

NS_DEFINE_NAMED_CID(MEMORY_PROFILER_CID);

static const mozilla::Module::CIDEntry kMemoryProfilerCIDs[] = {
  { &kMEMORY_PROFILER_CID, false, nullptr, MemoryProfilerConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kMemoryProfilerContracts[] = {
  { MEMORY_PROFILER_CONTRACT_ID, &kMEMORY_PROFILER_CID },
  { nullptr }
};

static const mozilla::Module kMemoryProfilerModule = {
  mozilla::Module::kVersion,
  kMemoryProfilerCIDs,
  kMemoryProfilerContracts
};

NSMODULE_DEFN(nsMemoryProfilerModule) = &kMemoryProfilerModule;
