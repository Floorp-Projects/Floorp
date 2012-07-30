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
  : mExecutor(new nsHtml5TreeOpExecutor(true))
  , mTreeBuilder(new nsHtml5TreeBuilder(mExecutor, nullptr))
  , mTokenizer(new nsHtml5Tokenizer(mTreeBuilder, false))
{
  MOZ_COUNT_CTOR(nsHtml5StringParser);
  mAtomTable.Init();
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
                                   PRInt32 aContextNamespace,
                                   bool aQuirks,
                                   bool aPreventScriptExecution)
{
  NS_ENSURE_TRUE(aSourceBuffer.Length() <= PR_INT32_MAX,
                 NS_ERROR_OUT_OF_MEMORY);

  nsIDocument* doc = aTargetNode->OwnerDoc();
  nsIURI* uri = doc->GetDocumentURI();
  NS_ENSURE_TRUE(uri, NS_ERROR_NOT_AVAILABLE);

  nsIContent* target = aTargetNode;
  mTreeBuilder->setFragmentContext(aContextLocalName,
                                   aContextNamespace,
                                   &target,
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

  Tokenize(aSourceBuffer, doc, true);
  return NS_OK;
}

nsresult
nsHtml5StringParser::ParseDocument(const nsAString& aSourceBuffer,
                                   nsIDocument* aTargetDoc,
                                   bool aScriptingEnabledForNoscriptParsing)
{
  MOZ_ASSERT(!aTargetDoc->GetFirstChild());

  NS_ENSURE_TRUE(aSourceBuffer.Length() <= PR_INT32_MAX,
                 NS_ERROR_OUT_OF_MEMORY);

  mTreeBuilder->setFragmentContext(nullptr,
                                   kNameSpaceID_None,
                                   nullptr,
                                   false);

  mTreeBuilder->SetPreventScriptExecution(true);

  Tokenize(aSourceBuffer, aTargetDoc, aScriptingEnabledForNoscriptParsing);
  return NS_OK;
}

void
nsHtml5StringParser::Tokenize(const nsAString& aSourceBuffer,
                              nsIDocument* aDocument,
                              bool aScriptingEnabledForNoscriptParsing) {

  nsIURI* uri = aDocument->GetDocumentURI();

  mExecutor->Init(aDocument, uri, nullptr, nullptr);

  mExecutor->SetParser(this);
  mExecutor->SetNodeInfoManager(aDocument->NodeInfoManager());

  NS_PRECONDITION(!mExecutor->HasStarted(),
                  "Tried to start parse without initializing the parser.");
  mTreeBuilder->setScriptingEnabled(aScriptingEnabledForNoscriptParsing);
  mTokenizer->start();
  mExecutor->Start(); // Don't call WillBuildModel in fragment case
  if (!aSourceBuffer.IsEmpty()) {
    bool lastWasCR = false;
    nsHtml5DependentUTF16Buffer buffer(aSourceBuffer);
    while (buffer.hasMore()) {
      buffer.adjust(lastWasCR);
      lastWasCR = false;
      if (buffer.hasMore()) {
        lastWasCR = mTokenizer->tokenizeBuffer(&buffer);
        if (mTreeBuilder->HasScript()) {
          // If we come here, we are in createContextualFragment() or in the
          // upcoming document.parse(). It's unclear if it's really necessary
          // to flush here, but let's do so for consistency with other flushes
          // to avoid different code paths on the executor side.
          mTreeBuilder->Flush(); // Move ops to the executor
          mExecutor->FlushDocumentWrite(); // run the ops
        }
      }
    }
  }
  mTokenizer->eof();
  mTreeBuilder->StreamEnded();
  mTreeBuilder->Flush();
  mExecutor->FlushDocumentWrite();
  mTokenizer->end();
  mExecutor->DropParserAndPerfHint();
  mExecutor->DropHeldElements();
  mTreeBuilder->DropHandles();
  mAtomTable.Clear();
  mExecutor->Reset();
}
