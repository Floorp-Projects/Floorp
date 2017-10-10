/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "nsString.h"
#include "nsUTF8Utils.h"
#include "prefread.h"

#ifdef TEST_PREFREAD
#include <stdio.h>
#define NS_WARNING(_s) printf(">>> " _s "!\n")
#define NS_NOTREACHED(_s) NS_WARNING(_s)
#else
#include "nsDebug.h" // for NS_WARNING
#endif

// Pref parser states.
enum
{
  PREF_PARSE_INIT,
  PREF_PARSE_MATCH_STRING,
  PREF_PARSE_UNTIL_NAME,
  PREF_PARSE_QUOTED_STRING,
  PREF_PARSE_UNTIL_COMMA,
  PREF_PARSE_UNTIL_VALUE,
  PREF_PARSE_INT_VALUE,
  PREF_PARSE_COMMENT_MAYBE_START,
  PREF_PARSE_COMMENT_BLOCK,
  PREF_PARSE_COMMENT_BLOCK_MAYBE_END,
  PREF_PARSE_ESC_SEQUENCE,
  PREF_PARSE_HEX_ESCAPE,
  PREF_PARSE_UTF16_LOW_SURROGATE,
  PREF_PARSE_UNTIL_OPEN_PAREN,
  PREF_PARSE_UNTIL_CLOSE_PAREN,
  PREF_PARSE_UNTIL_SEMICOLON,
  PREF_PARSE_UNTIL_EOL
};

#define UTF16_ESC_NUM_DIGITS 4
#define HEX_ESC_NUM_DIGITS 2
#define BITS_PER_HEX_DIGIT 4

static const char kUserPref[] = "user_pref";
static const char kPref[] = "pref";
static const char kPrefSticky[] = "sticky_pref";
static const char kTrue[] = "true";
static const char kFalse[] = "false";

// This function will increase the size of the buffer owned by the given pref
// parse state. We currently use a simple doubling algorithm, but the only hard
// requirement is that it increase the buffer by at least the size of the
// aPS->mEscTmp buffer used for escape processing (currently 6 bytes).
//
// The buffer is used to store partial pref lines. It is freed when the parse
// state is destroyed.
//
// @param aPS
//        parse state instance
//
// This function updates all pointers that reference an address within mLb
// since realloc may relocate the buffer.
//
// @return false if insufficient memory.
static bool
pref_GrowBuf(PrefParseState* aPS)
{
  int bufLen, curPos, valPos;

  bufLen = aPS->mLbEnd - aPS->mLb;
  curPos = aPS->mLbCur - aPS->mLb;
  valPos = aPS->mVb - aPS->mLb;

  if (bufLen == 0) {
    bufLen = 128; // default buffer size
  } else {
    bufLen <<= 1; // double buffer size
  }

#ifdef TEST_PREFREAD
  fprintf(stderr, ">>> realloc(%d)\n", bufLen);
#endif

  aPS->mLb = (char*)realloc(aPS->mLb, bufLen);
  if (!aPS->mLb) {
    return false;
  }

  aPS->mLbCur = aPS->mLb + curPos;
  aPS->mLbEnd = aPS->mLb + bufLen;
  aPS->mVb = aPS->mLb + valPos;

  return true;
}

// Report an error or a warning. If not specified, just dump to stderr.
static void
pref_ReportParseProblem(PrefParseState& aPS,
                        const char* aMessage,
                        int aLine,
                        bool aError)
{
  if (aPS.mReporter) {
    aPS.mReporter(aMessage, aLine, aError);
  } else {
    printf_stderr("**** Preference parsing %s (line %d) = %s **\n",
                  (aError ? "error" : "warning"),
                  aLine,
                  aMessage);
  }
}

// This function is called when a complete pref name-value pair has been
// extracted from the input data.
//
// @param aPS
//        parse state instance
//
// @return false to indicate a fatal error.
static bool
pref_DoCallback(PrefParseState* aPS)
{
  PrefValue value;

  switch (aPS->mVtype) {
    case PrefType::String:
      value.mStringVal = aPS->mVb;
      break;

    case PrefType::Int:
      if ((aPS->mVb[0] == '-' || aPS->mVb[0] == '+') && aPS->mVb[1] == '\0') {
        pref_ReportParseProblem(*aPS, "invalid integer value", 0, true);
        NS_WARNING("malformed integer value");
        return false;
      }
      value.mIntVal = atoi(aPS->mVb);
      break;

    case PrefType::Bool:
      value.mBoolVal = (aPS->mVb == kTrue);
      break;

    default:
      break;
  }

  (*aPS->mReader)(aPS->mClosure,
                  aPS->mLb,
                  value,
                  aPS->mVtype,
                  aPS->mIsDefault,
                  aPS->mIsStickyDefault);
  return true;
}

void
PREF_InitParseState(PrefParseState* aPS,
                    PrefReader aReader,
                    PrefParseErrorReporter aReporter,
                    void* aClosure)
{
  memset(aPS, 0, sizeof(*aPS));
  aPS->mReader = aReader;
  aPS->mClosure = aClosure;
  aPS->mReporter = aReporter;
}

void
PREF_FinalizeParseState(PrefParseState* aPS)
{
  if (aPS->mLb) {
    free(aPS->mLb);
  }
}

// Pseudo-BNF
// ----------
// function      = LJUNK function-name JUNK function-args
// function-name = "user_pref" | "pref" | "sticky_pref"
// function-args = "(" JUNK pref-name JUNK "," JUNK pref-value JUNK ")" JUNK ";"
// pref-name     = quoted-string
// pref-value    = quoted-string | "true" | "false" | integer-value
// JUNK          = *(WS | comment-block | comment-line)
// LJUNK         = *(WS | comment-block | comment-line | bcomment-line)
// WS            = SP | HT | LF | VT | FF | CR
// SP            = <US-ASCII SP, space (32)>
// HT            = <US-ASCII HT, horizontal-tab (9)>
// LF            = <US-ASCII LF, linefeed (10)>
// VT            = <US-ASCII HT, vertical-tab (11)>
// FF            = <US-ASCII FF, form-feed (12)>
// CR            = <US-ASCII CR, carriage return (13)>
// comment-block = <C/C++ style comment block>
// comment-line  = <C++ style comment line>
// bcomment-line = <bourne-shell style comment line>
//
bool
PREF_ParseBuf(PrefParseState* aPS, const char* aBuf, int aBufLen)
{
  const char* end;
  char c;
  char udigit;
  int state;

  // The line number is currently only used for the error/warning reporting.
  int lineNum = 0;

  state = aPS->mState;
  for (end = aBuf + aBufLen; aBuf != end; ++aBuf) {
    c = *aBuf;
    if (c == '\r' || c == '\n' || c == 0x1A) {
      lineNum++;
    }

    switch (state) {
      // initial state
      case PREF_PARSE_INIT:
        if (aPS->mLbCur != aPS->mLb) { // reset state
          aPS->mLbCur = aPS->mLb;
          aPS->mVb = nullptr;
          aPS->mVtype = PrefType::Invalid;
          aPS->mIsDefault = false;
          aPS->mIsStickyDefault = false;
        }
        switch (c) {
          case '/': // begin comment block or line?
            state = PREF_PARSE_COMMENT_MAYBE_START;
            break;
          case '#': // accept shell style comments
            state = PREF_PARSE_UNTIL_EOL;
            break;
          case 'u': // indicating user_pref
          case 's': // indicating sticky_pref
          case 'p': // indicating pref
            if (c == 'u') {
              aPS->mStrMatch = kUserPref;
            } else if (c == 's') {
              aPS->mStrMatch = kPrefSticky;
            } else {
              aPS->mStrMatch = kPref;
            }
            aPS->mStrIndex = 1;
            aPS->mNextState = PREF_PARSE_UNTIL_OPEN_PAREN;
            state = PREF_PARSE_MATCH_STRING;
            break;
            // else skip char
        }
        break;

      // string matching
      case PREF_PARSE_MATCH_STRING:
        if (c == aPS->mStrMatch[aPS->mStrIndex++]) {
          // If we've matched all characters, then move to next state.
          if (aPS->mStrMatch[aPS->mStrIndex] == '\0') {
            state = aPS->mNextState;
            aPS->mNextState = PREF_PARSE_INIT; // reset next state
          }
          // else wait for next char
        } else {
          pref_ReportParseProblem(*aPS, "non-matching string", lineNum, true);
          NS_WARNING("malformed pref file");
          return false;
        }
        break;

      // quoted string parsing
      case PREF_PARSE_QUOTED_STRING:
        // we assume that the initial quote has already been consumed
        if (aPS->mLbCur == aPS->mLbEnd && !pref_GrowBuf(aPS)) {
          return false; // out of memory
        }
        if (c == '\\') {
          state = PREF_PARSE_ESC_SEQUENCE;
        } else if (c == aPS->mQuoteChar) {
          *aPS->mLbCur++ = '\0';
          state = aPS->mNextState;
          aPS->mNextState = PREF_PARSE_INIT; // reset next state
        } else {
          *aPS->mLbCur++ = c;
        }
        break;

      // name parsing
      case PREF_PARSE_UNTIL_NAME:
        if (c == '\"' || c == '\'') {
          aPS->mIsDefault =
            (aPS->mStrMatch == kPref || aPS->mStrMatch == kPrefSticky);
          aPS->mIsStickyDefault = (aPS->mStrMatch == kPrefSticky);
          aPS->mQuoteChar = c;
          aPS->mNextState = PREF_PARSE_UNTIL_COMMA; // return here when done
          state = PREF_PARSE_QUOTED_STRING;
        } else if (c == '/') {     // allow embedded comment
          aPS->mNextState = state; // return here when done with comment
          state = PREF_PARSE_COMMENT_MAYBE_START;
        } else if (!isspace(c)) {
          pref_ReportParseProblem(
            *aPS, "need space, comment or quote", lineNum, true);
          NS_WARNING("malformed pref file");
          return false;
        }
        break;

      // parse until we find a comma separating name and value
      case PREF_PARSE_UNTIL_COMMA:
        if (c == ',') {
          aPS->mVb = aPS->mLbCur;
          state = PREF_PARSE_UNTIL_VALUE;
        } else if (c == '/') {     // allow embedded comment
          aPS->mNextState = state; // return here when done with comment
          state = PREF_PARSE_COMMENT_MAYBE_START;
        } else if (!isspace(c)) {
          pref_ReportParseProblem(
            *aPS, "need space, comment or comma", lineNum, true);
          NS_WARNING("malformed pref file");
          return false;
        }
        break;

      // value parsing
      case PREF_PARSE_UNTIL_VALUE:
        // The pref value type is unknown. So, we scan for the first character
        // of the value, and determine the type from that.
        if (c == '\"' || c == '\'') {
          aPS->mVtype = PrefType::String;
          aPS->mQuoteChar = c;
          aPS->mNextState = PREF_PARSE_UNTIL_CLOSE_PAREN;
          state = PREF_PARSE_QUOTED_STRING;
        } else if (c == 't' || c == 'f') {
          aPS->mVb = (char*)(c == 't' ? kTrue : kFalse);
          aPS->mVtype = PrefType::Bool;
          aPS->mStrMatch = aPS->mVb;
          aPS->mStrIndex = 1;
          aPS->mNextState = PREF_PARSE_UNTIL_CLOSE_PAREN;
          state = PREF_PARSE_MATCH_STRING;
        } else if (isdigit(c) || (c == '-') || (c == '+')) {
          aPS->mVtype = PrefType::Int;
          // write c to line buffer...
          if (aPS->mLbCur == aPS->mLbEnd && !pref_GrowBuf(aPS)) {
            return false; // out of memory
          }
          *aPS->mLbCur++ = c;
          state = PREF_PARSE_INT_VALUE;
        } else if (c == '/') {     // allow embedded comment
          aPS->mNextState = state; // return here when done with comment
          state = PREF_PARSE_COMMENT_MAYBE_START;
        } else if (!isspace(c)) {
          pref_ReportParseProblem(
            *aPS, "need value, comment or space", lineNum, true);
          NS_WARNING("malformed pref file");
          return false;
        }
        break;

      case PREF_PARSE_INT_VALUE:
        // grow line buffer if necessary...
        if (aPS->mLbCur == aPS->mLbEnd && !pref_GrowBuf(aPS)) {
          return false; // out of memory
        }
        if (isdigit(c)) {
          *aPS->mLbCur++ = c;
        } else {
          *aPS->mLbCur++ = '\0'; // stomp null terminator; we are done.
          if (c == ')') {
            state = PREF_PARSE_UNTIL_SEMICOLON;
          } else if (c == '/') { // allow embedded comment
            aPS->mNextState = PREF_PARSE_UNTIL_CLOSE_PAREN;
            state = PREF_PARSE_COMMENT_MAYBE_START;
          } else if (isspace(c)) {
            state = PREF_PARSE_UNTIL_CLOSE_PAREN;
          } else {
            pref_ReportParseProblem(
              *aPS, "while parsing integer", lineNum, true);
            NS_WARNING("malformed pref file");
            return false;
          }
        }
        break;

      // comment parsing
      case PREF_PARSE_COMMENT_MAYBE_START:
        switch (c) {
          case '*': // comment block
            state = PREF_PARSE_COMMENT_BLOCK;
            break;
          case '/': // comment line
            state = PREF_PARSE_UNTIL_EOL;
            break;
          default:
            // pref file is malformed
            pref_ReportParseProblem(
              *aPS, "while parsing comment", lineNum, true);
            NS_WARNING("malformed pref file");
            return false;
        }
        break;

      case PREF_PARSE_COMMENT_BLOCK:
        if (c == '*') {
          state = PREF_PARSE_COMMENT_BLOCK_MAYBE_END;
        }
        break;

      case PREF_PARSE_COMMENT_BLOCK_MAYBE_END:
        switch (c) {
          case '/':
            state = aPS->mNextState;
            aPS->mNextState = PREF_PARSE_INIT;
            break;
          case '*': // stay in this state
            break;
          default:
            state = PREF_PARSE_COMMENT_BLOCK;
            break;
        }
        break;

      // string escape sequence parsing
      case PREF_PARSE_ESC_SEQUENCE:
        // It's not necessary to resize the buffer here since we should be
        // writing only one character and the resize check would have been done
        // for us in the previous state.
        switch (c) {
          case '\"':
          case '\'':
          case '\\':
            break;
          case 'r':
            c = '\r';
            break;
          case 'n':
            c = '\n';
            break;
          case 'x': // hex escape -- always interpreted as Latin-1
          case 'u': // UTF16 escape
            aPS->mEscTmp[0] = c;
            aPS->mEscLen = 1;
            aPS->mUtf16[0] = aPS->mUtf16[1] = 0;
            aPS->mStrIndex =
              (c == 'x') ? HEX_ESC_NUM_DIGITS : UTF16_ESC_NUM_DIGITS;
            state = PREF_PARSE_HEX_ESCAPE;
            continue;
          default:
            pref_ReportParseProblem(
              *aPS, "preserving unexpected JS escape sequence", lineNum, false);
            NS_WARNING("preserving unexpected JS escape sequence");
            // Invalid escape sequence so we do have to write more than one
            // character. Grow line buffer if necessary...
            if ((aPS->mLbCur + 1) == aPS->mLbEnd && !pref_GrowBuf(aPS)) {
              return false; // out of memory
            }
            *aPS->mLbCur++ = '\\'; // preserve the escape sequence
            break;
        }
        *aPS->mLbCur++ = c;
        state = PREF_PARSE_QUOTED_STRING;
        break;

      // parsing a hex (\xHH) or mUtf16 escape (\uHHHH)
      case PREF_PARSE_HEX_ESCAPE:
        if (c >= '0' && c <= '9') {
          udigit = (c - '0');
        } else if (c >= 'A' && c <= 'F') {
          udigit = (c - 'A') + 10;
        } else if (c >= 'a' && c <= 'f') {
          udigit = (c - 'a') + 10;
        } else {
          // bad escape sequence found, write out broken escape as-is
          pref_ReportParseProblem(*aPS,
                                  "preserving invalid or incomplete hex escape",
                                  lineNum,
                                  false);
          NS_WARNING("preserving invalid or incomplete hex escape");
          *aPS->mLbCur++ = '\\'; // original escape slash
          if ((aPS->mLbCur + aPS->mEscLen) >= aPS->mLbEnd &&
              !pref_GrowBuf(aPS)) {
            return false;
          }
          for (int i = 0; i < aPS->mEscLen; ++i) {
            *aPS->mLbCur++ = aPS->mEscTmp[i];
          }

          // Push the non-hex character back for re-parsing. (++aBuf at the top
          // of the loop keeps this safe.)
          --aBuf;
          state = PREF_PARSE_QUOTED_STRING;
          continue;
        }

        // have a digit
        aPS->mEscTmp[aPS->mEscLen++] = c; // preserve it
        aPS->mUtf16[1] <<= BITS_PER_HEX_DIGIT;
        aPS->mUtf16[1] |= udigit;
        aPS->mStrIndex--;
        if (aPS->mStrIndex == 0) {
          // we have the full escape, convert to UTF8
          int utf16len = 0;
          if (aPS->mUtf16[0]) {
            // already have a high surrogate, this is a two char seq
            utf16len = 2;
          } else if (0xD800 == (0xFC00 & aPS->mUtf16[1])) {
            // a high surrogate, can't convert until we have the low
            aPS->mUtf16[0] = aPS->mUtf16[1];
            aPS->mUtf16[1] = 0;
            state = PREF_PARSE_UTF16_LOW_SURROGATE;
            break;
          } else {
            // a single mUtf16 character
            aPS->mUtf16[0] = aPS->mUtf16[1];
            utf16len = 1;
          }

          // The actual conversion.
          // Make sure there's room, 6 bytes is max utf8 len (in theory; 4
          // bytes covers the actual mUtf16 range).
          if (aPS->mLbCur + 6 >= aPS->mLbEnd && !pref_GrowBuf(aPS)) {
            return false;
          }

          ConvertUTF16toUTF8 converter(aPS->mLbCur);
          converter.write(aPS->mUtf16, utf16len);
          aPS->mLbCur += converter.Size();
          state = PREF_PARSE_QUOTED_STRING;
        }
        break;

      // looking for beginning of mUtf16 low surrogate
      case PREF_PARSE_UTF16_LOW_SURROGATE:
        if (aPS->mStrIndex == 0 && c == '\\') {
          ++aPS->mStrIndex;
        } else if (aPS->mStrIndex == 1 && c == 'u') {
          // escape sequence is correct, now parse hex
          aPS->mStrIndex = UTF16_ESC_NUM_DIGITS;
          aPS->mEscTmp[0] = 'u';
          aPS->mEscLen = 1;
          state = PREF_PARSE_HEX_ESCAPE;
        } else {
          // Didn't find expected low surrogate. Ignore high surrogate (it
          // would just get converted to nothing anyway) and start over with
          // this character.
          --aBuf;
          if (aPS->mStrIndex == 1) {
            state = PREF_PARSE_ESC_SEQUENCE;
          } else {
            state = PREF_PARSE_QUOTED_STRING;
          }
          continue;
        }
        break;

      // function open and close parsing
      case PREF_PARSE_UNTIL_OPEN_PAREN:
        // tolerate only whitespace and embedded comments
        if (c == '(') {
          state = PREF_PARSE_UNTIL_NAME;
        } else if (c == '/') {
          aPS->mNextState = state; // return here when done with comment
          state = PREF_PARSE_COMMENT_MAYBE_START;
        } else if (!isspace(c)) {
          pref_ReportParseProblem(
            *aPS, "need space, comment or open parentheses", lineNum, true);
          NS_WARNING("malformed pref file");
          return false;
        }
        break;

      case PREF_PARSE_UNTIL_CLOSE_PAREN:
        // tolerate only whitespace and embedded comments
        if (c == ')') {
          state = PREF_PARSE_UNTIL_SEMICOLON;
        } else if (c == '/') {
          aPS->mNextState = state; // return here when done with comment
          state = PREF_PARSE_COMMENT_MAYBE_START;
        } else if (!isspace(c)) {
          pref_ReportParseProblem(
            *aPS, "need space, comment or closing parentheses", lineNum, true);
          NS_WARNING("malformed pref file");
          return false;
        }
        break;

      // function terminator ';' parsing
      case PREF_PARSE_UNTIL_SEMICOLON:
        // tolerate only whitespace and embedded comments
        if (c == ';') {
          if (!pref_DoCallback(aPS)) {
            return false;
          }
          state = PREF_PARSE_INIT;
        } else if (c == '/') {
          aPS->mNextState = state; // return here when done with comment
          state = PREF_PARSE_COMMENT_MAYBE_START;
        } else if (!isspace(c)) {
          pref_ReportParseProblem(
            *aPS, "need space, comment or semicolon", lineNum, true);
          NS_WARNING("malformed pref file");
          return false;
        }
        break;

      // eol parsing
      case PREF_PARSE_UNTIL_EOL:
        // Need to handle mac, unix, or dos line endings. PREF_PARSE_INIT will
        // eat the next \n in case we have \r\n.
        if (c == '\r' || c == '\n' || c == 0x1A) {
          state = aPS->mNextState;
          aPS->mNextState = PREF_PARSE_INIT; // reset next state
        }
        break;
    }
  }
  aPS->mState = state;
  return true;
}

#ifdef TEST_PREFREAD

static void
pref_reader(void* aClosure,
            const char* aPref,
            PrefValue aVal,
            PrefType aType,
            bool aDefPref)
{
  printf("%spref(\"%s\", ", aDefPref ? "" : "user_", aPref);
  switch (aType) {
    case PREF_STRING:
      printf("\"%s\");\n", aVal.mStringVal);
      break;
    case PREF_INT:
      printf("%i);\n", aVal.mIntVal);
      break;
    case PREF_BOOL:
      printf("%s);\n", aVal.mBoolVal == false ? "false" : "true");
      break;
  }
}

int
main(int aArgc, char** aArgv)
{
  PrefParseState aPS;
  char buf[4096]; // i/o buffer
  FILE* fp;
  int n;

  if (aArgc == 1) {
    printf("usage: prefread file.js\n");
    return -1;
  }

  fp = fopen(aArgv[1], "r");
  if (!fp) {
    printf("failed to open file\n");
    return -1;
  }

  PREF_InitParseState(&aPS, pref_reader, nullptr, nullptr);

  while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
    PREF_ParseBuf(&aPS, buf, n);
  }

  PREF_FinalizeParseState(&aPS);

  fclose(fp);
  return 0;
}

#endif // TEST_PREFREAD
