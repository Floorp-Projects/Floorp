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
 * The Initial Developer of the Original Code is Robert Sayre.
 *
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsIInputStream.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsCharsetAlias.h"
#include "nsParserCIID.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsIScriptError.h"
#include "nsSAXAttributes.h"
#include "nsSAXLocator.h"
#include "nsSAXXMLReader.h"

#define XMLNS_URI "http://www.w3.org/2000/xmlns/"

static NS_DEFINE_CID(kParserCID, NS_PARSER_CID);

NS_IMPL_CYCLE_COLLECTION_CLASS(nsSAXXMLReader)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsSAXXMLReader)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mContentHandler)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDTDHandler)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mErrorHandler)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mLexicalHandler)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mBaseURI)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mParserObserver)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsSAXXMLReader)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mContentHandler)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDTDHandler)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mErrorHandler)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mLexicalHandler)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mBaseURI)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mParserObserver)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSAXXMLReader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSAXXMLReader)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSAXXMLReader)
  NS_INTERFACE_MAP_ENTRY(nsISAXXMLReader)
  NS_INTERFACE_MAP_ENTRY(nsIExpatSink)
  NS_INTERFACE_MAP_ENTRY(nsIExtendedExpatSink)
  NS_INTERFACE_MAP_ENTRY(nsIContentSink)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISAXXMLReader)
NS_INTERFACE_MAP_END

nsSAXXMLReader::nsSAXXMLReader() : mIsAsyncParse(false)
{
}

// nsIContentSink
NS_IMETHODIMP
nsSAXXMLReader::WillBuildModel(nsDTDMode)
{
  if (mContentHandler)
    return mContentHandler->StartDocument();

  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::DidBuildModel(bool aTerminated)
{
  if (mContentHandler)
    return mContentHandler->EndDocument();

  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::SetParser(nsParserBase *aParser)
{
  return NS_OK;
}

// nsIExtendedExpatSink
NS_IMETHODIMP
nsSAXXMLReader::HandleStartElement(const PRUnichar *aName,
                                   const PRUnichar **aAtts,
                                   PRUint32 aAttsCount,
                                   PRInt32 aIndex,
                                   PRUint32 aLineNumber)
{
  if (!mContentHandler)
    return NS_OK;

  nsCOMPtr<nsSAXAttributes> atts = new nsSAXAttributes();
  if (!atts)
    return NS_ERROR_OUT_OF_MEMORY;
  nsAutoString uri, localName, qName;
  for (; *aAtts; aAtts += 2) {
    SplitExpatName(aAtts[0], uri, localName, qName);
    // XXX don't have attr type information
    NS_NAMED_LITERAL_STRING(cdataType, "CDATA");
    // could support xmlns reporting, it's a standard SAX feature
    if (!uri.EqualsLiteral(XMLNS_URI)) {
      NS_ASSERTION(aAtts[1], "null passed to handler");
      atts->AddAttribute(uri, localName, qName, cdataType,
                         nsDependentString(aAtts[1]));
    }
  }

  // Deal with the element name
  SplitExpatName(aName, uri, localName, qName);
  return mContentHandler->StartElement(uri, localName, qName, atts);
}

NS_IMETHODIMP
nsSAXXMLReader::HandleEndElement(const PRUnichar *aName)
{
  if (mContentHandler) {
    nsAutoString uri, localName, qName;
    SplitExpatName(aName, uri, localName, qName);
    return mContentHandler->EndElement(uri, localName, qName);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::HandleComment(const PRUnichar *aName)
{
  NS_ASSERTION(aName, "null passed to handler");
  if (mLexicalHandler)
    return mLexicalHandler->Comment(nsDependentString(aName));
 
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::HandleCDataSection(const PRUnichar *aData,
                                   PRUint32 aLength)
{
  nsresult rv;
  if (mLexicalHandler) {
    rv = mLexicalHandler->StartCDATA();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mContentHandler) {
    rv = mContentHandler->Characters(Substring(aData, aData+aLength));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mLexicalHandler) {
    rv = mLexicalHandler->EndCDATA();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::HandleStartDTD(const PRUnichar *aName,
                               const PRUnichar *aSystemId,
                               const PRUnichar *aPublicId)
{
  PRUnichar nullChar = PRUnichar(0);
  if (!aName)
    aName = &nullChar;
  if (!aSystemId)
    aSystemId = &nullChar;
  if (!aPublicId)
    aPublicId = &nullChar;

  mSystemId = aSystemId;
  mPublicId = aPublicId;
  if (mLexicalHandler) {
    return mLexicalHandler->StartDTD(nsDependentString(aName),
                                     nsDependentString(aSystemId),
                                     nsDependentString(aPublicId));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::HandleDoctypeDecl(const nsAString & aSubset,
                                  const nsAString & aName,
                                  const nsAString & aSystemId,
                                  const nsAString & aPublicId,
                                  nsISupports* aCatalogData)
{
  if (mLexicalHandler)
    return mLexicalHandler->EndDTD();

  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::HandleCharacterData(const PRUnichar *aData,
                                    PRUint32 aLength)
{
  if (mContentHandler)
    return mContentHandler->Characters(Substring(aData, aData+aLength));

  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::HandleStartNamespaceDecl(const PRUnichar *aPrefix,
                                         const PRUnichar *aUri)
{
  if (!mContentHandler)
    return NS_OK;
  
  PRUnichar nullChar = PRUnichar(0);
  if (!aPrefix)
    aPrefix = &nullChar;
  if (!aUri)
    aUri = &nullChar;

  return mContentHandler->StartPrefixMapping(nsDependentString(aPrefix),
                                             nsDependentString(aUri));
}

NS_IMETHODIMP
nsSAXXMLReader::HandleEndNamespaceDecl(const PRUnichar *aPrefix)
{
  if (!mContentHandler)
    return NS_OK;
  
  if (aPrefix)
    return mContentHandler->EndPrefixMapping(nsDependentString(aPrefix));

  return mContentHandler->EndPrefixMapping(EmptyString());
}

NS_IMETHODIMP
nsSAXXMLReader::HandleProcessingInstruction(const PRUnichar *aTarget,
                                            const PRUnichar *aData)
{
  NS_ASSERTION(aTarget && aData, "null passed to handler");
  if (mContentHandler) {
    return mContentHandler->ProcessingInstruction(nsDependentString(aTarget),
                                                  nsDependentString(aData));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::HandleNotationDecl(const PRUnichar *aNotationName,
                                   const PRUnichar *aSystemId,
                                   const PRUnichar *aPublicId)
{
  NS_ASSERTION(aNotationName, "null passed to handler");
  if (mDTDHandler) {
    PRUnichar nullChar = PRUnichar(0);
    if (!aSystemId)
      aSystemId = &nullChar;
    if (!aPublicId)
      aPublicId = &nullChar;

    return mDTDHandler->NotationDecl(nsDependentString(aNotationName),
                                     nsDependentString(aSystemId),
                                     nsDependentString(aPublicId));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::HandleUnparsedEntityDecl(const PRUnichar *aEntityName,
                                         const PRUnichar *aSystemId,
                                         const PRUnichar *aPublicId,
                                         const PRUnichar *aNotationName)
{
  NS_ASSERTION(aEntityName && aNotationName, "null passed to handler");
  if (mDTDHandler) {
    PRUnichar nullChar = PRUnichar(0);
    if (!aSystemId)
      aSystemId = &nullChar;
    if (!aPublicId)
      aPublicId = &nullChar;

    return mDTDHandler->UnparsedEntityDecl(nsDependentString(aEntityName),
                                           nsDependentString(aSystemId),
                                           nsDependentString(aPublicId),
                                           nsDependentString(aNotationName));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::HandleXMLDeclaration(const PRUnichar *aVersion,
                                     const PRUnichar *aEncoding,
                                     PRInt32 aStandalone)
{
  // XXX need to decide what to do with this. It's a separate
  // optional interface in SAX.
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::ReportError(const PRUnichar* aErrorText,
                            const PRUnichar* aSourceText,
                            nsIScriptError *aError,
                            bool *_retval)
{
  NS_PRECONDITION(aError && aSourceText && aErrorText, "Check arguments!!!");
  // Normally, the expat driver should report the error.
  *_retval = true;

  if (mErrorHandler) {
    PRUint32 lineNumber;
    nsresult rv = aError->GetLineNumber(&lineNumber);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 columnNumber;
    rv = aError->GetColumnNumber(&columnNumber);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISAXLocator> locator = new nsSAXLocator(mPublicId,
                                                       mSystemId,
                                                       lineNumber,
                                                       columnNumber);
    if (!locator)
      return NS_ERROR_OUT_OF_MEMORY;

    rv = mErrorHandler->FatalError(locator, nsDependentString(aErrorText));
    if (NS_SUCCEEDED(rv)) {
      // The error handler has handled the script error.  Don't log to console.
      *_retval = false;
    }
  }

  return NS_OK;
}

// nsISAXXMLReader

NS_IMETHODIMP
nsSAXXMLReader::GetBaseURI(nsIURI **aBaseURI)
{
  NS_IF_ADDREF(*aBaseURI = mBaseURI);
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::SetBaseURI(nsIURI *aBaseURI)
{
  mBaseURI = aBaseURI;
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::GetContentHandler(nsISAXContentHandler **aContentHandler)
{
  NS_IF_ADDREF(*aContentHandler = mContentHandler);
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::SetContentHandler(nsISAXContentHandler *aContentHandler)
{
  mContentHandler = aContentHandler;
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::GetDtdHandler(nsISAXDTDHandler **aDtdHandler)
{
  NS_IF_ADDREF(*aDtdHandler = mDTDHandler);
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::SetDtdHandler(nsISAXDTDHandler *aDtdHandler)
{
  mDTDHandler = aDtdHandler;
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::GetErrorHandler(nsISAXErrorHandler **aErrorHandler)
{
  NS_IF_ADDREF(*aErrorHandler = mErrorHandler);
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::SetErrorHandler(nsISAXErrorHandler *aErrorHandler)
{
  mErrorHandler = aErrorHandler;
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::SetFeature(const nsAString &aName, bool aValue)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSAXXMLReader::GetFeature(const nsAString &aName, bool *aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSAXXMLReader::GetLexicalHandler(nsISAXLexicalHandler **aLexicalHandler)
{
  NS_IF_ADDREF(*aLexicalHandler = mLexicalHandler);
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::SetLexicalHandler(nsISAXLexicalHandler *aLexicalHandler)
{
  mLexicalHandler = aLexicalHandler;
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::SetProperty(const nsAString &aName, nsISupports* aValue)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSAXXMLReader::GetProperty(const nsAString &aName, bool *aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSAXXMLReader::ParseFromString(const nsAString &aStr,
                                const char *aContentType)
{
  // Don't call this in the middle of an async parse
  NS_ENSURE_TRUE(!mIsAsyncParse, NS_ERROR_FAILURE);

  NS_ConvertUTF16toUTF8 data(aStr);

  // The new stream holds a reference to the buffer
  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stream),
                                      data.get(), data.Length(),
                                      NS_ASSIGNMENT_DEPEND);
  NS_ENSURE_SUCCESS(rv, rv);
  return ParseFromStream(stream, "UTF-8", aContentType);
}

NS_IMETHODIMP
nsSAXXMLReader::ParseFromStream(nsIInputStream *aStream,
                                const char *aCharset,
                                const char *aContentType)
{
  // Don't call this in the middle of an async parse
  NS_ENSURE_TRUE(!mIsAsyncParse, NS_ERROR_FAILURE);

  NS_ENSURE_ARG(aStream);
  NS_ENSURE_ARG(aContentType);

  // Put the nsCOMPtr out here so we hold a ref to the stream as needed
  nsresult rv;
  nsCOMPtr<nsIInputStream> bufferedStream;
  if (!NS_InputStreamIsBuffered(aStream)) {
    rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream),
                                   aStream, 4096);
    NS_ENSURE_SUCCESS(rv, rv);
    aStream = bufferedStream;
  }
 
  rv = EnsureBaseURI();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> parserChannel;
  rv = NS_NewInputStreamChannel(getter_AddRefs(parserChannel), mBaseURI,
                                aStream, nsDependentCString(aContentType));
  if (!parserChannel || NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  if (aCharset)
    parserChannel->SetContentCharset(nsDependentCString(aCharset));

  rv = InitParser(nsnull, parserChannel);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mListener->OnStartRequest(parserChannel, nsnull);
  if (NS_FAILED(rv))
    parserChannel->Cancel(rv);

  /* When parsing a new document, we need to clear the XML identifiers.
     HandleStartDTD will set these values from the DTD declaration tag.
     We won't have them, of course, if there's a well-formedness error
     before the DTD tag (such as a space before an XML declaration).
   */
  mSystemId.Truncate();
  mPublicId.Truncate();

  nsresult status;
  parserChannel->GetStatus(&status);
  
  PRUint32 offset = 0;
  while (NS_SUCCEEDED(rv) && NS_SUCCEEDED(status)) {
    PRUint32 available;
    rv = aStream->Available(&available);
    if (rv == NS_BASE_STREAM_CLOSED) {
      rv = NS_OK;
      available = 0;
    }
    if (NS_FAILED(rv)) {
      parserChannel->Cancel(rv);
      break;
    }
    if (! available)
      break; // blocking input stream has none available when done

    rv = mListener->OnDataAvailable(parserChannel, nsnull,
                                    aStream, offset, available);
    if (NS_SUCCEEDED(rv))
      offset += available;
    else
      parserChannel->Cancel(rv);
    parserChannel->GetStatus(&status);
  }
  rv = mListener->OnStopRequest(parserChannel, nsnull, status);
  mListener = nsnull;

  return rv;
}

NS_IMETHODIMP
nsSAXXMLReader::ParseAsync(nsIRequestObserver *aObserver)
{
  mParserObserver = aObserver;
  mIsAsyncParse = true;
  return NS_OK;
}

// nsIRequestObserver

NS_IMETHODIMP
nsSAXXMLReader::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
{
  NS_ENSURE_TRUE(mIsAsyncParse, NS_ERROR_FAILURE);
  nsresult rv;
  rv = EnsureBaseURI();
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  rv = InitParser(mParserObserver, channel);
  NS_ENSURE_SUCCESS(rv, rv);
  // we don't need or want this anymore
  mParserObserver = nsnull;
  return mListener->OnStartRequest(aRequest, aContext);
}

NS_IMETHODIMP
nsSAXXMLReader::OnStopRequest(nsIRequest *aRequest, nsISupports *aContext,
                              nsresult status)
{
  NS_ENSURE_TRUE(mIsAsyncParse, NS_ERROR_FAILURE);
  NS_ENSURE_STATE(mListener);
  nsresult rv = mListener->OnStopRequest(aRequest, aContext, status);
  mListener = nsnull;
  mIsAsyncParse = false;
  return rv;
}

// nsIStreamListener

NS_IMETHODIMP
nsSAXXMLReader::OnDataAvailable(nsIRequest *aRequest, nsISupports *aContext,
                                nsIInputStream *aInputStream, PRUint32 offset,
                                PRUint32 count)
{
  NS_ENSURE_TRUE(mIsAsyncParse, NS_ERROR_FAILURE);
  NS_ENSURE_STATE(mListener);
  return mListener->OnDataAvailable(aRequest, aContext, aInputStream, offset,
                                    count);
}

nsresult
nsSAXXMLReader::InitParser(nsIRequestObserver *aObserver, nsIChannel *aChannel)
{
  nsresult rv;

  // setup the parser
  nsCOMPtr<nsIParser> parser = do_CreateInstance(kParserCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  parser->SetContentSink(this);

  PRInt32 charsetSource = kCharsetFromDocTypeDefault;
  nsCAutoString charset(NS_LITERAL_CSTRING("UTF-8"));
  TryChannelCharset(aChannel, charsetSource, charset);
  parser->SetDocumentCharset(charset, charsetSource);

  rv = parser->Parse(mBaseURI, aObserver);
  NS_ENSURE_SUCCESS(rv, rv);

  mListener = do_QueryInterface(parser, &rv);

  return rv;
}

// from nsDocument.cpp
bool
nsSAXXMLReader::TryChannelCharset(nsIChannel *aChannel,
                                  PRInt32& aCharsetSource,
                                  nsACString& aCharset)
{
  if (aCharsetSource >= kCharsetFromChannel)
    return true;
  
  if (aChannel) {
    nsCAutoString charsetVal;
    nsresult rv = aChannel->GetContentCharset(charsetVal);
    if (NS_SUCCEEDED(rv)) {
      if (NS_FAILED(nsCharsetAlias::GetPreferred(charsetVal, aCharset)))
        return false;

      aCharsetSource = kCharsetFromChannel;
      return true;
    }
  }

  return false;
}

nsresult
nsSAXXMLReader::EnsureBaseURI()
{
  if (mBaseURI) 
    return NS_OK;

  return NS_NewURI(getter_AddRefs(mBaseURI), "about:blank");
}

nsresult
nsSAXXMLReader::SplitExpatName(const PRUnichar *aExpatName,
                               nsString &aURI,
                               nsString &aLocalName,
                               nsString &aQName)
{
  /**
   * Adapted from RDFContentSinkImpl
   *
   * Expat can send the following:
   *    localName
   *    namespaceURI<separator>localName
   *    namespaceURI<separator>localName<separator>prefix
   *
   * and we use 0xFFFF for the <separator>.
   *
   */

  NS_ASSERTION(aExpatName, "null passed to handler");
  nsDependentString expatStr(aExpatName);
  PRInt32 break1, break2 = kNotFound;
  break1 = expatStr.FindChar(PRUnichar(0xFFFF));

  if (break1 == kNotFound) {
    aLocalName = expatStr; // no namespace
    aURI.Truncate();
    aQName = expatStr;
  } else {
    aURI = StringHead(expatStr, break1);
    break2 = expatStr.FindChar(PRUnichar(0xFFFF), break1 + 1);
    if (break2 == kNotFound) { // namespace, but no prefix
      aLocalName = Substring(expatStr, break1 + 1);
      aQName = aLocalName;
    } else { // namespace with prefix
      aLocalName = Substring(expatStr, break1 + 1, break2 - break1 - 1);
      aQName = Substring(expatStr, break2 + 1) +
        NS_LITERAL_STRING(":") + aLocalName;
    }
  }

  return NS_OK;
}
