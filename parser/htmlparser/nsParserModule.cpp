/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsParser.h"
#include "nsParserCIID.h"
#include "nsHTMLTags.h"
#include "nsSAXXMLReader.h"

//----------------------------------------------------------------------

NS_GENERIC_FACTORY_CONSTRUCTOR(nsParser)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSAXXMLReader)

NS_DEFINE_NAMED_CID(NS_PARSER_CID);
NS_DEFINE_NAMED_CID(NS_SAXXMLREADER_CID);

static const mozilla::Module::CIDEntry kParserCIDs[] = {
  { &kNS_PARSER_CID, false, nullptr, nsParserConstructor },
  { &kNS_SAXXMLREADER_CID, false, nullptr, nsSAXXMLReaderConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kParserContracts[] = {
  { NS_SAXXMLREADER_CONTRACTID, &kNS_SAXXMLREADER_CID },
  { nullptr }
};

static nsresult
Initialize()
{
  nsresult rv = nsHTMLTags::AddRefTable();
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
  CheckElementTable();
  nsHTMLTags::TestTagTable();
#endif

  return rv;
}

static void
Shutdown()
{
  nsHTMLTags::ReleaseTable();
}

static mozilla::Module kParserModule = {
  mozilla::Module::kVersion,
  kParserCIDs,
  kParserContracts,
  nullptr,
  nullptr,
  Initialize,
  Shutdown
};

NSMODULE_DEFN(nsParserModule) = &kParserModule;
