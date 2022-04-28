/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"

#if defined(NS_PRINTING)
#  include "nsPrintingPromptService.h"

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsPrintingPromptService,
                                         nsPrintingPromptService::GetSingleton)

NS_DEFINE_NAMED_CID(NS_PRINTINGPROMPTSERVICE_CID);
#endif

static const mozilla::Module::CIDEntry kEmbeddingCIDs[] = {
#if defined(NS_PRINTING)
    {&kNS_PRINTINGPROMPTSERVICE_CID, false, nullptr,
     nsPrintingPromptServiceConstructor
#  ifdef PROXY_PRINTING
     ,
     mozilla::Module::MAIN_PROCESS_ONLY
#  endif
    },
#endif
    {nullptr}};

static const mozilla::Module::ContractIDEntry kEmbeddingContracts[] = {
#if defined(NS_PRINTING)
    {NS_PRINTINGPROMPTSERVICE_CONTRACTID, &kNS_PRINTINGPROMPTSERVICE_CID
#  ifdef PROXY_PRINTING
     ,
     mozilla::Module::MAIN_PROCESS_ONLY
#  endif
    },
#endif
    {nullptr}};

extern const mozilla::Module kEmbeddingModule = {
    mozilla::Module::kVersion, kEmbeddingCIDs, kEmbeddingContracts};
