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
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Gong <shawn@mozilla.com>
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

#ifndef mozilla_CharTokenizer_h
#define mozilla_CharTokenizer_h

#include "nsDependentSubstring.h"

namespace mozilla {

template<PRUnichar delimiter>
class CharTokenizer
{
public:
    CharTokenizer(const nsSubstring& aSource)
    {
      aSource.BeginReading(mIter);
      aSource.EndReading(mEnd);
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
    const nsDependentSubstring nextToken()
    {
      nsSubstring::const_char_iterator begin = mIter;
      while (mIter != mEnd && (*mIter) != delimiter) {
        ++mIter;
      }
      nsSubstring::const_char_iterator end = mIter;
      if (mIter != mEnd) {
        ++mIter;
      }

      return Substring(begin, end);
    }

private:
    nsSubstring::const_char_iterator mIter, mEnd;
};

template<char delimiter>
class CCharTokenizer
{
public:
    CCharTokenizer(const nsCSubstring& aSource)
    {
      aSource.BeginReading(mIter);
      aSource.EndReading(mEnd);
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
      nsCSubstring::const_char_iterator begin = mIter;
      while (mIter != mEnd && (*mIter) != delimiter) {
        ++mIter;
      }
      nsCSubstring::const_char_iterator end = mIter;
      if (mIter != mEnd) {
        ++mIter;
      }

      return Substring(begin, end);
    }

private:
    nsCSubstring::const_char_iterator mIter, mEnd;
};

} // namespace mozilla

#endif /* mozilla_CharTokenizer_h */
