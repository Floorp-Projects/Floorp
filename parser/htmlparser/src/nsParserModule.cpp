/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsViewSourceHTML.h"

#if defined(NS_DEBUG)
#include "nsLoggingSink.h"
#include "nsExpatDriver.h"
#endif

//----------------------------------------------------------------------

#if defined(NS_DEBUG)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLoggingSink)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsExpatDriver)
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(nsParser)
NS_GENERIC_FACTORY_CONSTRUCTOR(CNavDTD)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsParserService)

NS_GENERIC_FACTORY_CONSTRUCTOR(CViewSourceHTML)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsSAXAttributes)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSAXXMLReader)

#if defined(NS_DEBUG)
NS_DEFINE_NAMED_CID(NS_LOGGING_SINK_CID);
NS_DEFINE_NAMED_CID(NS_EXPAT_DRIVER_CID);
#endif
NS_DEFINE_NAMED_CID(NS_PARSER_CID);
NS_DEFINE_NAMED_CID(NS_CNAVDTD_CID);
NS_DEFINE_NAMED_CID(NS_VIEWSOURCE_DTD_CID);
NS_DEFINE_NAMED_CID(NS_PARSERSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_SAXATTRIBUTES_CID);
NS_DEFINE_NAMED_CID(NS_SAXXMLREADER_CID);

static const mozilla::Module::CIDEntry kParserCIDs[] = {
#if defined(NS_DEBUG)
  { &kNS_LOGGING_SINK_CID, false, NULL, nsLoggingSinkConstructor },
  { &kNS_EXPAT_DRIVER_CID, false, NULL, nsExpatDriverConstructor },
#endif
  { &kNS_PARSER_CID, false, NULL, nsParserConstructor },
  { &kNS_CNAVDTD_CID, false, NULL, CNavDTDConstructor },
  { &kNS_VIEWSOURCE_DTD_CID, false, NULL, CViewSourceHTMLConstructor },
  { &kNS_PARSERSERVICE_CID, false, NULL, nsParserServiceConstructor },
  { &kNS_SAXATTRIBUTES_CID, false, NULL, nsSAXAttributesConstructor },
  { &kNS_SAXXMLREADER_CID, false, NULL, nsSAXXMLReaderConstructor },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kParserContracts[] = {
  { NS_PARSERSERVICE_CONTRACTID, &kNS_PARSERSERVICE_CID },
  { NS_SAXATTRIBUTES_CONTRACTID, &kNS_SAXATTRIBUTES_CID },
  { NS_SAXXMLREADER_CONTRACTID, &kNS_SAXXMLREADER_CID },
  { NULL }
};

static bool gInitialized = false;

static nsresult
Initialize()
{
  if (!gInitialized) {
    nsresult rv = nsHTMLTags::AddRefTable();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = nsHTMLEntities::AddRefTable();
    if (NS_FAILED(rv)) {
      nsHTMLTags::ReleaseTable();
      return rv;
    }
#ifdef NS_DEBUG
    CheckElementTable();
#endif
    CNewlineToken::AllocNewline();
    gInitialized = PR_TRUE;
  }

#ifdef DEBUG
  nsHTMLTags::TestTagTable();
#endif

  return nsParser::Init();
}

static void
Shutdown()
{
  if (gInitialized) {
    nsHTMLTags::ReleaseTable();
    nsHTMLEntities::ReleaseTable();
    nsDTDContext::ReleaseGlobalObjects();
    nsParser::Shutdown();
    CNewlineToken::FreeNewline();
    gInitialized = PR_FALSE;
  }
}

static mozilla::Module kParserModule = {
  mozilla::Module::kVersion,
  kParserCIDs,
  kParserContracts,
  NULL,
  NULL,
  Initialize,
  Shutdown
};

NSMODULE_DEFN(nsParserModule) = &kParserModule;
