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

static const char kWhitespace[] = " \r\n\t"; // Optimized for typical cases
static const PRUnichar kUTF16[] = { 'U', 'T', 'F', '-', '1', '6', '\0' };

/***************************** EXPAT CALL BACKS ******************************/
// The callback handlers that get called from the expat parser.

PR_STATIC_CALLBACK(void)
Driver_HandleStartElement(void *aUserData,
                          const XML_Char *aName,
                          const XML_Char **aAtts)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    NS_STATIC_CAST(nsExpatDriver*, aUserData)->HandleStartElement((const PRUnichar*)aName,
                                                                  (const PRUnichar**)aAtts);
  }
}

PR_STATIC_CALLBACK(void)
Driver_HandleEndElement(void *aUserData,
                        const XML_Char *aName)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    NS_STATIC_CAST(nsExpatDriver*, aUserData)->HandleEndElement((const PRUnichar*)aName);
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
    driver->HandleCharacterData((const PRUnichar*)aData, PRUint32(aLength));
  }
}

PR_STATIC_CALLBACK(void)
Driver_HandleComment(void *aUserData,
                     const XML_Char *aName)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if(aUserData) {
    NS_STATIC_CAST(nsExpatDriver*, aUserData)->HandleComment((const PRUnichar*)aName);
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
    driver->HandleProcessingInstruction((const PRUnichar*)aTarget, (const PRUnichar*)aData);
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
    driver->HandleDefault((const PRUnichar*)aData, PRUint32(aLength));
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
                              const XML_Char *aDoctypeName)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    NS_STATIC_CAST(nsExpatDriver*, aUserData)->HandleStartDoctypeDecl();
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

  return driver->HandleExternalEntityRef((const PRUnichar*)aOpenEntityNames,
                                         (const PRUnichar*)aBase,
                                         (const PRUnichar*)aSystemId,
                                         (const PRUnichar*)aPublicId);
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
  { "-//W3C//DTD SVG 20001102//EN",              "svg.dtd",     nsnull },
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
    mInDoctype(PR_FALSE),
    mInExternalDTD(PR_FALSE),
    mHandledXMLDeclaration(PR_FALSE),
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
    XML_BlockParser(mExpatParser);
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

  if (mInDoctype) {
    if (!mInExternalDTD) {
      mDoctypeText.Append(aValue);
    }
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

  if (mSink &&
      mSink->HandleProcessingInstruction(aTarget, aData) ==
      NS_ERROR_HTMLPARSER_BLOCK) {
    mInternalState = NS_ERROR_HTMLPARSER_BLOCK;
    XML_BlockParser(mExpatParser);
  }

  return NS_OK;
}

nsresult
nsExpatDriver::HandleXMLDeclaration(const PRUnichar *aValue,
                                    const PRUint32 aLength)
{
  mHandledXMLDeclaration = PR_TRUE;

  // <?xml version='a'?>
  // 0123456789012345678
  // ?> can start at position 17 at the earliest
  PRUint32 i;
  for (i = 17; i < aLength; ++i) {
    if (aValue[i] == '?')
      break;
  }

  // +1 because index starts from 0
  // +1 because '>' follows '?'
  i += 2;

  if (i > aLength) {
    return NS_OK; // Bad declaration
  }

  return mSink->HandleXMLDeclaration(aValue, i);
}

nsresult
nsExpatDriver::HandleDefault(const PRUnichar *aValue,
                             const PRUint32 aLength)
{
  NS_ASSERTION(mSink, "content sink not found!");

  if (mInDoctype) {
    if (!mInExternalDTD) {
      mDoctypeText.Append(aValue, aLength);
    }
  }
  else if (mSink) {
    if (!mHandledXMLDeclaration && !mBytesParsed) {
      static const PRUnichar xmlDecl[] = { '<', '?', 'x', 'm', 'l', ' ', '\0' };
      // strlen("<?xml version='a'?>") == 19, shortest decl
      if (aLength >= 19 && nsCRT::strncmp(aValue, xmlDecl, 6) == 0) {
        HandleXMLDeclaration(aValue, aLength);
      }
    }

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

/**
 * DOCTYPE declaration is covered with very strict rules, which
 * makes our life here simpler because the XML parser has already
 * detected errors. The only slightly problematic case is whitespace
 * between the tokens. There MUST be whitespace between the tokens
 * EXCEPT right before > and [.
 *
 * We assume the string will not contain the ending '>'.
 */
static void
GetDocTypeToken(nsString& aStr, nsString& aToken, PRBool aQuotedString)
{
  // If we don't do this we must look ahead before Cut() and adjust the cut
  // amount.
  aStr.Trim(kWhitespace, PR_TRUE, PR_FALSE);
  if (aQuotedString) {
    PRInt32 endQuote = aStr.FindChar(aStr[0], 1);
    aStr.Mid(aToken, 1, endQuote - 1);
    aStr.Cut(0, endQuote + 1);
  }
  else {
    static const char* kDelimiter = " [\r\n\t"; // Optimized for typical cases
    PRInt32 tokenEnd = aStr.FindCharInSet(kDelimiter);
    if (tokenEnd < 0) {
      tokenEnd = aStr.Length();
    }
    if (tokenEnd > 0) {
      aStr.Left(aToken, tokenEnd);
      aStr.Cut(0, tokenEnd);
    }
  }
}

nsresult
nsExpatDriver::HandleStartDoctypeDecl()
{
  mInDoctype = PR_TRUE;
  // Consuming a huge DOCTYPE translates to numerous
  // allocations. In an effort to avoid too many allocations
  // setting mDoctypeText's capacity to be 1K ( just a guesstimate! ).
  mDoctypeText.SetCapacity(1024);

  return NS_OK;
}

nsresult
nsExpatDriver::HandleEndDoctypeDecl()
{
  NS_ASSERTION(mSink, "content sink not found!");

  mInDoctype = PR_FALSE;

  if (mSink) {
    // let the sink know any additional knowledge that we have about the
    // document (currently, from bug 124570, we only expect to pass additional
    // agent sheets needed to layout the XML vocabulary of the document)
    nsCOMPtr<nsIURI> data;
    if (mCatalogData && mCatalogData->mAgentSheet) {
      NS_NewURI(getter_AddRefs(data), mCatalogData->mAgentSheet);
    }

    nsAutoString name;
    GetDocTypeToken(mDoctypeText, name, PR_FALSE);

    nsAutoString token, publicId, systemId;
    GetDocTypeToken(mDoctypeText, token, PR_FALSE);
    if (token.EqualsLiteral("PUBLIC")) {
      GetDocTypeToken(mDoctypeText, publicId, PR_TRUE);
      GetDocTypeToken(mDoctypeText, systemId, PR_TRUE);
    }
    else if (token.EqualsLiteral("SYSTEM")) {
      GetDocTypeToken(mDoctypeText, systemId, PR_TRUE);
    }

    // The rest is the internal subset with [] (minus whitespace)
    mDoctypeText.Trim(kWhitespace);

    // Take out the brackets too, if any
    if (mDoctypeText.Length() > 2) {
      const nsAString& internalSubset = Substring(mDoctypeText, 1,
                                                  mDoctypeText.Length() - 2);
      mInternalState = mSink->HandleDoctypeDecl(internalSubset, name, systemId,
                                                publicId, data);
    } else {
      // There's nothing but brackets, don't include them
      mInternalState = mSink->HandleDoctypeDecl(EmptyString(), name, systemId,
                                                publicId, data);
    }
  }

  mDoctypeText.SetCapacity(0);

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
  // Pass the buffer to expat for parsing. XML_Parse returns 0 for
  // fatal errors.
  if (XML_Parse((XML_Parser)aClosure, (char *)aFromSegment,
                aCount * sizeof(PRUnichar), 0)) {
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
  if (mInDoctype && !mInExternalDTD && openEntityNames) {
    mDoctypeText.Append(PRUnichar('%'));
    mDoctypeText.Append(nsDependentString(openEntityNames));
    mDoctypeText.Append(PRUnichar(';'));
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
    XML_Parser entParser =
      XML_ExternalEntityParserCreate(mExpatParser, 0, (const XML_Char*)kUTF16);
    if (entParser) {
      XML_SetBase(entParser, (const XML_Char*)absURL.get());

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
CreateSourceText(const PRInt32 aColNumber,
                 const PRUnichar *aSourceLine,
                 nsString& aSourceString)
{
  aSourceString.Append(aSourceLine);
  aSourceString.Append(PRUnichar('\n'));

  PRInt32 i;
  for (i = aColNumber - 1; i > 0; --i) {
    aSourceString.Append(PRUnichar('-'));
  }
  aSourceString.Append(PRUnichar('^'));

  return NS_OK;
}

nsresult
nsExpatDriver::HandleError(const char *aBuffer,
                           PRUint32 aLength,
                           PRBool aIsFinal)
{
  PRInt32 code = XML_GetErrorCode(mExpatParser);
  NS_WARN_IF_FALSE(code > XML_ERROR_NONE, "unexpected XML error code");

  // Map Expat error code to an error string
  // XXX Deal with error returns.
  nsAutoString description;
  nsParserMsgUtils::GetLocalizedStringByID(XMLPARSER_PROPERTIES, code,
                                           description);

  if (code == XML_ERROR_TAG_MISMATCH) {
    nsAutoString msg;
    nsParserMsgUtils::GetLocalizedStringByName(XMLPARSER_PROPERTIES,
                                               "Expected", msg);
    // . Expected: </%S>.
    PRUnichar *message =
      nsTextFormatter::smprintf(msg.get(),
                                MOZ_XML_GetMismatchedTag(mExpatParser));
    if (!message) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    description.Append(message);

    nsTextFormatter::smprintf_free(message);
  }

  nsAutoString sourceLine;
  if (!aIsFinal) {
    GetLine(aBuffer, aLength,
            XML_GetCurrentByteIndex(mExpatParser) - mBytesParsed,
            sourceLine);
  }
  else {
    sourceLine.Append(mLastLine);
  }

  // Adjust the column number so that it is one based rather than zero based.
  PRInt32 colNumber = XML_GetCurrentColumnNumber(mExpatParser) + 1;

  nsAutoString errorText;
  CreateErrorText(description.get(), (const PRUnichar*)XML_GetBase(mExpatParser),
                  XML_GetCurrentLineNumber(mExpatParser),
                  colNumber, errorText);

  nsAutoString sourceText;
  CreateSourceText(colNumber, sourceLine.get(), sourceText);

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

  if (mExpatParser && mInternalState == NS_OK) {
    if (!XML_Parse(mExpatParser, aBuffer, aLength, aIsFinal)) {
      if (mInternalState == NS_ERROR_HTMLPARSER_BLOCK ||
          mInternalState == NS_ERROR_HTMLPARSER_STOPPARSING) {
        mBytePosition = (XML_GetCurrentByteIndex(mExpatParser) - mBytesParsed);
        mBytesParsed += mBytePosition;
      }
      else {
        HandleError(aBuffer, aLength, aIsFinal);
        mInternalState = NS_ERROR_HTMLPARSER_STOPPARSING;
      }

      return mInternalState;
    }

    if (aBuffer && aLength != 0) {
      // Cache the last line in the buffer
      GetLine(aBuffer, aLength, aLength - sizeof(PRUnichar), mLastLine);
    }

    mBytesParsed += aLength;
    mBytePosition = 0;
  }

  return NS_OK;
}

void
nsExpatDriver::GetLine(const char* aSourceBuffer,
                       PRUint32 aLength,
                       PRUint32 aOffset,
                       nsString& aLine)
{
  // Figure out the line inside aSourceBuffer that contains character specified
  // by aOffset. Copy it into aLine.
  NS_ASSERTION(aOffset < aLength, "?");
  // Assert that the byteIndex and the length of the buffer are even.
  NS_ASSERTION(aOffset % 2 == 0 && aLength % 2 == 0, "?");

  // Will try to find the start of the line.
  PRUnichar* start = (PRUnichar*)&aSourceBuffer[aOffset];

  // Will try to find the end of the line.
  PRUnichar* end = (PRUnichar*)&aSourceBuffer[aOffset];

  // Track the position of the 'start' pointer into the buffer.
  PRUint32 startIndex = aOffset / sizeof(PRUnichar);

  // Track the position of the 'end' pointer into the buffer.
  PRUint32 endIndex = aOffset / sizeof(PRUnichar);

  PRUint32 numCharsInBuffer = aLength / sizeof(PRUnichar);

  // Use start to find the first new line before the error position and end to
  // find the first new line after the error position.
  PRBool reachedStart = startIndex <= 0 || *start == '\n' || *start == '\r';
  PRBool reachedEnd = endIndex >= numCharsInBuffer || *end == '\n' ||
                      *end == '\r';
  while (!reachedStart || !reachedEnd) {
    if (!reachedStart) {
      --start;
      --startIndex;
      reachedStart = startIndex <= 0 || *start == '\n' || *start == '\r';
    }
    if (!reachedEnd) {
      ++end;
      ++endIndex;
      reachedEnd = endIndex >= numCharsInBuffer || *end == '\n' ||
                   *end == '\r';
    }
  }

  aLine.Truncate(0);
  if (startIndex == endIndex) {
    // Special case if the error is on a line where the only character is a
    // newline. Do nothing.
  }
  else {
    NS_ASSERTION(endIndex - startIndex >= sizeof(PRUnichar), "?");
    // At this point, there are two cases.  Either the error is on the first
    // line or on subsequent lines.  If the error is on the first line,
    // startIndex will decrement all the way to zero.  If not, startIndex
    // will decrement to the position of the newline character on the
    // previous line.  So, in the first case, the start position of the
    // error line = startIndex (== 0).  In the second case, the start
    // position of the error line = startIndex + 1.  In both cases, the end
    // position of the error line will be (endIndex - 1).
    PRUint32 startPosn = startIndex <= 0 ? startIndex : startIndex + 1;

    // At this point the substring starting at startPosn and ending at
    // (endIndex - 1) is the line on which the error occurred. Copy that
    // substring into the error structure.
    const PRUnichar* unicodeBuffer = (const PRUnichar*)aSourceBuffer;
    aLine.Append(&unicodeBuffer[startPosn], endIndex - startPosn);
  }
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

  mInternalState = NS_OK; // Resume in case we're blocked.
  XML_UnblockParser(mExpatParser);

  nsScannerIterator start, end;
  aScanner.CurrentPosition(start);
  aScanner.EndReading(end);

  while (start != end) {
    PRUint32 fragLength = PRUint32(start.size_forward());

    mInternalState = ParseBuffer((const char*)start.get(),
                                 fragLength * sizeof(PRUnichar),
                                 aFlushTokens);

    if (NS_FAILED(mInternalState)) {
      if (mInternalState == NS_ERROR_HTMLPARSER_BLOCK) {
        // mBytePosition / 2 => character position, one char is two bytes.
        aScanner.SetPosition(start.advance(mBytePosition / 2), PR_TRUE);
        aScanner.Mark();
      }

      return mInternalState;
    }

    start.advance(fragLength);
  }

  aScanner.SetPosition(end, PR_TRUE);

  if (NS_SUCCEEDED(mInternalState)) {
    return aScanner.Eof();
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
  NS_ENSURE_ARG_POINTER(aSink);

  mSink = do_QueryInterface(aSink);
  NS_ENSURE_TRUE(mSink, NS_ERROR_FAILURE);

  mExpatParser = XML_ParserCreate((const XML_Char*)kUTF16);
  NS_ENSURE_TRUE(mExpatParser, NS_ERROR_FAILURE);

#ifdef XML_DTD
  XML_SetParamEntityParsing(mExpatParser, XML_PARAM_ENTITY_PARSING_ALWAYS);
#endif

  XML_SetBase(mExpatParser,
              (const XML_Char*)aParserContext.mScanner->GetFilename().get());

  // Set up the callbacks
  XML_SetElementHandler(mExpatParser, Driver_HandleStartElement,
                        Driver_HandleEndElement);
  XML_SetCharacterDataHandler(mExpatParser, Driver_HandleCharacterData);
  XML_SetProcessingInstructionHandler(mExpatParser,
                                      Driver_HandleProcessingInstruction);
  XML_SetDefaultHandlerExpand(mExpatParser, Driver_HandleDefault);
  XML_SetExternalEntityRefHandler(mExpatParser,
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
  return ParseBuffer(nsnull, 0, aIsFinalChunk);
}

NS_IMETHODIMP_(const nsIID&)
nsExpatDriver::GetMostDerivedIID(void) const
{
  return NS_GET_IID(nsIDTD);
}

NS_IMETHODIMP_(void)
nsExpatDriver::Terminate()
{
  if (mExpatParser) {
    XML_BlockParser(mExpatParser); // XXX - not sure what happens to the unparsed data.
  }
  mInternalState = NS_ERROR_HTMLPARSER_STOPPARSING;
}

NS_IMETHODIMP_(PRInt32)
nsExpatDriver::GetType()
{
  return NS_IPARSER_FLAG_XML;
}

/*************************** Unused methods **********************************/

NS_IMETHODIMP
nsExpatDriver::CollectSkippedContent(PRInt32 aTag, nsAString& aContent,
                                     PRInt32 &aLineNo)
{
  return NS_OK;
}

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
nsExpatDriver::IsBlockElement(PRInt32 aTagID,PRInt32 aParentID) const
{
  return PR_FALSE;
}

NS_IMETHODIMP_(PRBool)
nsExpatDriver::IsInlineElement(PRInt32 aTagID,PRInt32 aParentID) const
{
  return PR_FALSE;
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

NS_IMETHODIMP
nsExpatDriver::StringTagToIntTag(const nsAString &aTag, PRInt32* aIntTag) const
{
  return NS_OK;
}

NS_IMETHODIMP_(const PRUnichar *)
nsExpatDriver::IntTagToStringTag(PRInt32 aIntTag) const
{
  return 0;
}

NS_IMETHODIMP_(nsIAtom *)
nsExpatDriver::IntTagToAtom(PRInt32 aIntTag) const
{
  return 0;
}
