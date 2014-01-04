/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Likely.h"

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

// The null checks below annotated MOZ_LIKELY are not actually necessary.

void
nsHtml5Tokenizer::errUnquotedAttributeValOrNull(char16_t c)
{
  if (MOZ_LIKELY(mViewSource)) {
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
nsHtml5Tokenizer::errLtOrEqualsOrGraveInUnquotedAttributeOrNull(char16_t c)
{
  if (MOZ_LIKELY(mViewSource)) {
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
nsHtml5Tokenizer::errBadCharBeforeAttributeNameOrNull(char16_t c)
{
  if (MOZ_LIKELY(mViewSource)) {
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
nsHtml5Tokenizer::errBadCharAfterLt(char16_t c)
{
  if (MOZ_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errBadCharAfterLt");
  }
}

void
nsHtml5Tokenizer::errQuoteOrLtInAttributeNameOrNull(char16_t c)
{
  if (MOZ_LIKELY(mViewSource)) {
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

char16_t
nsHtml5Tokenizer::errNcrNonCharacter(char16_t ch)
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNcrNonCharacter");
  }
  return ch;
}

void
nsHtml5Tokenizer::errAstralNonCharacter(int32_t ch)
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNcrNonCharacter");
  }
}

char16_t
nsHtml5Tokenizer::errNcrControlChar(char16_t ch)
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNcrControlChar");
  }
  return ch;
}

void
nsHtml5Tokenizer::errGarbageAfterLtSlash()
{
  if (MOZ_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errGarbageAfterLtSlash");
  }
}

void
nsHtml5Tokenizer::errLtSlashGt()
{
  if (MOZ_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errLtSlashGt");
  }
}

void
nsHtml5Tokenizer::errCharRefLacksSemicolon()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errCharRefLacksSemicolon");
  }
}

void
nsHtml5Tokenizer::errNoDigitsInNCR()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNoDigitsInNCR");
  }
}

void
nsHtml5Tokenizer::errGtInSystemId()
{
  if (MOZ_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errGtInSystemId");
  }
}

void
nsHtml5Tokenizer::errGtInPublicId()
{
  if (MOZ_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errGtInPublicId");
  }
}

void
nsHtml5Tokenizer::errNamelessDoctype()
{
  if (MOZ_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNamelessDoctype");
  }
}

void
nsHtml5Tokenizer::errConsecutiveHyphens()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errConsecutiveHyphens");
  }
}

void
nsHtml5Tokenizer::errPrematureEndOfComment()
{
  if (MOZ_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errPrematureEndOfComment");
  }
}

void
nsHtml5Tokenizer::errBogusComment()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errBogusComment");
  }
}

void
nsHtml5Tokenizer::errSlashNotFollowedByGt()
{
  if (MOZ_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentSlash("errSlashNotFollowedByGt");
  }
}

void
nsHtml5Tokenizer::errNoSpaceBetweenAttributes()
{
  if (MOZ_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNoSpaceBetweenAttributes");
  }
}

void
nsHtml5Tokenizer::errAttributeValueMissing()
{
  if (MOZ_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errAttributeValueMissing");
  }
}

void
nsHtml5Tokenizer::errEqualsSignBeforeAttributeName()
{
  if (MOZ_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errEqualsSignBeforeAttributeName");
  }
}

void
nsHtml5Tokenizer::errLtGt()
{
  if (MOZ_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errLtGt");
  }
}

void
nsHtml5Tokenizer::errProcessingInstruction()
{
  if (MOZ_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errProcessingInstruction");
  }
}

void
nsHtml5Tokenizer::errUnescapedAmpersandInterpretedAsCharacterReference()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentAmpersand("errUnescapedAmpersandInterpretedAsCharacterReference");
  }
}

void
nsHtml5Tokenizer::errNotSemicolonTerminated()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNotSemicolonTerminated");
  }
}

void
nsHtml5Tokenizer::errNoNamedCharacterMatch()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentAmpersand("errNoNamedCharacterMatch");
  }
}

void
nsHtml5Tokenizer::errQuoteBeforeAttributeName(char16_t c)
{
  if (MOZ_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errQuoteBeforeAttributeName");
  }
}

void
nsHtml5Tokenizer::errExpectedPublicId()
{
  if (MOZ_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errExpectedPublicId");
  }
}

void
nsHtml5Tokenizer::errBogusDoctype()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errBogusDoctype");
  }
}

void
nsHtml5Tokenizer::errNcrSurrogate()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNcrSurrogate");
  }
}

void
nsHtml5Tokenizer::errNcrCr()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNcrCr");
  }
}

void
nsHtml5Tokenizer::errNcrInC1Range()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNcrInC1Range");
  }
}

void
nsHtml5Tokenizer::errEofInPublicId()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEofInPublicId");
  }
}

void
nsHtml5Tokenizer::errEofInComment()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEofInComment");
  }
}

void
nsHtml5Tokenizer::errEofInDoctype()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEofInDoctype");
  }
}

void
nsHtml5Tokenizer::errEofInAttributeValue()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEofInAttributeValue");
  }
}

void
nsHtml5Tokenizer::errEofInAttributeName()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEofInAttributeName");
  }
}

void
nsHtml5Tokenizer::errEofWithoutGt()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEofWithoutGt");
  }
}

void
nsHtml5Tokenizer::errEofInTagName()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEofInTagName");
  }
}

void
nsHtml5Tokenizer::errEofInEndTag()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEofInEndTag");
  }
}

void
nsHtml5Tokenizer::errEofAfterLt()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEofAfterLt");
  }
}

void
nsHtml5Tokenizer::errNcrOutOfRange()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNcrOutOfRange");
  }
}

void
nsHtml5Tokenizer::errNcrUnassigned()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNcrUnassigned");
  }
}

void
nsHtml5Tokenizer::errDuplicateAttribute()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errDuplicateAttribute");
  }
}

void
nsHtml5Tokenizer::errEofInSystemId()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEofInSystemId");
  }
}

void
nsHtml5Tokenizer::errExpectedSystemId()
{
  if (MOZ_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errExpectedSystemId");
  }
}

void
nsHtml5Tokenizer::errMissingSpaceBeforeDoctypeName()
{
  if (MOZ_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errMissingSpaceBeforeDoctypeName");
  }
}

void
nsHtml5Tokenizer::errHyphenHyphenBang()
{
  if (MOZ_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errHyphenHyphenBang");
  }
}

void
nsHtml5Tokenizer::errNcrControlChar()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNcrControlChar");
  }
}

void
nsHtml5Tokenizer::errNcrZero()
{
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNcrZero");
  }
}

void
nsHtml5Tokenizer::errNoSpaceBetweenDoctypeSystemKeywordAndQuote()
{
  if (MOZ_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNoSpaceBetweenDoctypeSystemKeywordAndQuote");
  }
}

void
nsHtml5Tokenizer::errNoSpaceBetweenPublicAndSystemIds()
{
  if (MOZ_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNoSpaceBetweenPublicAndSystemIds");
  }
}

void
nsHtml5Tokenizer::errNoSpaceBetweenDoctypePublicKeywordAndQuote()
{
  if (MOZ_LIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentNode("errNoSpaceBetweenDoctypePublicKeywordAndQuote");
  }
}
