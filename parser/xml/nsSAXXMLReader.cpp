/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIInputStream.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsNullPrincipal.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsIScriptError.h"
#include "nsSAXAttributes.h"
#include "nsSAXLocator.h"
#include "nsSAXXMLReader.h"
#include "nsCharsetSource.h"

#include "mozilla/dom/EncodingUtils.h"

using mozilla::dom::EncodingUtils;

#define XMLNS_URI "http://www.w3.org/2000/xmlns/"

static NS_DEFINE_CID(kParserCID, NS_PARSER_CID);

NS_IMPL_CYCLE_COLLECTION(nsSAXXMLReader,
                         mContentHandler,
                         mDTDHandler,
                         mErrorHandler,
                         mLexicalHandler,
                         mDeclarationHandler,
                         mBaseURI,
                         mListener,
                         mParserObserver)
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

nsSAXXMLReader::nsSAXXMLReader() :
    mIsAsyncParse(false),
    mEnableNamespacePrefixes(false)
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
nsSAXXMLReader::HandleStartElement(const char16_t *aName,
                                   const char16_t **aAtts,
                                   uint32_t aAttsCount,
                                   uint32_t aLineNumber)
{
  if (!mContentHandler)
    return NS_OK;

  RefPtr<nsSAXAttributes> atts = new nsSAXAttributes();
  if (!atts)
    return NS_ERROR_OUT_OF_MEMORY;
  nsAutoString uri, localName, qName;
  for (; *aAtts; aAtts += 2) {
    SplitExpatName(aAtts[0], uri, localName, qName);
    // XXX don't have attr type information
    NS_NAMED_LITERAL_STRING(cdataType, "CDATA");
    // could support xmlns reporting, it's a standard SAX feature
    if (mEnableNamespacePrefixes || !uri.EqualsLiteral(XMLNS_URI)) {
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
nsSAXXMLReader::HandleEndElement(const char16_t *aName)
{
  if (mContentHandler) {
    nsAutoString uri, localName, qName;
    SplitExpatName(aName, uri, localName, qName);
    return mContentHandler->EndElement(uri, localName, qName);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::HandleComment(const char16_t *aName)
{
  NS_ASSERTION(aName, "null passed to handler");
  if (mLexicalHandler)
    return mLexicalHandler->Comment(nsDependentString(aName));
 
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::HandleCDataSection(const char16_t *aData,
                                   uint32_t aLength)
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
nsSAXXMLReader::HandleStartDTD(const char16_t *aName,
                               const char16_t *aSystemId,
                               const char16_t *aPublicId)
{
  char16_t nullChar = char16_t(0);
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
                                     nsDependentString(aPublicId),
                                     nsDependentString(aSystemId));
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
nsSAXXMLReader::HandleCharacterData(const char16_t *aData,
                                    uint32_t aLength)
{
  if (mContentHandler)
    return mContentHandler->Characters(Substring(aData, aData+aLength));

  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::HandleStartNamespaceDecl(const char16_t *aPrefix,
                                         const char16_t *aUri)
{
  if (!mContentHandler)
    return NS_OK;
  
  char16_t nullChar = char16_t(0);
  if (!aPrefix)
    aPrefix = &nullChar;
  if (!aUri)
    aUri = &nullChar;

  return mContentHandler->StartPrefixMapping(nsDependentString(aPrefix),
                                             nsDependentString(aUri));
}

NS_IMETHODIMP
nsSAXXMLReader::HandleEndNamespaceDecl(const char16_t *aPrefix)
{
  if (!mContentHandler)
    return NS_OK;
  
  if (aPrefix)
    return mContentHandler->EndPrefixMapping(nsDependentString(aPrefix));

  return mContentHandler->EndPrefixMapping(EmptyString());
}

NS_IMETHODIMP
nsSAXXMLReader::HandleProcessingInstruction(const char16_t *aTarget,
                                            const char16_t *aData)
{
  NS_ASSERTION(aTarget && aData, "null passed to handler");
  if (mContentHandler) {
    return mContentHandler->ProcessingInstruction(nsDependentString(aTarget),
                                                  nsDependentString(aData));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::HandleNotationDecl(const char16_t *aNotationName,
                                   const char16_t *aSystemId,
                                   const char16_t *aPublicId)
{
  NS_ASSERTION(aNotationName, "null passed to handler");
  if (mDTDHandler) {
    char16_t nullChar = char16_t(0);
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
nsSAXXMLReader::HandleUnparsedEntityDecl(const char16_t *aEntityName,
                                         const char16_t *aSystemId,
                                         const char16_t *aPublicId,
                                         const char16_t *aNotationName)
{
  NS_ASSERTION(aEntityName && aNotationName, "null passed to handler");
  if (mDTDHandler) {
    char16_t nullChar = char16_t(0);
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
nsSAXXMLReader::HandleXMLDeclaration(const char16_t *aVersion,
                                     const char16_t *aEncoding,
                                     int32_t aStandalone)
{
  NS_ASSERTION(aVersion, "null passed to handler");
  if (mDeclarationHandler) {
    char16_t nullChar = char16_t(0);
    if (!aEncoding)
      aEncoding = &nullChar;
    mDeclarationHandler->HandleXMLDeclaration(nsDependentString(aVersion),
                                              nsDependentString(aEncoding),
                                              aStandalone > 0);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::ReportError(const char16_t* aErrorText,
                            const char16_t* aSourceText,
                            nsIScriptError *aError,
                            bool *_retval)
{
  NS_PRECONDITION(aError && aSourceText && aErrorText, "Check arguments!!!");
  // Normally, the expat driver should report the error.
  *_retval = true;

  if (mErrorHandler) {
    uint32_t lineNumber;
    nsresult rv = aError->GetLineNumber(&lineNumber);
    NS_ENSURE_SUCCESS(rv, rv);

    uint32_t columnNumber;
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
  if (aName.EqualsLiteral("http://xml.org/sax/features/namespace-prefixes")) {
    mEnableNamespacePrefixes = aValue;
    return NS_OK;
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSAXXMLReader::GetFeature(const nsAString &aName, bool *aResult)
{
  if (aName.EqualsLiteral("http://xml.org/sax/features/namespace-prefixes")) {
    *aResult = mEnableNamespacePrefixes;
    return NS_OK;
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSAXXMLReader::GetDeclarationHandler(nsIMozSAXXMLDeclarationHandler **aDeclarationHandler) {
  NS_IF_ADDREF(*aDeclarationHandler = mDeclarationHandler);
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::SetDeclarationHandler(nsIMozSAXXMLDeclarationHandler *aDeclarationHandler) {
  mDeclarationHandler = aDeclarationHandler;
  return NS_OK;
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

  nsCOMPtr<nsIPrincipal> nullPrincipal = nsNullPrincipal::Create();

  // The following channel is never openend, so it does not matter what
  // securityFlags we pass; let's follow the principle of least privilege.
  nsCOMPtr<nsIChannel> parserChannel;
  rv = NS_NewInputStreamChannel(getter_AddRefs(parserChannel),
                                mBaseURI,
                                aStream,
                                nullPrincipal,
                                nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED,
                                nsIContentPolicy::TYPE_OTHER,
                                nsDependentCString(aContentType));
  if (!parserChannel || NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  if (aCharset)
    parserChannel->SetContentCharset(nsDependentCString(aCharset));

  rv = InitParser(nullptr, parserChannel);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mListener->OnStartRequest(parserChannel, nullptr);
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
  
  uint64_t offset = 0;
  while (NS_SUCCEEDED(rv) && NS_SUCCEEDED(status)) {
    uint64_t available;
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

    if (available > UINT32_MAX)
      available = UINT32_MAX;

    rv = mListener->OnDataAvailable(parserChannel, nullptr,
                                    aStream,
                                    offset,
                                    (uint32_t)available);
    if (NS_SUCCEEDED(rv))
      offset += available;
    else
      parserChannel->Cancel(rv);
    parserChannel->GetStatus(&status);
  }
  rv = mListener->OnStopRequest(parserChannel, nullptr, status);
  mListener = nullptr;

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
  mParserObserver = nullptr;
  return mListener->OnStartRequest(aRequest, aContext);
}

NS_IMETHODIMP
nsSAXXMLReader::OnStopRequest(nsIRequest *aRequest, nsISupports *aContext,
                              nsresult status)
{
  NS_ENSURE_TRUE(mIsAsyncParse, NS_ERROR_FAILURE);
  NS_ENSURE_STATE(mListener);
  nsresult rv = mListener->OnStopRequest(aRequest, aContext, status);
  mListener = nullptr;
  mIsAsyncParse = false;
  return rv;
}

// nsIStreamListener

NS_IMETHODIMP
nsSAXXMLReader::OnDataAvailable(nsIRequest *aRequest, nsISupports *aContext,
                                nsIInputStream *aInputStream, uint64_t offset,
                                uint32_t count)
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

  int32_t charsetSource = kCharsetFromDocTypeDefault;
  nsAutoCString charset(NS_LITERAL_CSTRING("UTF-8"));
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
                                  int32_t& aCharsetSource,
                                  nsACString& aCharset)
{
  if (aCharsetSource >= kCharsetFromChannel)
    return true;
  
  if (aChannel) {
    nsAutoCString charsetVal;
    nsresult rv = aChannel->GetContentCharset(charsetVal);
    if (NS_SUCCEEDED(rv)) {
      nsAutoCString preferred;
      if (!EncodingUtils::FindEncodingForLabel(charsetVal, preferred))
        return false;

      aCharset = preferred;
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
nsSAXXMLReader::SplitExpatName(const char16_t *aExpatName,
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
  int32_t break1, break2 = kNotFound;
  break1 = expatStr.FindChar(char16_t(0xFFFF));

  if (break1 == kNotFound) {
    aLocalName = expatStr; // no namespace
    aURI.Truncate();
    aQName = expatStr;
  } else {
    aURI = StringHead(expatStr, break1);
    break2 = expatStr.FindChar(char16_t(0xFFFF), break1 + 1);
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
