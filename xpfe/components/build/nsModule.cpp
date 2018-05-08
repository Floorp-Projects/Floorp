/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsDirectoryViewer.h"
#include "rdf.h"
#include "nsRDFCID.h"
#include "nsCURILoader.h"

// Factory constructors
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsHTTPIndex, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDirectoryViewerFactory)

NS_DEFINE_NAMED_CID(NS_DIRECTORYVIEWERFACTORY_CID);
NS_DEFINE_NAMED_CID(NS_HTTPINDEX_SERVICE_CID);

static const mozilla::Module::CIDEntry kXPFECIDs[] = {
    { &kNS_DIRECTORYVIEWERFACTORY_CID, false, nullptr, nsDirectoryViewerFactoryConstructor },
    { &kNS_HTTPINDEX_SERVICE_CID, false, nullptr, nsHTTPIndexConstructor },
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kXPFEContracts[] = {
    { "@mozilla.org/xpfe/http-index-format-factory-constructor", &kNS_DIRECTORYVIEWERFACTORY_CID },
    { NS_HTTPINDEX_SERVICE_CONTRACTID, &kNS_HTTPINDEX_SERVICE_CID },
    { NS_HTTPINDEX_DATASOURCE_CONTRACTID, &kNS_HTTPINDEX_SERVICE_CID },
    { nullptr }
};

static const mozilla::Module::CategoryEntry kXPFECategories[] = {
    { "Gecko-Content-Viewers", "application/http-index-format", "@mozilla.org/xpfe/http-index-format-factory-constructor" },
    { nullptr }
};

static const mozilla::Module kXPFEModule = {
    mozilla::Module::kVersion,
    kXPFECIDs,
    kXPFEContracts,
    kXPFECategories
};

NSMODULE_DEFN(application) = &kXPFEModule;
