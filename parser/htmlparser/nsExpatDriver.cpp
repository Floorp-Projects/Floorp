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
#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsError.h"
#include "nsXPCOMCIDInternal.h"
#include "nsUnicharInputStream.h"
#include "nsContentUtils.h"
#include "NullPrincipal.h"

#include "mozilla/Logging.h"

using mozilla::fallible;
using mozilla::LogLevel;

#define kExpatSeparatorChar 0xFFFF

static const char16_t kUTF16[] = { 'U', 'T', 'F', '-', '1', '6', '\0' };

static mozilla::LazyLogModule gExpatDriverLog("expatdriver");

/***************************** EXPAT CALL BACKS ******************************/
// The callback handlers that get called from the expat parser.

static void
Driver_HandleXMLDeclaration(void *aUserData,
                            const XML_Char *aVersion,
                            const XML_Char *aEncoding,
                            int aStandalone)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    nsExpatDriver* driver = static_cast<nsExpatDriver*>(aUserData);
    driver->HandleXMLDeclaration(aVersion, aEncoding, aStandalone);
  }
}

static void
Driver_HandleStartElement(void *aUserData,
                          const XML_Char *aName,
                          const XML_Char **aAtts)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    static_cast<nsExpatDriver*>(aUserData)->HandleStartElement(aName,
                                                                  aAtts);
  }
}

static void
Driver_HandleEndElement(void *aUserData,
                        const XML_Char *aName)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    static_cast<nsExpatDriver*>(aUserData)->HandleEndElement(aName);
  }
}

static void
Driver_HandleCharacterData(void *aUserData,
                           const XML_Char *aData,
                           int aLength)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    nsExpatDriver* driver = static_cast<nsExpatDriver*>(aUserData);
    driver->HandleCharacterData(aData, uint32_t(aLength));
  }
}

static void
Driver_HandleComment(void *aUserData,
                     const XML_Char *aName)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if(aUserData) {
    static_cast<nsExpatDriver*>(aUserData)->HandleComment(aName);
  }
}

static void
Driver_HandleProcessingInstruction(void *aUserData,
                                   const XML_Char *aTarget,
                                   const XML_Char *aData)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    nsExpatDriver* driver = static_cast<nsExpatDriver*>(aUserData);
    driver->HandleProcessingInstruction(aTarget, aData);
  }
}

static void
Driver_HandleDefault(void *aUserData,
                     const XML_Char *aData,
                     int aLength)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    nsExpatDriver* driver = static_cast<nsExpatDriver*>(aUserData);
    driver->HandleDefault(aData, uint32_t(aLength));
  }
}

static void
Driver_HandleStartCdataSection(void *aUserData)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    static_cast<nsExpatDriver*>(aUserData)->HandleStartCdataSection();
  }
}

static void
Driver_HandleEndCdataSection(void *aUserData)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    static_cast<nsExpatDriver*>(aUserData)->HandleEndCdataSection();
  }
}

static void
Driver_HandleStartDoctypeDecl(void *aUserData,
                              const XML_Char *aDoctypeName,
                              const XML_Char *aSysid,
                              const XML_Char *aPubid,
                              int aHasInternalSubset)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    static_cast<nsExpatDriver*>(aUserData)->
      HandleStartDoctypeDecl(aDoctypeName, aSysid, aPubid, !!aHasInternalSubset);
  }
}

static void
Driver_HandleEndDoctypeDecl(void *aUserData)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    static_cast<nsExpatDriver*>(aUserData)->HandleEndDoctypeDecl();
  }
}

static int
Driver_HandleExternalEntityRef(void *aExternalEntityRefHandler,
                               const XML_Char *aOpenEntityNames,
                               const XML_Char *aBase,
                               const XML_Char *aSystemId,
                               const XML_Char *aPublicId)
{
  NS_ASSERTION(aExternalEntityRefHandler, "expat driver should exist");
  if (!aExternalEntityRefHandler) {
    return 1;
  }

  nsExpatDriver* driver = static_cast<nsExpatDriver*>
                                     (aExternalEntityRefHandler);

  return driver->HandleExternalEntityRef(aOpenEntityNames, aBase, aSystemId,
                                         aPublicId);
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
  { "-//W3C//DTD XHTML 1.0 Transitional//EN",    "htmlmathml-f.ent", nullptr },
  { "-//W3C//DTD XHTML 1.1//EN",                 "htmlmathml-f.ent", nullptr },
  { "-//W3C//DTD XHTML 1.0 Strict//EN",          "htmlmathml-f.ent", nullptr },
  { "-//W3C//DTD XHTML 1.0 Frameset//EN",        "htmlmathml-f.ent", nullptr },
  { "-//W3C//DTD XHTML Basic 1.0//EN",           "htmlmathml-f.ent", nullptr },
  { "-//W3C//DTD XHTML 1.1 plus MathML 2.0//EN", "htmlmathml-f.ent", nullptr },
  { "-//W3C//DTD XHTML 1.1 plus MathML 2.0 plus SVG 1.1//EN", "htmlmathml-f.ent", nullptr },
  { "-//W3C//DTD MathML 2.0//EN",                "htmlmathml-f.ent", nullptr },
  { "-//WAPFORUM//DTD XHTML Mobile 1.0//EN",     "htmlmathml-f.ent", nullptr },
  { nullptr, nullptr, nullptr }
};

static const nsCatalogData*
LookupCatalogData(const char16_t* aPublicID)
{
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
// DTD instead of taking the filename from the URI.
static void
GetLocalDTDURI(const nsCatalogData* aCatalogData, nsIURI* aDTD,
              nsIURI** aResult)
{
  NS_ASSERTION(aDTD, "Null parameter.");

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
  : mExpatParser(nullptr),
    mInCData(false),
    mInInternalSubset(false),
    mInExternalDTD(false),
    mMadeFinalCallToExpat(false),
    mIsFinalChunk(false),
    mInternalState(NS_OK),
    mExpatBuffered(0),
    mCatalogData(nullptr),
    mInnerWindowID(0)
{
}

nsExpatDriver::~nsExpatDriver()
{
  if (mExpatParser) {
    XML_ParserFree(mExpatParser);
  }
}

nsresult
nsExpatDriver::HandleStartElement(const char16_t *aValue,
                                  const char16_t **aAtts)
{
  NS_ASSERTION(mSink, "content sink not found!");

  // Calculate the total number of elements in aAtts.
  // XML_GetSpecifiedAttributeCount will only give us the number of specified
  // attrs (twice that number, actually), so we have to check for default attrs
  // ourselves.
  uint32_t attrArrayLength;
  for (attrArrayLength = XML_GetSpecifiedAttributeCount(mExpatParser);
       aAtts[attrArrayLength];
       attrArrayLength += 2) {
    // Just looping till we find out what the length is
  }

  if (mSink) {
    nsresult rv = mSink->
      HandleStartElement(aValue, aAtts, attrArrayLength,
                         XML_GetCurrentLineNumber(mExpatParser));
    MaybeStopParser(rv);
  }

  return NS_OK;
}

nsresult
nsExpatDriver::HandleEndElement(const char16_t *aValue)
{
  NS_ASSERTION(mSink, "content sink not found!");
  NS_ASSERTION(mInternalState != NS_ERROR_HTMLPARSER_BLOCK,
               "Shouldn't block from HandleStartElement.");

  if (mSink && mInternalState != NS_ERROR_HTMLPARSER_STOPPARSING) {
    nsresult rv = mSink->HandleEndElement(aValue);
    MaybeStopParser(rv);
  }

  return NS_OK;
}

nsresult
nsExpatDriver::HandleCharacterData(const char16_t *aValue,
                                   const uint32_t aLength)
{
  NS_ASSERTION(mSink, "content sink not found!");

  if (mInCData) {
    if (!mCDataText.Append(aValue, aLength, fallible)) {
      MaybeStopParser(NS_ERROR_OUT_OF_MEMORY);
    }
  }
  else if (mSink) {
    nsresult rv = mSink->HandleCharacterData(aValue, aLength);
    MaybeStopParser(rv);
  }

  return NS_OK;
}

nsresult
nsExpatDriver::HandleComment(const char16_t *aValue)
{
  NS_ASSERTION(mSink, "content sink not found!");

  if (mInExternalDTD) {
    // Ignore comments from external DTDs
    return NS_OK;
  }

  if (mInInternalSubset) {
    mInternalSubset.AppendLiteral("<!--");
    mInternalSubset.Append(aValue);
    mInternalSubset.AppendLiteral("-->");
  }
  else if (mSink) {
    nsresult rv = mSink->HandleComment(aValue);
    MaybeStopParser(rv);
  }

  return NS_OK;
}

nsresult
nsExpatDriver::HandleProcessingInstruction(const char16_t *aTarget,
                                           const char16_t *aData)
{
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
  }
  else if (mSink) {
    nsresult rv = mSink->HandleProcessingInstruction(aTarget, aData);
    MaybeStopParser(rv);
  }

  return NS_OK;
}

nsresult
nsExpatDriver::HandleXMLDeclaration(const char16_t *aVersion,
                                    const char16_t *aEncoding,
                                    int32_t aStandalone)
{
  if (mSink) {
    nsresult rv = mSink->HandleXMLDeclaration(aVersion, aEncoding, aStandalone);
    MaybeStopParser(rv);
  }

  return NS_OK;
}

nsresult
nsExpatDriver::HandleDefault(const char16_t *aValue,
                             const uint32_t aLength)
{
  NS_ASSERTION(mSink, "content sink not found!");

  if (mInExternalDTD) {
    // Ignore newlines in external DTDs
    return NS_OK;
  }

  if (mInInternalSubset) {
    mInternalSubset.Append(aValue, aLength);
  }
  else if (mSink) {
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

nsresult
nsExpatDriver::HandleStartCdataSection()
{
  mInCData = true;

  return NS_OK;
}

nsresult
nsExpatDriver::HandleEndCdataSection()
{
  NS_ASSERTION(mSink, "content sink not found!");

  mInCData = false;
  if (mSink) {
    nsresult rv = mSink->HandleCDataSection(mCDataText.get(),
                                            mCDataText.Length());
    MaybeStopParser(rv);
  }
  mCDataText.Truncate();

  return NS_OK;
}

nsresult
nsExpatDriver::HandleStartDoctypeDecl(const char16_t* aDoctypeName,
                                      const char16_t* aSysid,
                                      const char16_t* aPubid,
                                      bool aHasInternalSubset)
{
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

nsresult
nsExpatDriver::HandleEndDoctypeDecl()
{
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
  
  mInternalSubset.SetCapacity(0);

  return NS_OK;
}

static nsresult
ExternalDTDStreamReaderFunc(nsIUnicharInputStream* aIn,
                            void* aClosure,
                            const char16_t* aFromSegment,
                            uint32_t aToOffset,
                            uint32_t aCount,
                            uint32_t *aWriteCount)
{
  // Pass the buffer to expat for parsing.
  if (XML_Parse((XML_Parser)aClosure, (const char *)aFromSegment,
                aCount * sizeof(char16_t), 0) == XML_STATUS_OK) {
    *aWriteCount = aCount;

    return NS_OK;
  }

  *aWriteCount = 0;

  return NS_ERROR_FAILURE;
}

int
nsExpatDriver::HandleExternalEntityRef(const char16_t *openEntityNames,
                                       const char16_t *base,
                                       const char16_t *systemId,
                                       const char16_t *publicId)
{
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
    AppendUTF16toUTF8(publicId, message);
    message += "\" systemId \"";
    AppendUTF16toUTF8(systemId, message);
    message += "\" base \"";
    AppendUTF16toUTF8(base, message);
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
    XML_Parser entParser = XML_ExternalEntityParserCreate(mExpatParser, 0,
                                                          kUTF16);
    if (entParser) {
      XML_SetBase(entParser, absURL.get());

      mInExternalDTD = true;

      uint32_t totalRead;
      do {
        rv = uniIn->ReadSegments(ExternalDTDStreamReaderFunc, entParser,
                                 uint32_t(-1), &totalRead);
      } while (NS_SUCCEEDED(rv) && totalRead > 0);

      result = XML_Parse(entParser, nullptr, 0, 1);

      mInExternalDTD = false;

      XML_ParserFree(entParser);
    }
  }

  return result;
}

nsresult
nsExpatDriver::OpenInputStreamFromExternalDTD(const char16_t* aFPIStr,
                                              const char16_t* aURLStr,
                                              const char16_t* aBaseURL,
                                              nsIInputStream** aStream,
                                              nsAString& aAbsURL)
{
  nsCOMPtr<nsIURI> baseURI;
  nsresult rv = NS_NewURI(getter_AddRefs(baseURI),
                          NS_ConvertUTF16toUTF8(aBaseURL));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), NS_ConvertUTF16toUTF8(aURLStr), nullptr,
                 baseURI);
  NS_ENSURE_SUCCESS(rv, rv);

  // make sure the URI is allowed to be loaded in sync
  bool isUIResource = false;
  rv = NS_URIChainHasFlags(uri, nsIProtocolHandler::URI_IS_UI_RESOURCE,
                           &isUIResource);
  NS_ENSURE_SUCCESS(rv, rv);

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
    rv = NS_NewChannel(getter_AddRefs(channel),
                       uri,
                       nsContentUtils::GetSystemPrincipal(),
                       nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                       nsIContentPolicy::TYPE_DTD);
  }
  else {
    NS_ASSERTION(mSink == nsCOMPtr<nsIExpatSink>(do_QueryInterface(mOriginalSink)),
                 "In nsExpatDriver::OpenInputStreamFromExternalDTD: "
                 "mOriginalSink not the same object as mSink?");
    nsCOMPtr<nsIPrincipal> loadingPrincipal;
    if (mOriginalSink) {
      nsCOMPtr<nsIDocument> doc;
      doc = do_QueryInterface(mOriginalSink->GetTarget());
      if (doc) {
        loadingPrincipal = doc->NodePrincipal();
      }
    }
    if (!loadingPrincipal) {
      loadingPrincipal = NullPrincipal::CreateWithoutOriginAttributes();
    }
    rv = NS_NewChannel(getter_AddRefs(channel),
                       uri,
                       loadingPrincipal,
                       nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS |
                       nsILoadInfo::SEC_ALLOW_CHROME,
                       nsIContentPolicy::TYPE_DTD);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString absURL;
  rv = uri->GetSpec(absURL);
  NS_ENSURE_SUCCESS(rv, rv);
  CopyUTF8toUTF16(absURL, aAbsURL);

  channel->SetContentType(NS_LITERAL_CSTRING("application/xml"));
  return channel->Open2(aStream);
}

static nsresult
CreateErrorText(const char16_t* aDescription,
                const char16_t* aSourceURL,
                const uint32_t aLineNumber,
                const uint32_t aColNumber,
                nsString& aErrorString)
{
  aErrorString.Truncate();

  nsAutoString msg;
  nsresult rv =
    nsParserMsgUtils::GetLocalizedStringByName(XMLPARSER_PROPERTIES,
                                               "XMLParsingError", msg);
  NS_ENSURE_SUCCESS(rv, rv);

  // XML Parsing Error: %1$S\nLocation: %2$S\nLine Number %3$u, Column %4$u:
  nsTextFormatter::ssprintf(aErrorString, msg.get(), aDescription,
                            aSourceURL, aLineNumber, aColNumber);
  return NS_OK;
}

static nsresult
AppendErrorPointer(const int32_t aColNumber,
                   const char16_t *aSourceLine,
                   nsString& aSourceString)
{
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
    }
    else {
      aSourceString.Append(char16_t('-'));
      ++minuses;
    }
  }
  aSourceString.Append(char16_t('^'));

  return NS_OK;
}

nsresult
nsExpatDriver::HandleError()
{
  int32_t code = XML_GetErrorCode(mExpatParser);
  NS_ASSERTION(code > XML_ERROR_NONE, "unexpected XML error code");

  // Map Expat error code to an error string
  // XXX Deal with error returns.
  nsAutoString description;
  nsParserMsgUtils::GetLocalizedStringByID(XMLPARSER_PROPERTIES, code,
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
    const char16_t *mismatch = MOZ_XML_GetMismatchedTag(mExpatParser);
    const char16_t *uriEnd = nullptr;
    const char16_t *nameEnd = nullptr;
    const char16_t *pos;
    for (pos = mismatch; *pos; ++pos) {
      if (*pos == kExpatSeparatorChar) {
        if (uriEnd) {
          nameEnd = pos;
        }
        else {
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
    const char16_t *nameStart = uriEnd ? uriEnd + 1 : mismatch;
    tagName.Append(nameStart, (nameEnd ? nameEnd : pos) - nameStart);

    nsAutoString msg;
    nsParserMsgUtils::GetLocalizedStringByName(XMLPARSER_PROPERTIES,
                                               "Expected", msg);

    // . Expected: </%S>.
    nsAutoString message;
    nsTextFormatter::ssprintf(message, msg.get(), tagName.get());
    description.Append(message);
  }

  // Adjust the column number so that it is one based rather than zero based.
  uint32_t colNumber = XML_GetCurrentColumnNumber(mExpatParser) + 1;
  uint32_t lineNumber = XML_GetCurrentLineNumber(mExpatParser);

  nsAutoString errorText;
  CreateErrorText(description.get(), XML_GetBase(mExpatParser), lineNumber,
                  colNumber, errorText);

  NS_ASSERTION(mSink, "no sink?");

  nsAutoString sourceText(mLastLine);
  AppendErrorPointer(colNumber, mLastLine.get(), sourceText);

  // Try to create and initialize the script error.
  nsCOMPtr<nsIScriptError> serr(do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
  nsresult rv = NS_ERROR_FAILURE;
  if (serr) {
    rv = serr->InitWithWindowID(errorText,
                                mURISpec,
                                mLastLine,
                                lineNumber, colNumber,
                                nsIScriptError::errorFlag, "malformed-xml",
                                mInnerWindowID);
  }

  // If it didn't initialize, we can't do any logging.
  bool shouldReportError = NS_SUCCEEDED(rv);

  if (mSink && shouldReportError) {
    rv = mSink->ReportError(errorText.get(), 
                            sourceText.get(), 
                            serr, 
                            &shouldReportError);
    if (NS_FAILED(rv)) {
      shouldReportError = true;
    }
  }

  if (mOriginalSink) {
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(mOriginalSink->GetTarget());
    if (doc && doc->SuppressParserErrorConsoleMessages()) {
      shouldReportError = false;
    }
  }

  if (shouldReportError) {
    nsCOMPtr<nsIConsoleService> cs
      (do_GetService(NS_CONSOLESERVICE_CONTRACTID));  
    if (cs) {
      cs->LogMessage(serr);
    }
  }

  return NS_ERROR_HTMLPARSER_STOPPARSING;
}

void
nsExpatDriver::ParseBuffer(const char16_t *aBuffer,
                           uint32_t aLength,
                           bool aIsFinal,
                           uint32_t *aConsumed)
{
  NS_ASSERTION((aBuffer && aLength != 0) || (!aBuffer && aLength == 0), "?");
  NS_ASSERTION(mInternalState != NS_OK || aIsFinal || aBuffer,
               "Useless call, we won't call Expat");
  MOZ_ASSERT(!BlockedOrInterrupted() || !aBuffer,
             "Non-null buffer when resuming");
  MOZ_ASSERT(XML_GetCurrentByteIndex(mExpatParser) % sizeof(char16_t) == 0,
             "Consumed part of a char16_t?");

  if (mExpatParser && (mInternalState == NS_OK || BlockedOrInterrupted())) {
    int32_t parserBytesBefore = XML_GetCurrentByteIndex(mExpatParser);
    NS_ASSERTION(parserBytesBefore >= 0, "Unexpected value");

    XML_Status status;
    if (BlockedOrInterrupted()) {
      mInternalState = NS_OK; // Resume in case we're blocked.
      status = XML_ResumeParser(mExpatParser);
    }
    else {
      status = XML_Parse(mExpatParser,
                         reinterpret_cast<const char*>(aBuffer),
                         aLength * sizeof(char16_t), aIsFinal);
    }

    int32_t parserBytesConsumed = XML_GetCurrentByteIndex(mExpatParser);

    NS_ASSERTION(parserBytesConsumed >= 0, "Unexpected value");
    NS_ASSERTION(parserBytesConsumed >= parserBytesBefore,
                 "How'd this happen?");
    NS_ASSERTION(parserBytesConsumed % sizeof(char16_t) == 0,
                 "Consumed part of a char16_t?");

    // Consumed something.
    *aConsumed = (parserBytesConsumed - parserBytesBefore) / sizeof(char16_t);
    NS_ASSERTION(*aConsumed <= aLength + mExpatBuffered,
                 "Too many bytes consumed?");

    NS_ASSERTION(status != XML_STATUS_SUSPENDED || BlockedOrInterrupted(), 
                 "Inconsistent expat suspension state.");

    if (status == XML_STATUS_ERROR) {
      mInternalState = NS_ERROR_HTMLPARSER_STOPPARSING;
    }
  }
  else {
    *aConsumed = 0;
  }
}

NS_IMETHODIMP
nsExpatDriver::ConsumeToken(nsScanner& aScanner, bool& aFlushTokens)
{
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

    const char16_t *buffer;
    uint32_t length;
    if (blocked || noMoreBuffers) {
      // If we're blocked we just resume Expat so we don't need a buffer, if
      // there aren't any more buffers we pass a null buffer to Expat.
      buffer = nullptr;
      length = 0;

      if (blocked) {
        MOZ_LOG(gExpatDriverLog, LogLevel::Debug,
               ("Resuming Expat, will parse data remaining in Expat's "
                "buffer.\nContent of Expat's buffer:\n-----\n%s\n-----\n",
                NS_ConvertUTF16toUTF8(currentExpatPosition.get(),
                                      mExpatBuffered).get()));
      }
      else {
        NS_ASSERTION(mExpatBuffered == Distance(currentExpatPosition, end),
                     "Didn't pass all the data to Expat?");
        MOZ_LOG(gExpatDriverLog, LogLevel::Debug,
               ("Last call to Expat, will parse data remaining in Expat's "
                "buffer.\nContent of Expat's buffer:\n-----\n%s\n-----\n",
                NS_ConvertUTF16toUTF8(currentExpatPosition.get(),
                                      mExpatBuffered).get()));
      }
    }
    else {
      buffer = start.get();
      length = uint32_t(start.size_forward());

      MOZ_LOG(gExpatDriverLog, LogLevel::Debug,
             ("Calling Expat, will parse data remaining in Expat's buffer and "
              "new data.\nContent of Expat's buffer:\n-----\n%s\n-----\nNew "
              "data:\n-----\n%s\n-----\n",
              NS_ConvertUTF16toUTF8(currentExpatPosition.get(),
                                    mExpatBuffered).get(),
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
      XML_Size lastLineLength = XML_GetCurrentColumnNumber(mExpatParser);

      if (lastLineLength <= consumed) {
        // The length of the last line was less than what expat consumed, so
        // there was at least one line break in the consumed data. Store the
        // last line until the point where we stopped parsing.
        nsScannerIterator startLastLine = currentExpatPosition;
        startLastLine.advance(-((ptrdiff_t)lastLineLength));
        if (!CopyUnicodeTo(startLastLine, currentExpatPosition, mLastLine)) {
          return (mInternalState = NS_ERROR_OUT_OF_MEMORY);
        }
      }
      else {
        // There was no line break in the consumed data, append the consumed
        // data.
        if (!AppendUnicodeTo(oldExpatPosition,
                             currentExpatPosition,
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
      if (XML_GetErrorCode(mExpatParser) != XML_ERROR_NONE) {
        NS_ASSERTION(mInternalState == NS_ERROR_HTMLPARSER_STOPPARSING,
                     "Unexpected error");

        // Look for the next newline after the last one we consumed
        nsScannerIterator lastLine = currentExpatPosition;
        while (lastLine != end) {
          length = uint32_t(lastLine.size_forward());
          uint32_t endOffset = 0;
          const char16_t *buffer = lastLine.get();
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

NS_IMETHODIMP
nsExpatDriver::WillBuildModel(const CParserContext& aParserContext,
                              nsITokenizer* aTokenizer,
                              nsIContentSink* aSink)
{
  mSink = do_QueryInterface(aSink);
  if (!mSink) {
    NS_ERROR("nsExpatDriver didn't get an nsIExpatSink");
    // Make sure future calls to us bail out as needed
    mInternalState = NS_ERROR_UNEXPECTED;
    return mInternalState;
  }

  mOriginalSink = aSink;

  static const XML_Memory_Handling_Suite memsuite = {
    malloc,
    realloc,
    free
  };

  static const char16_t kExpatSeparator[] = { kExpatSeparatorChar, '\0' };

  mExpatParser = XML_ParserCreate_MM(kUTF16, &memsuite, kExpatSeparator);
  NS_ENSURE_TRUE(mExpatParser, NS_ERROR_FAILURE);

  XML_SetReturnNSTriplet(mExpatParser, XML_TRUE);

#ifdef XML_DTD
  XML_SetParamEntityParsing(mExpatParser, XML_PARAM_ENTITY_PARSING_ALWAYS);
#endif

  mURISpec = aParserContext.mScanner->GetFilename();

  XML_SetBase(mExpatParser, mURISpec.get());

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(mOriginalSink->GetTarget());
  if (doc) {
    nsCOMPtr<nsPIDOMWindowOuter> win = doc->GetWindow();
    nsCOMPtr<nsPIDOMWindowInner> inner;
    if (win) {
      inner = win->GetCurrentInnerWindow();
    } else {
      bool aHasHadScriptHandlingObject;
      nsIScriptGlobalObject *global =
        doc->GetScriptHandlingObject(aHasHadScriptHandlingObject);
      if (global) {
        inner = do_QueryInterface(global);
      }
    }
    if (inner) {
      mInnerWindowID = inner->WindowID();
    }
  }

  // Set up the callbacks
  XML_SetXmlDeclHandler(mExpatParser, Driver_HandleXMLDeclaration); 
  XML_SetElementHandler(mExpatParser, Driver_HandleStartElement,
                        Driver_HandleEndElement);
  XML_SetCharacterDataHandler(mExpatParser, Driver_HandleCharacterData);
  XML_SetProcessingInstructionHandler(mExpatParser,
                                      Driver_HandleProcessingInstruction);
  XML_SetDefaultHandlerExpand(mExpatParser, Driver_HandleDefault);
  XML_SetExternalEntityRefHandler(mExpatParser,
                                  (XML_ExternalEntityRefHandler)
                                          Driver_HandleExternalEntityRef);
  XML_SetExternalEntityRefHandlerArg(mExpatParser, this);
  XML_SetCommentHandler(mExpatParser, Driver_HandleComment);
  XML_SetCdataSectionHandler(mExpatParser, Driver_HandleStartCdataSection,
                             Driver_HandleEndCdataSection);

  XML_SetParamEntityParsing(mExpatParser,
                            XML_PARAM_ENTITY_PARSING_UNLESS_STANDALONE);
  XML_SetDoctypeDeclHandler(mExpatParser, Driver_HandleStartDoctypeDecl,
                            Driver_HandleEndDoctypeDecl);

  // Set up the user data.
  XML_SetUserData(mExpatParser, this);

  return mInternalState;
}

NS_IMETHODIMP
nsExpatDriver::BuildModel(nsITokenizer* aTokenizer, nsIContentSink* aSink)
{
  return mInternalState;
}

NS_IMETHODIMP
nsExpatDriver::DidBuildModel(nsresult anErrorCode)
{
  mOriginalSink = nullptr;
  mSink = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsExpatDriver::WillTokenize(bool aIsFinalChunk)
{
  mIsFinalChunk = aIsFinalChunk;
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsExpatDriver::Terminate()
{
  // XXX - not sure what happens to the unparsed data.
  if (mExpatParser) {
    XML_StopParser(mExpatParser, XML_FALSE);
  }
  mInternalState = NS_ERROR_HTMLPARSER_STOPPARSING;
}

NS_IMETHODIMP_(int32_t)
nsExpatDriver::GetType()
{
  return NS_IPARSER_FLAG_XML;
}

NS_IMETHODIMP_(nsDTDMode)
nsExpatDriver::GetMode() const
{
  return eDTDMode_full_standards;
}

/*************************** Unused methods **********************************/

NS_IMETHODIMP_(bool)
nsExpatDriver::IsContainer(int32_t aTag) const
{
  return true;
}

NS_IMETHODIMP_(bool)
nsExpatDriver::CanContain(int32_t aParent,int32_t aChild) const
{
  return true;
}

void
nsExpatDriver::MaybeStopParser(nsresult aState)
{
  if (NS_FAILED(aState)) {
    // If we had a failure we want to override NS_ERROR_HTMLPARSER_INTERRUPTED
    // and we want to override NS_ERROR_HTMLPARSER_BLOCK but not with
    // NS_ERROR_HTMLPARSER_INTERRUPTED.
    if (NS_SUCCEEDED(mInternalState) ||
        mInternalState == NS_ERROR_HTMLPARSER_INTERRUPTED ||
        (mInternalState == NS_ERROR_HTMLPARSER_BLOCK &&
         aState != NS_ERROR_HTMLPARSER_INTERRUPTED)) {
      mInternalState = (aState == NS_ERROR_HTMLPARSER_INTERRUPTED ||
                        aState == NS_ERROR_HTMLPARSER_BLOCK) ?
                       aState :
                       NS_ERROR_HTMLPARSER_STOPPARSING;
    }

    // If we get an error then we need to stop Expat (by calling XML_StopParser
    // with false as the last argument). If the parser should be blocked or
    // interrupted we need to pause Expat (by calling XML_StopParser with
    // true as the last argument).
    XML_StopParser(mExpatParser, BlockedOrInterrupted());
  }
  else if (NS_SUCCEEDED(mInternalState)) {
    // Only clobber mInternalState with the success code if we didn't block or
    // interrupt before.
    mInternalState = aState;
  }
}
