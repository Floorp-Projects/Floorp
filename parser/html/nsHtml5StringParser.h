/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5StringParser_h
#define nsHtml5StringParser_h

#include "mozilla/UniquePtr.h"
#include "nsHtml5AtomTable.h"
#include "nsParserBase.h"

class nsHtml5OplessBuilder;
class nsHtml5TreeBuilder;
class nsHtml5Tokenizer;
class nsIContent;
namespace mozilla {
namespace dom {
class Document;
}
}  // namespace mozilla

class nsHtml5StringParser : public nsParserBase {
 public:
  NS_DECL_ISUPPORTS

  /**
   * Constructor for use ONLY by nsContentUtils. Others, please call the
   * nsContentUtils statics that wrap this.
   */
  nsHtml5StringParser();

  /**
   * Invoke the fragment parsing algorithm (innerHTML).
   * DO NOT CALL from outside nsContentUtils.cpp.
   *
   * @param aSourceBuffer the string being set as innerHTML
   * @param aTargetNode the target container
   * @param aContextLocalName local name of context node
   * @param aContextNamespace namespace of context node
   * @param aQuirks true to make <table> not close <p>
   * @param aPreventScriptExecution true to prevent scripts from executing;
   * don't set to false when parsing into a target node that has been bound
   * to tree.
   */
  nsresult ParseFragment(const nsAString& aSourceBuffer,
                         nsIContent* aTargetNode, nsAtom* aContextLocalName,
                         int32_t aContextNamespace, bool aQuirks,
                         bool aPreventScriptExecution);

  /**
   * Parse an entire HTML document from a source string.
   * DO NOT CALL from outside nsContentUtils.cpp.
   *
   */
  nsresult ParseDocument(const nsAString& aSourceBuffer,
                         mozilla::dom::Document* aTargetDoc,
                         bool aScriptingEnabledForNoscriptParsing);

 private:
  virtual ~nsHtml5StringParser();

  nsresult Tokenize(const nsAString& aSourceBuffer,
                    mozilla::dom::Document* aDocument,
                    bool aScriptingEnabledForNoscriptParsing);

  /**
   * The tree operation executor
   */
  RefPtr<nsHtml5OplessBuilder> mBuilder;

  /**
   * The HTML5 tree builder
   */
  const mozilla::UniquePtr<nsHtml5TreeBuilder> mTreeBuilder;

  /**
   * The HTML5 tokenizer
   */
  const mozilla::UniquePtr<nsHtml5Tokenizer> mTokenizer;

  /**
   * The scoped atom table
   */
  nsHtml5AtomTable mAtomTable;
};

#endif  // nsHtml5StringParser_h
