/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

nsAutoPtr<nsHtml5Highlighter> mViewSource;

/**
 * Starts handling text/plain. This is a one-way initialization. There is
 * no corresponding EndPlainText() call.
 */
void StartPlainText();

void EnableViewSource(nsHtml5Highlighter* aHighlighter);

bool FlushViewSource();

void StartViewSource(const nsAutoString& aTitle);

void EndViewSource();

void errGarbageAfterLtSlash();

void errLtSlashGt();

void errWarnLtSlashInRcdata();

void errCharRefLacksSemicolon();

void errNoDigitsInNCR();

void errGtInSystemId();

void errGtInPublicId();

void errNamelessDoctype();

void errConsecutiveHyphens();

void errPrematureEndOfComment();

void errBogusComment();

void errUnquotedAttributeValOrNull(PRUnichar c);

void errSlashNotFollowedByGt();

void errNoSpaceBetweenAttributes();

void errLtOrEqualsOrGraveInUnquotedAttributeOrNull(PRUnichar c);

void errAttributeValueMissing();

void errBadCharBeforeAttributeNameOrNull(PRUnichar c);

void errEqualsSignBeforeAttributeName();

void errBadCharAfterLt(PRUnichar c);

void errLtGt();

void errProcessingInstruction();

void errUnescapedAmpersandInterpretedAsCharacterReference();

void errNotSemicolonTerminated();

void errNoNamedCharacterMatch();

void errQuoteBeforeAttributeName(PRUnichar c);

void errQuoteOrLtInAttributeNameOrNull(PRUnichar c);

void errExpectedPublicId();

void errBogusDoctype();

void maybeErrAttributesOnEndTag(nsHtml5HtmlAttributes* attrs);

void maybeErrSlashInEndTag(bool selfClosing);

PRUnichar errNcrNonCharacter(PRUnichar ch);

void errAstralNonCharacter(int32_t ch);

void errNcrSurrogate();

PRUnichar errNcrControlChar(PRUnichar ch);

void errNcrCr();

void errNcrInC1Range();

void errEofInPublicId();

void errEofInComment();

void errEofInDoctype();

void errEofInAttributeValue();

void errEofInAttributeName();

void errEofWithoutGt();

void errEofInTagName();

void errEofInEndTag();

void errEofAfterLt();

void errNcrOutOfRange();

void errNcrUnassigned();

void errDuplicateAttribute();

void errEofInSystemId();

void errExpectedSystemId();

void errMissingSpaceBeforeDoctypeName();

void errHyphenHyphenBang();

void errNcrControlChar();

void errNcrZero();

void errNoSpaceBetweenDoctypeSystemKeywordAndQuote();

void errNoSpaceBetweenPublicAndSystemIds();

void errNoSpaceBetweenDoctypePublicKeywordAndQuote();
