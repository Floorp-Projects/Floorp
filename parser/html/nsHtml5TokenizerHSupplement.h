/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

private:
inline void silentCarriageReturn() {
  nextCharOnNewLine = true;
  lastCR = true;
}

inline void silentLineFeed() { nextCharOnNewLine = true; }

inline char16_t checkChar(char16_t* buf, int32_t pos) {
  // The name of this method comes from the validator.
  // We aren't checking a char here. We read the next
  // UTF-16 code unit and, before returning it, adjust
  // the line and column numbers.
  char16_t c = buf[pos];
  if (MOZ_UNLIKELY(nextCharOnNewLine)) {
    // By changing the line and column here instead
    // of doing so eagerly when seeing the line break
    // causes the line break itself to be considered
    // column-wise at the end of a line.
    line++;
    col = 1;
    nextCharOnNewLine = false;
  } else if (MOZ_LIKELY(!NS_IS_LOW_SURROGATE(c))) {
    // SpiderMonkey wants to count scalar values
    // instead of UTF-16 code units. We omit low
    // surrogates from the count so that only the
    // high surrogate increments the count for
    // two-code-unit scalar values.
    //
    // It's somewhat questionable from the performance
    // perspective to make the human-perceivable column
    // count correct for non-BMP characters in the case
    // where there is a single scalar value per extended
    // grapheme cluster when even on the BMP there are
    // various cases where the scalar count doesn't make
    // much sense as a human-perceived "column count" due
    // to extended grapheme clusters consisting of more
    // than one scalar value.
    col++;
  }
  return c;
}

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
