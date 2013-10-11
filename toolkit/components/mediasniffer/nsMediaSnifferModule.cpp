/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"

#include "nsMediaSniffer.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsMediaSniffer)

NS_DEFINE_NAMED_CID(NS_MEDIA_SNIFFER_CID);

static const mozilla::Module::CIDEntry kMediaSnifferCIDs[] = {
    { &kNS_MEDIA_SNIFFER_CID, false, nullptr, nsMediaSnifferConstructor },
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kMediaSnifferContracts[] = {
    { NS_MEDIA_SNIFFER_CONTRACTID, &kNS_MEDIA_SNIFFER_CID },
    { nullptr }
};

static const mozilla::Module::CategoryEntry kMediaSnifferCategories[] = {
    { "content-sniffing-services", NS_MEDIA_SNIFFER_CONTRACTID, NS_MEDIA_SNIFFER_CONTRACTID},
    { "net-content-sniffers", NS_MEDIA_SNIFFER_CONTRACTID, NS_MEDIA_SNIFFER_CONTRACTID},
    { nullptr }
};

static const mozilla::Module kMediaSnifferModule = {
    mozilla::Module::kVersion,
    kMediaSnifferCIDs,
    kMediaSnifferContracts,
    kMediaSnifferCategories
};

NSMODULE_DEFN(nsMediaSnifferModule) = &kMediaSnifferModule;
