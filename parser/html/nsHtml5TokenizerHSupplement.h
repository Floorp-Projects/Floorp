/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

friend struct nsHtml5ViewSourcePolicy;
friend struct nsHtml5LineColPolicy;
friend struct nsHtml5FastestPolicy;

private:
int32_t col;
bool nextCharOnNewLine;

public:
inline int32_t getColumnNumber() { return col; }

inline void setColumnNumberAndResetNextLine(int32_t aCol) {
  col = aCol;
  // The restored position only ever points to the position of
  // script tag's > character, so we can unconditionally use
  // `false` below.
  nextCharOnNewLine = false;
}

inline nsHtml5HtmlAttributes* GetAttributes() { return attributes; }

/**
 * Makes sure the buffers are large enough to be able to tokenize aLength
 * UTF-16 code units before having to make the buffers larger.
 *
 * @param aLength the number of UTF-16 code units to be tokenized before the
 *                next call to this method.
 * @return true if successful; false if out of memory
 */
bool EnsureBufferSpace(int32_t aLength);

bool TemplatePushedOrHeadPopped();

void RememberGt(int32_t aPos);

void AtKilobyteBoundary() { suspendAfterCurrentTokenIfNotInText(); }

bool IsInTokenStartedAtKilobyteBoundary() {
  return suspensionAfterCurrentNonTextTokenPending();
}

mozilla::UniquePtr<nsHtml5Highlighter> mViewSource;

/**
 * Starts handling text/plain. This is a one-way initialization. There is
 * no corresponding EndPlainText() call.
 */
void StartPlainText();

void EnableViewSource(nsHtml5Highlighter* aHighlighter);

bool ShouldFlushViewSource();

mozilla::Result<bool, nsresult> FlushViewSource();

void StartViewSource(const nsAutoString& aTitle);

void StartViewSourceCharacters();

[[nodiscard]] bool EndViewSource();

void RewindViewSource();

void SetViewSourceOpSink(nsAHtml5TreeOpSink* aOpSink);

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

void errUnquotedAttributeValOrNull(char16_t c);

void errSlashNotFollowedByGt();

void errNoSpaceBetweenAttributes();

void errLtOrEqualsOrGraveInUnquotedAttributeOrNull(char16_t c);

void errAttributeValueMissing();

void errBadCharBeforeAttributeNameOrNull(char16_t c);

void errEqualsSignBeforeAttributeName();

void errBadCharAfterLt(char16_t c);

void errLtGt();

void errProcessingInstruction();

void errUnescapedAmpersandInterpretedAsCharacterReference();

void errNotSemicolonTerminated();

void errNoNamedCharacterMatch();

void errQuoteBeforeAttributeName(char16_t c);

void errQuoteOrLtInAttributeNameOrNull(char16_t c);

void errExpectedPublicId();

void errBogusDoctype();

void maybeErrAttributesOnEndTag(nsHtml5HtmlAttributes* attrs);

void maybeErrSlashInEndTag(bool selfClosing);

char16_t errNcrNonCharacter(char16_t ch);

void errAstralNonCharacter(int32_t ch);

void errNcrSurrogate();

char16_t errNcrControlChar(char16_t ch);

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

void errNestedComment();

void errNcrControlChar();

void errNcrZero();

void errNoSpaceBetweenDoctypeSystemKeywordAndQuote();

void errNoSpaceBetweenPublicAndSystemIds();

void errNoSpaceBetweenDoctypePublicKeywordAndQuote();
