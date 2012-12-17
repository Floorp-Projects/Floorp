/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsCOMPtr.h"
#include "nsProfiler.h"
#include "nsProfilerCIID.h"

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsProfiler, Init)

NS_DEFINE_NAMED_CID(NS_PROFILER_CID);

static const mozilla::Module::CIDEntry kProfilerCIDs[] = {
    { &kNS_PROFILER_CID, false, NULL, nsProfilerConstructor },
    { NULL }
};

static const mozilla::Module::ContractIDEntry kProfilerContracts[] = {
    { "@mozilla.org/tools/profiler;1", &kNS_PROFILER_CID },
    { NULL }
};

static const mozilla::Module kProfilerModule = {
    mozilla::Module::kVersion,
    kProfilerCIDs,
    kProfilerContracts
};

NSMODULE_DEFN(nsProfilerModule) = &kProfilerModule;
