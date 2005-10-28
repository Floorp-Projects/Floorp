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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsExpatDriver.h"
#include "nsIParser.h"
#include "nsCOMPtr.h"
#include "nsParserCIID.h"
#include "CParserContext.h"
#include "nsIExpatSink.h"
#include "nsIContentSink.h"
#include "nsParserMsgUtils.h"
#include "nsIURL.h"
#include "nsIUnicharInputStream.h"
#include "nsNetUtil.h"
#include "prprf.h"
#include "prmem.h"
#include "nsTextFormatter.h"
#include "nsDirectoryServiceDefs.h"
#include "nsCRT.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"

#define kExpatSeparatorChar 0xFFFF

static const PRUnichar kUTF16[] = { 'U', 'T', 'F', '-', '1', '6', '\0' };

/***************************** EXPAT CALL BACKS ******************************/
// The callback handlers that get called from the expat parser.

PR_STATIC_CALLBACK(void)
Driver_HandleXMLDeclaration(void *aUserData,
                            const XML_Char *aVersion,
                            const XML_Char *aEncoding,
                            int aStandalone)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    nsExpatDriver* driver = NS_STATIC_CAST(nsExpatDriver*, aUserData);
    driver->HandleXMLDeclaration(aVersion, aEncoding, aStandalone);
  }
}

PR_STATIC_CALLBACK(void)
Driver_HandleStartElement(void *aUserData,
                          const XML_Char *aName,
                          const XML_Char **aAtts)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    NS_STATIC_CAST(nsExpatDriver*, aUserData)->HandleStartElement(aName,
                                                                  aAtts);
  }
}

PR_STATIC_CALLBACK(void)
Driver_HandleEndElement(void *aUserData,
                        const XML_Char *aName)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    NS_STATIC_CAST(nsExpatDriver*, aUserData)->HandleEndElement(aName);
  }
}

PR_STATIC_CALLBACK(void)
Driver_HandleCharacterData(void *aUserData,
                           const XML_Char *aData,
                           int aLength)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    nsExpatDriver* driver = NS_STATIC_CAST(nsExpatDriver*, aUserData);
    driver->HandleCharacterData(aData, PRUint32(aLength));
  }
}

PR_STATIC_CALLBACK(void)
Driver_HandleComment(void *aUserData,
                     const XML_Char *aName)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if(aUserData) {
    NS_STATIC_CAST(nsExpatDriver*, aUserData)->HandleComment(aName);
  }
}

PR_STATIC_CALLBACK(void)
Driver_HandleProcessingInstruction(void *aUserData,
                                   const XML_Char *aTarget,
                                   const XML_Char *aData)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    nsExpatDriver* driver = NS_STATIC_CAST(nsExpatDriver*, aUserData);
    driver->HandleProcessingInstruction(aTarget, aData);
  }
}

PR_STATIC_CALLBACK(void)
Driver_HandleDefault(void *aUserData,
                     const XML_Char *aData,
                     int aLength)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    nsExpatDriver* driver = NS_STATIC_CAST(nsExpatDriver*, aUserData);
    driver->HandleDefault(aData, PRUint32(aLength));
  }
}

PR_STATIC_CALLBACK(void)
Driver_HandleStartCdataSection(void *aUserData)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    NS_STATIC_CAST(nsExpatDriver*, aUserData)->HandleStartCdataSection();
  }
}

PR_STATIC_CALLBACK(void)
Driver_HandleEndCdataSection(void *aUserData)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    NS_STATIC_CAST(nsExpatDriver*, aUserData)->HandleEndCdataSection();
  }
}

PR_STATIC_CALLBACK(void)
Driver_HandleStartDoctypeDecl(void *aUserData,
                              const XML_Char *aDoctypeName,
                              const XML_Char *aSysid,
                              const XML_Char *aPubid,
                              int aHasInternalSubset)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    NS_STATIC_CAST(nsExpatDriver*, aUserData)->
      HandleStartDoctypeDecl(aDoctypeName, aSysid, aPubid, aHasInternalSubset);
  }
}

PR_STATIC_CALLBACK(void)
Driver_HandleEndDoctypeDecl(void *aUserData)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    NS_STATIC_CAST(nsExpatDriver*, aUserData)->HandleEndDoctypeDecl();
  }
}

PR_STATIC_CALLBACK(int)
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

  nsExpatDriver* driver = NS_STATIC_CAST(nsExpatDriver*,
                                         aExternalEntityRefHandler);

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
  { "-//W3C//DTD XHTML 1.0 Transitional//EN",    "xhtml11.dtd", nsnull },
  { "-//W3C//DTD XHTML 1.1//EN",                 "xhtml11.dtd", nsnull },
  { "-//W3C//DTD XHTML 1.0 Strict//EN",          "xhtml11.dtd", nsnull },
  { "-//W3C//DTD XHTML 1.0 Frameset//EN",        "xhtml11.dtd", nsnull },
  { "-//W3C//DTD XHTML Basic 1.0//EN",           "xhtml11.dtd", nsnull },
  { "-//W3C//DTD XHTML 1.1 plus MathML 2.0//EN", "mathml.dtd",  "resource://gre/res/mathml.css" },
  { "-//W3C//DTD XHTML 1.1 plus MathML 2.0 plus SVG 1.1//EN", "mathml.dtd", "resource://gre/res/mathml.css" },
  { "-//W3C//DTD MathML 2.0//EN",                "mathml.dtd",  "resource://gre/res/mathml.css" },
  { "-//WAPFORUM//DTD XHTML Mobile 1.0//EN",     "xhtml11.dtd", nsnull },
  { nsnull, nsnull, nsnull }
};

static const nsCatalogData*
LookupCatalogData(const PRUnichar* aPublicID)
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

  return nsnull;
}

// aCatalogData can be null. If not null, it provides a hook to additional
// built-in knowledge on the resource that we are trying to load. Returns true
// if the local DTD specified in the catalog data exists or if the filename
// contained within the url exists in the special DTD directory. If either of
// this exists, aResult is set to the file: url that points to the DTD file
// found in the local DTD directory.
static PRBool
IsLoadableDTD(const nsCatalogData* aCatalogData, nsIURI* aDTD,
              nsIURI** aResult)
{
  NS_ASSERTION(aDTD, "Null parameter.");

  nsCAutoString fileName;
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
      return PR_FALSE;
    }

    dtdURL->GetFileName(fileName);
    if (fileName.IsEmpty()) {
      return PR_FALSE;
    }
  }

  nsCOMPtr<nsIFile> dtdPath;
  NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(dtdPath));
  if (!dtdPath) {
    return PR_FALSE;
  }

  nsCOMPtr<nsILocalFile> lfile = do_QueryInterface(dtdPath);

  // append res/dtd/<fileName>
  // can't do AppendRelativeNativePath("res/dtd/" + fileName)
  // as that won't work on all platforms.
  lfile->AppendNative(NS_LITERAL_CSTRING("res"));
  lfile->AppendNative(NS_LITERAL_CSTRING("dtd"));
  lfile->AppendNative(fileName);

  PRBool exists;
  dtdPath->Exists(&exists);
  if (!exists) {
    return PR_FALSE;
  }

  // The DTD was found in the local DTD directory.
  // Set aDTD to a file: url pointing to the local DTD
  NS_NewFileURI(aResult, dtdPath);

  return *aResult != nsnull;
}

/***************************** END CATALOG UTILS *****************************/

NS_IMPL_ISUPPORTS2(nsExpatDriver,
                   nsITokenizer,
                   nsIDTD)

nsresult
NS_NewExpatDriver(nsIDTD** aResult)
{
  *aResult = new nsExpatDriver();
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aResult);

  return NS_OK;
}

nsExpatDriver::nsExpatDriver()
  : mExpatParser(nsnull),
    mInCData(PR_FALSE),
    mInInternalSubset(PR_FALSE),
    mInExternalDTD(PR_FALSE),
    mBytePosition(0),
    mInternalState(NS_OK),
    mBytesParsed(0),
    mCatalogData(nsnull)
{
}

nsExpatDriver::~nsExpatDriver()
{
  if (mExpatParser) {
    XML_ParserFree(mExpatParser);
  }
}

nsresult
nsExpatDriver::HandleStartElement(const PRUnichar *aValue,
                                  const PRUnichar **aAtts)
{
  NS_ASSERTION(mSink, "content sink not found!");

  // Calculate the total number of elements in aAtts.
  // XML_GetSpecifiedAttributeCount will only give us the number of specified
  // attrs (twice that number, actually), so we have to check for default attrs
  // ourselves.
  PRUint32 attrArrayLength;
  for (attrArrayLength = XML_GetSpecifiedAttributeCount(mExpatParser);
       aAtts[attrArrayLength];
       attrArrayLength += 2) {
    // Just looping till we find out what the length is
  }

  if (mSink) {
    mSink->HandleStartElement(aValue, aAtts,
                              attrArrayLength,
                              XML_GetIdAttributeIndex(mExpatParser),
                              XML_GetCurrentLineNumber(mExpatParser));
  }

  return NS_OK;
}

nsresult
nsExpatDriver::HandleEndElement(const PRUnichar *aValue)
{
  NS_ASSERTION(mSink, "content sink not found!");

  if (mSink &&
      mSink->HandleEndElement(aValue) == NS_ERROR_HTMLPARSER_BLOCK) {
    mInternalState = NS_ERROR_HTMLPARSER_BLOCK;
    XML_StopParser(mExpatParser, XML_TRUE);
  }

  return NS_OK;
}

nsresult
nsExpatDriver::HandleCharacterData(const PRUnichar *aValue,
                                   const PRUint32 aLength)
{
  NS_ASSERTION(mSink, "content sink not found!");

  if (mInCData) {
    mCDataText.Append(aValue, aLength);
  }
  else if (mSink) {
    mInternalState = mSink->HandleCharacterData(aValue, aLength);
  }

  return NS_OK;
}

nsresult
nsExpatDriver::HandleComment(const PRUnichar *aValue)
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
    mInternalState = mSink->HandleComment(aValue);
  }

  return NS_OK;
}

nsresult
nsExpatDriver::HandleProcessingInstruction(const PRUnichar *aTarget,
                                           const PRUnichar *aData)
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
  else if (mSink &&
           mSink->HandleProcessingInstruction(aTarget, aData) ==
           NS_ERROR_HTMLPARSER_BLOCK) {
    mInternalState = NS_ERROR_HTMLPARSER_BLOCK;
    XML_StopParser(mExpatParser, XML_TRUE);
  }

  return NS_OK;
}

nsresult
nsExpatDriver::HandleXMLDeclaration(const PRUnichar *aVersion,
                                    const PRUnichar *aEncoding,
                                    PRInt32 aStandalone)
{
  return mSink->HandleXMLDeclaration(aVersion, aEncoding, aStandalone);
}

nsresult
nsExpatDriver::HandleDefault(const PRUnichar *aValue,
                             const PRUint32 aLength)
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
    static const PRUnichar newline[] = { '\n', '\0' };
    PRUint32 i;
    for (i = 0; i < aLength && NS_SUCCEEDED(mInternalState); ++i) {
      if (aValue[i] == '\n' || aValue[i] == '\r') {
        mInternalState = mSink->HandleCharacterData(newline, 1);
      }
    }
  }

  return NS_OK;
}

nsresult
nsExpatDriver::HandleStartCdataSection()
{
  mInCData = PR_TRUE;

  return NS_OK;
}

nsresult
nsExpatDriver::HandleEndCdataSection()
{
  NS_ASSERTION(mSink, "content sink not found!");

  mInCData = PR_FALSE;
  if (mSink) {
    mInternalState = mSink->HandleCDataSection(mCDataText.get(),
                                               mCDataText.Length());
  }
  mCDataText.Truncate();

  return NS_OK;
}

nsresult
nsExpatDriver::HandleStartDoctypeDecl(const PRUnichar* aDoctypeName,
                                      const PRUnichar* aSysid,
                                      const PRUnichar* aPubid,
                                      PRBool aHasInternalSubset)
{
  mDoctypeName = aDoctypeName;
  mSystemID = aSysid;
  mPublicID = aPubid;

  if (aHasInternalSubset) {
    // Consuming a huge internal subset translates to numerous
    // allocations. In an effort to avoid too many allocations
    // setting mInternalSubset's capacity to be 1K ( just a guesstimate! ).
    mInInternalSubset = PR_TRUE;
    mInternalSubset.SetCapacity(1024);
  }

  return NS_OK;
}

nsresult
nsExpatDriver::HandleEndDoctypeDecl()
{
  NS_ASSERTION(mSink, "content sink not found!");

  mInInternalSubset = PR_FALSE;

  if (mSink) {
    // let the sink know any additional knowledge that we have about the
    // document (currently, from bug 124570, we only expect to pass additional
    // agent sheets needed to layout the XML vocabulary of the document)
    nsCOMPtr<nsIURI> data;
    if (mCatalogData && mCatalogData->mAgentSheet) {
      NS_NewURI(getter_AddRefs(data), mCatalogData->mAgentSheet);
    }

    // Note: mInternalSubset already doesn't include the [] around it.
    mInternalState = mSink->HandleDoctypeDecl(mInternalSubset, mDoctypeName,
                                              mSystemID, mPublicID, data);
    
  }
  
  mInternalSubset.SetCapacity(0);

  return NS_OK;
}

static NS_METHOD
ExternalDTDStreamReaderFunc(nsIUnicharInputStream* aIn,
                            void* aClosure,
                            const PRUnichar* aFromSegment,
                            PRUint32 aToOffset,
                            PRUint32 aCount,
                            PRUint32 *aWriteCount)
{
  // Pass the buffer to expat for parsing.
  if (XML_Parse((XML_Parser)aClosure, (const char *)aFromSegment,
                aCount * sizeof(PRUnichar), 0) == XML_STATUS_OK) {
    *aWriteCount = aCount;

    return NS_OK;
  }

  *aWriteCount = 0;

  return NS_ERROR_FAILURE;
}

int
nsExpatDriver::HandleExternalEntityRef(const PRUnichar *openEntityNames,
                                       const PRUnichar *base,
                                       const PRUnichar *systemId,
                                       const PRUnichar *publicId)
{
  if (mInInternalSubset && !mInExternalDTD && openEntityNames) {
    mInternalSubset.Append(PRUnichar('%'));
    mInternalSubset.Append(nsDependentString(openEntityNames));
    mInternalSubset.Append(PRUnichar(';'));
  }

  // Load the external entity into a buffer.
  nsCOMPtr<nsIInputStream> in;
  nsAutoString absURL;
  nsresult rv = OpenInputStreamFromExternalDTD(publicId, systemId, base,
                                               getter_AddRefs(in), absURL);
  NS_ENSURE_SUCCESS(rv, 1);

  nsCOMPtr<nsIUnicharInputStream> uniIn;
  rv = NS_NewUTF8ConverterStream(getter_AddRefs(uniIn), in, 1024);
  NS_ENSURE_SUCCESS(rv, 1);

  int result = 1;
  if (uniIn) {
    XML_Parser entParser = XML_ExternalEntityParserCreate(mExpatParser, 0,
                                                          kUTF16);
    if (entParser) {
      XML_SetBase(entParser, absURL.get());

      mInExternalDTD = PR_TRUE;

      PRUint32 totalRead;
      do {
        rv = uniIn->ReadSegments(ExternalDTDStreamReaderFunc, entParser,
                                 PRUint32(-1), &totalRead);
      } while (NS_SUCCEEDED(rv) && totalRead > 0);

      result = XML_Parse(entParser, nsnull, 0, 1);

      mInExternalDTD = PR_FALSE;

      XML_ParserFree(entParser);
    }
  }

  return result;
}

nsresult
nsExpatDriver::OpenInputStreamFromExternalDTD(const PRUnichar* aFPIStr,
                                              const PRUnichar* aURLStr,
                                              const PRUnichar* aBaseURL,
                                              nsIInputStream** aStream,
                                              nsAString& aAbsURL)
{
  nsCOMPtr<nsIURI> baseURI;
  nsresult rv = NS_NewURI(getter_AddRefs(baseURI),
                          NS_ConvertUTF16toUTF8(aBaseURL));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), NS_ConvertUTF16toUTF8(aURLStr), nsnull,
                 baseURI);
  NS_ENSURE_SUCCESS(rv, rv);

  // check if it is alright to load this uri
  PRBool isChrome = PR_FALSE;
  uri->SchemeIs("chrome", &isChrome);
  if (!isChrome) {
    // since the url is not a chrome url, check to see if we can map the DTD
    // to a known local DTD, or if a DTD file of the same name exists in the
    // special DTD directory
    if (aFPIStr) {
      // see if the Formal Public Identifier (FPI) maps to a catalog entry
      mCatalogData = LookupCatalogData(aFPIStr);
    }

    nsCOMPtr<nsIURI> localURI;
    if (!IsLoadableDTD(mCatalogData, uri, getter_AddRefs(localURI))) {
      return NS_ERROR_NOT_IMPLEMENTED;
    }

    localURI.swap(uri);
  }

  rv = NS_OpenURI(aStream, uri);

  nsCAutoString absURL;
  uri->GetSpec(absURL);

  CopyUTF8toUTF16(absURL, aAbsURL);

  return rv;
}

static nsresult
CreateErrorText(const PRUnichar* aDescription,
                const PRUnichar* aSourceURL,
                const PRInt32 aLineNumber,
                const PRInt32 aColNumber,
                nsString& aErrorString)
{
  aErrorString.Truncate();

  nsAutoString msg;
  nsresult rv =
    nsParserMsgUtils::GetLocalizedStringByName(XMLPARSER_PROPERTIES,
                                               "XMLParsingError", msg);
  NS_ENSURE_SUCCESS(rv, rv);

  // XML Parsing Error: %1$S\nLocation: %2$S\nLine Number %3$d, Column %4$d:
  PRUnichar *message = nsTextFormatter::smprintf(msg.get(), aDescription,
                                                 aSourceURL, aLineNumber,
                                                 aColNumber);
  if (!message) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  aErrorString.Assign(message);
  nsTextFormatter::smprintf_free(message);

  return NS_OK;
}

static nsresult
AppendErrorPointer(const PRInt32 aColNumber,
                   const PRUnichar *aSourceLine,
                   nsString& aSourceString)
{
  aSourceString.Append(PRUnichar('\n'));

  // Last character will be '^'.
  PRInt32 last = aColNumber - 1;
  PRInt32 i;
  PRUint32 minuses = 0;
  for (i = 0; i < last; ++i) {
    if (aSourceLine[i] == '\t') {
      // Since this uses |white-space: pre;| a tab stop equals 8 spaces.
      PRUint32 add = 8 - (minuses % 8);
      aSourceString.AppendASCII("--------", add);
      minuses += add;
    }
    else {
      aSourceString.Append(PRUnichar('-'));
      ++minuses;
    }
  }
  aSourceString.Append(PRUnichar('^'));

  return NS_OK;
}

nsresult
nsExpatDriver::HandleError()
{
  PRInt32 code = XML_GetErrorCode(mExpatParser);
  NS_WARN_IF_FALSE(code > XML_ERROR_NONE, "unexpected XML error code");

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
    const PRUnichar *mismatch = MOZ_XML_GetMismatchedTag(mExpatParser);
    const PRUnichar *uriEnd = nsnull;
    const PRUnichar *nameEnd = nsnull;
    const PRUnichar *pos;
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
      tagName.Append(PRUnichar(':'));
    }
    const PRUnichar *nameStart = uriEnd ? uriEnd + 1 : mismatch;
    tagName.Append(nameStart, (nameEnd ? nameEnd : pos) - nameStart);
    
    nsAutoString msg;
    nsParserMsgUtils::GetLocalizedStringByName(XMLPARSER_PROPERTIES,
                                               "Expected", msg);

    // . Expected: </%S>.
    PRUnichar *message = nsTextFormatter::smprintf(msg.get(), tagName.get());
    if (!message) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    description.Append(message);

    nsTextFormatter::smprintf_free(message);
  }

  // Adjust the column number so that it is one based rather than zero based.
  PRInt32 colNumber = XML_GetCurrentColumnNumber(mExpatParser) + 1;
  PRInt32 lineNumber = XML_GetCurrentLineNumber(mExpatParser);

  nsAutoString errorText;
  CreateErrorText(description.get(), XML_GetBase(mExpatParser), lineNumber,
                  colNumber, errorText);

  nsCOMPtr<nsIConsoleService> cs
    (do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  nsCOMPtr<nsIScriptError> serr(do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
  if (serr && cs) {
    if (NS_SUCCEEDED(serr->Init(description.get(),
                                mURISpec.get(),
                                mLastLine.get(),
                                lineNumber, colNumber,
                                nsIScriptError::errorFlag, "malformed-xml")))
      cs->LogMessage(serr);
  }

  nsAutoString sourceText(mLastLine);
  AppendErrorPointer(colNumber, mLastLine.get(), sourceText);

  NS_ASSERTION(mSink, "no sink?");
  if (mSink) {
    mSink->ReportError(errorText.get(), sourceText.get());
  }

  return NS_ERROR_HTMLPARSER_STOPPARSING;
}

nsresult
nsExpatDriver::ParseBuffer(const char* aBuffer,
                           PRUint32 aLength,
                           PRBool aIsFinal)
{
  NS_ASSERTION((aBuffer && aLength != 0) || (!aBuffer && aLength == 0), "?");
  NS_ASSERTION(aLength % sizeof(PRUnichar) == 0,
               "We can have a PRUnichar spanning chunks?");
  NS_PRECONDITION(mBytesParsed % sizeof(PRUnichar) == 0,
                  "Parsed part of a PRUnichar?");
  NS_PRECONDITION(mBytePosition % sizeof(PRUnichar) == 0,
                  "Parsed part of a PRUnichar?");
  NS_PRECONDITION(XML_GetCurrentByteIndex(mExpatParser) == -1 ||
                  XML_GetCurrentByteIndex(mExpatParser) % sizeof(PRUnichar) == 0,
                  "Consumed part of a PRUnichar?");

  if (mExpatParser && (mInternalState == NS_OK ||
                       mInternalState == NS_ERROR_HTMLPARSER_BLOCK)) {
    XML_Status status;
    if (mInternalState == NS_ERROR_HTMLPARSER_BLOCK) {
      mInternalState = NS_OK; // Resume in case we're blocked.
      status = XML_ResumeParser(mExpatParser);
    }
    else {
      status = XML_Parse(mExpatParser, aBuffer, aLength, aIsFinal);
    }

    PRInt32 parserBytesConsumed = XML_GetCurrentByteIndex(mExpatParser);

    NS_ASSERTION(parserBytesConsumed == -1 ||
                 parserBytesConsumed % sizeof(PRUnichar) == 0,
                 "Consumed part of a PRUnichar?");

    // Now figure out the startOffset for appending to mLastLine -- this
    // calculation is the same no matter whether we saw an error, got blocked,
    // parsed it all but didn't consume it all, or whatever else happened.
    const PRUnichar* const buffer =
      NS_REINTERPRET_CAST(const PRUnichar*, aBuffer);
    PRUint32 startOffset;
    // If expat failed to consume some bytes last time it won't consume them
    // this time without also consuming some bytes from aBuffer. Note that when
    // resuming after suspending the parser, aBuffer will contain the data that
    // Expat also has in its internal buffer.
    // Note that if expat consumed everything we passed it, it will have nulled
    // out all its internal pointers to data, so parserBytesConsumed will come
    // out -1 in that case.
    if (buffer) {
      if (parserBytesConsumed < 0 ||
          (PRUint32)parserBytesConsumed >= mBytesParsed) {
        // We consumed something
        if (parserBytesConsumed < 0) {
          NS_ASSERTION(parserBytesConsumed == -1,
                       "Unexpected negative value?");
          // Consumed everything.
          startOffset = aLength;
        }
        else {
          // Consumed something, but not all
          NS_ASSERTION(parserBytesConsumed - mBytesParsed <= aLength,
                       "Too many bytes consumed?");
          startOffset = (parserBytesConsumed - mBytesParsed) /
                        sizeof(PRUnichar);
        }
        // Now startOffset points one past the last PRUnichar consumed in
        // buffer
        while (startOffset-- != 0) {
          if (buffer[startOffset] == '\n' || buffer[startOffset] == '\r') {
            mLastLine.Truncate();
            break;
          }
        }
        // Now startOffset is pointing to the last newline we consumed (or to
        // -1 if there weren't any consumed in this chunk).  Make startOffset
        // point to the first char of the first line whose end we haven't
        // consumed yet.
        ++startOffset;
      }
      else {
        // Didn't parse anything new.  So append all of aBuffer.
        startOffset = 0;
      }
    }

    if (status == XML_STATUS_SUSPENDED) {
      NS_ASSERTION(mInternalState == NS_ERROR_HTMLPARSER_BLOCK,
                   "mInternalState out of sync!");
      NS_ASSERTION((PRUint32)parserBytesConsumed >= mBytesParsed,
                   "How'd this happen?");
      mBytePosition = parserBytesConsumed - mBytesParsed;
      mBytesParsed = parserBytesConsumed;
      if (buffer) {
        PRUint32 endOffset = mBytePosition / sizeof(PRUnichar);
        NS_ASSERTION(startOffset <= endOffset,
                     "Something is confused about what we've consumed");
        // Only append the data we actually parsed.  The rest will come
        // through this method again.
        mLastLine.Append(Substring(buffer + startOffset, buffer + endOffset));
      }

      return mInternalState;
    }

    PRUint32 length = aLength / sizeof(PRUnichar);
    if (status == XML_STATUS_ERROR) {
      // Look for the next newline after the last one we consumed
      if (buffer) {
        PRUint32 endOffset = startOffset;
        while (endOffset < length && buffer[endOffset] != '\n' &&
               buffer[endOffset] != '\r') {
          ++endOffset;
        }
        mLastLine.Append(Substring(buffer + startOffset, buffer + endOffset));
      }
      HandleError();
      mInternalState = NS_ERROR_HTMLPARSER_STOPPARSING;

      return mInternalState;
    }

    if (!aIsFinal && buffer) {
      mLastLine.Append(Substring(buffer + startOffset, buffer + length));
    }
    mBytesParsed += aLength;
    mBytePosition = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsExpatDriver::CreateNewInstance(nsIDTD** aInstancePtrResult)
{
  return NS_NewExpatDriver(aInstancePtrResult);
}

NS_IMETHODIMP
nsExpatDriver::ConsumeToken(nsScanner& aScanner,
                            PRBool& aFlushTokens)
{
  // Ask the scanner to send us all the data it has
  // scanned and pass that data to expat.

  nsScannerIterator start, end;
  aScanner.CurrentPosition(start);
  aScanner.EndReading(end);

  while (start != end) {
    PRUint32 fragLength = PRUint32(start.size_forward());

    mInternalState = ParseBuffer((const char*)start.get(),
                                 fragLength * sizeof(PRUnichar),
                                 PR_FALSE);

    if (NS_FAILED(mInternalState)) {
      if (mInternalState == NS_ERROR_HTMLPARSER_BLOCK) {
        aScanner.SetPosition(start.advance(mBytePosition / sizeof(PRUnichar)),
                             PR_TRUE);
        aScanner.Mark();
      }

      return mInternalState;
    }

    start.advance(fragLength);
  }

  aScanner.SetPosition(end, PR_TRUE);

  if (NS_SUCCEEDED(mInternalState)) {
    return aScanner.FillBuffer();
  }

  return NS_OK;
}

NS_IMETHODIMP_(eAutoDetectResult)
nsExpatDriver::CanParse(CParserContext& aParserContext)
{
  NS_ASSERTION(!aParserContext.mMimeType.IsEmpty(),
               "How'd we get here with an unknown type?");
  
  if (eViewSource != aParserContext.mParserCommand &&
      aParserContext.mDocType == eXML) {
    // The parser context already looked at the MIME type for us
  
    return ePrimaryDetect;
  }

  return eUnknownDetect;
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

  static const XML_Memory_Handling_Suite memsuite =
    {
      (void *(*)(size_t))PR_Malloc,
      (void *(*)(void *, size_t))PR_Realloc,
      PR_Free
    };

  static const PRUnichar kExpatSeparator[] = { kExpatSeparatorChar, '\0' };

  mExpatParser = XML_ParserCreate_MM(kUTF16, &memsuite, kExpatSeparator);
  NS_ENSURE_TRUE(mExpatParser, NS_ERROR_FAILURE);

  XML_SetReturnNSTriplet(mExpatParser, XML_TRUE);

#ifdef XML_DTD
  XML_SetParamEntityParsing(mExpatParser, XML_PARAM_ENTITY_PARSING_ALWAYS);
#endif

  mURISpec = aParserContext.mScanner->GetFilename();

  XML_SetBase(mExpatParser, mURISpec.get());

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

  return aSink->WillBuildModel();
}

NS_IMETHODIMP
nsExpatDriver::BuildModel(nsIParser* aParser,
                          nsITokenizer* aTokenizer,
                          nsITokenObserver* anObserver,
                          nsIContentSink* aSink)
{
  return mInternalState;
}

NS_IMETHODIMP
nsExpatDriver::DidBuildModel(nsresult anErrorCode,
                             PRBool aNotifySink,
                             nsIParser* aParser,
                             nsIContentSink* aSink)
{
  // Check for mSink is intentional. This would make sure
  // that DidBuildModel() is called only once on the sink.
  nsresult result = NS_OK;
  if (mSink) {
    result = aSink->DidBuildModel();
    mSink = nsnull;
  }

  return result;
}

NS_IMETHODIMP
nsExpatDriver::WillTokenize(PRBool aIsFinalChunk,
                            nsTokenAllocator* aTokenAllocator)
{
  return NS_OK;
}

NS_IMETHODIMP
nsExpatDriver::WillResumeParse(nsIContentSink* aSink)
{
  return aSink ? aSink->WillResume() : NS_OK;
}

NS_IMETHODIMP
nsExpatDriver::WillInterruptParse(nsIContentSink* aSink)
{
  return aSink ? aSink->WillInterrupt() : NS_OK;
}

NS_IMETHODIMP
nsExpatDriver::DidTokenize(PRBool aIsFinalChunk)
{
  return mInternalState == NS_OK ? ParseBuffer(nsnull, 0, aIsFinalChunk) :
                                   NS_OK;
}

NS_IMETHODIMP_(const nsIID&)
nsExpatDriver::GetMostDerivedIID(void) const
{
  return NS_GET_IID(nsIDTD);
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

NS_IMETHODIMP_(PRInt32)
nsExpatDriver::GetType()
{
  return NS_IPARSER_FLAG_XML;
}

/*************************** Unused methods **********************************/

NS_IMETHODIMP_(CToken*)
nsExpatDriver::PushTokenFront(CToken* aToken)
{
  return 0;
}

NS_IMETHODIMP_(CToken*)
nsExpatDriver::PushToken(CToken* aToken)
{
  return 0;
}

NS_IMETHODIMP_(CToken*)
nsExpatDriver::PopToken(void)
{
  return 0;
}

NS_IMETHODIMP_(CToken*)
nsExpatDriver::PeekToken(void)
{
  return 0;
}

NS_IMETHODIMP_(CToken*)
nsExpatDriver::GetTokenAt(PRInt32 anIndex)
{
  return 0;
}

NS_IMETHODIMP_(PRInt32)
nsExpatDriver::GetCount(void)
{
  return 0;
}

NS_IMETHODIMP_(nsTokenAllocator*)
nsExpatDriver::GetTokenAllocator(void)
{
  return 0;
}

NS_IMETHODIMP_(void)
nsExpatDriver::PrependTokens(nsDeque& aDeque)
{
}

NS_IMETHODIMP
nsExpatDriver::CopyState(nsITokenizer* aTokenizer)
{
  return NS_OK;
}

NS_IMETHODIMP
nsExpatDriver::HandleToken(CToken* aToken,nsIParser* aParser)
{
  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
nsExpatDriver::IsContainer(PRInt32 aTag) const
{
  return PR_TRUE;
}

NS_IMETHODIMP_(PRBool)
nsExpatDriver::CanContain(PRInt32 aParent,PRInt32 aChild) const
{
  return PR_TRUE;
}
