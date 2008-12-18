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

#ifndef __nsCommaSeparatedTokenizer_h
#define __nsCommaSeparatedTokenizer_h

#include "nsDependentSubstring.h"

/**
 * This parses a comma separated string into tokens. Whitespace surrounding
 * tokens are not treated as part of tokens, however whitespace inside a token
 * is. If the final token is the empty string it is not returned.
 *
 * Some examples:
 *
 * "foo, bar, baz" ->      "foo" "bar" "baz"
 * "foo,bar,baz" ->        "foo" "bar" "baz"
 * "foo , bar hi , baz" -> "foo" "bar hi" "baz"
 * "foo, ,bar,baz" ->      "foo" "" "bar" "baz"
 * "foo,,bar,baz" ->       "foo" "" "bar" "baz"
 * "foo,bar,baz," ->       "foo" "bar" "baz"
 */

class nsCommaSeparatedTokenizer
{
public:
    nsCommaSeparatedTokenizer(const nsSubstring& aSource)
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
    PRBool hasMoreTokens()
    {
        NS_ASSERTION(mIter == mEnd || !isWhitespace(*mIter),
                     "Should be at beginning of token if there is one");

        return mIter != mEnd;
    }

    PRBool lastTokenEndedWithComma()
    {
        return mLastTokenEndedWithComma;
    }

    /**
     * Returns the next token.
     */
    const nsDependentSubstring nextToken()
    {
        nsSubstring::const_char_iterator end = mIter, begin = mIter;

        NS_ASSERTION(mIter == mEnd || !isWhitespace(*mIter),
                     "Should be at beginning of token if there is one");

        // Search until we hit comma or end
        while (mIter != mEnd && *mIter != ',') {
          while (mIter != mEnd && !isWhitespace(*mIter) && *mIter != ',') {
              ++mIter;
          }
          end = mIter;

          while (mIter != mEnd && isWhitespace(*mIter)) {
              ++mIter;
          }
        }
        mLastTokenEndedWithComma = mIter != mEnd;

        // Skip comma
        if (mLastTokenEndedWithComma) {
            NS_ASSERTION(*mIter == ',', "Ended loop too soon");
            ++mIter;

            while (mIter != mEnd && isWhitespace(*mIter)) {
                ++mIter;
            }
        }
        
        return Substring(begin, end);
    }

private:
    nsSubstring::const_char_iterator mIter, mEnd;
    PRPackedBool mLastTokenEndedWithComma;

    PRBool isWhitespace(PRUnichar aChar)
    {
        return aChar <= ' ' &&
               (aChar == ' ' || aChar == '\n' ||
                aChar == '\r'|| aChar == '\t');
    }
};

class nsCCommaSeparatedTokenizer
{
public:
    nsCCommaSeparatedTokenizer(const nsCSubstring& aSource)
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
    PRBool hasMoreTokens()
    {
        return mIter != mEnd;
    }

    /**
     * Returns the next token.
     */
    const nsDependentCSubstring nextToken()
    {
        nsCSubstring::const_char_iterator end = mIter, begin = mIter;

        // Search until we hit comma or end
        while (mIter != mEnd && *mIter != ',') {
          while (mIter != mEnd && !isWhitespace(*mIter) && *mIter != ',') {
              ++mIter;
          }
          end = mIter;

          while (mIter != mEnd && isWhitespace(*mIter)) {
              ++mIter;
          }
        }
        
        // Skip comma
        if (mIter != mEnd) {
            NS_ASSERTION(*mIter == ',', "Ended loop too soon");
            ++mIter;

            while (mIter != mEnd && isWhitespace(*mIter)) {
                ++mIter;
            }
        }
        
        return Substring(begin, end);
    }

private:
    nsCSubstring::const_char_iterator mIter, mEnd;

    PRBool isWhitespace(unsigned char aChar)
    {
        return aChar <= ' ' &&
               (aChar == ' ' || aChar == '\n' ||
                aChar == '\r'|| aChar == '\t');
    }
};

#endif /* __nsWhitespaceTokenizer_h */
