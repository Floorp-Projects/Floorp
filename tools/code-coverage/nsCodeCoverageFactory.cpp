/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsCOMPtr.h"
#include "nsCodeCoverage.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsCodeCoverage)

NS_DEFINE_NAMED_CID(NS_CODECOVERAGE_CID);

static const mozilla::Module::CIDEntry kCodeCoverageCIDs[] = {
    { &kNS_CODECOVERAGE_CID, false, nullptr, nsCodeCoverageConstructor },
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kCodeCoverageContracts[] = {
    { "@mozilla.org/tools/code-coverage;1", &kNS_CODECOVERAGE_CID },
    { nullptr }
};

static const mozilla::Module kCodeCoverageModule = {
    mozilla::Module::kVersion,
    kCodeCoverageCIDs,
    kCodeCoverageContracts
};

NSMODULE_DEFN(nsCodeCoverageModule) = &kCodeCoverageModule;
