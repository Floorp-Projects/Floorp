/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5StringParser.h"
#include "nsHtml5TreeOpExecutor.h"
#include "nsHtml5TreeBuilder.h"
#include "nsHtml5Tokenizer.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsHtml5DependentUTF16Buffer.h"

NS_IMPL_ISUPPORTS0(nsHtml5StringParser)

nsHtml5StringParser::nsHtml5StringParser()
  : mBuilder(new nsHtml5OplessBuilder())
  , mTreeBuilder(new nsHtml5TreeBuilder(mBuilder))
  , mTokenizer(new nsHtml5Tokenizer(mTreeBuilder, false))
{
  MOZ_COUNT_CTOR(nsHtml5StringParser);
  mTokenizer->setInterner(&mAtomTable);
}

nsHtml5StringParser::~nsHtml5StringParser()
{
  MOZ_COUNT_DTOR(nsHtml5StringParser);
}

nsresult
nsHtml5StringParser::ParseFragment(const nsAString& aSourceBuffer,
                                   nsIContent* aTargetNode,
                                   nsIAtom* aContextLocalName,
                                   int32_t aContextNamespace,
                                   bool aQuirks,
                                   bool aPreventScriptExecution)
{
  NS_ENSURE_TRUE(aSourceBuffer.Length() <= INT32_MAX,
                 NS_ERROR_OUT_OF_MEMORY);

  nsIDocument* doc = aTargetNode->OwnerDoc();
  nsIURI* uri = doc->GetDocumentURI();
  NS_ENSURE_TRUE(uri, NS_ERROR_NOT_AVAILABLE);

  mTreeBuilder->setFragmentContext(aContextLocalName,
                                   aContextNamespace,
                                   aTargetNode,
                                   aQuirks);

#ifdef DEBUG
  if (!aPreventScriptExecution) {
    NS_ASSERTION(!aTargetNode->IsInDoc(),
                 "If script execution isn't prevented, "
                 "the target node must not be in doc.");
    nsCOMPtr<nsIDOMDocumentFragment> domFrag = do_QueryInterface(aTargetNode);
    NS_ASSERTION(domFrag,
      "If script execution isn't prevented, must parse to DOM fragment.");
  }
#endif

  mTreeBuilder->SetPreventScriptExecution(aPreventScriptExecution);

  return Tokenize(aSourceBuffer, doc, true);
}

nsresult
nsHtml5StringParser::ParseDocument(const nsAString& aSourceBuffer,
                                   nsIDocument* aTargetDoc,
                                   bool aScriptingEnabledForNoscriptParsing)
{
  MOZ_ASSERT(!aTargetDoc->GetFirstChild());

  NS_ENSURE_TRUE(aSourceBuffer.Length() <= INT32_MAX,
                 NS_ERROR_OUT_OF_MEMORY);

  mTreeBuilder->setFragmentContext(nullptr,
                                   kNameSpaceID_None,
                                   nullptr,
                                   false);

  mTreeBuilder->SetPreventScriptExecution(true);

  return Tokenize(aSourceBuffer, aTargetDoc, aScriptingEnabledForNoscriptParsing);
}

nsresult
nsHtml5StringParser::Tokenize(const nsAString& aSourceBuffer,
                              nsIDocument* aDocument,
                              bool aScriptingEnabledForNoscriptParsing) {

  nsIURI* uri = aDocument->GetDocumentURI();

  mBuilder->Init(aDocument, uri, nullptr, nullptr);

  mBuilder->SetParser(this);
  mBuilder->SetNodeInfoManager(aDocument->NodeInfoManager());

  // Mark the parser as *not* broken by passing NS_OK
  nsresult rv = mBuilder->MarkAsBroken(NS_OK);

  mTreeBuilder->setScriptingEnabled(aScriptingEnabledForNoscriptParsing);
  mTreeBuilder->setIsSrcdocDocument(aDocument->IsSrcdocDocument()); 
  mBuilder->Start();
  mTokenizer->start();
  if (!aSourceBuffer.IsEmpty()) {
    bool lastWasCR = false;
    nsHtml5DependentUTF16Buffer buffer(aSourceBuffer);
    while (buffer.hasMore()) {
      buffer.adjust(lastWasCR);
      lastWasCR = false;
      if (buffer.hasMore()) {
        if (!mTokenizer->EnsureBufferSpace(buffer.getLength())) {
          rv = mBuilder->MarkAsBroken(NS_ERROR_OUT_OF_MEMORY);
          break;
        }
        lastWasCR = mTokenizer->tokenizeBuffer(&buffer);
        if (NS_FAILED(rv = mBuilder->IsBroken())) {
          break;
        }
      }
    }
  }
  if (NS_SUCCEEDED(rv)) {
    mTokenizer->eof();
  }
  mTokenizer->end();
  mBuilder->Finish();
  mAtomTable.Clear();
  return rv;
}
