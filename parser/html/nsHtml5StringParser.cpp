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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
  , mTreeBuilder(new nsHtml5TreeBuilder(mExecutor, nsnull))
  , mTokenizer(new nsHtml5Tokenizer(mTreeBuilder, false))
{
  MOZ_COUNT_CTOR(nsHtml5StringParser);
  mAtomTable.Init(); // we aren't checking for OOM anyway...
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

  mExecutor->EnableFragmentMode(aPreventScriptExecution);

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

  mTreeBuilder->setFragmentContext(nsnull,
                                   kNameSpaceID_None,
                                   nsnull,
                                   false);

  mExecutor->PreventScriptExecution();

  Tokenize(aSourceBuffer, aTargetDoc, aScriptingEnabledForNoscriptParsing);
  return NS_OK;
}

void
nsHtml5StringParser::Tokenize(const nsAString& aSourceBuffer,
                              nsIDocument* aDocument,
                              bool aScriptingEnabledForNoscriptParsing) {

  nsIURI* uri = aDocument->GetDocumentURI();

  mExecutor->Init(aDocument, uri, nsnull, nsnull);

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
          // Flush on each script, because the execution prevention code
          // can handle at most one script per flush.
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
