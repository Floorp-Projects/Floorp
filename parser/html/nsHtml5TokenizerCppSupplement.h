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
 * The Original Code is HTML5 View Source code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henri Sivonen <hsivonen@iki.fi>
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

void
nsHtml5Tokenizer::StartPlainText()
{
  stateSave = NS_HTML5TOKENIZER_PLAINTEXT;
}

void
nsHtml5Tokenizer::EnableViewSource(nsHtml5Highlighter* aHighlighter)
{
  mViewSource = aHighlighter;
}

bool
nsHtml5Tokenizer::FlushViewSource()
{
  return mViewSource->FlushOps();
}

void
nsHtml5Tokenizer::StartViewSource(const nsAutoString& aTitle)
{
  mViewSource->Start(aTitle);
}

void
nsHtml5Tokenizer::EndViewSource()
{
  mViewSource->End();
}

void
nsHtml5Tokenizer::errWarnLtSlashInRcdata()
{
}

// The null checks below annotated NS_LIKELY are not actually necessary.

void
nsHtml5Tokenizer::errUnquotedAttributeValOrNull(PRUnichar c)
{
  if (NS_LIKELY(mViewSource)) {
    switch (c) {
      case '<':
        mViewSource->AddErrorToCurrentNode("errUnquotedAttributeLt");
        return;
      case '`':
        mViewSource->AddErrorToCurrentNode("errUnquotedAttributeGrave");
        return;
      case '\'':
      case '"':
        mViewSource->AddErrorToCurrentNode("errUnquotedAttributeQuote");
        return;
      case '=':
        mViewSource->AddErrorToCurrentNode("errUnquotedAttributeEquals");
        return;
    }
  }
}

void
nsHtml5Tokenizer::errLtOrEqualsOrGraveInUnquotedAttributeOrNull(PRUnichar c)
{
  if (NS_LIKELY(mViewSource)) {
    switch (c) {
      case '=':
        mViewSource->AddErrorToCurrentNode("errUnquotedAttributeStartEquals");
        return;
      case '<':
        mViewSource->AddErrorToCurrentNode("errUnquotedAttributeStartLt");
        return;
      case '`':
        mViewSource->AddErrorToCurrentNode("errUnquotedAttributeStartGrave");
        return;
    }
  }
}

void
nsHtml5Tokenizer::errBadCharBeforeAttributeNameOrNull(PRUnichar c)
{
  if (NS_LIKELY(mViewSource)) {
    if (c == '<') {
      mViewSource->AddErrorToCurrentNode("errBadCharBeforeAttributeNameLt");
    } else if (c == '=') {
      errEqualsSignBeforeAttributeName();
    } else if (c != 0xFFFD) {
      errQuoteBeforeAttributeName(c);
    }
  }
}

void
nsHtml5Tokenizer::errBadCharAfterLt(PRUnichar c)
{
  if (NS_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errBadCharAfterLt");
  }
}

void
nsHtml5Tokenizer::errQuoteOrLtInAttributeNameOrNull(PRUnichar c)
{
  if (NS_LIKELY(mViewSource)) {
    if (c == '<') {
      mViewSource->AddErrorToCurrentNode("errLtInAttributeName");
    } else if (c != 0xFFFD) {
      mViewSource->AddErrorToCurrentNode("errQuoteInAttributeName");
    }
  }
}

void
nsHtml5Tokenizer::maybeErrAttributesOnEndTag(nsHtml5HtmlAttributes* attrs)
{
  if (mViewSource && attrs->getLength() != 0) {
    /*
     * When an end tag token is emitted with attributes, that is a parse
     * error.
     */
    mViewSource->AddErrorToCurrentRun("maybeErrAttributesOnEndTag");
  }
}

void
nsHtml5Tokenizer::maybeErrSlashInEndTag(bool selfClosing)
{
  if (mViewSource && selfClosing && endTag) {
    mViewSource->AddErrorToCurrentSlash("maybeErrSlashInEndTag");
  }
}

PRUnichar
nsHtml5Tokenizer::errNcrNonCharacter(PRUnichar ch)
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNcrNonCharacter");
  }
  return ch;
}

void
nsHtml5Tokenizer::errAstralNonCharacter(PRInt32 ch)
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNcrNonCharacter");
  }
}

PRUnichar
nsHtml5Tokenizer::errNcrControlChar(PRUnichar ch)
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNcrControlChar");
  }
  return ch;
}

void
nsHtml5Tokenizer::errGarbageAfterLtSlash()
{
  if (NS_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errGarbageAfterLtSlash");
  }
}

void
nsHtml5Tokenizer::errLtSlashGt()
{
  if (NS_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errLtSlashGt");
  }
}

void
nsHtml5Tokenizer::errCharRefLacksSemicolon()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errCharRefLacksSemicolon");
  }
}

void
nsHtml5Tokenizer::errNoDigitsInNCR()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNoDigitsInNCR");
  }
}

void
nsHtml5Tokenizer::errGtInSystemId()
{
  if (NS_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errGtInSystemId");
  }
}

void
nsHtml5Tokenizer::errGtInPublicId()
{
  if (NS_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errGtInPublicId");
  }
}

void
nsHtml5Tokenizer::errNamelessDoctype()
{
  if (NS_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNamelessDoctype");
  }
}

void
nsHtml5Tokenizer::errConsecutiveHyphens()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errConsecutiveHyphens");
  }
}

void
nsHtml5Tokenizer::errPrematureEndOfComment()
{
  if (NS_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errPrematureEndOfComment");
  }
}

void
nsHtml5Tokenizer::errBogusComment()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errBogusComment");
  }
}

void
nsHtml5Tokenizer::errSlashNotFollowedByGt()
{
  if (NS_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentSlash("errSlashNotFollowedByGt");
  }
}

void
nsHtml5Tokenizer::errNoSpaceBetweenAttributes()
{
  if (NS_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNoSpaceBetweenAttributes");
  }
}

void
nsHtml5Tokenizer::errAttributeValueMissing()
{
  if (NS_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errAttributeValueMissing");
  }
}

void
nsHtml5Tokenizer::errEqualsSignBeforeAttributeName()
{
  if (NS_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errEqualsSignBeforeAttributeName");
  }
}

void
nsHtml5Tokenizer::errLtGt()
{
  if (NS_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errLtGt");
  }
}

void
nsHtml5Tokenizer::errProcessingInstruction()
{
  if (NS_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errProcessingInstruction");
  }
}

void
nsHtml5Tokenizer::errUnescapedAmpersandInterpretedAsCharacterReference()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentAmpersand("errUnescapedAmpersandInterpretedAsCharacterReference");
  }
}

void
nsHtml5Tokenizer::errNotSemicolonTerminated()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNotSemicolonTerminated");
  }
}

void
nsHtml5Tokenizer::errNoNamedCharacterMatch()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentAmpersand("errNoNamedCharacterMatch");
  }
}

void
nsHtml5Tokenizer::errQuoteBeforeAttributeName(PRUnichar c)
{
  if (NS_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errQuoteBeforeAttributeName");
  }
}

void
nsHtml5Tokenizer::errExpectedPublicId()
{
  if (NS_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errExpectedPublicId");
  }
}

void
nsHtml5Tokenizer::errBogusDoctype()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errBogusDoctype");
  }
}

void
nsHtml5Tokenizer::errNcrSurrogate()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNcrSurrogate");
  }
}

void
nsHtml5Tokenizer::errNcrCr()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNcrCr");
  }
}

void
nsHtml5Tokenizer::errNcrInC1Range()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNcrInC1Range");
  }
}

void
nsHtml5Tokenizer::errEofInPublicId()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEofInPublicId");
  }
}

void
nsHtml5Tokenizer::errEofInComment()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEofInComment");
  }
}

void
nsHtml5Tokenizer::errEofInDoctype()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEofInDoctype");
  }
}

void
nsHtml5Tokenizer::errEofInAttributeValue()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEofInAttributeValue");
  }
}

void
nsHtml5Tokenizer::errEofInAttributeName()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEofInAttributeName");
  }
}

void
nsHtml5Tokenizer::errEofWithoutGt()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEofWithoutGt");
  }
}

void
nsHtml5Tokenizer::errEofInTagName()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEofInTagName");
  }
}

void
nsHtml5Tokenizer::errEofInEndTag()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEofInEndTag");
  }
}

void
nsHtml5Tokenizer::errEofAfterLt()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEofAfterLt");
  }
}

void
nsHtml5Tokenizer::errNcrOutOfRange()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNcrOutOfRange");
  }
}

void
nsHtml5Tokenizer::errNcrUnassigned()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNcrUnassigned");
  }
}

void
nsHtml5Tokenizer::errDuplicateAttribute()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errDuplicateAttribute");
  }
}

void
nsHtml5Tokenizer::errEofInSystemId()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEofInSystemId");
  }
}

void
nsHtml5Tokenizer::errExpectedSystemId()
{
  if (NS_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errExpectedSystemId");
  }
}

void
nsHtml5Tokenizer::errMissingSpaceBeforeDoctypeName()
{
  if (NS_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errMissingSpaceBeforeDoctypeName");
  }
}

void
nsHtml5Tokenizer::errHyphenHyphenBang()
{
  if (NS_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errHyphenHyphenBang");
  }
}

void
nsHtml5Tokenizer::errNcrControlChar()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNcrControlChar");
  }
}

void
nsHtml5Tokenizer::errNcrZero()
{
  if (NS_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNcrZero");
  }
}

void
nsHtml5Tokenizer::errNoSpaceBetweenDoctypeSystemKeywordAndQuote()
{
  if (NS_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNoSpaceBetweenDoctypeSystemKeywordAndQuote");
  }
}

void
nsHtml5Tokenizer::errNoSpaceBetweenPublicAndSystemIds()
{
  if (NS_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNoSpaceBetweenPublicAndSystemIds");
  }
}

void
nsHtml5Tokenizer::errNoSpaceBetweenDoctypePublicKeywordAndQuote()
{
  if (NS_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNoSpaceBetweenDoctypePublicKeywordAndQuote");
  }
}
