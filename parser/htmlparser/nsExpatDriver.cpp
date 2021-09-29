/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsExpatDriver.h"
#include "nsCOMPtr.h"
#include "nsParserCIID.h"
#include "CParserContext.h"
#include "nsIExpatSink.h"
#include "nsIContentSink.h"
#include "nsIDocShell.h"
#include "nsParserMsgUtils.h"
#include "nsIURL.h"
#include "nsIUnicharInputStream.h"
#include "nsIProtocolHandler.h"
#include "nsNetUtil.h"
#include "nsTextFormatter.h"
#include "nsDirectoryServiceDefs.h"
#include "nsCRT.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsIScriptGlobalObject.h"
#include "nsIContentPolicy.h"
#include "nsComponentManagerUtils.h"
#include "nsContentPolicyUtils.h"
#include "nsError.h"
#include "nsXPCOMCIDInternal.h"
#include "nsUnicharInputStream.h"
#include "nsContentUtils.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/IntegerTypeTraits.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryComms.h"

#include "mozilla/Logging.h"

#ifdef MOZ_WASM_SANDBOXING_EXPAT
#  include "mozilla/ipc/LibrarySandboxPreload.h"
#endif

using mozilla::fallible;
using mozilla::LogLevel;
using mozilla::MakeStringSpan;
using mozilla::dom::Document;

#define kExpatSeparatorChar 0xFFFF

static const char16_t kUTF16[] = {'U', 'T', 'F', '-', '1', '6', '\0'};

static mozilla::LazyLogModule gExpatDriverLog("expatdriver");

// Use the same maximum tree depth as Chromium (see
// https://chromium.googlesource.com/chromium/src/+/f464165c1dedff1c955d3c051c5a9a1c6a0e8f6b/third_party/WebKit/Source/core/xml/parser/XMLDocumentParser.cpp#85).
static const uint16_t sMaxXMLTreeDepth = 5000;

/***************************** RLBOX HELPERS ********************************/
// Helpers for calling sandboxed expat functions in handlers

#define RLBOX_EXPAT_SAFE_CALL(foo, verifier, ...)                          \
  aSandbox.invoke_sandbox_function(foo, self->mExpatParser, ##__VA_ARGS__) \
      .copy_and_verify(verifier)

#define RLBOX_EXPAT_SAFE_MCALL(foo, verifier, ...)                    \
  mSandbox->invoke_sandbox_function(foo, mExpatParser, ##__VA_ARGS__) \
      .copy_and_verify(verifier)

#define RLBOX_EXPAT_MCALL(foo, ...) \
  mSandbox->invoke_sandbox_function(foo, mExpatParser, ##__VA_ARGS__)

/* safe_unverified is used whenever it's safe to not use a validator */
template <typename T>
static T safe_unverified(T val) {
  return val;
}
/*************************** END RLBOX HELPERS ******************************/

/***************************** EXPAT CALL BACKS ******************************/
// The callback handlers that get called from the expat parser.

static void Driver_HandleXMLDeclaration(
    rlbox_sandbox_expat& aSandbox, tainted_expat<void*> t_aUserData,
    tainted_expat<const XML_Char*> t_aVersion,
    tainted_expat<const XML_Char*> t_aEncoding,
    tainted_expat<int> t_aStandalone) {
  nsExpatDriver* driver =
      static_cast<nsExpatDriver*>(aSandbox.lookup_app_ptr(t_aUserData));
  NS_ASSERTION(driver, "expat driver should exist");

  int aStandalone = t_aStandalone.copy_and_verify([&](auto a) {
    MOZ_RELEASE_ASSERT(a >= -1 && a <= 1, "Unexpected standalone parameter");
    return a;
  });

  if (driver) {
    bool copied_aVersion = false;
    auto* aVersion =
        transfer_xmlstring_no_validate(aSandbox, t_aVersion, copied_aVersion);
    bool copied_aEncoding = false;
    auto* aEncoding =
        transfer_xmlstring_no_validate(aSandbox, t_aEncoding, copied_aEncoding);

    driver->HandleXMLDeclaration(aVersion, aEncoding, aStandalone);

    if (copied_aVersion) {
      free(aVersion);
    }
    if (copied_aEncoding) {
      free(aEncoding);
    }
  }
}

static void Driver_HandleCharacterData(rlbox_sandbox_expat& aSandbox,
                                       tainted_expat<void*> t_aUserData,
                                       tainted_expat<const XML_Char*> t_aData,
                                       tainted_expat<int> t_aLength) {
  nsExpatDriver* driver =
      static_cast<nsExpatDriver*>(aSandbox.lookup_app_ptr(t_aUserData));
  NS_ASSERTION(driver, "expat driver should exist");
  if (driver) {
    // aData is not null terminated; even with bad length we will not span
    // beyond sandbox boundary
    if (t_aData != nullptr) {
      uint32_t aLength = static_cast<uint32_t>(
          t_aLength.copy_and_verify(safe_unverified<int>));
      bool copied_aData = false;
      auto* aData = rlbox::copy_memory_or_deny_access(
          aSandbox, rlbox::sandbox_const_cast<char16_t*>(t_aData),
          aLength * sizeof(char16_t), false, copied_aData);
      driver->HandleCharacterData(aData, aLength);
      if (copied_aData) {
        free(aData);
      }
    } else {
      driver->HandleCharacterData(nullptr, 0);
    }
  }
}

static void Driver_HandleComment(rlbox_sandbox_expat& aSandbox,
                                 tainted_expat<void*> t_aUserData,
                                 tainted_expat<const XML_Char*> t_aName) {
  nsExpatDriver* driver = aSandbox.lookup_app_ptr(
      rlbox::sandbox_static_cast<nsExpatDriver*>(t_aUserData));
  NS_ASSERTION(driver, "expat driver should exist");
  if (driver) {
    bool copied_aName = false;
    auto* aName =
        transfer_xmlstring_no_validate(aSandbox, t_aName, copied_aName);

    driver->HandleComment(aName);
    if (copied_aName) {
      free(aName);
    }
  }
}

static void Driver_HandleProcessingInstruction(
    rlbox_sandbox_expat& aSandbox, tainted_expat<void*> t_aUserData,
    tainted_expat<const XML_Char*> t_aTarget,
    tainted_expat<const XML_Char*> t_aData) {
  nsExpatDriver* driver = aSandbox.lookup_app_ptr(
      rlbox::sandbox_static_cast<nsExpatDriver*>(t_aUserData));
  NS_ASSERTION(driver, "expat driver should exist");
  if (driver) {
    bool copied_aTarget = false;
    auto* aTarget =
        transfer_xmlstring_no_validate(aSandbox, t_aTarget, copied_aTarget);
    bool copied_aData = false;
    auto* aData =
        transfer_xmlstring_no_validate(aSandbox, t_aData, copied_aData);

    driver->HandleProcessingInstruction(aTarget, aData);
    if (copied_aTarget) {
      free(aTarget);
    }
    if (copied_aData) {
      free(aData);
    }
  }
}

static void Driver_HandleStartCdataSection(rlbox_sandbox_expat& aSandbox,
                                           tainted_expat<void*> t_aUserData) {
  nsExpatDriver* driver = aSandbox.lookup_app_ptr(
      rlbox::sandbox_static_cast<nsExpatDriver*>(t_aUserData));
  NS_ASSERTION(driver, "expat driver should exist");
  if (driver) {
    driver->HandleStartCdataSection();
  }
}

static void Driver_HandleEndCdataSection(rlbox_sandbox_expat& aSandbox,
                                         tainted_expat<void*> t_aUserData) {
  nsExpatDriver* driver = aSandbox.lookup_app_ptr(
      rlbox::sandbox_static_cast<nsExpatDriver*>(t_aUserData));
  NS_ASSERTION(driver, "expat driver should exist");
  if (driver) {
    driver->HandleEndCdataSection();
  }
}

static void Driver_HandleStartDoctypeDecl(
    rlbox_sandbox_expat& aSandbox, tainted_expat<void*> t_aUserData,
    tainted_expat<const XML_Char*> t_aDoctypeName,
    tainted_expat<const XML_Char*> t_aSysid,
    tainted_expat<const XML_Char*> t_aPubid,
    tainted_expat<int> t_aHasInternalSubset) {
  nsExpatDriver* driver = aSandbox.lookup_app_ptr(
      rlbox::sandbox_static_cast<nsExpatDriver*>(t_aUserData));
  NS_ASSERTION(driver, "expat driver should exist");
  if (driver) {
    bool copied_aDoctypeName = false;
    auto* aDoctypeName = transfer_xmlstring_no_validate(
        aSandbox, t_aDoctypeName, copied_aDoctypeName);
    bool copied_aSysid = false;
    auto* aSysid =
        transfer_xmlstring_no_validate(aSandbox, t_aSysid, copied_aSysid);
    bool copied_aPubid = false;
    auto* aPubid =
        transfer_xmlstring_no_validate(aSandbox, t_aPubid, copied_aPubid);

    bool aHasInternalSubset =
        !!(t_aHasInternalSubset.copy_and_verify(safe_unverified<int>));

    driver->HandleStartDoctypeDecl(aDoctypeName, aSysid, aPubid,
                                   aHasInternalSubset);

    if (copied_aDoctypeName) {
      free(aDoctypeName);
    }
    if (copied_aSysid) {
      free(aSysid);
    }
    if (copied_aPubid) {
      free(aPubid);
    }
  }
}

static void Driver_HandleEndDoctypeDecl(rlbox_sandbox_expat& aSandbox,
                                        tainted_expat<void*> t_aUserData) {
  nsExpatDriver* driver = aSandbox.lookup_app_ptr(
      rlbox::sandbox_static_cast<nsExpatDriver*>(t_aUserData));
  NS_ASSERTION(driver, "expat driver should exist");
  if (driver) {
    driver->HandleEndDoctypeDecl();
  }
}

static tainted_expat<int> Driver_HandleExternalEntityRef(
    rlbox_sandbox_expat& aSandbox, tainted_expat<XML_Parser> t_aParser,
    tainted_expat<const XML_Char*> t_aOpenEntityNames,
    tainted_expat<const XML_Char*> t_aBase,
    tainted_expat<const XML_Char*> t_aSystemId,
    tainted_expat<const XML_Char*> t_aPublicId) {
  tainted_expat<void*> t_aUserData =
      rlbox::sandbox_static_cast<void*>(t_aParser);
  nsExpatDriver* driver =
      static_cast<nsExpatDriver*>(aSandbox.lookup_app_ptr(t_aUserData));
  NS_ASSERTION(driver, "expat driver should exist");
  if (!driver) {
    return 1;
  }

  bool copied_aOpenEntityNames = false;
  auto* aOpenEntityNames = transfer_xmlstring_no_validate(
      aSandbox, t_aOpenEntityNames, copied_aOpenEntityNames);
  bool copied_aBase = false;
  auto* aBase = transfer_xmlstring_no_validate(aSandbox, t_aBase, copied_aBase);
  bool copied_aSystemId = false;
  auto* aSystemId =
      transfer_xmlstring_no_validate(aSandbox, t_aSystemId, copied_aSystemId);
  bool copied_aPublicId = false;
  auto* aPublicId =
      transfer_xmlstring_no_validate(aSandbox, t_aPublicId, copied_aPublicId);

  auto rval = driver->HandleExternalEntityRef(aOpenEntityNames, aBase,
                                              aSystemId, aPublicId);

  if (copied_aOpenEntityNames) {
    free(aOpenEntityNames);
  }
  if (copied_aBase) {
    free(aBase);
  }
  if (copied_aSystemId) {
    free(aSystemId);
  }
  if (copied_aPublicId) {
    free(aPublicId);
  }

  return rval;
}

/***************************** END CALL BACKS ********************************/

/***************************** CATALOG UTILS *********************************/

// Initially added for bug 113400 to switch from the remote "XHTML 1.0 plus
// MathML 2.0" DTD to the the lightweight customized version that Mozilla uses.
// Since Mozilla is not validating, no need to fetch a *huge* file at each
// click.
// XXX The cleanest solution here would be to fix Bug 98413: Implement XML
// Catalogs.
struct nsCatalogData {
  const char* mPublicID;
  const char* mLocalDTD;
  const char* mAgentSheet;
};

// The order of this table is guestimated to be in the optimum order
static const nsCatalogData kCatalogTable[] = {
    {"-//W3C//DTD XHTML 1.0 Transitional//EN", "htmlmathml-f.ent", nullptr},
    {"-//W3C//DTD XHTML 1.1//EN", "htmlmathml-f.ent", nullptr},
    {"-//W3C//DTD XHTML 1.0 Strict//EN", "htmlmathml-f.ent", nullptr},
    {"-//W3C//DTD XHTML 1.0 Frameset//EN", "htmlmathml-f.ent", nullptr},
    {"-//W3C//DTD XHTML Basic 1.0//EN", "htmlmathml-f.ent", nullptr},
    {"-//W3C//DTD XHTML 1.1 plus MathML 2.0//EN", "htmlmathml-f.ent", nullptr},
    {"-//W3C//DTD XHTML 1.1 plus MathML 2.0 plus SVG 1.1//EN",
     "htmlmathml-f.ent", nullptr},
    {"-//W3C//DTD MathML 2.0//EN", "htmlmathml-f.ent", nullptr},
    {"-//WAPFORUM//DTD XHTML Mobile 1.0//EN", "htmlmathml-f.ent", nullptr},
    {nullptr, nullptr, nullptr}};

static const nsCatalogData* LookupCatalogData(const char16_t* aPublicID) {
  nsDependentString publicID(aPublicID);

  // linear search for now since the number of entries is going to
  // be negligible, and the fix for bug 98413 would get rid of this
  // code anyway
  const nsCatalogData* data = kCatalogTable;
  while (data->mPublicID) {
    if (publicID.EqualsASCII(data->mPublicID)) {
      return data;
    }
    ++data;
  }

  return nullptr;
}

// This function provides a resource URI to a local DTD
// in resource://gre/res/dtd/ which may or may not exist.
// If aCatalogData is provided, it is used to remap the
// DTD instead of taking the filename from the URI.  aDTD
// may be null in some cases that are relying on
// aCatalogData working for them.
static void GetLocalDTDURI(const nsCatalogData* aCatalogData, nsIURI* aDTD,
                           nsIURI** aResult) {
  nsAutoCString fileName;
  if (aCatalogData) {
    // remap the DTD to a known local DTD
    fileName.Assign(aCatalogData->mLocalDTD);
  }

  if (fileName.IsEmpty()) {
    // Try to see if the user has installed the DTD file -- we extract the
    // filename.ext of the DTD here. Hence, for any DTD for which we have
    // no predefined mapping, users just have to copy the DTD file to our
    // special DTD directory and it will be picked.
    nsCOMPtr<nsIURL> dtdURL = do_QueryInterface(aDTD);
    if (!dtdURL) {
      // Not a URL with a filename, or maybe it was null.  Either way, nothing
      // else we can do here.
      return;
    }

    dtdURL->GetFileName(fileName);
    if (fileName.IsEmpty()) {
      return;
    }
  }

  nsAutoCString respath("resource://gre/res/dtd/");
  respath += fileName;
  NS_NewURI(aResult, respath);
}

/***************************** END CATALOG UTILS *****************************/

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsExpatDriver)
  NS_INTERFACE_MAP_ENTRY(nsITokenizer)
  NS_INTERFACE_MAP_ENTRY(nsIDTD)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDTD)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsExpatDriver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsExpatDriver)

NS_IMPL_CYCLE_COLLECTION(nsExpatDriver, mSink)

nsExpatDriver::nsExpatDriver()
    : mSandboxData(nullptr),
      mSandbox(nullptr),
      mAppPtr(),
      mExpatParser(nullptr),
      mInCData(false),
      mInInternalSubset(false),
      mInExternalDTD(false),
      mMadeFinalCallToExpat(false),
      mIsFinalChunk(false),
      mInternalState(NS_OK),
      mExpatBuffered(0),
      mTagDepth(0),
      mCatalogData(nullptr),
      mInnerWindowID(0) {}

nsExpatDriver::~nsExpatDriver() {
  if (mSandboxData) {
    if (mExpatParser) {
      RLBOX_EXPAT_MCALL(MOZ_XML_ParserFree);

      mAppPtr.unregister();
      mExpatParser = nullptr;
      mSandbox = nullptr;
      mSandboxData = nullptr;
    }
  }
}

/* static */
void nsExpatDriver::HandleStartElement(
    rlbox_sandbox_expat& aSandbox, tainted_expat<void*> t_aUserData,
    tainted_expat<const char16_t*> t_aName,
    tainted_expat<const char16_t**> t_aAtts) {
  nsExpatDriver* self = aSandbox.lookup_app_ptr(
      rlbox::sandbox_static_cast<nsExpatDriver*>(t_aUserData));
  NS_ASSERTION(self, "expat driver should exist");
  if (self) {
    NS_ASSERTION(self->mSink, "content sink not found!");

    bool copied_aName = false;
    auto* aName =
        transfer_xmlstring_no_validate(aSandbox, t_aName, copied_aName);

    // Calculate the total number of elements in aAtts.
    // XML_GetSpecifiedAttributeCount will only give us the number of specified
    // attrs (twice that number, actually), so we have to check for default
    // attrs ourselves.
    int count = RLBOX_EXPAT_SAFE_CALL(MOZ_XML_GetSpecifiedAttributeCount,
                                      safe_unverified<int>);
    MOZ_RELEASE_ASSERT(count >= 0, "Unexpected attribute count");
    uint32_t attrArrayLength;
    for (attrArrayLength = count;
         (t_aAtts[attrArrayLength] != nullptr)
             .unverified_safe_because("Bad length is checked later");
         attrArrayLength += 2) {
      // Just looping till we find out what the length is
    }
    // A malicious length could result in an overflow when we allocate aAtts
    // and then access elements of the array.
    MOZ_RELEASE_ASSERT(attrArrayLength < UINT32_MAX, "Overflow attempt");

    // Copy tainted aAtts from sandbox
    bool copied_aAtts = false;  // We're copying or transfering all elements
    char16_t** aAtts = static_cast<char16_t**>(
        malloc(sizeof(char16_t*) * (attrArrayLength + 1)));
    MOZ_RELEASE_ASSERT(aAtts);
    for (uint32_t i = 0; i < attrArrayLength; i++) {
      aAtts[i] =
          transfer_xmlstring_no_validate(aSandbox, t_aAtts[i], copied_aAtts);
      MOZ_RELEASE_ASSERT(aAtts[i]);
    }
    aAtts[attrArrayLength] = nullptr;

    if (self->mSink) {
      // We store the tagdepth in a PRUint16, so make sure the limit fits in a
      // PRUint16.
      static_assert(
          sMaxXMLTreeDepth <=
          std::numeric_limits<decltype(nsExpatDriver::mTagDepth)>::max());

      if (++self->mTagDepth > sMaxXMLTreeDepth) {
        self->MaybeStopParser(NS_ERROR_HTMLPARSER_HIERARCHYTOODEEP);
        goto cleanup;
      }

      nsresult rv = self->mSink->HandleStartElement(
          aName, const_cast<const char16_t**>(aAtts), attrArrayLength,
          RLBOX_EXPAT_SAFE_CALL(MOZ_XML_GetCurrentLineNumber,
                                safe_unverified<XML_Size>),
          RLBOX_EXPAT_SAFE_CALL(MOZ_XML_GetCurrentColumnNumber,
                                safe_unverified<XML_Size>));
      self->MaybeStopParser(rv);
    }
  cleanup:
    if (copied_aName) {
      free(aName);
    }
    if (copied_aAtts) {
      for (uint32_t i = 0; i < attrArrayLength; i++) {
        free(aAtts[i]);
      }
    }
    free(aAtts);
    return;
  }
}

/* static */
void nsExpatDriver::HandleStartElementForSystemPrincipal(
    rlbox_sandbox_expat& aSandbox, tainted_expat<void*> t_aUserData,
    tainted_expat<const char16_t*> t_aName,
    tainted_expat<const char16_t**> t_aAtts) {
  nsExpatDriver* self = aSandbox.lookup_app_ptr(
      rlbox::sandbox_static_cast<nsExpatDriver*>(t_aUserData));
  NS_ASSERTION(self, "expat driver should exist");
  if (self) {
    if (!RLBOX_EXPAT_SAFE_CALL(MOZ_XML_ProcessingEntityValue,
                               safe_unverified<XML_Bool>)) {
      HandleStartElement(aSandbox, t_aUserData, t_aName, t_aAtts);
    } else {
      nsCOMPtr<Document> doc =
          do_QueryInterface(self->mOriginalSink->GetTarget());

      // Adjust the column number so that it is one based rather than zero
      // based.
      uint32_t colNumber = RLBOX_EXPAT_SAFE_CALL(MOZ_XML_GetCurrentColumnNumber,
                                                 safe_unverified<XML_Size>) +
                           1;
      uint32_t lineNumber = RLBOX_EXPAT_SAFE_CALL(MOZ_XML_GetCurrentLineNumber,
                                                  safe_unverified<XML_Size>);

      int32_t nameSpaceID;
      RefPtr<nsAtom> prefix, localName;
      bool copied_aName = false;
      auto* aName =
          transfer_xmlstring_no_validate(aSandbox, t_aName, copied_aName);
      nsContentUtils::SplitExpatName(aName, getter_AddRefs(prefix),
                                     getter_AddRefs(localName), &nameSpaceID);

      nsAutoString error;
      error.AppendLiteral("Ignoring element <");
      if (prefix) {
        error.Append(prefix->GetUTF16String());
        error.Append(':');
      }
      error.Append(localName->GetUTF16String());
      error.AppendLiteral("> created from entity value.");

      nsContentUtils::ReportToConsoleNonLocalized(
          error, nsIScriptError::warningFlag, "XML Document"_ns, doc, nullptr,
          u""_ns, lineNumber, colNumber);

      if (copied_aName) {
        free(aName);
      }
    }
  }
}

/* static */
void nsExpatDriver::HandleEndElement(rlbox_sandbox_expat& aSandbox,
                                     tainted_expat<void*> t_aUserData,
                                     tainted_expat<const char16_t*> t_aName) {
  nsExpatDriver* self = aSandbox.lookup_app_ptr(
      rlbox::sandbox_static_cast<nsExpatDriver*>(t_aUserData));
  NS_ASSERTION(self, "expat driver should exist");
  if (self) {
    bool copied_aName = false;
    auto* aName =
        transfer_xmlstring_no_validate(aSandbox, t_aName, copied_aName);

    NS_ASSERTION(self->mSink, "content sink not found!");
    NS_ASSERTION(self->mInternalState != NS_ERROR_HTMLPARSER_BLOCK,
                 "Shouldn't block from HandleStartElement.");

    if (self->mSink &&
        self->mInternalState != NS_ERROR_HTMLPARSER_STOPPARSING) {
      nsresult rv = self->mSink->HandleEndElement(aName);
      --self->mTagDepth;
      self->MaybeStopParser(rv);
    }
    if (copied_aName) {
      free(aName);
    }
  }
}

/* static */
void nsExpatDriver::HandleEndElementForSystemPrincipal(
    rlbox_sandbox_expat& aSandbox, tainted_expat<void*> t_aUserData,
    tainted_expat<const char16_t*> t_aName) {
  nsExpatDriver* self = aSandbox.lookup_app_ptr(
      rlbox::sandbox_static_cast<nsExpatDriver*>(t_aUserData));
  NS_ASSERTION(self, "expat driver should exist");
  if (self) {
    if (!RLBOX_EXPAT_SAFE_CALL(MOZ_XML_ProcessingEntityValue,
                               safe_unverified<XML_Bool>)) {
      HandleEndElement(aSandbox, t_aUserData, t_aName);
    }
  }
}

nsresult nsExpatDriver::HandleCharacterData(const char16_t* aValue,
                                            const uint32_t aLength) {
  NS_ASSERTION(mSink, "content sink not found!");

  if (mInCData) {
    if (!mCDataText.Append(aValue, aLength, fallible)) {
      MaybeStopParser(NS_ERROR_OUT_OF_MEMORY);
    }
  } else if (mSink) {
    nsresult rv = mSink->HandleCharacterData(aValue, aLength);
    MaybeStopParser(rv);
  }

  return NS_OK;
}

nsresult nsExpatDriver::HandleComment(const char16_t* aValue) {
  NS_ASSERTION(mSink, "content sink not found!");

  if (mInExternalDTD) {
    // Ignore comments from external DTDs
    return NS_OK;
  }

  if (mInInternalSubset) {
    mInternalSubset.AppendLiteral("<!--");
    mInternalSubset.Append(aValue);
    mInternalSubset.AppendLiteral("-->");
  } else if (mSink) {
    nsresult rv = mSink->HandleComment(aValue);
    MaybeStopParser(rv);
  }

  return NS_OK;
}

nsresult nsExpatDriver::HandleProcessingInstruction(const char16_t* aTarget,
                                                    const char16_t* aData) {
  NS_ASSERTION(mSink, "content sink not found!");

  if (mInExternalDTD) {
    // Ignore PIs in external DTDs for now.  Eventually we want to
    // pass them to the sink in a way that doesn't put them in the DOM
    return NS_OK;
  }

  if (mInInternalSubset) {
    mInternalSubset.AppendLiteral("<?");
    mInternalSubset.Append(aTarget);
    mInternalSubset.Append(' ');
    mInternalSubset.Append(aData);
    mInternalSubset.AppendLiteral("?>");
  } else if (mSink) {
    nsresult rv = mSink->HandleProcessingInstruction(aTarget, aData);
    MaybeStopParser(rv);
  }

  return NS_OK;
}

nsresult nsExpatDriver::HandleXMLDeclaration(const char16_t* aVersion,
                                             const char16_t* aEncoding,
                                             int32_t aStandalone) {
  if (mSink) {
    nsresult rv = mSink->HandleXMLDeclaration(aVersion, aEncoding, aStandalone);
    MaybeStopParser(rv);
  }

  return NS_OK;
}

nsresult nsExpatDriver::HandleDefault(const char16_t* aValue,
                                      const uint32_t aLength) {
  NS_ASSERTION(mSink, "content sink not found!");

  if (mInExternalDTD) {
    // Ignore newlines in external DTDs
    return NS_OK;
  }

  if (mInInternalSubset) {
    mInternalSubset.Append(aValue, aLength);
  } else if (mSink) {
    uint32_t i;
    nsresult rv = mInternalState;
    for (i = 0; i < aLength && NS_SUCCEEDED(rv); ++i) {
      if (aValue[i] == '\n' || aValue[i] == '\r') {
        rv = mSink->HandleCharacterData(&aValue[i], 1);
      }
    }
    MaybeStopParser(rv);
  }

  return NS_OK;
}

nsresult nsExpatDriver::HandleStartCdataSection() {
  mInCData = true;

  return NS_OK;
}

nsresult nsExpatDriver::HandleEndCdataSection() {
  NS_ASSERTION(mSink, "content sink not found!");

  mInCData = false;
  if (mSink) {
    nsresult rv =
        mSink->HandleCDataSection(mCDataText.get(), mCDataText.Length());
    MaybeStopParser(rv);
  }
  mCDataText.Truncate();

  return NS_OK;
}

nsresult nsExpatDriver::HandleStartDoctypeDecl(const char16_t* aDoctypeName,
                                               const char16_t* aSysid,
                                               const char16_t* aPubid,
                                               bool aHasInternalSubset) {
  mDoctypeName = aDoctypeName;
  mSystemID = aSysid;
  mPublicID = aPubid;

  if (aHasInternalSubset) {
    // Consuming a huge internal subset translates to numerous
    // allocations. In an effort to avoid too many allocations
    // setting mInternalSubset's capacity to be 1K ( just a guesstimate! ).
    mInInternalSubset = true;
    mInternalSubset.SetCapacity(1024);
  } else {
    // Distinguish missing internal subset from an empty one
    mInternalSubset.SetIsVoid(true);
  }

  return NS_OK;
}

nsresult nsExpatDriver::HandleEndDoctypeDecl() {
  NS_ASSERTION(mSink, "content sink not found!");

  mInInternalSubset = false;

  if (mSink) {
    // let the sink know any additional knowledge that we have about the
    // document (currently, from bug 124570, we only expect to pass additional
    // agent sheets needed to layout the XML vocabulary of the document)
    nsCOMPtr<nsIURI> data;
#if 0
    if (mCatalogData && mCatalogData->mAgentSheet) {
      NS_NewURI(getter_AddRefs(data), mCatalogData->mAgentSheet);
    }
#endif

    // The unused support for "catalog style sheets" was removed. It doesn't
    // look like we'll ever fix bug 98413 either.
    MOZ_ASSERT(!mCatalogData || !mCatalogData->mAgentSheet,
               "Need to add back support for catalog style sheets");

    // Note: mInternalSubset already doesn't include the [] around it.
    nsresult rv = mSink->HandleDoctypeDecl(mInternalSubset, mDoctypeName,
                                           mSystemID, mPublicID, data);
    MaybeStopParser(rv);
  }

  mInternalSubset.Truncate();

  return NS_OK;
}

static nsresult ExternalDTDStreamReaderFunc(nsIUnicharInputStream* aIn,
                                            void* aClosure,
                                            const char16_t* aFromSegment,
                                            uint32_t aToOffset, uint32_t aCount,
                                            uint32_t* aWriteCount) {
  nsresult rv = NS_ERROR_FAILURE;

  // Get sandbox and parser
  auto* pair =
      (std::pair<rlbox_sandbox_expat*, tainted_expat<XML_Parser>>*)(aClosure);
  auto* mSandbox = pair->first;
  auto mExpatParser = pair->second;

  // Transfer segment into the sandbox
  size_t len = aCount * sizeof(char16_t);
  bool copied_aFromSegment = false;
  auto t_aFromSegment = rlbox::copy_memory_or_grant_access(
      *mSandbox, reinterpret_cast<const char*>(aFromSegment), len, false,
      copied_aFromSegment);
  // Pass the buffer to expat for parsing.
  if (RLBOX_EXPAT_SAFE_MCALL(MOZ_XML_Parse, status_verifier, t_aFromSegment,
                             len, 0) == XML_STATUS_OK) {
    *aWriteCount = aCount;
    rv = NS_OK;
  } else {
    *aWriteCount = 0;
    rv = NS_ERROR_FAILURE;
  }

  if (copied_aFromSegment) {
    mSandbox->free_in_sandbox(t_aFromSegment);
  }

  return rv;
}

int nsExpatDriver::HandleExternalEntityRef(const char16_t* openEntityNames,
                                           const char16_t* base,
                                           const char16_t* systemId,
                                           const char16_t* publicId) {
  if (mInInternalSubset && !mInExternalDTD && openEntityNames) {
    mInternalSubset.Append(char16_t('%'));
    mInternalSubset.Append(nsDependentString(openEntityNames));
    mInternalSubset.Append(char16_t(';'));
  }

  // Load the external entity into a buffer.
  nsCOMPtr<nsIInputStream> in;
  nsAutoString absURL;
  nsresult rv = OpenInputStreamFromExternalDTD(publicId, systemId, base,
                                               getter_AddRefs(in), absURL);
  if (NS_FAILED(rv)) {
#ifdef DEBUG
    nsCString message("Failed to open external DTD: publicId \"");
    AppendUTF16toUTF8(MakeStringSpan(publicId), message);
    message += "\" systemId \"";
    AppendUTF16toUTF8(MakeStringSpan(systemId), message);
    message += "\" base \"";
    AppendUTF16toUTF8(MakeStringSpan(base), message);
    message += "\" URL \"";
    AppendUTF16toUTF8(absURL, message);
    message += "\"";
    NS_WARNING(message.get());
#endif
    return 1;
  }

  nsCOMPtr<nsIUnicharInputStream> uniIn;
  rv = NS_NewUnicharInputStream(in, getter_AddRefs(uniIn));
  NS_ENSURE_SUCCESS(rv, 1);

  int result = 1;
  if (uniIn) {
    bool copied_kUTF16 = false;
    auto t_kUTF16 = rlbox::copy_memory_or_grant_access(
        *mSandbox, kUTF16, sizeof(kUTF16), false, copied_kUTF16);
    tainted_expat<XML_Parser> t_entParser;
    t_entParser = RLBOX_EXPAT_MCALL(MOZ_XML_ExternalEntityParserCreate, nullptr,
                                    t_kUTF16);
    if (copied_kUTF16) {
      mSandbox->free_in_sandbox(t_kUTF16);
    }
    if (t_entParser) {
      bool copied_absURL = false;
      auto t_absURL = rlbox::copy_memory_or_grant_access(
          *mSandbox, (XML_Char*)absURL.get(),
          (absURL.Length() + 1) * sizeof(XML_Char), false, copied_absURL);
      mSandbox->invoke_sandbox_function(MOZ_XML_SetBase, t_entParser, t_absURL);
      if (copied_absURL) {
        mSandbox->free_in_sandbox(t_absURL);
      }

      mInExternalDTD = true;

      uint32_t totalRead;
      do {
        std::pair<rlbox_sandbox_expat*, tainted_expat<XML_Parser>> pair =
            std::make_pair(mSandbox, t_entParser);
        rv = uniIn->ReadSegments(ExternalDTDStreamReaderFunc, &pair,
                                 uint32_t(-1), &totalRead);
      } while (NS_SUCCEEDED(rv) && totalRead > 0);

      result = mSandbox
                   ->invoke_sandbox_function(MOZ_XML_Parse, t_entParser,
                                             nullptr, 0, 1)
                   .copy_and_verify(status_verifier);

      mInExternalDTD = false;

      mSandbox->invoke_sandbox_function(MOZ_XML_ParserFree, t_entParser);
    }
  }

  return result;
}

nsresult nsExpatDriver::OpenInputStreamFromExternalDTD(const char16_t* aFPIStr,
                                                       const char16_t* aURLStr,
                                                       const char16_t* aBaseURL,
                                                       nsIInputStream** aStream,
                                                       nsAString& aAbsURL) {
  nsCOMPtr<nsIURI> baseURI;
  nsresult rv =
      NS_NewURI(getter_AddRefs(baseURI), NS_ConvertUTF16toUTF8(aBaseURL));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), NS_ConvertUTF16toUTF8(aURLStr), nullptr,
                 baseURI);
  // Even if the URI is malformed (most likely because we have a
  // non-hierarchical base URI and a relative DTD URI, with the latter
  // being the normal XHTML DTD case), we can try to see whether we
  // have catalog data for aFPIStr.
  if (NS_WARN_IF(NS_FAILED(rv) && rv != NS_ERROR_MALFORMED_URI)) {
    return rv;
  }

  // make sure the URI, if we have one, is allowed to be loaded in sync
  bool isUIResource = false;
  if (uri) {
    rv = NS_URIChainHasFlags(uri, nsIProtocolHandler::URI_IS_UI_RESOURCE,
                             &isUIResource);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIURI> localURI;
  if (!isUIResource) {
    // Check to see if we can map the DTD to a known local DTD, or if a DTD
    // file of the same name exists in the special DTD directory
    if (aFPIStr) {
      // see if the Formal Public Identifier (FPI) maps to a catalog entry
      mCatalogData = LookupCatalogData(aFPIStr);
      GetLocalDTDURI(mCatalogData, uri, getter_AddRefs(localURI));
    }
    if (!localURI) {
      return NS_ERROR_NOT_IMPLEMENTED;
    }
  }

  nsCOMPtr<nsIChannel> channel;
  if (localURI) {
    localURI.swap(uri);
    rv = NS_NewChannel(getter_AddRefs(channel), uri,
                       nsContentUtils::GetSystemPrincipal(),
                       nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
                       nsIContentPolicy::TYPE_DTD);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    NS_ASSERTION(
        mSink == nsCOMPtr<nsIExpatSink>(do_QueryInterface(mOriginalSink)),
        "In nsExpatDriver::OpenInputStreamFromExternalDTD: "
        "mOriginalSink not the same object as mSink?");
    nsContentPolicyType policyType = nsIContentPolicy::TYPE_INTERNAL_DTD;
    if (mOriginalSink) {
      nsCOMPtr<Document> doc;
      doc = do_QueryInterface(mOriginalSink->GetTarget());
      if (doc) {
        if (doc->SkipDTDSecurityChecks()) {
          policyType = nsIContentPolicy::TYPE_INTERNAL_FORCE_ALLOWED_DTD;
        }
        rv = NS_NewChannel(
            getter_AddRefs(channel), uri, doc,
            nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_INHERITS_SEC_CONTEXT |
                nsILoadInfo::SEC_ALLOW_CHROME,
            policyType);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
    if (!channel) {
      nsCOMPtr<nsIPrincipal> nullPrincipal =
          mozilla::NullPrincipal::CreateWithoutOriginAttributes();
      rv = NS_NewChannel(
          getter_AddRefs(channel), uri, nullPrincipal,
          nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_INHERITS_SEC_CONTEXT |
              nsILoadInfo::SEC_ALLOW_CHROME,
          policyType);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  nsAutoCString absURL;
  rv = uri->GetSpec(absURL);
  NS_ENSURE_SUCCESS(rv, rv);
  CopyUTF8toUTF16(absURL, aAbsURL);

  channel->SetContentType("application/xml"_ns);
  return channel->Open(aStream);
}

static nsresult CreateErrorText(const char16_t* aDescription,
                                const char16_t* aSourceURL,
                                const uint32_t aLineNumber,
                                const uint32_t aColNumber,
                                nsString& aErrorString, bool spoofEnglish) {
  aErrorString.Truncate();

  nsAutoString msg;
  nsresult rv = nsParserMsgUtils::GetLocalizedStringByName(
      spoofEnglish ? XMLPARSER_PROPERTIES_en_US : XMLPARSER_PROPERTIES,
      "XMLParsingError", msg);
  NS_ENSURE_SUCCESS(rv, rv);

  // XML Parsing Error: %1$S\nLocation: %2$S\nLine Number %3$u, Column %4$u:
  nsTextFormatter::ssprintf(aErrorString, msg.get(), aDescription, aSourceURL,
                            aLineNumber, aColNumber);
  return NS_OK;
}

static nsresult AppendErrorPointer(const int32_t aColNumber,
                                   const char16_t* aSourceLine,
                                   nsString& aSourceString) {
  aSourceString.Append(char16_t('\n'));

  // Last character will be '^'.
  int32_t last = aColNumber - 1;
  int32_t i;
  uint32_t minuses = 0;
  for (i = 0; i < last; ++i) {
    if (aSourceLine[i] == '\t') {
      // Since this uses |white-space: pre;| a tab stop equals 8 spaces.
      uint32_t add = 8 - (minuses % 8);
      aSourceString.AppendASCII("--------", add);
      minuses += add;
    } else {
      aSourceString.Append(char16_t('-'));
      ++minuses;
    }
  }
  aSourceString.Append(char16_t('^'));

  return NS_OK;
}

nsresult nsExpatDriver::HandleError() {
  int32_t code = RLBOX_EXPAT_SAFE_MCALL(MOZ_XML_GetErrorCode, error_verifier);

  // Map Expat error code to an error string
  // XXX Deal with error returns.
  nsAutoString description;
  nsCOMPtr<Document> doc;
  if (mOriginalSink) {
    doc = do_QueryInterface(mOriginalSink->GetTarget());
  }

  bool spoofEnglish =
      nsContentUtils::SpoofLocaleEnglish() && (!doc || !doc->AllowsL10n());
  nsParserMsgUtils::GetLocalizedStringByID(
      spoofEnglish ? XMLPARSER_PROPERTIES_en_US : XMLPARSER_PROPERTIES, code,
      description);

  if (code == XML_ERROR_TAG_MISMATCH) {
    /**
     *  Expat can send the following:
     *    localName
     *    namespaceURI<separator>localName
     *    namespaceURI<separator>localName<separator>prefix
     *
     *  and we use 0xFFFF for the <separator>.
     *
     */

    bool copied_mismatch = false;
    auto t_mismatch = RLBOX_EXPAT_MCALL(MOZ_XML_GetMismatchedTag);
    auto* mismatch =
        transfer_xmlstring_no_validate(*mSandbox, t_mismatch, copied_mismatch);
    const char16_t* uriEnd = nullptr;
    const char16_t* nameEnd = nullptr;
    const char16_t* pos;
    for (pos = mismatch; *pos; ++pos) {
      if (*pos == kExpatSeparatorChar) {
        if (uriEnd) {
          nameEnd = pos;
        } else {
          uriEnd = pos;
        }
      }
    }

    nsAutoString tagName;
    if (uriEnd && nameEnd) {
      // We have a prefix.
      tagName.Append(nameEnd + 1, pos - nameEnd - 1);
      tagName.Append(char16_t(':'));
    }
    const char16_t* nameStart = uriEnd ? uriEnd + 1 : mismatch;
    tagName.Append(nameStart, (nameEnd ? nameEnd : pos) - nameStart);

    nsAutoString msg;
    nsParserMsgUtils::GetLocalizedStringByName(
        spoofEnglish ? XMLPARSER_PROPERTIES_en_US : XMLPARSER_PROPERTIES,
        "Expected", msg);

    // . Expected: </%S>.
    nsAutoString message;
    nsTextFormatter::ssprintf(message, msg.get(), tagName.get());
    description.Append(message);

    if (copied_mismatch) {
      free(mismatch);
    }
  }

  // Adjust the column number so that it is one based rather than zero based.
  uint32_t colNumber = RLBOX_EXPAT_SAFE_MCALL(MOZ_XML_GetCurrentColumnNumber,
                                              safe_unverified<XML_Size>) +
                       1;
  uint32_t lineNumber = RLBOX_EXPAT_SAFE_MCALL(MOZ_XML_GetCurrentLineNumber,
                                               safe_unverified<XML_Size>);

  nsAutoString errorText;
  bool copied_aBase = false;
  auto* aBase = transfer_xmlstring_no_validate(
      *mSandbox, RLBOX_EXPAT_MCALL(MOZ_XML_GetBase), copied_aBase);
  CreateErrorText(description.get(), aBase, lineNumber, colNumber, errorText,
                  spoofEnglish);
  if (copied_aBase) {
    free(aBase);
  }

  nsAutoString sourceText(mLastLine);
  AppendErrorPointer(colNumber, mLastLine.get(), sourceText);

  if (doc && nsContentUtils::IsChromeDoc(doc)) {
    nsCString path = doc->GetDocumentURI()->GetSpecOrDefault();
    nsCOMPtr<nsISupports> container = doc->GetContainer();
    nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(container);
    nsCString docShellDestroyed("unknown"_ns);
    if (docShell) {
      bool destroyed = false;
      docShell->IsBeingDestroyed(&destroyed);
      docShellDestroyed.Assign(destroyed ? "true"_ns : "false"_ns);
    }

    mozilla::Maybe<nsTArray<mozilla::Telemetry::EventExtraEntry>> extra =
        mozilla::Some<nsTArray<mozilla::Telemetry::EventExtraEntry>>({
            mozilla::Telemetry::EventExtraEntry{"error_code"_ns,
                                                nsPrintfCString("%u", code)},
            mozilla::Telemetry::EventExtraEntry{
                "location"_ns, nsPrintfCString("%u:%u", lineNumber, colNumber)},
            mozilla::Telemetry::EventExtraEntry{
                "last_line"_ns, NS_ConvertUTF16toUTF8(mLastLine)},
            mozilla::Telemetry::EventExtraEntry{
                "last_line_len"_ns, nsPrintfCString("%u", mLastLine.Length())},
            mozilla::Telemetry::EventExtraEntry{
                "hidden"_ns, doc->Hidden() ? "true"_ns : "false"_ns},
            mozilla::Telemetry::EventExtraEntry{"destroyed"_ns,
                                                docShellDestroyed},
        });

    mozilla::Telemetry::SetEventRecordingEnabled("ysod"_ns, true);
    mozilla::Telemetry::RecordEvent(
        mozilla::Telemetry::EventID::Ysod_Shown_Ysod, mozilla::Some(path),
        extra);
  }

  // Try to create and initialize the script error.
  nsCOMPtr<nsIScriptError> serr(do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
  nsresult rv = NS_ERROR_FAILURE;
  if (serr) {
    rv = serr->InitWithWindowID(errorText, mURISpec, mLastLine, lineNumber,
                                colNumber, nsIScriptError::errorFlag,
                                "malformed-xml", mInnerWindowID);
  }

  // If it didn't initialize, we can't do any logging.
  bool shouldReportError = NS_SUCCEEDED(rv);

  // mSink might be null here if our parser was terminated.
  if (mSink && shouldReportError) {
    rv = mSink->ReportError(errorText.get(), sourceText.get(), serr,
                            &shouldReportError);
    if (NS_FAILED(rv)) {
      shouldReportError = true;
    }
  }

  // mOriginalSink might be null here if our parser was terminated.
  if (mOriginalSink) {
    nsCOMPtr<Document> doc = do_QueryInterface(mOriginalSink->GetTarget());
    if (doc && doc->SuppressParserErrorConsoleMessages()) {
      shouldReportError = false;
    }
  }

  if (shouldReportError) {
    nsCOMPtr<nsIConsoleService> cs(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
    if (cs) {
      cs->LogMessage(serr);
    }
  }

  return NS_ERROR_HTMLPARSER_STOPPARSING;
}

void nsExpatDriver::ParseBuffer(const char16_t* aBuffer, uint32_t aLength,
                                bool aIsFinal, uint32_t* aConsumed) {
  NS_ASSERTION((aBuffer && aLength != 0) || (!aBuffer && aLength == 0), "?");
  NS_ASSERTION(mInternalState != NS_OK || aIsFinal || aBuffer,
               "Useless call, we won't call Expat");
  MOZ_ASSERT(!BlockedOrInterrupted() || !aBuffer,
             "Non-null buffer when resuming");

  auto parserBytesBefore_verifier = [&](auto parserBytesBefore) {
    MOZ_RELEASE_ASSERT(parserBytesBefore >= 0, "Unexpected value");
    MOZ_RELEASE_ASSERT(parserBytesBefore % sizeof(char16_t) == 0,
                       "Consumed part of a char16_t?");
    return parserBytesBefore;
  };
  int32_t parserBytesBefore = RLBOX_EXPAT_SAFE_MCALL(
      XML_GetCurrentByteIndex, parserBytesBefore_verifier);

  if (mExpatParser != nullptr &&
      (mInternalState == NS_OK || BlockedOrInterrupted())) {
    XML_Status status;
    if (BlockedOrInterrupted()) {
      mInternalState = NS_OK;  // Resume in case we're blocked.
      status = RLBOX_EXPAT_SAFE_MCALL(MOZ_XML_ResumeParser, status_verifier);
    } else if (aBuffer) {
      bool copied_aBuffer = false;
      size_t len = aLength * sizeof(char16_t);
      auto t_aBuffer = rlbox::copy_memory_or_grant_access(
          *mSandbox, reinterpret_cast<const char*>(aBuffer), len, false,
          copied_aBuffer);

      status = RLBOX_EXPAT_SAFE_MCALL(MOZ_XML_Parse, status_verifier, t_aBuffer,
                                      len, aIsFinal);

      if (copied_aBuffer) {
        mSandbox->free_in_sandbox(t_aBuffer);
      }
    } else {
      status = RLBOX_EXPAT_SAFE_MCALL(MOZ_XML_Parse, status_verifier, nullptr,
                                      0, aIsFinal);
    }

    auto parserBytesConsumed_verifier = [&](auto parserBytesConsumed) {
      MOZ_RELEASE_ASSERT(parserBytesConsumed >= 0, "Unexpected value");
      MOZ_RELEASE_ASSERT(parserBytesConsumed >= parserBytesBefore,
                         "How'd this happen?");
      MOZ_RELEASE_ASSERT(parserBytesConsumed % sizeof(char16_t) == 0,
                         "Consumed part of a char16_t?");
      return parserBytesConsumed;
    };
    int32_t parserBytesConsumed = RLBOX_EXPAT_SAFE_MCALL(
        XML_GetCurrentByteIndex, parserBytesConsumed_verifier);

    // Consumed something.
    *aConsumed = (parserBytesConsumed - parserBytesBefore) / sizeof(char16_t);
    NS_ASSERTION(*aConsumed <= aLength + mExpatBuffered,
                 "Too many bytes consumed?");

    NS_ASSERTION(status != XML_STATUS_SUSPENDED || BlockedOrInterrupted(),
                 "Inconsistent expat suspension state.");

    if (status == XML_STATUS_ERROR) {
      mInternalState = NS_ERROR_HTMLPARSER_STOPPARSING;
    }
  } else {
    *aConsumed = 0;
  }
}

NS_IMETHODIMP
nsExpatDriver::ConsumeToken(nsScanner& aScanner, bool& aFlushTokens) {
  // We keep the scanner pointing to the position where Expat will start
  // parsing.
  nsScannerIterator currentExpatPosition;
  aScanner.CurrentPosition(currentExpatPosition);

  // This is the start of the first buffer that we need to pass to Expat.
  nsScannerIterator start = currentExpatPosition;
  start.advance(mExpatBuffered);

  // This is the end of the last buffer (at this point, more data could come in
  // later).
  nsScannerIterator end;
  aScanner.EndReading(end);

  MOZ_LOG(gExpatDriverLog, LogLevel::Debug,
          ("Remaining in expat's buffer: %i, remaining in scanner: %zu.",
           mExpatBuffered, Distance(start, end)));

  // We want to call Expat if we have more buffers, or if we know there won't
  // be more buffers (and so we want to flush the remaining data), or if we're
  // currently blocked and there's data in Expat's buffer.
  while (start != end || (mIsFinalChunk && !mMadeFinalCallToExpat) ||
         (BlockedOrInterrupted() && mExpatBuffered > 0)) {
    bool noMoreBuffers = start == end && mIsFinalChunk;
    bool blocked = BlockedOrInterrupted();

    const char16_t* buffer;
    uint32_t length;
    if (blocked || noMoreBuffers) {
      // If we're blocked we just resume Expat so we don't need a buffer, if
      // there aren't any more buffers we pass a null buffer to Expat.
      buffer = nullptr;
      length = 0;

      if (blocked) {
        MOZ_LOG(
            gExpatDriverLog, LogLevel::Debug,
            ("Resuming Expat, will parse data remaining in Expat's "
             "buffer.\nContent of Expat's buffer:\n-----\n%s\n-----\n",
             NS_ConvertUTF16toUTF8(currentExpatPosition.get(), mExpatBuffered)
                 .get()));
      } else {
        NS_ASSERTION(mExpatBuffered == Distance(currentExpatPosition, end),
                     "Didn't pass all the data to Expat?");
        MOZ_LOG(
            gExpatDriverLog, LogLevel::Debug,
            ("Last call to Expat, will parse data remaining in Expat's "
             "buffer.\nContent of Expat's buffer:\n-----\n%s\n-----\n",
             NS_ConvertUTF16toUTF8(currentExpatPosition.get(), mExpatBuffered)
                 .get()));
      }
    } else {
      buffer = start.get();
      length = uint32_t(start.size_forward());

      MOZ_LOG(gExpatDriverLog, LogLevel::Debug,
              ("Calling Expat, will parse data remaining in Expat's buffer and "
               "new data.\nContent of Expat's buffer:\n-----\n%s\n-----\nNew "
               "data:\n-----\n%s\n-----\n",
               NS_ConvertUTF16toUTF8(currentExpatPosition.get(), mExpatBuffered)
                   .get(),
               NS_ConvertUTF16toUTF8(start.get(), length).get()));
    }

    uint32_t consumed;
    ParseBuffer(buffer, length, noMoreBuffers, &consumed);
    if (consumed > 0) {
      nsScannerIterator oldExpatPosition = currentExpatPosition;
      currentExpatPosition.advance(consumed);

      // We consumed some data, we want to store the last line of data that
      // was consumed in case we run into an error (to show the line in which
      // the error occurred).

      // The length of the last line that Expat has parsed.
      XML_Size lastLineLength = RLBOX_EXPAT_SAFE_MCALL(
          XML_GetCurrentColumnNumber, safe_unverified<XML_Size>);

      if (lastLineLength <= consumed) {
        // The length of the last line was less than what expat consumed, so
        // there was at least one line break in the consumed data. Store the
        // last line until the point where we stopped parsing.
        nsScannerIterator startLastLine = currentExpatPosition;
        startLastLine.advance(-((ptrdiff_t)lastLineLength));
        if (!CopyUnicodeTo(startLastLine, currentExpatPosition, mLastLine)) {
          return (mInternalState = NS_ERROR_OUT_OF_MEMORY);
        }
      } else {
        // There was no line break in the consumed data, append the consumed
        // data.
        if (!AppendUnicodeTo(oldExpatPosition, currentExpatPosition,
                             mLastLine)) {
          return (mInternalState = NS_ERROR_OUT_OF_MEMORY);
        }
      }
    }

    mExpatBuffered += length - consumed;

    if (BlockedOrInterrupted()) {
      MOZ_LOG(gExpatDriverLog, LogLevel::Debug,
              ("Blocked or interrupted parser (probably for loading linked "
               "stylesheets or scripts)."));

      aScanner.SetPosition(currentExpatPosition, true);
      aScanner.Mark();

      return mInternalState;
    }

    if (noMoreBuffers && mExpatBuffered == 0) {
      mMadeFinalCallToExpat = true;
    }

    if (NS_FAILED(mInternalState)) {
      if (RLBOX_EXPAT_SAFE_MCALL(MOZ_XML_GetErrorCode, error_verifier) !=
          XML_ERROR_NONE) {
        NS_ASSERTION(mInternalState == NS_ERROR_HTMLPARSER_STOPPARSING,
                     "Unexpected error");

        // Look for the next newline after the last one we consumed
        nsScannerIterator lastLine = currentExpatPosition;
        while (lastLine != end) {
          length = uint32_t(lastLine.size_forward());
          uint32_t endOffset = 0;
          const char16_t* buffer = lastLine.get();
          while (endOffset < length && buffer[endOffset] != '\n' &&
                 buffer[endOffset] != '\r') {
            ++endOffset;
          }
          mLastLine.Append(Substring(buffer, buffer + endOffset));
          if (endOffset < length) {
            // We found a newline.
            break;
          }

          lastLine.advance(length);
        }

        HandleError();
      }

      return mInternalState;
    }

    // Either we have more buffers, or we were blocked (and we'll flush in the
    // next iteration), or we should have emptied Expat's buffer.
    NS_ASSERTION(!noMoreBuffers || blocked ||
                     (mExpatBuffered == 0 && currentExpatPosition == end),
                 "Unreachable data left in Expat's buffer");

    start.advance(length);

    // It's possible for start to have passed end if we received more data
    // (e.g. if we spun the event loop in an inline script). Reload end now
    // to compensate.
    aScanner.EndReading(end);
  }

  aScanner.SetPosition(currentExpatPosition, true);
  aScanner.Mark();

  MOZ_LOG(gExpatDriverLog, LogLevel::Debug,
          ("Remaining in expat's buffer: %i, remaining in scanner: %zu.",
           mExpatBuffered, Distance(currentExpatPosition, end)));

  return NS_SUCCEEDED(mInternalState) ? NS_ERROR_HTMLPARSER_EOF : NS_OK;
}

// Static sandboxes for handling system and untrusted documents
static std::shared_ptr<RLBoxExpatData> sSandboxData;
static std::shared_ptr<RLBoxExpatData> sSystemSandboxData;

RLBoxExpatData::RLBoxExpatData(bool isSystemPrincipal) {
  // Create sandbox
  mSandbox = new rlbox_sandbox_expat();
  MOZ_RELEASE_ASSERT(mSandbox, "Failed to create sandbox");
#ifdef MOZ_WASM_SANDBOXING_EXPAT
  mSandbox->create_sandbox(mozilla::ipc::GetSandboxedRLBoxPath().get());
#else
  mSandbox->create_sandbox();
#endif

  // Register callbacks
  mHandleXMLDeclaration =
      mSandbox->register_callback(Driver_HandleXMLDeclaration);

  if (isSystemPrincipal) {
    mHandleStartElement = mSandbox->register_callback(
        nsExpatDriver::HandleStartElementForSystemPrincipal);
    mHandleEndElement = mSandbox->register_callback(
        nsExpatDriver::HandleEndElementForSystemPrincipal);
  } else {
    mHandleStartElement =
        mSandbox->register_callback(nsExpatDriver::HandleStartElement);
    mHandleEndElement =
        mSandbox->register_callback(nsExpatDriver::HandleEndElement);
  }

  mHandleCharacterData =
      mSandbox->register_callback(Driver_HandleCharacterData);
  mHandleProcessingInstruction =
      mSandbox->register_callback(Driver_HandleProcessingInstruction);
  mHandleExternalEntityRef =
      mSandbox->register_callback(Driver_HandleExternalEntityRef);
  mHandleComment = mSandbox->register_callback(Driver_HandleComment);
  mHandleStartCdataSection =
      mSandbox->register_callback(Driver_HandleStartCdataSection);
  mHandleEndCdataSection =
      mSandbox->register_callback(Driver_HandleEndCdataSection);
  mHandleStartDoctypeDecl =
      mSandbox->register_callback(Driver_HandleStartDoctypeDecl);
  mHandleEndDoctypeDecl =
      mSandbox->register_callback(Driver_HandleEndDoctypeDecl);
}

RLBoxExpatData::~RLBoxExpatData() {
  // Unregister callbacks
  mHandleXMLDeclaration.unregister();
  mHandleStartElement.unregister();
  mHandleEndElement.unregister();
  mHandleCharacterData.unregister();
  mHandleProcessingInstruction.unregister();
  mHandleExternalEntityRef.unregister();
  mHandleComment.unregister();
  mHandleStartCdataSection.unregister();
  mHandleEndCdataSection.unregister();
  mHandleStartDoctypeDecl.unregister();
  mHandleEndDoctypeDecl.unregister();
  // Destroy sandbox
  mSandbox->destroy_sandbox();
  delete mSandbox;
  mSandbox = nullptr;
}

// Create and load sandbox (if not already done).
std::shared_ptr<RLBoxExpatData> RLBoxExpatData::GetRLBoxExpatData(
    bool isSystemPrincipal) {
#ifdef MOZ_WASM_SANDBOXING_EXPAT
  mozilla::ipc::PreloadSandboxedDynamicLibrary();
#endif

  if (isSystemPrincipal) {
    if (!sSystemSandboxData) {
      sSystemSandboxData = std::make_shared<RLBoxExpatData>(isSystemPrincipal);
      mozilla::ClearOnShutdown(&sSystemSandboxData);
    }
    return sSystemSandboxData;
  }
  if (!sSandboxData) {
    sSandboxData = std::make_shared<RLBoxExpatData>(isSystemPrincipal);
    mozilla::ClearOnShutdown(&sSandboxData);
  }
  return sSandboxData;
}

NS_IMETHODIMP
nsExpatDriver::WillBuildModel(const CParserContext& aParserContext,
                              nsITokenizer* aTokenizer, nsIContentSink* aSink) {
  mSink = do_QueryInterface(aSink);
  if (!mSink) {
    NS_ERROR("nsExpatDriver didn't get an nsIExpatSink");
    // Make sure future calls to us bail out as needed
    mInternalState = NS_ERROR_UNEXPECTED;
    return mInternalState;
  }

  mOriginalSink = aSink;

  static const char16_t kExpatSeparator[] = {kExpatSeparatorChar, '\0'};

  // Get the doc if any
  nsCOMPtr<Document> doc = do_QueryInterface(mOriginalSink->GetTarget());
  if (doc) {
    nsCOMPtr<nsPIDOMWindowOuter> win = doc->GetWindow();
    nsCOMPtr<nsPIDOMWindowInner> inner;
    if (win) {
      inner = win->GetCurrentInnerWindow();
    } else {
      bool aHasHadScriptHandlingObject;
      nsIScriptGlobalObject* global =
          doc->GetScriptHandlingObject(aHasHadScriptHandlingObject);
      if (global) {
        inner = do_QueryInterface(global);
      }
    }
    if (inner) {
      mInnerWindowID = inner->WindowID();
    }
  }

  // Get the sandbox data (pointer to sandbox and callbacks)
  mSandboxData = RLBoxExpatData::GetRLBoxExpatData(
      doc && doc->NodePrincipal()->IsSystemPrincipal());
  mSandbox = mSandboxData->mSandbox;

  tainted_expat<const XML_Memory_Handling_Suite*> t_memsuite = nullptr;
#ifndef MOZ_WASM_SANDBOXING_EXPAT
  // In the noop sandbox we can use the moz allocator.
  static const XML_Memory_Handling_Suite s_memsuite = {malloc, realloc, free};
  // Since the code running in a noop sandbox is not actually sandboxed, we
  // can expose the memsuite directly using UNSAFE_accept_pointer.  We don't
  // want to convert these functions to callbackbacks that can be used when
  // expat is actually in a memory isolated sandbox -- expat should use the
  // sandbox-local memsuite.
  t_memsuite = mSandbox->UNSAFE_accept_pointer(&s_memsuite);
#endif

  // Create expat parser.
  // We need to copy the encoding and namespace separator into the sandbox.
  // For the noop sandbox we pass in the memsuite; for the Wasm sandbox, we
  // pass in nullptr to let expat use the standar library memory suite.
  bool copied_kExpatSeparator = false;
  auto t_kExpatSeparator = rlbox::copy_memory_or_grant_access(
      *mSandbox, kExpatSeparator, sizeof(kExpatSeparator), false,
      copied_kExpatSeparator);
  bool copied_kUTF16 = false;
  auto t_kUTF16 = rlbox::copy_memory_or_grant_access(
      *mSandbox, kUTF16, sizeof(kUTF16), false, copied_kUTF16);
  mExpatParser = mSandbox->invoke_sandbox_function(
      MOZ_XML_ParserCreate_MM,
      rlbox::sandbox_const_cast<const char16_t*>(t_kUTF16), t_memsuite,
      t_kExpatSeparator);
  NS_ENSURE_TRUE(mExpatParser, NS_ERROR_FAILURE);
  if (copied_kExpatSeparator) {
    mSandbox->free_in_sandbox(t_kExpatSeparator);
  }
  if (copied_kUTF16) {
    mSandbox->free_in_sandbox(t_kUTF16);
  }

  RLBOX_EXPAT_MCALL(MOZ_XML_SetReturnNSTriplet, XML_TRUE);

#ifdef XML_DTD
  RLBOX_EXPAT_MCALL(MOZ_XML_SetParamEntityParsing,
                    XML_PARAM_ENTITY_PARSING_ALWAYS);
#endif

  mURISpec = aParserContext.mScanner->GetFilename();

  bool copied_mURI = false;
  tainted_expat<const XML_Char*> t_mURI = rlbox::copy_memory_or_grant_access(
      *mSandbox, (const XML_Char*)mURISpec.get(),
      (mURISpec.Length() + 1) * sizeof(XML_Char), false, copied_mURI);
  RLBOX_EXPAT_MCALL(MOZ_XML_SetBase, t_mURI);
  if (copied_mURI) {
    mSandbox->free_in_sandbox(t_mURI);
  }

  // Set up the callbacks
  RLBOX_EXPAT_MCALL(MOZ_XML_SetXmlDeclHandler,
                    mSandboxData->mHandleXMLDeclaration);
  RLBOX_EXPAT_MCALL(MOZ_XML_SetElementHandler,
                    mSandboxData->mHandleStartElement,
                    mSandboxData->mHandleEndElement);
  RLBOX_EXPAT_MCALL(MOZ_XML_SetCharacterDataHandler,
                    mSandboxData->mHandleCharacterData);
  RLBOX_EXPAT_MCALL(MOZ_XML_SetProcessingInstructionHandler,
                    mSandboxData->mHandleProcessingInstruction);
  RLBOX_EXPAT_MCALL(MOZ_XML_SetDefaultHandlerExpand,
                    mSandboxData->mHandleCharacterData);
  RLBOX_EXPAT_MCALL(MOZ_XML_SetExternalEntityRefHandler,
                    mSandboxData->mHandleExternalEntityRef);
  RLBOX_EXPAT_MCALL(MOZ_XML_SetCommentHandler, mSandboxData->mHandleComment);
  RLBOX_EXPAT_MCALL(MOZ_XML_SetCdataSectionHandler,
                    mSandboxData->mHandleStartCdataSection,
                    mSandboxData->mHandleEndCdataSection);

  RLBOX_EXPAT_MCALL(MOZ_XML_SetParamEntityParsing,
                    XML_PARAM_ENTITY_PARSING_UNLESS_STANDALONE);
  RLBOX_EXPAT_MCALL(MOZ_XML_SetDoctypeDeclHandler,
                    mSandboxData->mHandleStartDoctypeDecl,
                    mSandboxData->mHandleEndDoctypeDecl);

  // Set up the user data.
  mAppPtr = mSandbox->get_app_pointer(static_cast<void*>(this));
  tainted_expat<void*> t_driver = mAppPtr.to_tainted();
  RLBOX_EXPAT_MCALL(XML_SetExternalEntityRefHandlerArg, t_driver);
  RLBOX_EXPAT_MCALL(XML_SetUserData, t_driver);

  return mInternalState;
}

NS_IMETHODIMP
nsExpatDriver::BuildModel(nsITokenizer* aTokenizer, nsIContentSink* aSink) {
  return mInternalState;
}

NS_IMETHODIMP
nsExpatDriver::DidBuildModel(nsresult anErrorCode) {
  mOriginalSink = nullptr;
  mSink = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsExpatDriver::WillTokenize(bool aIsFinalChunk) {
  mIsFinalChunk = aIsFinalChunk;
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsExpatDriver::Terminate() {
  // XXX - not sure what happens to the unparsed data.
  if (mExpatParser) {
    RLBOX_EXPAT_MCALL(MOZ_XML_StopParser, XML_FALSE);
  }
  mInternalState = NS_ERROR_HTMLPARSER_STOPPARSING;
}

NS_IMETHODIMP_(int32_t)
nsExpatDriver::GetType() { return NS_IPARSER_FLAG_XML; }

NS_IMETHODIMP_(nsDTDMode)
nsExpatDriver::GetMode() const { return eDTDMode_full_standards; }

/*************************** Unused methods **********************************/

NS_IMETHODIMP_(bool)
nsExpatDriver::IsContainer(int32_t aTag) const { return true; }

NS_IMETHODIMP_(bool)
nsExpatDriver::CanContain(int32_t aParent, int32_t aChild) const {
  return true;
}

void nsExpatDriver::MaybeStopParser(nsresult aState) {
  if (NS_FAILED(aState)) {
    // If we had a failure we want to override NS_ERROR_HTMLPARSER_INTERRUPTED
    // and we want to override NS_ERROR_HTMLPARSER_BLOCK but not with
    // NS_ERROR_HTMLPARSER_INTERRUPTED.
    if (NS_SUCCEEDED(mInternalState) ||
        mInternalState == NS_ERROR_HTMLPARSER_INTERRUPTED ||
        (mInternalState == NS_ERROR_HTMLPARSER_BLOCK &&
         aState != NS_ERROR_HTMLPARSER_INTERRUPTED)) {
      mInternalState = (aState == NS_ERROR_HTMLPARSER_INTERRUPTED ||
                        aState == NS_ERROR_HTMLPARSER_BLOCK)
                           ? aState
                           : NS_ERROR_HTMLPARSER_STOPPARSING;
    }

    // If we get an error then we need to stop Expat (by calling XML_StopParser
    // with false as the last argument). If the parser should be blocked or
    // interrupted we need to pause Expat (by calling XML_StopParser with
    // true as the last argument).
    RLBOX_EXPAT_MCALL(MOZ_XML_StopParser, BlockedOrInterrupted());
  } else if (NS_SUCCEEDED(mInternalState)) {
    // Only clobber mInternalState with the success code if we didn't block or
    // interrupt before.
    mInternalState = aState;
  }
}
