/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsExpatDriver.h"
#include "mozilla/fallible.h"
#include "nsCOMPtr.h"
#include "CParserContext.h"
#include "nsIExpatSink.h"
#include "nsIContentSink.h"
#include "nsIDocShell.h"
#include "nsParserMsgUtils.h"
#include "nsIURL.h"
#include "nsIUnicharInputStream.h"
#include "nsIProtocolHandler.h"
#include "nsNetUtil.h"
#include "nsString.h"
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
#include "mozilla/Array.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/IntegerTypeTraits.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryComms.h"

#include "nsThreadUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/RLBoxUtils.h"
#include "mozilla/UniquePtr.h"

#include "mozilla/Logging.h"

using mozilla::fallible;
using mozilla::LogLevel;
using mozilla::MakeStringSpan;
using mozilla::Maybe;
using mozilla::Unused;
using mozilla::dom::Document;

// We only pass chunks of length sMaxChunkLength to Expat in the RLBOX sandbox.
// The RLBOX sandbox has a limited amount of memory, and we have to account for
// other memory use by Expat (including the buffering it does).
// Note that sMaxChunkLength is in number of characters.
#ifdef DEBUG
// On debug builds we set a much lower limit (1kB) to try to hit boundary
// conditions more frequently.
static const uint32_t sMaxChunkLength = 1024 / sizeof(char16_t);
#else
static const uint32_t sMaxChunkLength = (128 * 1024) / sizeof(char16_t);
#endif

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

#define RLBOX_EXPAT_SAFE_MCALL(foo, verifier, ...)                \
  Sandbox()                                                       \
      ->invoke_sandbox_function(foo, mExpatParser, ##__VA_ARGS__) \
      .copy_and_verify(verifier)

#define RLBOX_EXPAT_CALL(foo, ...) \
  aSandbox.invoke_sandbox_function(foo, self->mExpatParser, ##__VA_ARGS__)

#define RLBOX_EXPAT_MCALL(foo, ...) \
  Sandbox()->invoke_sandbox_function(foo, mExpatParser, ##__VA_ARGS__)

#define RLBOX_SAFE_PRINT "Value used only for printing"
#define MOZ_RELEASE_ASSERT_TAINTED(cond, ...)                        \
  MOZ_RELEASE_ASSERT((cond).unverified_safe_because("Sanity check"), \
                     ##__VA_ARGS__)

/* safe_unverified is used whenever it's safe to not use a validator */
template <typename T>
static T safe_unverified(T val) {
  return val;
}

/* status_verifier is a type validator for XML_Status */
inline enum XML_Status status_verifier(enum XML_Status s) {
  MOZ_RELEASE_ASSERT(s >= XML_STATUS_ERROR && s <= XML_STATUS_SUSPENDED,
                     "unexpected status code");
  return s;
}

/* error_verifier is a type validator for XML_Error */
inline enum XML_Error error_verifier(enum XML_Error code) {
  MOZ_RELEASE_ASSERT(
      code >= XML_ERROR_NONE && code <= XML_ERROR_INVALID_ARGUMENT,
      "unexpected XML error code");
  return code;
}

/* We use unverified_xml_string to just expose sandbox expat strings to Firefox
 * without any validation. On 64-bit we have guard pages at the sandbox
 * boundary; on 32-bit we don't and a string could be used to read beyond the
 * sandbox boundary. In our attacker model this is okay (the attacker can just
 * Spectre).
 *
 * Nevertheless, we should try to add strings validators to the consumer code
 * of expat whenever we have some semantics. At the very lest we should make
 * sure that the strings are never written to. Bug 1693991 tracks this.
 */
static const XML_Char* unverified_xml_string(uintptr_t ptr) {
  return reinterpret_cast<const XML_Char*>(ptr);
}

/* The TransferBuffer class is used to copy (or directly expose in the
 * noop-sandbox case) buffers into the expat sandbox (and automatically
 * when out of scope).
 */
template <typename T>
using TransferBuffer =
    mozilla::RLBoxTransferBufferToSandbox<T, rlbox_expat_sandbox_type>;

/*************************** END RLBOX HELPERS ******************************/

/***************************** EXPAT CALL BACKS ******************************/
// The callback handlers that get called from the expat parser.

static void Driver_HandleXMLDeclaration(
    rlbox_sandbox_expat& aSandbox, tainted_expat<void*> /* aUserData */,
    tainted_expat<const XML_Char*> aVersion,
    tainted_expat<const XML_Char*> aEncoding, tainted_expat<int> aStandalone) {
  nsExpatDriver* driver = static_cast<nsExpatDriver*>(aSandbox.sandbox_storage);
  MOZ_ASSERT(driver);

  int standalone = aStandalone.copy_and_verify([&](auto a) {
    // Standalone argument can be -1, 0, or 1 (see
    // /parser/expat/lib/expat.h#185)
    MOZ_RELEASE_ASSERT(a >= -1 && a <= 1, "Unexpected standalone parameter");
    return a;
  });

  const auto* version = aVersion.copy_and_verify_address(unverified_xml_string);
  const auto* encoding =
      aEncoding.copy_and_verify_address(unverified_xml_string);
  driver->HandleXMLDeclaration(version, encoding, standalone);
}

static void Driver_HandleCharacterData(rlbox_sandbox_expat& aSandbox,
                                       tainted_expat<void*> /* aUserData */,
                                       tainted_expat<const XML_Char*> aData,
                                       tainted_expat<int> aLength) {
  nsExpatDriver* driver = static_cast<nsExpatDriver*>(aSandbox.sandbox_storage);
  MOZ_ASSERT(driver);
  // aData is not null terminated; even with bad length we will not span beyond
  // sandbox boundary
  uint32_t length =
      static_cast<uint32_t>(aLength.copy_and_verify(safe_unverified<int>));
  const auto* data = aData.unverified_safe_pointer_because(
      length, "Only care that the data is within sandbox boundary.");
  driver->HandleCharacterData(data, length);
}

static void Driver_HandleComment(rlbox_sandbox_expat& aSandbox,
                                 tainted_expat<void*> /* aUserData */,
                                 tainted_expat<const XML_Char*> aName) {
  nsExpatDriver* driver = static_cast<nsExpatDriver*>(aSandbox.sandbox_storage);
  MOZ_ASSERT(driver);
  const auto* name = aName.copy_and_verify_address(unverified_xml_string);
  driver->HandleComment(name);
}

static void Driver_HandleProcessingInstruction(
    rlbox_sandbox_expat& aSandbox, tainted_expat<void*> /* aUserData */,
    tainted_expat<const XML_Char*> aTarget,
    tainted_expat<const XML_Char*> aData) {
  nsExpatDriver* driver = static_cast<nsExpatDriver*>(aSandbox.sandbox_storage);
  MOZ_ASSERT(driver);
  const auto* target = aTarget.copy_and_verify_address(unverified_xml_string);
  const auto* data = aData.copy_and_verify_address(unverified_xml_string);
  driver->HandleProcessingInstruction(target, data);
}

static void Driver_HandleDefault(rlbox_sandbox_expat& aSandbox,
                                 tainted_expat<void*> /* aUserData */,
                                 tainted_expat<const XML_Char*> aData,
                                 tainted_expat<int> aLength) {
  nsExpatDriver* driver = static_cast<nsExpatDriver*>(aSandbox.sandbox_storage);
  MOZ_ASSERT(driver);
  // aData is not null terminated; even with bad length we will not span
  // beyond sandbox boundary
  uint32_t length =
      static_cast<uint32_t>(aLength.copy_and_verify(safe_unverified<int>));
  const auto* data = aData.unverified_safe_pointer_because(
      length, "Only care that the data is within sandbox boundary.");
  driver->HandleDefault(data, length);
}

static void Driver_HandleStartCdataSection(
    rlbox_sandbox_expat& aSandbox, tainted_expat<void*> /* aUserData */) {
  nsExpatDriver* driver = static_cast<nsExpatDriver*>(aSandbox.sandbox_storage);
  MOZ_ASSERT(driver);
  driver->HandleStartCdataSection();
}

static void Driver_HandleEndCdataSection(rlbox_sandbox_expat& aSandbox,
                                         tainted_expat<void*> /* aUserData */) {
  nsExpatDriver* driver = static_cast<nsExpatDriver*>(aSandbox.sandbox_storage);
  MOZ_ASSERT(driver);
  driver->HandleEndCdataSection();
}

static void Driver_HandleStartDoctypeDecl(
    rlbox_sandbox_expat& aSandbox, tainted_expat<void*> /* aUserData */,
    tainted_expat<const XML_Char*> aDoctypeName,
    tainted_expat<const XML_Char*> aSysid,
    tainted_expat<const XML_Char*> aPubid,
    tainted_expat<int> aHasInternalSubset) {
  nsExpatDriver* driver = static_cast<nsExpatDriver*>(aSandbox.sandbox_storage);
  MOZ_ASSERT(driver);
  const auto* doctypeName =
      aDoctypeName.copy_and_verify_address(unverified_xml_string);
  const auto* sysid = aSysid.copy_and_verify_address(unverified_xml_string);
  const auto* pubid = aPubid.copy_and_verify_address(unverified_xml_string);
  bool hasInternalSubset =
      !!(aHasInternalSubset.copy_and_verify(safe_unverified<int>));
  driver->HandleStartDoctypeDecl(doctypeName, sysid, pubid, hasInternalSubset);
}

static void Driver_HandleEndDoctypeDecl(rlbox_sandbox_expat& aSandbox,
                                        tainted_expat<void*> /* aUserData */) {
  nsExpatDriver* driver = static_cast<nsExpatDriver*>(aSandbox.sandbox_storage);
  MOZ_ASSERT(driver);
  driver->HandleEndDoctypeDecl();
}

static tainted_expat<int> Driver_HandleExternalEntityRef(
    rlbox_sandbox_expat& aSandbox, tainted_expat<XML_Parser> /* aParser */,
    tainted_expat<const XML_Char*> aOpenEntityNames,
    tainted_expat<const XML_Char*> aBase,
    tainted_expat<const XML_Char*> aSystemId,
    tainted_expat<const XML_Char*> aPublicId) {
  nsExpatDriver* driver = static_cast<nsExpatDriver*>(aSandbox.sandbox_storage);
  MOZ_ASSERT(driver);

  const auto* openEntityNames =
      aOpenEntityNames.copy_and_verify_address(unverified_xml_string);
  const auto* base = aBase.copy_and_verify_address(unverified_xml_string);
  const auto* systemId =
      aSystemId.copy_and_verify_address(unverified_xml_string);
  const auto* publicId =
      aPublicId.copy_and_verify_address(unverified_xml_string);
  return driver->HandleExternalEntityRef(openEntityNames, base, systemId,
                                         publicId);
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
  NS_INTERFACE_MAP_ENTRY(nsIDTD)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsExpatDriver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsExpatDriver)

NS_IMPL_CYCLE_COLLECTION(nsExpatDriver, mSink)

nsExpatDriver::nsExpatDriver()
    : mExpatParser(nullptr),
      mInCData(false),
      mInInternalSubset(false),
      mInExternalDTD(false),
      mMadeFinalCallToExpat(false),
      mInParser(false),
      mInternalState(NS_OK),
      mExpatBuffered(0),
      mTagDepth(0),
      mCatalogData(nullptr),
      mInnerWindowID(0) {}

nsExpatDriver::~nsExpatDriver() { Destroy(); }

void nsExpatDriver::Destroy() {
  if (mSandboxPoolData) {
    SandboxData()->DetachDriver();
    if (mExpatParser) {
      RLBOX_EXPAT_MCALL(MOZ_XML_ParserFree);
    }
  }
  mSandboxPoolData.reset();
  mURIs.Clear();
  mExpatParser = nullptr;
}

// The AllocAttrs class is used to speed up copying attributes from the
// sandboxed expat by fast allocating attributes on the stack and only falling
// back to malloc when we need to allocate lots of attributes.
class MOZ_STACK_CLASS AllocAttrs {
#define NUM_STACK_SLOTS 16
 public:
  const char16_t** Init(size_t size) {
    if (size <= NUM_STACK_SLOTS) {
      return mInlineArr;
    }
    mHeapPtr = mozilla::MakeUnique<const char16_t*[]>(size);
    return mHeapPtr.get();
  }

 private:
  const char16_t* mInlineArr[NUM_STACK_SLOTS];
  mozilla::UniquePtr<const char16_t*[]> mHeapPtr;
#undef NUM_STACK_SLOTS
};

/* static */
void nsExpatDriver::HandleStartElement(rlbox_sandbox_expat& aSandbox,
                                       tainted_expat<void*> /* aUserData */,
                                       tainted_expat<const char16_t*> aName,
                                       tainted_expat<const char16_t**> aAttrs) {
  nsExpatDriver* self = static_cast<nsExpatDriver*>(aSandbox.sandbox_storage);
  MOZ_ASSERT(self && self->mSink);

  const auto* name = aName.copy_and_verify_address(unverified_xml_string);

  // Calculate the total number of elements in aAttrs.
  // XML_GetSpecifiedAttributeCount will only give us the number of specified
  // attrs (twice that number, actually), so we have to check for default
  // attrs ourselves.
  tainted_expat<int> count =
      RLBOX_EXPAT_CALL(MOZ_XML_GetSpecifiedAttributeCount);
  MOZ_RELEASE_ASSERT_TAINTED(count >= 0, "Unexpected attribute count");

  tainted_expat<uint64_t> attrArrayLengthTainted;
  for (attrArrayLengthTainted = rlbox::sandbox_static_cast<uint64_t>(count);
       (aAttrs[attrArrayLengthTainted] != nullptr)
           .unverified_safe_because("Bad length is checked later");
       attrArrayLengthTainted += 2) {
    // Just looping till we find out what the length is
  }

  uint32_t attrArrayLength =
      attrArrayLengthTainted.copy_and_verify([&](uint64_t value) {
        // A malicious length could result in an overflow when we allocate
        // aAttrs and then access elements of the array.
        MOZ_RELEASE_ASSERT(value < UINT32_MAX, "Overflow attempt");
        return value;
      });

  // Copy tainted aAttrs from sandbox
  AllocAttrs allocAttrs;
  const char16_t** attrs = allocAttrs.Init(attrArrayLength + 1);
  if (NS_WARN_IF(!aAttrs || !attrs)) {
    self->MaybeStopParser(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  for (uint32_t i = 0; i < attrArrayLength; i++) {
    attrs[i] = aAttrs[i].copy_and_verify_address(unverified_xml_string);
  }
  attrs[attrArrayLength] = nullptr;

  if (self->mSink) {
    // We store the tagdepth in a PRUint16, so make sure the limit fits in a
    // PRUint16.
    static_assert(
        sMaxXMLTreeDepth <=
        std::numeric_limits<decltype(nsExpatDriver::mTagDepth)>::max());

    if (++self->mTagDepth > sMaxXMLTreeDepth) {
      self->MaybeStopParser(NS_ERROR_HTMLPARSER_HIERARCHYTOODEEP);
      return;
    }

    nsresult rv = self->mSink->HandleStartElement(
        name, attrs, attrArrayLength,
        RLBOX_EXPAT_SAFE_CALL(MOZ_XML_GetCurrentLineNumber,
                              safe_unverified<XML_Size>),
        RLBOX_EXPAT_SAFE_CALL(MOZ_XML_GetCurrentColumnNumber,
                              safe_unverified<XML_Size>));
    self->MaybeStopParser(rv);
  }
}

/* static */
void nsExpatDriver::HandleStartElementForSystemPrincipal(
    rlbox_sandbox_expat& aSandbox, tainted_expat<void*> aUserData,
    tainted_expat<const char16_t*> aName,
    tainted_expat<const char16_t**> aAttrs) {
  nsExpatDriver* self = static_cast<nsExpatDriver*>(aSandbox.sandbox_storage);
  MOZ_ASSERT(self);
  if (!RLBOX_EXPAT_SAFE_CALL(MOZ_XML_ProcessingEntityValue,
                             safe_unverified<XML_Bool>)) {
    HandleStartElement(aSandbox, aUserData, aName, aAttrs);
  } else {
    nsCOMPtr<Document> doc =
        do_QueryInterface(self->mOriginalSink->GetTarget());

    // Adjust the column number so that it is one based rather than zero
    // based.
    tainted_expat<XML_Size> colNumber =
        RLBOX_EXPAT_CALL(MOZ_XML_GetCurrentColumnNumber) + 1;
    tainted_expat<XML_Size> lineNumber =
        RLBOX_EXPAT_CALL(MOZ_XML_GetCurrentLineNumber);

    int32_t nameSpaceID;
    RefPtr<nsAtom> prefix, localName;
    const auto* name = aName.copy_and_verify_address(unverified_xml_string);
    nsContentUtils::SplitExpatName(name, getter_AddRefs(prefix),
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
        u""_ns, lineNumber.unverified_safe_because(RLBOX_SAFE_PRINT),
        colNumber.unverified_safe_because(RLBOX_SAFE_PRINT));
  }
}

/* static */
void nsExpatDriver::HandleEndElement(rlbox_sandbox_expat& aSandbox,
                                     tainted_expat<void*> aUserData,
                                     tainted_expat<const char16_t*> aName) {
  nsExpatDriver* self = static_cast<nsExpatDriver*>(aSandbox.sandbox_storage);
  MOZ_ASSERT(self);
  const auto* name = aName.copy_and_verify_address(unverified_xml_string);

  NS_ASSERTION(self->mSink, "content sink not found!");
  NS_ASSERTION(self->mInternalState != NS_ERROR_HTMLPARSER_BLOCK,
               "Shouldn't block from HandleStartElement.");

  if (self->mSink && self->mInternalState != NS_ERROR_HTMLPARSER_STOPPARSING) {
    nsresult rv = self->mSink->HandleEndElement(name);
    --self->mTagDepth;
    self->MaybeStopParser(rv);
  }
}

/* static */
void nsExpatDriver::HandleEndElementForSystemPrincipal(
    rlbox_sandbox_expat& aSandbox, tainted_expat<void*> aUserData,
    tainted_expat<const char16_t*> aName) {
  nsExpatDriver* self = static_cast<nsExpatDriver*>(aSandbox.sandbox_storage);
  MOZ_ASSERT(self);
  if (!RLBOX_EXPAT_SAFE_CALL(MOZ_XML_ProcessingEntityValue,
                             safe_unverified<XML_Bool>)) {
    HandleEndElement(aSandbox, aUserData, aName);
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

// Wrapper class for passing the sandbox data and parser as a closure to
// ExternalDTDStreamReaderFunc.
class RLBoxExpatClosure {
 public:
  RLBoxExpatClosure(RLBoxExpatSandboxData* aSbxData,
                    tainted_expat<XML_Parser> aExpatParser)
      : mSbxData(aSbxData), mExpatParser(aExpatParser){};
  inline rlbox_sandbox_expat* Sandbox() const { return mSbxData->Sandbox(); };
  inline tainted_expat<XML_Parser> Parser() const { return mExpatParser; };

 private:
  RLBoxExpatSandboxData* mSbxData;
  tainted_expat<XML_Parser> mExpatParser;
};

static nsresult ExternalDTDStreamReaderFunc(nsIUnicharInputStream* aIn,
                                            void* aClosure,
                                            const char16_t* aFromSegment,
                                            uint32_t aToOffset, uint32_t aCount,
                                            uint32_t* aWriteCount) {
  MOZ_ASSERT(aClosure && aFromSegment && aWriteCount);

  *aWriteCount = 0;

  // Get sandbox and parser
  auto* closure = reinterpret_cast<RLBoxExpatClosure*>(aClosure);
  MOZ_ASSERT(closure);

  // Transfer segment into the sandbox
  auto fromSegment =
      TransferBuffer<char16_t>(closure->Sandbox(), aFromSegment, aCount);
  NS_ENSURE_TRUE(*fromSegment, NS_ERROR_OUT_OF_MEMORY);

  // Pass the buffer to expat for parsing.
  if (closure->Sandbox()
          ->invoke_sandbox_function(
              MOZ_XML_Parse, closure->Parser(),
              rlbox::sandbox_reinterpret_cast<const char*>(*fromSegment),
              aCount * sizeof(char16_t), 0)
          .copy_and_verify(status_verifier) == XML_STATUS_OK) {
    *aWriteCount = aCount;
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
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

  nsCOMPtr<nsIURI> baseURI = GetBaseURI(base);
  NS_ENSURE_TRUE(baseURI, 1);

  // Load the external entity into a buffer.
  nsCOMPtr<nsIInputStream> in;
  nsCOMPtr<nsIURI> absURI;
  nsresult rv = OpenInputStreamFromExternalDTD(
      publicId, systemId, baseURI, getter_AddRefs(in), getter_AddRefs(absURI));
  if (NS_FAILED(rv)) {
#ifdef DEBUG
    nsCString message("Failed to open external DTD: publicId \"");
    AppendUTF16toUTF8(MakeStringSpan(publicId), message);
    message += "\" systemId \"";
    AppendUTF16toUTF8(MakeStringSpan(systemId), message);
    message += "\" base \"";
    message.Append(baseURI->GetSpecOrDefault());
    message += "\" URL \"";
    if (absURI) {
      message.Append(absURI->GetSpecOrDefault());
    }
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
    auto utf16 = TransferBuffer<char16_t>(
        Sandbox(), kUTF16, nsCharTraits<char16_t>::length(kUTF16) + 1);
    NS_ENSURE_TRUE(*utf16, 1);
    tainted_expat<XML_Parser> entParser;
    entParser =
        RLBOX_EXPAT_MCALL(MOZ_XML_ExternalEntityParserCreate, nullptr, *utf16);
    if (entParser) {
      auto baseURI = GetExpatBaseURI(absURI);
      auto url = TransferBuffer<XML_Char>(Sandbox(), &baseURI[0],
                                          ArrayLength(baseURI));
      NS_ENSURE_TRUE(*url, 1);
      Sandbox()->invoke_sandbox_function(MOZ_XML_SetBase, entParser, *url);

      mInExternalDTD = true;

      bool inParser = mInParser;  // Save in-parser status
      mInParser = true;

      RLBoxExpatClosure closure(SandboxData(), entParser);
      uint32_t totalRead;
      do {
        rv = uniIn->ReadSegments(ExternalDTDStreamReaderFunc, &closure,
                                 uint32_t(-1), &totalRead);
      } while (NS_SUCCEEDED(rv) && totalRead > 0);

      result =
          Sandbox()
              ->invoke_sandbox_function(MOZ_XML_Parse, entParser, nullptr, 0, 1)
              .copy_and_verify(status_verifier);

      mInParser = inParser;  // Restore in-parser status
      mInExternalDTD = false;

      Sandbox()->invoke_sandbox_function(MOZ_XML_ParserFree, entParser);
    }
  }

  return result;
}

nsresult nsExpatDriver::OpenInputStreamFromExternalDTD(const char16_t* aFPIStr,
                                                       const char16_t* aURLStr,
                                                       nsIURI* aBaseURI,
                                                       nsIInputStream** aStream,
                                                       nsIURI** aAbsURI) {
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), NS_ConvertUTF16toUTF8(aURLStr),
                          nullptr, aBaseURI);
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

  uri.forget(aAbsURI);

  channel->SetContentType("application/xml"_ns);
  return channel->Open(aStream);
}

static nsresult CreateErrorText(const char16_t* aDescription,
                                const char16_t* aSourceURL,
                                tainted_expat<XML_Size> aLineNumber,
                                tainted_expat<XML_Size> aColNumber,
                                nsString& aErrorString, bool spoofEnglish) {
  aErrorString.Truncate();

  nsAutoString msg;
  nsresult rv = nsParserMsgUtils::GetLocalizedStringByName(
      spoofEnglish ? XMLPARSER_PROPERTIES_en_US : XMLPARSER_PROPERTIES,
      "XMLParsingError", msg);
  NS_ENSURE_SUCCESS(rv, rv);

  // XML Parsing Error: %1$S\nLocation: %2$S\nLine Number %3$u, Column %4$u:
  nsTextFormatter::ssprintf(
      aErrorString, msg.get(), aDescription, aSourceURL,
      aLineNumber.unverified_safe_because(RLBOX_SAFE_PRINT),
      aColNumber.unverified_safe_because(RLBOX_SAFE_PRINT));
  return NS_OK;
}

static nsresult AppendErrorPointer(tainted_expat<XML_Size> aColNumber,
                                   const char16_t* aSourceLine,
                                   size_t aSourceLineLength,
                                   nsString& aSourceString) {
  aSourceString.Append(char16_t('\n'));

  MOZ_RELEASE_ASSERT_TAINTED(aColNumber != static_cast<XML_Size>(0),
                             "Unexpected value of column");

  // Last character will be '^'.
  XML_Size last =
      (aColNumber - 1).copy_and_verify([&](XML_Size val) -> XML_Size {
        if (val > aSourceLineLength) {
          // Unexpected value of last column, just return a safe value
          return 0;
        }
        return val;
      });

  XML_Size i;
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
  int32_t code =
      RLBOX_EXPAT_MCALL(MOZ_XML_GetErrorCode).copy_and_verify(error_verifier);

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

    const char16_t* mismatch =
        RLBOX_EXPAT_MCALL(MOZ_XML_GetMismatchedTag)
            .copy_and_verify_address(unverified_xml_string);
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
  }

  // Adjust the column number so that it is one based rather than zero based.
  tainted_expat<XML_Size> colNumber =
      RLBOX_EXPAT_MCALL(MOZ_XML_GetCurrentColumnNumber) + 1;
  tainted_expat<XML_Size> lineNumber =
      RLBOX_EXPAT_MCALL(MOZ_XML_GetCurrentLineNumber);

  // Copy out the two character bufer that holds the expatBase
  const std::unique_ptr<XML_Char[]> expatBase =
      RLBOX_EXPAT_MCALL(MOZ_XML_GetBase)
          .copy_and_verify_range(
              [](std::unique_ptr<XML_Char[]> val) {
                // No additional checks needed as this is sent to GetBaseURI
                // which checks its inputs
                return val;
              },
              ExpatBaseURI::Length);
  nsAutoString uri;
  nsCOMPtr<nsIURI> baseURI;
  if (expatBase && (baseURI = GetBaseURI(expatBase.get()))) {
    // Let's ignore if this fails, we're already reporting a parse error.
    Unused << CopyUTF8toUTF16(baseURI->GetSpecOrDefault(), uri, fallible);
  }
  nsAutoString errorText;
  CreateErrorText(description.get(), uri.get(), lineNumber, colNumber,
                  errorText, spoofEnglish);

  nsAutoString sourceText(mLastLine);
  AppendErrorPointer(colNumber, mLastLine.get(), mLastLine.Length(),
                     sourceText);

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
                "location"_ns,
                nsPrintfCString(
                    "%lu:%lu",
                    lineNumber.unverified_safe_because(RLBOX_SAFE_PRINT),
                    colNumber.unverified_safe_because(RLBOX_SAFE_PRINT))},
            mozilla::Telemetry::EventExtraEntry{
                "last_line"_ns, NS_ConvertUTF16toUTF8(mLastLine)},
            mozilla::Telemetry::EventExtraEntry{
                "last_line_len"_ns, nsPrintfCString("%zu", mLastLine.Length())},
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
    rv = serr->InitWithSourceURI(
        errorText, mURIs.SafeElementAt(0), mLastLine,
        lineNumber.unverified_safe_because(RLBOX_SAFE_PRINT),
        colNumber.unverified_safe_because(RLBOX_SAFE_PRINT),
        nsIScriptError::errorFlag, "malformed-xml", mInnerWindowID);
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

// Because we need to allocate a buffer in the RLBOX sandbox, and copy the data
// to it for Expat to parse, we are limited in size by the memory available in
// the RLBOX sandbox. nsExpatDriver::ChunkAndParseBuffer divides the buffer into
// chunks of sMaxChunkLength characters or less, and passes them to
// nsExpatDriver::ParseBuffer. That should ensure that we almost never run out
// of memory in the sandbox.
void nsExpatDriver::ChunkAndParseBuffer(const char16_t* aBuffer,
                                        uint32_t aLength, bool aIsFinal,
                                        uint32_t* aPassedToExpat,
                                        uint32_t* aConsumed,
                                        XML_Size* aLastLineLength) {
  *aConsumed = 0;
  *aLastLineLength = 0;

  uint32_t remainder = aLength;
  while (remainder > sMaxChunkLength) {
    ParseChunk(aBuffer, sMaxChunkLength, ChunkOrBufferIsFinal::None, aConsumed,
               aLastLineLength);
    aBuffer += sMaxChunkLength;
    remainder -= sMaxChunkLength;
    if (NS_FAILED(mInternalState)) {
      // Stop parsing if there's an error (including if we're blocked or
      // interrupted).
      *aPassedToExpat = aLength - remainder;
      return;
    }
  }

  ParseChunk(aBuffer, remainder,
             aIsFinal ? ChunkOrBufferIsFinal::FinalChunkAndBuffer
                      : ChunkOrBufferIsFinal::FinalChunk,
             aConsumed, aLastLineLength);
  *aPassedToExpat = aLength;
}

void nsExpatDriver::ParseChunk(const char16_t* aBuffer, uint32_t aLength,
                               ChunkOrBufferIsFinal aIsFinal,
                               uint32_t* aConsumed, XML_Size* aLastLineLength) {
  NS_ASSERTION((aBuffer && aLength != 0) || (!aBuffer && aLength == 0), "?");
  NS_ASSERTION(mInternalState != NS_OK ||
                   (aIsFinal == ChunkOrBufferIsFinal::FinalChunkAndBuffer) ||
                   aBuffer,
               "Useless call, we won't call Expat");
  MOZ_ASSERT(!BlockedOrInterrupted() || !aBuffer,
             "Non-null buffer when resuming");
  MOZ_ASSERT(mExpatParser);

  auto parserBytesBefore_verifier = [&](auto parserBytesBefore) {
    MOZ_RELEASE_ASSERT(parserBytesBefore >= 0, "Unexpected value");
    MOZ_RELEASE_ASSERT(parserBytesBefore % sizeof(char16_t) == 0,
                       "Consumed part of a char16_t?");
    return parserBytesBefore;
  };
  int32_t parserBytesBefore = RLBOX_EXPAT_SAFE_MCALL(
      XML_GetCurrentByteIndex, parserBytesBefore_verifier);

  if (mInternalState != NS_OK && !BlockedOrInterrupted()) {
    return;
  }

  XML_Status status;
  bool inParser = mInParser;  // Save in-parser status
  mInParser = true;
  Maybe<TransferBuffer<char16_t>> buffer;
  if (BlockedOrInterrupted()) {
    mInternalState = NS_OK;  // Resume in case we're blocked.
    status = RLBOX_EXPAT_SAFE_MCALL(MOZ_XML_ResumeParser, status_verifier);
  } else {
    buffer.emplace(Sandbox(), aBuffer, aLength);
    MOZ_RELEASE_ASSERT(!aBuffer || !!*buffer.ref(),
                       "Chunking should avoid OOM in ParseBuffer");

    status = RLBOX_EXPAT_SAFE_MCALL(
        MOZ_XML_Parse, status_verifier,
        rlbox::sandbox_reinterpret_cast<const char*>(*buffer.ref()),
        aLength * sizeof(char16_t),
        aIsFinal == ChunkOrBufferIsFinal::FinalChunkAndBuffer);
  }
  mInParser = inParser;  // Restore in-parser status

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
  *aConsumed += (parserBytesConsumed - parserBytesBefore) / sizeof(char16_t);

  NS_ASSERTION(status != XML_STATUS_SUSPENDED || BlockedOrInterrupted(),
               "Inconsistent expat suspension state.");

  if (status == XML_STATUS_ERROR) {
    mInternalState = NS_ERROR_HTMLPARSER_STOPPARSING;
  }

  if (*aConsumed > 0 &&
      (aIsFinal != ChunkOrBufferIsFinal::None || NS_FAILED(mInternalState))) {
    *aLastLineLength = RLBOX_EXPAT_SAFE_MCALL(MOZ_XML_GetCurrentColumnNumber,
                                              safe_unverified<XML_Size>);
  }
}

nsresult nsExpatDriver::ResumeParse(nsScanner& aScanner, bool aIsFinalChunk) {
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
  while (start != end || (aIsFinalChunk && !mMadeFinalCallToExpat) ||
         (BlockedOrInterrupted() && mExpatBuffered > 0)) {
    bool noMoreBuffers = start == end && aIsFinalChunk;
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

    uint32_t passedToExpat;
    uint32_t consumed;
    XML_Size lastLineLength;
    ChunkAndParseBuffer(buffer, length, noMoreBuffers, &passedToExpat,
                        &consumed, &lastLineLength);
    MOZ_ASSERT_IF(passedToExpat != length, NS_FAILED(mInternalState));
    MOZ_ASSERT(consumed <= passedToExpat + mExpatBuffered);
    if (consumed > 0) {
      nsScannerIterator oldExpatPosition = currentExpatPosition;
      currentExpatPosition.advance(consumed);

      // We consumed some data, we want to store the last line of data that
      // was consumed in case we run into an error (to show the line in which
      // the error occurred).

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

    mExpatBuffered += passedToExpat - consumed;

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

mozilla::UniquePtr<mozilla::RLBoxSandboxDataBase>
RLBoxExpatSandboxPool::CreateSandboxData(uint64_t aSize) {
  // Create expat sandbox
  auto sandbox = mozilla::MakeUnique<rlbox_sandbox_expat>();

#ifdef MOZ_WASM_SANDBOXING_EXPAT
  const w2c_mem_capacity capacity =
      get_valid_wasm2c_memory_capacity(aSize, true /* 32-bit wasm memory*/);
  bool create_ok = sandbox->create_sandbox(/* infallible = */ false, &capacity);
#else
  bool create_ok = sandbox->create_sandbox();
#endif

  NS_ENSURE_TRUE(create_ok, nullptr);

  mozilla::UniquePtr<RLBoxExpatSandboxData> sbxData =
      mozilla::MakeUnique<RLBoxExpatSandboxData>(aSize);

  // Register callbacks common to both system and non-system principals
  sbxData->mHandleXMLDeclaration =
      sandbox->register_callback(Driver_HandleXMLDeclaration);
  sbxData->mHandleCharacterData =
      sandbox->register_callback(Driver_HandleCharacterData);
  sbxData->mHandleProcessingInstruction =
      sandbox->register_callback(Driver_HandleProcessingInstruction);
  sbxData->mHandleDefault = sandbox->register_callback(Driver_HandleDefault);
  sbxData->mHandleExternalEntityRef =
      sandbox->register_callback(Driver_HandleExternalEntityRef);
  sbxData->mHandleComment = sandbox->register_callback(Driver_HandleComment);
  sbxData->mHandleStartCdataSection =
      sandbox->register_callback(Driver_HandleStartCdataSection);
  sbxData->mHandleEndCdataSection =
      sandbox->register_callback(Driver_HandleEndCdataSection);
  sbxData->mHandleStartDoctypeDecl =
      sandbox->register_callback(Driver_HandleStartDoctypeDecl);
  sbxData->mHandleEndDoctypeDecl =
      sandbox->register_callback(Driver_HandleEndDoctypeDecl);

  sbxData->mSandbox = std::move(sandbox);

  return sbxData;
}

mozilla::StaticRefPtr<RLBoxExpatSandboxPool> RLBoxExpatSandboxPool::sSingleton;

void RLBoxExpatSandboxPool::Initialize(size_t aDelaySeconds) {
  mozilla::AssertIsOnMainThread();
  RLBoxExpatSandboxPool::sSingleton = new RLBoxExpatSandboxPool(aDelaySeconds);
  ClearOnShutdown(&RLBoxExpatSandboxPool::sSingleton);
}

void RLBoxExpatSandboxData::AttachDriver(bool aIsSystemPrincipal,
                                         void* aDriver) {
  MOZ_ASSERT(!mSandbox->sandbox_storage);
  MOZ_ASSERT(mHandleStartElement.is_unregistered());
  MOZ_ASSERT(mHandleEndElement.is_unregistered());

  if (aIsSystemPrincipal) {
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

  mSandbox->sandbox_storage = aDriver;
}

void RLBoxExpatSandboxData::DetachDriver() {
  mSandbox->sandbox_storage = nullptr;
  mHandleStartElement.unregister();
  mHandleEndElement.unregister();
}

RLBoxExpatSandboxData::~RLBoxExpatSandboxData() {
  MOZ_ASSERT(mSandbox);

  // DetachDriver should always be called before a sandbox goes back into the
  // pool, and thus before it's freed.
  MOZ_ASSERT(!mSandbox->sandbox_storage);
  MOZ_ASSERT(mHandleStartElement.is_unregistered());
  MOZ_ASSERT(mHandleEndElement.is_unregistered());

  // Unregister callbacks
  mHandleXMLDeclaration.unregister();
  mHandleCharacterData.unregister();
  mHandleProcessingInstruction.unregister();
  mHandleDefault.unregister();
  mHandleExternalEntityRef.unregister();
  mHandleComment.unregister();
  mHandleStartCdataSection.unregister();
  mHandleEndCdataSection.unregister();
  mHandleStartDoctypeDecl.unregister();
  mHandleEndDoctypeDecl.unregister();
  // Destroy sandbox
  mSandbox->destroy_sandbox();
  MOZ_COUNT_DTOR(RLBoxExpatSandboxData);
}

nsresult nsExpatDriver::Initialize(nsIURI* aURI, nsIContentSink* aSink) {
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

  // Create sandbox
  //
  // We have to make sure the sandbox is large enough. We unscientifically
  // request two MB. Note that the parsing itself is chunked so as not to
  // require a large sandbox.
  static const uint64_t minSandboxSize = 2 * 1024 * 1024;
  MOZ_ASSERT(!mSandboxPoolData);
  mSandboxPoolData =
      RLBoxExpatSandboxPool::sSingleton->PopOrCreate(minSandboxSize);
  NS_ENSURE_TRUE(mSandboxPoolData, NS_ERROR_OUT_OF_MEMORY);

  MOZ_ASSERT(SandboxData());

  SandboxData()->AttachDriver(doc && doc->NodePrincipal()->IsSystemPrincipal(),
                              static_cast<void*>(this));

  // Create expat parser.
  // We need to copy the encoding and namespace separator into the sandbox.
  // For the noop sandbox we pass in the memsuite; for the Wasm sandbox, we
  // pass in nullptr to let expat use the standard library memory suite.
  auto expatSeparator = TransferBuffer<char16_t>(
      Sandbox(), kExpatSeparator,
      nsCharTraits<char16_t>::length(kExpatSeparator) + 1);
  MOZ_RELEASE_ASSERT(*expatSeparator);
  auto utf16 = TransferBuffer<char16_t>(
      Sandbox(), kUTF16, nsCharTraits<char16_t>::length(kUTF16) + 1);
  MOZ_RELEASE_ASSERT(*utf16);
  mExpatParser = Sandbox()->invoke_sandbox_function(
      MOZ_XML_ParserCreate_MM, *utf16, nullptr, *expatSeparator);
  NS_ENSURE_TRUE(mExpatParser, NS_ERROR_FAILURE);

  RLBOX_EXPAT_MCALL(MOZ_XML_SetReturnNSTriplet, XML_TRUE);

#ifdef XML_DTD
  RLBOX_EXPAT_MCALL(MOZ_XML_SetParamEntityParsing,
                    XML_PARAM_ENTITY_PARSING_ALWAYS);
#endif

  auto baseURI = GetExpatBaseURI(aURI);
  auto uri =
      TransferBuffer<XML_Char>(Sandbox(), &baseURI[0], ArrayLength(baseURI));
  RLBOX_EXPAT_MCALL(MOZ_XML_SetBase, *uri);

  // Set up the callbacks
  RLBOX_EXPAT_MCALL(MOZ_XML_SetXmlDeclHandler,
                    SandboxData()->mHandleXMLDeclaration);
  RLBOX_EXPAT_MCALL(MOZ_XML_SetElementHandler,
                    SandboxData()->mHandleStartElement,
                    SandboxData()->mHandleEndElement);
  RLBOX_EXPAT_MCALL(MOZ_XML_SetCharacterDataHandler,
                    SandboxData()->mHandleCharacterData);
  RLBOX_EXPAT_MCALL(MOZ_XML_SetProcessingInstructionHandler,
                    SandboxData()->mHandleProcessingInstruction);
  RLBOX_EXPAT_MCALL(MOZ_XML_SetDefaultHandlerExpand,
                    SandboxData()->mHandleDefault);
  RLBOX_EXPAT_MCALL(MOZ_XML_SetExternalEntityRefHandler,
                    SandboxData()->mHandleExternalEntityRef);
  RLBOX_EXPAT_MCALL(MOZ_XML_SetCommentHandler, SandboxData()->mHandleComment);
  RLBOX_EXPAT_MCALL(MOZ_XML_SetCdataSectionHandler,
                    SandboxData()->mHandleStartCdataSection,
                    SandboxData()->mHandleEndCdataSection);

  RLBOX_EXPAT_MCALL(MOZ_XML_SetParamEntityParsing,
                    XML_PARAM_ENTITY_PARSING_UNLESS_STANDALONE);
  RLBOX_EXPAT_MCALL(MOZ_XML_SetDoctypeDeclHandler,
                    SandboxData()->mHandleStartDoctypeDecl,
                    SandboxData()->mHandleEndDoctypeDecl);

  return mInternalState;
}

NS_IMETHODIMP
nsExpatDriver::BuildModel(nsIContentSink* aSink) { return mInternalState; }

void nsExpatDriver::DidBuildModel() {
  if (!mInParser) {
    // Because nsExpatDriver is cycle-collected, it gets destroyed
    // asynchronously. We want to eagerly release the sandbox back into the
    // pool so that it can be reused immediately, unless this is a reentrant
    // call (which we track with mInParser).
    Destroy();
  }
  mOriginalSink = nullptr;
  mSink = nullptr;
}

NS_IMETHODIMP_(void)
nsExpatDriver::Terminate() {
  // XXX - not sure what happens to the unparsed data.
  if (mExpatParser) {
    RLBOX_EXPAT_MCALL(MOZ_XML_StopParser, XML_FALSE);
  }
  mInternalState = NS_ERROR_HTMLPARSER_STOPPARSING;
}

/*************************** Unused methods **********************************/

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

    // Note that due to Bug 1742913, we need to explicitly cast the parameter to
    // an int so that the value is correctly zero extended.
    int resumable = BlockedOrInterrupted();
    RLBOX_EXPAT_MCALL(MOZ_XML_StopParser, resumable);
  } else if (NS_SUCCEEDED(mInternalState)) {
    // Only clobber mInternalState with the success code if we didn't block or
    // interrupt before.
    mInternalState = aState;
  }
}

nsExpatDriver::ExpatBaseURI nsExpatDriver::GetExpatBaseURI(nsIURI* aURI) {
  mURIs.AppendElement(aURI);

  MOZ_RELEASE_ASSERT(mURIs.Length() <= std::numeric_limits<XML_Char>::max());

  return ExpatBaseURI(static_cast<XML_Char>(mURIs.Length()), XML_T('\0'));
}

nsIURI* nsExpatDriver::GetBaseURI(const XML_Char* aBase) const {
  MOZ_ASSERT(aBase[0] != '\0' && aBase[1] == '\0');

  if (aBase[0] == '\0' || aBase[1] != '\0') {
    return nullptr;
  }

  uint32_t index = aBase[0] - 1;
  MOZ_ASSERT(index < mURIs.Length());

  return mURIs.SafeElementAt(index);
}

inline RLBoxExpatSandboxData* nsExpatDriver::SandboxData() const {
  return reinterpret_cast<RLBoxExpatSandboxData*>(
      mSandboxPoolData->SandboxData());
}

inline rlbox_sandbox_expat* nsExpatDriver::Sandbox() const {
  return SandboxData()->Sandbox();
}
