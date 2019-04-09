/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"

#if defined(MOZ_XUL) && defined(NS_PRINTING)
#  include "nsPrintingPromptService.h"
#  include "nsPrintingProxy.h"

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsPrintingPromptService,
                                         nsPrintingPromptService::GetSingleton)
#  ifdef PROXY_PRINTING
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsPrintingProxy,
                                         nsPrintingProxy::GetInstance)
#  endif

NS_DEFINE_NAMED_CID(NS_PRINTINGPROMPTSERVICE_CID);
#endif

static const mozilla::Module::CIDEntry kEmbeddingCIDs[] = {
#if defined(MOZ_XUL) && defined(NS_PRINTING)
#  ifdef PROXY_PRINTING
    {&kNS_PRINTINGPROMPTSERVICE_CID, false, nullptr,
     nsPrintingPromptServiceConstructor, mozilla::Module::MAIN_PROCESS_ONLY},
    {&kNS_PRINTINGPROMPTSERVICE_CID, false, nullptr, nsPrintingProxyConstructor,
     mozilla::Module::CONTENT_PROCESS_ONLY},
#  else
    {&kNS_PRINTINGPROMPTSERVICE_CID, false, nullptr,
     nsPrintingPromptServiceConstructor},
#  endif
#endif
    {nullptr}};

static const mozilla::Module::ContractIDEntry kEmbeddingContracts[] = {
#if defined(MOZ_XUL) && defined(NS_PRINTING)
    {NS_PRINTINGPROMPTSERVICE_CONTRACTID, &kNS_PRINTINGPROMPTSERVICE_CID},
#endif
    {nullptr}};

extern const mozilla::Module kEmbeddingModule = {
    mozilla::Module::kVersion, kEmbeddingCIDs, kEmbeddingContracts};
