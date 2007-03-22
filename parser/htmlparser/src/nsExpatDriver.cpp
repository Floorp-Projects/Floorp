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
 *   Henri Sivonen <hsivonen@iki.fi>
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
#include "nsIExtendedExpatSink.h"
#include "nsIContentSink.h"
#include "nsParserMsgUtils.h"
#include "nsIURL.h"
#include "nsIUnicharInputStream.h"
#include "nsISimpleUnicharStreamFactory.h"
#include "nsNetUtil.h"
#include "prprf.h"
#include "prmem.h"
#include "nsTextFormatter.h"
#include "nsDirectoryServiceDefs.h"
#include "nsCRT.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsXPCOMCIDInternal.h"
#include "nsUnicharInputStream.h"

#define kExpatSeparatorChar 0xFFFF

static const PRUnichar kUTF16[] = { 'U', 'T', 'F', '-', '1', '6', '\0' };

#ifdef PR_LOGGING
static PRLogModuleInfo *gExpatDriverLog = PR_NewLogModule("expatdriver");
#endif

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

PR_STATIC_CALLBACK(void)
Driver_HandleStartNamespaceDecl(void *aUserData,
                                const XML_Char *aPrefix,
                                const XML_Char *aUri)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    NS_STATIC_CAST(nsExpatDriver*, aUserData)->
      HandleStartNamespaceDecl(aPrefix, aUri);
  }
}

PR_STATIC_CALLBACK(void)
Driver_HandleEndNamespaceDecl(void *aUserData,
                              const XML_Char *aPrefix)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    NS_STATIC_CAST(nsExpatDriver*, aUserData)->
      HandleEndNamespaceDecl(aPrefix);
  }
}

PR_STATIC_CALLBACK(void)
Driver_HandleNotationDecl(void *aUserData,
                          const XML_Char *aNotationName,
                          const XML_Char *aBase,
                          const XML_Char *aSysid,
                          const XML_Char *aPubid)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    NS_STATIC_CAST(nsExpatDriver*, aUserData)->
      HandleNotationDecl(aNotationName, aBase, aSysid, aPubid);
  }
}

PR_STATIC_CALLBACK(void)
Driver_HandleUnparsedEntityDecl(void *aUserData,
                                const XML_Char *aEntityName,
                                const XML_Char *aBase,
                                const XML_Char *aSysid,
                                const XML_Char *aPubid,
                                const XML_Char *aNotationName)
{
  NS_ASSERTION(aUserData, "expat driver should exist");
  if (aUserData) {
    NS_STATIC_CAST(nsExpatDriver*, aUserData)->
      HandleUnparsedEntityDecl(aEntityName, aBase, aSysid, aPubid,
                               aNotationName);
  }
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

nsExpatDriver::nsExpatDriver()
  : mExpatParser(nsnull),
    mInCData(PR_FALSE),
    mInInternalSubset(PR_FALSE),
    mInExternalDTD(PR_FALSE),
    mIsFinalChunk(PR_FALSE),
    mInternalState(NS_OK),
    mExpatBuffered(0),
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
    mInternalState = mSink->
      HandleStartElement(aValue, aAtts, attrArrayLength,
                         XML_GetIdAttributeIndex(mExpatParser),
                         XML_GetCurrentLineNumber(mExpatParser));
    MaybeStopParser();
  }

  return NS_OK;
}

nsresult
nsExpatDriver::HandleEndElement(const PRUnichar *aValue)
{
  NS_ASSERTION(mSink, "content sink not found!");

  if (mSink) {
    mInternalState = mSink->HandleEndElement(aValue);
    MaybeStopParser();
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
    MaybeStopParser();
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
    MaybeStopParser();
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
  else if (mSink) {
    mInternalState = mSink->HandleProcessingInstruction(aTarget, aData);
    MaybeStopParser();
  }

  return NS_OK;
}

nsresult
nsExpatDriver::HandleXMLDeclaration(const PRUnichar *aVersion,
                                    const PRUnichar *aEncoding,
                                    PRInt32 aStandalone)
{
  if (mSink) {
    mInternalState = mSink->HandleXMLDeclaration(aVersion, aEncoding, aStandalone);
    MaybeStopParser();
  }

  return NS_OK;
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
    // The line breaks should not be normalized twice. See bug 343870.
    MaybeStopParser();
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
    MaybeStopParser();
  }
  mCDataText.Truncate();

  return NS_OK;
}

nsresult
nsExpatDriver::HandleStartNamespaceDecl(const PRUnichar* aPrefix,
                                        const PRUnichar* aUri)
{
  if (mExtendedSink) {
    mInternalState = mExtendedSink->HandleStartNamespaceDecl(aPrefix,
                                                            aUri);
  }
  return NS_OK;
}

nsresult
nsExpatDriver::HandleEndNamespaceDecl(const PRUnichar* aPrefix)
{
  if (mExtendedSink) {
    mInternalState = mExtendedSink->HandleEndNamespaceDecl(aPrefix);
  }
  return NS_OK;
}

nsresult
nsExpatDriver::HandleNotationDecl(const PRUnichar* aNotationName,
                                  const PRUnichar* aBase,
                                  const PRUnichar* aSysid,
                                  const PRUnichar* aPubid)
{
  if (mExtendedSink) {
    mInternalState = mExtendedSink->HandleNotationDecl(aNotationName,
                                                       aSysid,
                                                       aPubid);
  }
  return NS_OK;
}

nsresult
nsExpatDriver::HandleUnparsedEntityDecl(const PRUnichar* aEntityName,
                                        const PRUnichar* aBase,
                                        const PRUnichar* aSysid,
                                        const PRUnichar* aPubid,
                                        const PRUnichar* aNotationName)
{
  if (mExtendedSink) {
    mInternalState = mExtendedSink->HandleUnparsedEntityDecl(aEntityName,
                                                             aSysid,
                                                             aPubid,
                                                             aNotationName);
  }
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

  if (mExtendedSink) {
    mInternalState = mExtendedSink->HandleStartDTD(aDoctypeName,
                                                   aSysid, aPubid);
  }

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
    MaybeStopParser();
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
  rv = nsSimpleUnicharStreamFactory::GetInstance()->
    CreateInstanceFromUTF8Stream(in, getter_AddRefs(uniIn));
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
                const PRUint32 aLineNumber,
                const PRUint32 aColNumber,
                nsString& aErrorString)
{
  aErrorString.Truncate();

  nsAutoString msg;
  nsresult rv =
    nsParserMsgUtils::GetLocalizedStringByName(XMLPARSER_PROPERTIES,
                                               "XMLParsingError", msg);
  NS_ENSURE_SUCCESS(rv, rv);

  // XML Parsing Error: %1$S\nLocation: %2$S\nLine Number %3$u, Column %4$u:
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
  PRUint32 colNumber = XML_GetCurrentColumnNumber(mExpatParser) + 1;
  PRUint32 lineNumber = XML_GetCurrentLineNumber(mExpatParser);

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
    rv = serr->Init(description.get(),
                    mURISpec.get(),
                    mLastLine.get(),
                    lineNumber, colNumber,
                    nsIScriptError::errorFlag, "malformed-xml");
  }

  // If it didn't initialize, we can't do any logging.
  PRBool shouldReportError = NS_SUCCEEDED(rv);

  if (mSink && shouldReportError) {
    rv = mSink->ReportError(errorText.get(), 
                            sourceText.get(), 
                            serr, 
                            &shouldReportError);
    if (NS_FAILED(rv)) {
      shouldReportError = PR_TRUE;
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
nsExpatDriver::ParseBuffer(const PRUnichar *aBuffer,
                           PRUint32 aLength,
                           PRBool aIsFinal,
                           PRUint32 *aConsumed)
{
  NS_ASSERTION((aBuffer && aLength != 0) || (!aBuffer && aLength == 0), "?");
  NS_ASSERTION(mInternalState != NS_OK || aIsFinal || aBuffer,
               "Useless call, we won't call Expat");
  NS_PRECONDITION(!BlockedOrInterrupted() || !aBuffer,
                  "Non-null buffer when resuming");
  NS_PRECONDITION(XML_GetCurrentByteIndex(mExpatParser) % sizeof(PRUnichar) == 0,
                  "Consumed part of a PRUnichar?");

  if (mExpatParser && (mInternalState == NS_OK || BlockedOrInterrupted())) {
    PRInt32 parserBytesBefore = XML_GetCurrentByteIndex(mExpatParser);
    NS_ASSERTION(parserBytesBefore >= 0, "Unexpected value");

    XML_Status status;
    if (BlockedOrInterrupted()) {
      mInternalState = NS_OK; // Resume in case we're blocked.
      status = XML_ResumeParser(mExpatParser);
    }
    else {
      status = XML_Parse(mExpatParser,
                         NS_REINTERPRET_CAST(const char*, aBuffer),
                         aLength * sizeof(PRUnichar), aIsFinal);
    }

    PRInt32 parserBytesConsumed = XML_GetCurrentByteIndex(mExpatParser);

    NS_ASSERTION(parserBytesConsumed >= 0, "Unexpected value");
    NS_ASSERTION(parserBytesConsumed >= parserBytesBefore,
                 "How'd this happen?");
    NS_ASSERTION(parserBytesConsumed % sizeof(PRUnichar) == 0,
                 "Consumed part of a PRUnichar?");

    // Consumed something.
    *aConsumed = (parserBytesConsumed - parserBytesBefore) / sizeof(PRUnichar);
    NS_ASSERTION(*aConsumed <= aLength + mExpatBuffered,
                 "Too many bytes consumed?");

    NS_ASSERTION(status != XML_STATUS_SUSPENDED || 
                 (mInternalState == NS_ERROR_HTMLPARSER_BLOCK || 
                  mInternalState == NS_ERROR_HTMLPARSER_INTERRUPTED), 
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
nsExpatDriver::ConsumeToken(nsScanner& aScanner, PRBool& aFlushTokens)
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

  PR_LOG(gExpatDriverLog, PR_LOG_DEBUG,
         ("Remaining in expat's buffer: %i, remaining in scanner: %i.",
          mExpatBuffered, Distance(start, end)));

  PRBool flush = mIsFinalChunk;

  // We want to call Expat if we have more buffers, or if we know there won't
  // be more buffers (and so we want to flush the remaining data), or if we're
  // currently blocked and there's data in Expat's buffer.
  while (start != end || flush ||
         (BlockedOrInterrupted() && mExpatBuffered > 0)) {
    PRBool noMoreBuffers = start == end && mIsFinalChunk;
    PRBool blocked = BlockedOrInterrupted();

    // If we're resuming and we know there won't be more data we want to
    // flush the remaining data after we resumed the parser (so loop once
    // more).
    flush = blocked && noMoreBuffers;

    const PRUnichar *buffer;
    PRUint32 length;
    if (blocked || noMoreBuffers) {
      // If we're blocked we just resume Expat so we don't need a buffer, if
      // there aren't any more buffers we pass a null buffer to Expat.
      buffer = nsnull;
      length = 0;

      if (blocked) {
        PR_LOG(gExpatDriverLog, PR_LOG_DEBUG,
               ("Resuming Expat, will parse data remaining in Expat's "
                "buffer.\nContent of Expat's buffer:\n-----\n%s\n-----\n",
                NS_ConvertUTF16toUTF8(currentExpatPosition.get(),
                                      mExpatBuffered).get()));
      }
      else {
        NS_ASSERTION(mExpatBuffered == Distance(currentExpatPosition, end),
                     "Didn't pass all the data to Expat?");
        PR_LOG(gExpatDriverLog, PR_LOG_DEBUG,
               ("Last call to Expat, will parse data remaining in Expat's "
                "buffer.\nContent of Expat's buffer:\n-----\n%s\n-----\n",
                NS_ConvertUTF16toUTF8(currentExpatPosition.get(),
                                      mExpatBuffered).get()));
      }
    }
    else {
      buffer = start.get();
      length = PRUint32(start.size_forward());

      PR_LOG(gExpatDriverLog, PR_LOG_DEBUG,
             ("Calling Expat, will parse data remaining in Expat's buffer and "
              "new data.\nContent of Expat's buffer:\n-----\n%s\n-----\nNew "
              "data:\n-----\n%s\n-----\n",
              NS_ConvertUTF16toUTF8(currentExpatPosition.get(),
                                    mExpatBuffered).get(),
              NS_ConvertUTF16toUTF8(start.get(), length).get()));
    }

    PRUint32 consumed;
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
        CopyUnicodeTo(startLastLine, currentExpatPosition, mLastLine);
      }
      else {
        // There was no line break in the consumed data, append the consumed
        // data.
        AppendUnicodeTo(oldExpatPosition, currentExpatPosition, mLastLine);
      }
    }

    mExpatBuffered += length - consumed;

    if (BlockedOrInterrupted()) {
      PR_LOG(gExpatDriverLog, PR_LOG_DEBUG,
             ("Blocked or interrupted parser (probably for loading linked "
              "stylesheets or scripts)."));

      aScanner.SetPosition(currentExpatPosition, PR_TRUE);
      aScanner.Mark();

      return mInternalState;
    }

    if (NS_FAILED(mInternalState)) {
      if (XML_GetErrorCode(mExpatParser) != XML_ERROR_NONE) {
        NS_ASSERTION(mInternalState == NS_ERROR_HTMLPARSER_STOPPARSING,
                     "Unexpected error");

        // Look for the next newline after the last one we consumed
        nsScannerIterator lastLine = currentExpatPosition;
        while (lastLine != end) {
          length = PRUint32(lastLine.size_forward());
          PRUint32 endOffset = 0;
          const PRUnichar *buffer = lastLine.get();
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
  }

  aScanner.SetPosition(currentExpatPosition, PR_TRUE);
  aScanner.Mark();

  PR_LOG(gExpatDriverLog, PR_LOG_DEBUG,
         ("Remaining in expat's buffer: %i, remaining in scanner: %i.",
          mExpatBuffered, Distance(currentExpatPosition, end)));

  return NS_SUCCEEDED(mInternalState) ? aScanner.FillBuffer() : NS_OK;
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

  // If the sink is an nsIExtendedExpatSink,
  // register some addtional handlers.
  mExtendedSink = do_QueryInterface(mSink);
  if (mExtendedSink) {
    XML_SetNamespaceDeclHandler(mExpatParser,
                                Driver_HandleStartNamespaceDecl,
                                Driver_HandleEndNamespaceDecl);
    XML_SetUnparsedEntityDeclHandler(mExpatParser,
                                     Driver_HandleUnparsedEntityDecl);
    XML_SetNotationDeclHandler(mExpatParser,
                               Driver_HandleNotationDecl);
  }

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

  mExtendedSink = nsnull;

  return result;
}

NS_IMETHODIMP
nsExpatDriver::WillTokenize(PRBool aIsFinalChunk,
                            nsTokenAllocator* aTokenAllocator)
{
  mIsFinalChunk = aIsFinalChunk;
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

void
nsExpatDriver::MaybeStopParser()
{
  if (BlockedOrInterrupted()) {
    XML_StopParser(mExpatParser, XML_TRUE);
  }
}
