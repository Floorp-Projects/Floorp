/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsIClassInfoImpl.h"

#include "nsSample.h"

////////////////////////////////////////////////////////////////////////
// With the below sample, you can define an implementation glue
// that talks with xpcom for creation of component nsSampleImpl
// that implement the interface nsISample. This can be extended for
// any number of components.
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// Define the contructor function for the object nsSampleImpl
//
// What this does is defines a function nsSampleImplConstructor which we
// will specific in the nsModuleComponentInfo table. This function will
// be used by the generic factory to create an instance of nsSampleImpl.
//
// NOTE: This creates an instance of nsSampleImpl by using the default
//		 constructor nsSampleImpl::nsSampleImpl()
//
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSampleImpl)

// The following line defines a kNS_SAMPLE_CID CID variable.
NS_DEFINE_NAMED_CID(NS_SAMPLE_CID);

// Build a table of ClassIDs (CIDs) which are implemented by this module. CIDs
// should be completely unique UUIDs.
// each entry has the form { CID, service, factoryproc, constructorproc }
// where factoryproc is usually NULL.
static const mozilla::Module::CIDEntry kSampleCIDs[] = {
    { &kNS_SAMPLE_CID, false, NULL, nsSampleImplConstructor },
    { NULL }
};

// Build a table which maps contract IDs to CIDs.
// A contract is a string which identifies a particular set of functionality. In some
// cases an extension component may override the contract ID of a builtin gecko component
// to modify or extend functionality.
static const mozilla::Module::ContractIDEntry kSampleContracts[] = {
    { NS_SAMPLE_CONTRACTID, &kNS_SAMPLE_CID },
    { NULL }
};

// Category entries are category/key/value triples which can be used
// to register contract ID as content handlers or to observe certain
// notifications. Most modules do not need to register any category
// entries: this is just a sample of how you'd do it.
// @see nsICategoryManager for information on retrieving category data.
static const mozilla::Module::CategoryEntry kSampleCategories[] = {
    { "my-category", "my-key", NS_SAMPLE_CONTRACTID },
    { NULL }
};

static const mozilla::Module kSampleModule = {
    mozilla::Module::kVersion,
    kSampleCIDs,
    kSampleContracts,
    kSampleCategories
};

// The following line implements the one-and-only "NSModule" symbol exported from this
// shared library.
NSMODULE_DEFN(nsSampleModule) = &kSampleModule;

// The following line implements the one-and-only "NSGetModule" symbol
// for compatibility with mozilla 1.9.2. You should only use this
// if you need a binary which is backwards-compatible and if you use
// interfaces carefully across multiple versions.
NS_IMPL_MOZILLA192_NSGETMODULE(&kSampleModule)
