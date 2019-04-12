/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "PrototypeDocumentParser.h"

#include "nsXULPrototypeCache.h"
#include "nsXULContentSink.h"
#include "nsParserCIID.h"
#include "mozilla/Encoding.h"
#include "nsCharsetSource.h"
#include "mozilla/dom/PrototypeDocumentContentSink.h"

using namespace mozilla::dom;

static NS_DEFINE_CID(kParserCID, NS_PARSER_CID);

namespace mozilla {
namespace parser {

PrototypeDocumentParser::PrototypeDocumentParser(nsIURI* aDocumentURI,
                                                 dom::Document* aDocument)
    : mDocumentURI(aDocumentURI),
      mDocument(aDocument),
      mPrototypeAlreadyLoaded(false),
      mIsComplete(false) {}

PrototypeDocumentParser::~PrototypeDocumentParser() {}

NS_INTERFACE_TABLE_HEAD(PrototypeDocumentParser)
  NS_INTERFACE_TABLE(PrototypeDocumentParser, nsIParser, nsIStreamListener,
                     nsIRequestObserver)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(PrototypeDocumentParser)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(PrototypeDocumentParser)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PrototypeDocumentParser)

NS_IMPL_CYCLE_COLLECTION(PrototypeDocumentParser, mDocumentURI, mOriginalSink,
                         mDocument, mStreamListener, mCurrentPrototype)

NS_IMETHODIMP_(void)
PrototypeDocumentParser::SetContentSink(nsIContentSink* aSink) {
  MOZ_ASSERT(aSink, "sink cannot be null!");
  mOriginalSink = static_cast<PrototypeDocumentContentSink*>(aSink);
  MOZ_ASSERT(mOriginalSink);

  aSink->SetParser(this);
}

NS_IMETHODIMP_(nsIContentSink*)
PrototypeDocumentParser::GetContentSink() { return mOriginalSink; }

nsIStreamListener* PrototypeDocumentParser::GetStreamListener() { return this; }

NS_IMETHODIMP_(bool)
PrototypeDocumentParser::IsComplete() { return mIsComplete; }

NS_IMETHODIMP
PrototypeDocumentParser::Parse(nsIURI* aURL, nsIRequestObserver* aListener,
                               void* aKey, nsDTDMode aMode) {
  // Look in the chrome cache: we've got this puppy loaded
  // already.
  nsXULPrototypeDocument* proto =
      IsChromeURI(mDocumentURI)
          ? nsXULPrototypeCache::GetInstance()->GetPrototype(mDocumentURI)
          : nullptr;

  // We don't abort on failure here because there are too many valid
  // cases that can return failure, and the null-ness of |proto| is enough
  // to trigger the fail-safe parse-from-disk solution. Example failure cases
  // (for reference) include:
  //
  // NS_ERROR_NOT_AVAILABLE: the URI cannot be found in the startup cache,
  //                         parse from disk
  // other: the startup cache file could not be found, probably
  //        due to being accessed before a profile has been selected (e.g.
  //        loading chrome for the profile manager itself). This must be
  //        parsed from disk.
  nsresult rv;
  if (proto) {
    mCurrentPrototype = proto;

    // Set up the right principal on the document.
    mDocument->SetPrincipals(proto->DocumentPrincipal(),
                             proto->DocumentPrincipal());
  } else {
    // It's just a vanilla document load. Create a parser to deal
    // with the stream n' stuff.

    nsCOMPtr<nsIParser> parser;
    // Get the document's principal
    nsCOMPtr<nsIPrincipal> principal = mDocument->NodePrincipal();
    rv =
        PrepareToLoadPrototype(mDocumentURI, principal, getter_AddRefs(parser));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStreamListener> listener = do_QueryInterface(parser, &rv);
    NS_ASSERTION(NS_SUCCEEDED(rv), "parser doesn't support nsIStreamListener");
    if (NS_FAILED(rv)) return rv;

    mStreamListener = listener;

    parser->Parse(mDocumentURI);
  }

  // If we're racing with another document to load proto, wait till the
  // load has finished loading before trying build the document.
  // Either the nsXULContentSink finishing to load the XML or
  // the nsXULPrototypeDocument completing deserialization will trigger the
  // OnPrototypeLoadDone callback.
  // If the prototype is already loaded, OnPrototypeLoadDone will be called
  // in OnStopRequest.
  RefPtr<PrototypeDocumentParser> self = this;
  rv = mCurrentPrototype->AwaitLoadDone(
      [self]() { self->OnPrototypeLoadDone(); }, &mPrototypeAlreadyLoaded);
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}

NS_IMETHODIMP
PrototypeDocumentParser::OnStartRequest(nsIRequest* request) {
  if (mStreamListener) {
    return mStreamListener->OnStartRequest(request);
  }
  // There's already a prototype cached, so return cached here so the original
  // request will be aborted. Either OnStopRequest or the prototype load
  // finishing will notify the content sink that we're done loading the
  // prototype.
  return NS_ERROR_PARSED_DATA_CACHED;
}

NS_IMETHODIMP
PrototypeDocumentParser::OnStopRequest(nsIRequest* request, nsresult aStatus) {
  if (mStreamListener) {
    return mStreamListener->OnStopRequest(request, aStatus);
  }
  if (mPrototypeAlreadyLoaded) {
    return this->OnPrototypeLoadDone();
  }
  // The prototype will handle calling OnPrototypeLoadDone when it is ready.
  return NS_OK;
}

NS_IMETHODIMP
PrototypeDocumentParser::OnDataAvailable(nsIRequest* request,
                                         nsIInputStream* aInStr,
                                         uint64_t aSourceOffset,
                                         uint32_t aCount) {
  if (mStreamListener) {
    return mStreamListener->OnDataAvailable(request, aInStr, aSourceOffset,
                                            aCount);
  }
  MOZ_ASSERT_UNREACHABLE("Cached prototype doesn't receive data");
  return NS_ERROR_UNEXPECTED;
}

nsresult PrototypeDocumentParser::OnPrototypeLoadDone() {
  MOZ_ASSERT(!mIsComplete, "Should not be called more than once.");
  mIsComplete = true;

  return mOriginalSink->OnPrototypeLoadDone(mCurrentPrototype);
}

nsresult PrototypeDocumentParser::PrepareToLoadPrototype(
    nsIURI* aURI, nsIPrincipal* aDocumentPrincipal, nsIParser** aResult) {
  nsresult rv;

  // Create a new prototype document.
  rv = NS_NewXULPrototypeDocument(getter_AddRefs(mCurrentPrototype));
  if (NS_FAILED(rv)) return rv;

  rv = mCurrentPrototype->InitPrincipal(aURI, aDocumentPrincipal);
  if (NS_FAILED(rv)) {
    mCurrentPrototype = nullptr;
    return rv;
  }

  // Store the new prototype right away so if there are multiple requests
  // for the same document they all get the same prototype.
  if (IsChromeURI(mDocumentURI) &&
      nsXULPrototypeCache::GetInstance()->IsEnabled()) {
    nsXULPrototypeCache::GetInstance()->PutPrototype(mCurrentPrototype);
  }

  mDocument->SetPrincipals(aDocumentPrincipal, aDocumentPrincipal);

  // Create a XUL content sink, a parser, and kick off a load for
  // the document.
  RefPtr<XULContentSinkImpl> sink = new XULContentSinkImpl();

  rv = sink->Init(mDocument, mCurrentPrototype);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to initialize datasource sink");
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIParser> parser = do_CreateInstance(kParserCID, &rv);
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create parser");
  if (NS_FAILED(rv)) return rv;

  parser->SetCommand(eViewNormal);

  parser->SetDocumentCharset(UTF_8_ENCODING, kCharsetFromDocTypeDefault);
  parser->SetContentSink(sink);  // grabs a reference to the parser

  parser.forget(aResult);
  return NS_OK;
}

}  // namespace parser
}  // namespace mozilla
