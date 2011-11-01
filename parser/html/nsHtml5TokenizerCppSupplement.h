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
nsHtml5Tokenizer::StartViewSource()
{
  mViewSource->Start();
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

void
nsHtml5Tokenizer::errUnquotedAttributeValOrNull(PRUnichar c)
{
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

void
nsHtml5Tokenizer::errLtOrEqualsOrGraveInUnquotedAttributeOrNull(PRUnichar c)
{
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

void
nsHtml5Tokenizer::errBadCharBeforeAttributeNameOrNull(PRUnichar c)
{
  if (c == '<') {
    mViewSource->AddErrorToCurrentNode("errBadCharBeforeAttributeNameLt");
  } else if (c == '=') {
    errEqualsSignBeforeAttributeName();
  } else if (c != 0xFFFD) {
    errQuoteBeforeAttributeName(c);
  }
}

void
nsHtml5Tokenizer::errBadCharAfterLt(PRUnichar c)
{
  mViewSource->AddErrorToCurrentNode("errBadCharAfterLt");
}

void
nsHtml5Tokenizer::errQuoteOrLtInAttributeNameOrNull(PRUnichar c)
{
  if (c == '<') {
    mViewSource->AddErrorToCurrentNode("errLtInAttributeName");
  } else if (c != 0xFFFD) {
    mViewSource->AddErrorToCurrentNode("errQuoteInAttributeName");
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
    mViewSource->AddErrorToCurrentMarkupDecl("maybeErrAttributesOnEndTag");
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
  if (mViewSource) {
    mViewSource->AddErrorToCurrentNode("errNcrNonCharacter");
  }
  return ch;
}

void
nsHtml5Tokenizer::errAstralNonCharacter(int ch)
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentNode("errAstralNonCharacter");
  }
}

PRUnichar
nsHtml5Tokenizer::errNcrControlChar(PRUnichar ch)
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentNode("errNcrControlChar");
  }
  return ch;
}

void
nsHtml5Tokenizer::errGarbageAfterLtSlash()
{
  mViewSource->AddErrorToCurrentNode("errGarbageAfterLtSlash");
}

void
nsHtml5Tokenizer::errLtSlashGt()
{
  mViewSource->AddErrorToCurrentNode("errLtSlashGt");
}

void
nsHtml5Tokenizer::errCharRefLacksSemicolon()
{
  mViewSource->AddErrorToCurrentNode("errCharRefLacksSemicolon");
}

void
nsHtml5Tokenizer::errNoDigitsInNCR()
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentNode("errNoDigitsInNCR");
  }
}

void
nsHtml5Tokenizer::errGtInSystemId()
{
  mViewSource->AddErrorToCurrentNode("errGtInSystemId");
}

void
nsHtml5Tokenizer::errGtInPublicId()
{
  mViewSource->AddErrorToCurrentNode("errGtInPublicId");
}

void
nsHtml5Tokenizer::errNamelessDoctype()
{
  mViewSource->AddErrorToCurrentNode("errNamelessDoctype");
}

void
nsHtml5Tokenizer::errConsecutiveHyphens()
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentNode("errConsecutiveHyphens");
  }
}

void
nsHtml5Tokenizer::errPrematureEndOfComment()
{
  mViewSource->AddErrorToCurrentNode("errPrematureEndOfComment");
}

void
nsHtml5Tokenizer::errBogusComment()
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentNode("errBogusComment");
  }
}

void
nsHtml5Tokenizer::errSlashNotFollowedByGt()
{
  mViewSource->AddErrorToCurrentSlash("errSlashNotFollowedByGt");
}

void
nsHtml5Tokenizer::errNoSpaceBetweenAttributes()
{
  mViewSource->AddErrorToCurrentNode("errNoSpaceBetweenAttributes");
}

void
nsHtml5Tokenizer::errAttributeValueMissing()
{
  mViewSource->AddErrorToCurrentNode("errAttributeValueMissing");
}

void
nsHtml5Tokenizer::errEqualsSignBeforeAttributeName()
{
  mViewSource->AddErrorToCurrentNode("errEqualsSignBeforeAttributeName");
}

void
nsHtml5Tokenizer::errLtGt()
{
  mViewSource->AddErrorToCurrentNode("errLtGt");
}

void
nsHtml5Tokenizer::errProcessingInstruction()
{
  mViewSource->AddErrorToCurrentNode("errProcessingInstruction");
}

void
nsHtml5Tokenizer::errUnescapedAmpersandInterpretedAsCharacterReference()
{
  mViewSource->AddErrorToCurrentAmpersand("errUnescapedAmpersandInterpretedAsCharacterReference");
}

void
nsHtml5Tokenizer::errNotSemicolonTerminated()
{
  mViewSource->AddErrorToCurrentNode("errNotSemicolonTerminated");
}

void
nsHtml5Tokenizer::errNoNamedCharacterMatch()
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentAmpersand("errNoNamedCharacterMatch");
  }
}

void
nsHtml5Tokenizer::errQuoteBeforeAttributeName(PRUnichar c)
{
  mViewSource->AddErrorToCurrentNode("errQuoteBeforeAttributeName");
}

void
nsHtml5Tokenizer::errExpectedPublicId()
{
  mViewSource->AddErrorToCurrentNode("errExpectedPublicId");
}

void
nsHtml5Tokenizer::errBogusDoctype()
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentNode("errBogusDoctype");
  }
}

void
nsHtml5Tokenizer::errNcrSurrogate()
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentNode("errNcrSurrogate");
  }
}

void
nsHtml5Tokenizer::errNcrCr()
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentNode("errNcrCr");
  }
}

void
nsHtml5Tokenizer::errNcrInC1Range()
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentNode("errNcrInC1Range");
  }
}

void
nsHtml5Tokenizer::errEofInPublicId()
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentMarkupDecl("errEofInPublicId");
  }
}

void
nsHtml5Tokenizer::errEofInComment()
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentMarkupDecl("errEofInComment");
  }
}

void
nsHtml5Tokenizer::errEofInDoctype()
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentMarkupDecl("errEofInDoctype");
  }
}

void
nsHtml5Tokenizer::errEofInAttributeValue()
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentMarkupDecl("errEofInAttributeValue");
  }
}

void
nsHtml5Tokenizer::errEofInAttributeName()
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentMarkupDecl("errEofInAttributeName");
  }
}

void
nsHtml5Tokenizer::errEofWithoutGt()
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentMarkupDecl("errEofWithoutGt");
  }
}

void
nsHtml5Tokenizer::errEofInTagName()
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentMarkupDecl("errEofInTagName");
  }
}

void
nsHtml5Tokenizer::errEofInEndTag()
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentMarkupDecl("errEofInEndTag");
  }
}

void
nsHtml5Tokenizer::errEofAfterLt()
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentMarkupDecl("errEofAfterLt");
  }
}

void
nsHtml5Tokenizer::errNcrOutOfRange()
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentNode("errNcrOutOfRange");
  }
}

void
nsHtml5Tokenizer::errNcrUnassigned()
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentNode("errNcrUnassigned");
  }
}

void
nsHtml5Tokenizer::errDuplicateAttribute()
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentNode("errDuplicateAttribute");
  }
}

void
nsHtml5Tokenizer::errEofInSystemId()
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentMarkupDecl("errEofInSystemId");
  }
}

void
nsHtml5Tokenizer::errExpectedSystemId()
{
  mViewSource->AddErrorToCurrentNode("errExpectedSystemId");
}

void
nsHtml5Tokenizer::errMissingSpaceBeforeDoctypeName()
{
  mViewSource->AddErrorToCurrentNode("errMissingSpaceBeforeDoctypeName");
}

void
nsHtml5Tokenizer::errHyphenHyphenBang()
{
  mViewSource->AddErrorToCurrentNode("errHyphenHyphenBang");
}

void
nsHtml5Tokenizer::errNcrControlChar()
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentNode("errNcrControlChar");
  }
}

void
nsHtml5Tokenizer::errNcrZero()
{
  if (mViewSource) {
    mViewSource->AddErrorToCurrentNode("errNcrZero");
  }
}

void
nsHtml5Tokenizer::errNoSpaceBetweenDoctypeSystemKeywordAndQuote()
{
  mViewSource->AddErrorToCurrentNode("errNoSpaceBetweenDoctypeSystemKeywordAndQuote");
}

void
nsHtml5Tokenizer::errNoSpaceBetweenPublicAndSystemIds()
{
  mViewSource->AddErrorToCurrentNode("errNoSpaceBetweenPublicAndSystemIds");
}

void
nsHtml5Tokenizer::errNoSpaceBetweenDoctypePublicKeywordAndQuote()
{
  mViewSource->AddErrorToCurrentNode("errNoSpaceBetweenDoctypePublicKeywordAndQuote");
}
