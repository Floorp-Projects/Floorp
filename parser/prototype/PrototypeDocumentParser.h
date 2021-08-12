/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_parser_PrototypeDocumentParser_h
#define mozilla_parser_PrototypeDocumentParser_h

#include "nsCycleCollectionParticipant.h"
#include "nsIContentSink.h"
#include "nsIParser.h"
#include "nsXULPrototypeDocument.h"

class nsIExpatSink;

namespace mozilla {
namespace dom {
class PrototypeDocumentContentSink;
}  // namespace dom
}  // namespace mozilla

namespace mozilla {
namespace parser {

// The PrototypeDocumentParser is more of a stub than a real parser. It is
// responsible for loading an nsXULPrototypeDocument either from the startup
// cache or creating a new prototype from the original source if a cached
// version does not exist. Once the parser finishes loading the prototype it
// will notify the content sink.
class PrototypeDocumentParser final : public nsIParser,
                                      public nsIStreamListener {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(PrototypeDocumentParser, nsIParser)

  explicit PrototypeDocumentParser(nsIURI* aDocumentURI,
                                   dom::Document* aDocument);

  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  // Start nsIParser
  // Ideally, this would just implement nsBaseParser since most of these are
  // stubs, but Document.h expects an nsIParser.
  NS_IMETHOD_(void) SetContentSink(nsIContentSink* aSink) override;

  NS_IMETHOD_(nsIContentSink*) GetContentSink() override;

  NS_IMETHOD_(void) GetCommand(nsCString& aCommand) override {}

  NS_IMETHOD_(void) SetCommand(const char* aCommand) override {}

  NS_IMETHOD_(void) SetCommand(eParserCommands aParserCommand) override {}

  virtual void SetDocumentCharset(NotNull<const Encoding*> aEncoding,
                                  int32_t aSource,
                                  bool aChannelHadCharset) override {}

  NS_IMETHOD GetChannel(nsIChannel** aChannel) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD GetDTD(nsIDTD** aDTD) override { return NS_ERROR_NOT_IMPLEMENTED; }

  virtual nsIStreamListener* GetStreamListener() override;

  NS_IMETHOD ContinueInterruptedParsing() override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD_(void) BlockParser() override {}

  NS_IMETHOD_(void) UnblockParser() override {}

  NS_IMETHOD_(void) ContinueInterruptedParsingAsync() override {}

  NS_IMETHOD_(bool) IsParserEnabled() override { return true; }

  NS_IMETHOD_(bool) IsComplete() override;

  NS_IMETHOD Parse(nsIURI* aURL, void* aKey = nullptr) override;

  NS_IMETHOD Terminate() override { return NS_ERROR_NOT_IMPLEMENTED; }

  NS_IMETHOD ParseFragment(const nsAString& aSourceBuffer,
                           nsTArray<nsString>& aTagStack) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD BuildModel() override { return NS_ERROR_NOT_IMPLEMENTED; }

  NS_IMETHOD CancelParsingEvents() override { return NS_ERROR_NOT_IMPLEMENTED; }

  virtual void Reset() override {}

  virtual bool IsInsertionPointDefined() override { return false; }

  void IncrementScriptNestingLevel() final {}

  void DecrementScriptNestingLevel() final {}

  bool HasNonzeroScriptNestingLevel() const final { return false; }

  virtual void MarkAsNotScriptCreated(const char* aCommand) override {}

  virtual bool IsScriptCreated() override { return false; }

  // End nsIParser

 private:
  virtual ~PrototypeDocumentParser();

 protected:
  nsresult PrepareToLoadPrototype(nsIURI* aURI,
                                  nsIPrincipal* aDocumentPrincipal,
                                  nsIParser** aResult);

  // This is invoked whenever the prototype for this document is loaded
  // and should be walked, regardless of whether the XUL cache is
  // disabled, whether the protototype was loaded, whether the
  // prototype was loaded from the cache or created by parsing the
  // actual XUL source, etc.
  nsresult OnPrototypeLoadDone();

  nsCOMPtr<nsIURI> mDocumentURI;
  RefPtr<dom::PrototypeDocumentContentSink> mOriginalSink;
  RefPtr<dom::Document> mDocument;

  // The XML parser that data is forwarded to when the prototype does not exist
  // and must be parsed from disk.
  nsCOMPtr<nsIStreamListener> mStreamListener;

  // The current prototype that we are walking to construct the
  // content model.
  RefPtr<nsXULPrototypeDocument> mCurrentPrototype;

  // True if there was a prototype in the cache and it finished loading
  // already.
  bool mPrototypeAlreadyLoaded;

  // True after the parser has notified the content sink that it is done.
  bool mIsComplete;
};

}  // namespace parser
}  // namespace mozilla

#endif  // mozilla_parser_PrototypeDocumentParser_h
