/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIAtom.h"
#include "nsString.h"
#include "nspr.h"
#include "nsCOMPtr.h"
#include "mozilla/ModuleUtils.h"
#include "nsParserCIID.h"
#include "nsParser.h"
#include "CNavDTD.h"
#include "nsHTMLEntities.h"
#include "nsHTMLTokenizer.h"
//#include "nsTextTokenizer.h"
#include "nsElementTable.h"
#include "nsParserService.h"
#include "nsSAXAttributes.h"
#include "nsSAXLocator.h"
#include "nsSAXXMLReader.h"

#if defined(DEBUG)
#include "nsExpatDriver.h"
#endif

//----------------------------------------------------------------------

#if defined(DEBUG)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsExpatDriver)
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(nsParser)
NS_GENERIC_FACTORY_CONSTRUCTOR(CNavDTD)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsParserService)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsSAXAttributes)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSAXXMLReader)

#if defined(DEBUG)
NS_DEFINE_NAMED_CID(NS_EXPAT_DRIVER_CID);
#endif
NS_DEFINE_NAMED_CID(NS_PARSER_CID);
NS_DEFINE_NAMED_CID(NS_CNAVDTD_CID);
NS_DEFINE_NAMED_CID(NS_PARSERSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_SAXATTRIBUTES_CID);
NS_DEFINE_NAMED_CID(NS_SAXXMLREADER_CID);

static const mozilla::Module::CIDEntry kParserCIDs[] = {
#if defined(DEBUG)
  { &kNS_EXPAT_DRIVER_CID, false, nullptr, nsExpatDriverConstructor },
#endif
  { &kNS_PARSER_CID, false, nullptr, nsParserConstructor },
  { &kNS_CNAVDTD_CID, false, nullptr, CNavDTDConstructor },
  { &kNS_PARSERSERVICE_CID, false, nullptr, nsParserServiceConstructor },
  { &kNS_SAXATTRIBUTES_CID, false, nullptr, nsSAXAttributesConstructor },
  { &kNS_SAXXMLREADER_CID, false, nullptr, nsSAXXMLReaderConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kParserContracts[] = {
  { NS_PARSERSERVICE_CONTRACTID, &kNS_PARSERSERVICE_CID },
  { NS_SAXATTRIBUTES_CONTRACTID, &kNS_SAXATTRIBUTES_CID },
  { NS_SAXXMLREADER_CONTRACTID, &kNS_SAXXMLREADER_CID },
  { nullptr }
};

static nsresult
Initialize()
{
  nsresult rv = nsHTMLTags::AddRefTable();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsHTMLEntities::AddRefTable();
  if (NS_FAILED(rv)) {
    nsHTMLTags::ReleaseTable();
    return rv;
  }
#ifdef DEBUG
  CheckElementTable();
#endif

#ifdef DEBUG
  nsHTMLTags::TestTagTable();
#endif

  return nsParser::Init();
}

static void
Shutdown()
{
  nsHTMLTags::ReleaseTable();
  nsHTMLEntities::ReleaseTable();
  nsParser::Shutdown();
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
