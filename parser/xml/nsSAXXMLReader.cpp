/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSAXXMLReader.h"

#include "mozilla/Encoding.h"
#include "nsIInputStream.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "NullPrincipal.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsIScriptError.h"
#include "nsSAXAttributes.h"
#include "nsCharsetSource.h"

using mozilla::Encoding;
using mozilla::NotNull;

#define XMLNS_URI "http://www.w3.org/2000/xmlns/"

static NS_DEFINE_CID(kParserCID, NS_PARSER_CID);

NS_IMPL_CYCLE_COLLECTION(nsSAXXMLReader,
                         mContentHandler,
                         mErrorHandler,
                         mBaseURI,
                         mListener,
                         mParserObserver)
NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSAXXMLReader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSAXXMLReader)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSAXXMLReader)
  NS_INTERFACE_MAP_ENTRY(nsISAXXMLReader)
  NS_INTERFACE_MAP_ENTRY(nsIExpatSink)
  NS_INTERFACE_MAP_ENTRY(nsIContentSink)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISAXXMLReader)
NS_INTERFACE_MAP_END

nsSAXXMLReader::nsSAXXMLReader()
  : mIsAsyncParse(false)
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

// nsIExpatSink

NS_IMETHODIMP
nsSAXXMLReader::HandleStartElement(const char16_t *aName,
                                   const char16_t **aAtts,
                                   uint32_t aAttsCount,
                                   uint32_t aLineNumber,
                                   uint32_t aColumnNumber)
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
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::HandleCDataSection(const char16_t *aData,
                                   uint32_t aLength)
{
  if (mContentHandler) {
    nsresult rv = mContentHandler->Characters(Substring(aData, aData+aLength));
    NS_ENSURE_SUCCESS(rv, rv);
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
nsSAXXMLReader::HandleXMLDeclaration(const char16_t *aVersion,
                                     const char16_t *aEncoding,
                                     int32_t aStandalone)
{
  NS_ASSERTION(aVersion, "null passed to handler");
  return NS_OK;
}

NS_IMETHODIMP
nsSAXXMLReader::ReportError(const char16_t* aErrorText,
                            const char16_t* aSourceText,
                            nsIScriptError *aError,
                            bool *_retval)
{
  MOZ_ASSERT(aError && aSourceText && aErrorText, "Check arguments!!!");
  // Normally, the expat driver should report the error.
  *_retval = true;

  if (mErrorHandler) {
    nsresult rv = mErrorHandler->FatalError(nsDependentString(aErrorText));
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
  auto encoding = UTF_8_ENCODING;
  TryChannelCharset(aChannel, charsetSource, encoding);
  parser->SetDocumentCharset(encoding, charsetSource);

  rv = parser->Parse(mBaseURI, aObserver);
  NS_ENSURE_SUCCESS(rv, rv);

  mListener = do_QueryInterface(parser, &rv);

  return rv;
}

// from nsDocument.cpp
bool
nsSAXXMLReader::TryChannelCharset(nsIChannel *aChannel,
                                  int32_t& aCharsetSource,
                                  NotNull<const Encoding*>& aEncoding)
{
  if (aCharsetSource >= kCharsetFromChannel)
    return true;
  
  if (aChannel) {
    nsAutoCString charsetVal;
    nsresult rv = aChannel->GetContentCharset(charsetVal);
    if (NS_SUCCEEDED(rv)) {
      const Encoding* preferred = Encoding::ForLabel(charsetVal);
      if (!preferred)
        return false;

      aEncoding = WrapNotNull(preferred);
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
