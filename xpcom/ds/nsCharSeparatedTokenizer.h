/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
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

#ifndef __nsCharSeparatedTokenizer_h
#define __nsCharSeparatedTokenizer_h

#include "nsDependentSubstring.h"
#include "nsCRT.h"

/**
 * This parses a SeparatorChar-separated string into tokens.
 * Whitespace surrounding tokens is not treated as part of tokens, however
 * whitespace inside a token is. If the final token is the empty string, it is
 * not returned.
 *
 * Some examples, with SeparatorChar = ',':
 *
 * "foo, bar, baz" ->      "foo" "bar" "baz"
 * "foo,bar,baz" ->        "foo" "bar" "baz"
 * "foo , bar hi , baz" -> "foo" "bar hi" "baz"
 * "foo, ,bar,baz" ->      "foo" "" "bar" "baz"
 * "foo,,bar,baz" ->       "foo" "" "bar" "baz"
 * "foo,bar,baz," ->       "foo" "bar" "baz"
 *
 * The function used for whitespace detection is a template argument.
 * By default, it is NS_IsAsciiWhitespace.
 */
template<bool IsWhitespace(PRUnichar) = NS_IsAsciiWhitespace>
class nsCharSeparatedTokenizerTemplate
{
public:
    // Flags -- only one for now. If we need more, they should be defined to
    // be 1<<1, 1<<2, etc. (They're masks, and aFlags/mFlags are bitfields.)
    enum {
        SEPARATOR_OPTIONAL = 1
    };

    nsCharSeparatedTokenizerTemplate(const nsSubstring& aSource,
                                     PRUnichar aSeparatorChar,
                                     PRUint32  aFlags = 0)
        : mFirstTokenBeganWithWhitespace(PR_FALSE),
          mLastTokenEndedWithWhitespace(PR_FALSE),
          mLastTokenEndedWithSeparator(PR_FALSE),
          mSeparatorChar(aSeparatorChar),
          mFlags(aFlags)
    {
        aSource.BeginReading(mIter);
        aSource.EndReading(mEnd);

        // Skip initial whitespace
        while (mIter != mEnd && IsWhitespace(*mIter)) {
            mFirstTokenBeganWithWhitespace = PR_TRUE;
            ++mIter;
        }
    }

    /**
     * Checks if any more tokens are available.
     */
    bool hasMoreTokens()
    {
        NS_ASSERTION(mIter == mEnd || !IsWhitespace(*mIter),
                     "Should be at beginning of token if there is one");

        return mIter != mEnd;
    }

    bool firstTokenBeganWithWhitespace() const
    {
        return mFirstTokenBeganWithWhitespace;
    }

    bool lastTokenEndedWithSeparator() const
    {
        return mLastTokenEndedWithSeparator;
    }

    bool lastTokenEndedWithWhitespace() const
    {
        return mLastTokenEndedWithWhitespace;
    }

    /**
     * Returns the next token.
     */
    const nsDependentSubstring nextToken()
    {
        nsSubstring::const_char_iterator end = mIter, begin = mIter;

        NS_ASSERTION(mIter == mEnd || !IsWhitespace(*mIter),
                     "Should be at beginning of token if there is one");

        // Search until we hit separator or end (or whitespace, if separator
        // isn't required -- see clause with 'break' below).
        while (mIter != mEnd && *mIter != mSeparatorChar) {
          // Skip to end of current word.
          while (mIter != mEnd &&
                 !IsWhitespace(*mIter) && *mIter != mSeparatorChar) {
              ++mIter;
          }
          end = mIter;

          // Skip whitespace after current word.
          mLastTokenEndedWithWhitespace = PR_FALSE;
          while (mIter != mEnd && IsWhitespace(*mIter)) {
              mLastTokenEndedWithWhitespace = PR_TRUE;
              ++mIter;
          }
          if (mFlags & SEPARATOR_OPTIONAL) {
            // We've hit (and skipped) whitespace, and that's sufficient to end
            // our token, regardless of whether we've reached a SeparatorChar.
            break;
          } // (else, we'll keep looping until we hit mEnd or SeparatorChar)
        }

        mLastTokenEndedWithSeparator = (mIter != mEnd &&
                                        *mIter == mSeparatorChar);
        NS_ASSERTION((mFlags & SEPARATOR_OPTIONAL) ||
                     (mLastTokenEndedWithSeparator == (mIter != mEnd)),
                     "If we require a separator and haven't hit the end of "
                     "our string, then we shouldn't have left the loop "
                     "unless we hit a separator");

        // Skip separator (and any whitespace after it), if we're at one.
        if (mLastTokenEndedWithSeparator) {
            ++mIter;

            while (mIter != mEnd && IsWhitespace(*mIter)) {
                ++mIter;
            }
        }

        return Substring(begin, end);
    }

private:
    nsSubstring::const_char_iterator mIter, mEnd;
    bool mFirstTokenBeganWithWhitespace;
    bool mLastTokenEndedWithWhitespace;
    bool mLastTokenEndedWithSeparator;
    PRUnichar mSeparatorChar;
    PRUint32  mFlags;
};

class nsCharSeparatedTokenizer: public nsCharSeparatedTokenizerTemplate<>
{
public:
    nsCharSeparatedTokenizer(const nsSubstring& aSource,
                             PRUnichar aSeparatorChar,
                             PRUint32  aFlags = 0)
      : nsCharSeparatedTokenizerTemplate<>(aSource, aSeparatorChar, aFlags)
    {
    }
};

class nsCCharSeparatedTokenizer
{
public:
    nsCCharSeparatedTokenizer(const nsCSubstring& aSource,
                              char aSeparatorChar)
        : mSeparatorChar(aSeparatorChar)
    {
        aSource.BeginReading(mIter);
        aSource.EndReading(mEnd);

        while (mIter != mEnd && isWhitespace(*mIter)) {
            ++mIter;
        }
    }

    /**
     * Checks if any more tokens are available.
     */
    bool hasMoreTokens()
    {
        return mIter != mEnd;
    }

    /**
     * Returns the next token.
     */
    const nsDependentCSubstring nextToken()
    {
        nsCSubstring::const_char_iterator end = mIter, begin = mIter;

        // Search until we hit separator or end.
        while (mIter != mEnd && *mIter != mSeparatorChar) {
          while (mIter != mEnd &&
                 !isWhitespace(*mIter) && *mIter != mSeparatorChar) {
              ++mIter;
          }
          end = mIter;

          while (mIter != mEnd && isWhitespace(*mIter)) {
              ++mIter;
          }
        }

        // Skip separator (and any whitespace after it).
        if (mIter != mEnd) {
            NS_ASSERTION(*mIter == mSeparatorChar, "Ended loop too soon");
            ++mIter;

            while (mIter != mEnd && isWhitespace(*mIter)) {
                ++mIter;
            }
        }

        return Substring(begin, end);
    }

private:
    nsCSubstring::const_char_iterator mIter, mEnd;
    char mSeparatorChar;

    bool isWhitespace(unsigned char aChar)
    {
        return aChar <= ' ' &&
               (aChar == ' ' || aChar == '\n' ||
                aChar == '\r'|| aChar == '\t');
    }
};

#endif /* __nsCharSeparatedTokenizer_h */
