/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsParser.h"
#include "nsParserCIID.h"
#include "nsHTMLTags.h"

//----------------------------------------------------------------------

NS_GENERIC_FACTORY_CONSTRUCTOR(nsParser)

NS_DEFINE_NAMED_CID(NS_PARSER_CID);

static const mozilla::Module::CIDEntry kParserCIDs[] = {
    {&kNS_PARSER_CID, false, nullptr, nsParserConstructor}, {nullptr}};

static nsresult Initialize() {
  nsresult rv = nsHTMLTags::AddRefTable();
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
  CheckElementTable();
  nsHTMLTags::TestTagTable();
#endif

  return rv;
}

static void Shutdown() { nsHTMLTags::ReleaseTable(); }

extern const mozilla::Module kParserModule = {mozilla::Module::kVersion,
                                              kParserCIDs,
                                              nullptr,
                                              nullptr,
                                              nullptr,
                                              Initialize,
                                              Shutdown};
