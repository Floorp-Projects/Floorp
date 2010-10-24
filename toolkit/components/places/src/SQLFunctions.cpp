/* vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Places code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

#include "mozilla/storage.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsWhitespaceTokenizer.h"
#include "nsEscape.h"
#include "mozIPlacesAutoComplete.h"
#include "SQLFunctions.h"
#include "nsUTF8Utils.h"

using namespace mozilla::storage;

////////////////////////////////////////////////////////////////////////////////
//// Anonymous Helpers

namespace {

  typedef nsACString::const_char_iterator const_char_iterator;

  /**
   * Get a pointer to the word boundary after aStart if aStart points to an
   * ASCII letter (i.e. [a-zA-Z]).  Otherwise, return aNext, which we assume
   * points to the next character in the UTF-8 sequence.
   *
   * We define a word boundary as anything that's not [a-z] -- this lets us
   * match CamelCase words.
   *
   * @param aStart the beginning of the UTF-8 sequence
   * @param aNext the next character in the sequence
   * @param aEnd the first byte which is not part of the sequence
   *
   * @return a pointer to the next word boundary after aStart
   */
  static
  NS_ALWAYS_INLINE const_char_iterator
  nextWordBoundary(const_char_iterator const aStart,
                   const_char_iterator const aNext,
                   const_char_iterator const aEnd) {

    const_char_iterator cur = aStart;
    if (('a' <= *cur && *cur <= 'z') ||
        ('A' <= *cur && *cur <= 'Z')) {

      // Since we'll halt as soon as we see a non-ASCII letter, we can do a
      // simple byte-by-byte comparison here and avoid the overhead of a
      // UTF8CharEnumerator.
      do {
        cur++;
      } while (cur < aEnd && 'a' <= *cur && *cur <= 'z');
    }
    else {
      cur = aNext;
    }

    return cur;
  }

  enum FindInStringBehavior {
    eFindOnBoundary,
    eFindAnywhere
  };

  /**
   * findAnywhere and findOnBoundary do almost the same thing, so it's natural
   * to implement them in terms of a single function.  They're both
   * performance-critical functions, however, and checking aBehavior makes them
   * a bit slower.  Our solution is to define findInString as NS_ALWAYS_INLINE
   * and rely on the compiler to optimize out the aBehavior check.
   *
   * @param aToken
   *        The token we're searching for
   * @param aSourceString
   *        The string in which we're searching
   * @param aBehavior
   *        eFindOnBoundary if we should only consider matchines which occur on
   *        word boundaries, or eFindAnywhere if we should consider matches
   *        which appear anywhere.
   *
   * @return true if aToken was found in aSourceString, false otherwise.
   */
  static
  NS_ALWAYS_INLINE bool
  findInString(const nsDependentCSubstring &aToken,
               const nsACString &aSourceString,
               FindInStringBehavior aBehavior)
  {
    // CaseInsensitiveUTF8CharsEqual assumes that there's at least one byte in
    // the both strings, so don't pass an empty token here.
    NS_PRECONDITION(!aToken.IsEmpty(), "Don't search for an empty token!");

    // We cannot match anything if there is nothing to search.
    if (aSourceString.IsEmpty()) {
      return false;
    }

    const_char_iterator tokenStart(aToken.BeginReading()),
                        tokenEnd(aToken.EndReading()),
                        sourceStart(aSourceString.BeginReading()),
                        sourceEnd(aSourceString.EndReading());

    do {
      // We are on a word boundary (if aBehavior == eFindOnBoundary).  See if
      // aToken matches sourceStart.

      // Check whether the first character in the token matches the character
      // at sourceStart.  At the same time, get a pointer to the next character
      // in both the token and the source.
      const_char_iterator sourceNext, tokenCur;
      PRBool error;
      if (CaseInsensitiveUTF8CharsEqual(sourceStart, tokenStart,
                                        sourceEnd, tokenEnd,
                                        &sourceNext, &tokenCur, &error)) {

        // We don't need to check |error| here -- if
        // CaseInsensitiveUTF8CharCompare encounters an error, it'll also
        // return false and we'll catch the error outside the if.

        const_char_iterator sourceCur = sourceNext;
        while (true) {
          if (tokenCur >= tokenEnd) {
            // We matched the whole token!
            return true;
          }

          if (sourceCur >= sourceEnd) {
            // We ran into the end of source while matching a token.  This
            // means we'll never find the token we're looking for.
            return false;
          }

          if (!CaseInsensitiveUTF8CharsEqual(sourceCur, tokenCur,
                                             sourceEnd, tokenEnd,
                                             &sourceCur, &tokenCur, &error)) {
            // sourceCur doesn't match tokenCur (or there's an error), so break
            // out of this loop.
            break;
          }
        }
      }

      // If something went wrong above, get out of here!
      if (NS_UNLIKELY(error)) {
        return false;
      }

      // We didn't match the token.  If we're searching for matches on word
      // boundaries, skip to the next word boundary.  Otherwise, advance
      // forward one character, using the sourceNext pointer we saved earlier.

      if (aBehavior == eFindOnBoundary) {
        sourceStart = nextWordBoundary(sourceStart, sourceNext, sourceEnd);
      }
      else {
        sourceStart = sourceNext;
      }

    } while (sourceStart < sourceEnd);

    return false;
  }

} // End anonymous namespace

namespace mozilla {
namespace places {

////////////////////////////////////////////////////////////////////////////////
//// AutoComplete Matching Function

  //////////////////////////////////////////////////////////////////////////////
  //// MatchAutoCompleteFunction

  /* static */
  nsresult
  MatchAutoCompleteFunction::create(mozIStorageConnection *aDBConn)
  {
    nsRefPtr<MatchAutoCompleteFunction> function(new MatchAutoCompleteFunction);
    NS_ENSURE_TRUE(function, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = aDBConn->CreateFunction(
      NS_LITERAL_CSTRING("autocomplete_match"), kArgIndexLength, function
    );
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  /* static */
  void
  MatchAutoCompleteFunction::fixupURISpec(const nsCString &aURISpec,
                                          nsCString &_fixedSpec)
  {
    nsCString unescapedSpec;
    (void)NS_UnescapeURL(aURISpec, esc_SkipControl | esc_AlwaysCopy,
                         unescapedSpec);

    // If this unescaped string is valid UTF-8, we'll use it.  Otherwise,
    // we will simply use our original string.
    NS_ASSERTION(_fixedSpec.IsEmpty(),
                 "Passing a non-empty string as an out parameter!");
    if (IsUTF8(unescapedSpec))
      _fixedSpec.Assign(unescapedSpec);
    else
      _fixedSpec.Assign(aURISpec);

    if (StringBeginsWith(_fixedSpec, NS_LITERAL_CSTRING("http://")))
      _fixedSpec.Cut(0, 7);
    else if (StringBeginsWith(_fixedSpec, NS_LITERAL_CSTRING("https://")))
      _fixedSpec.Cut(0, 8);
    else if (StringBeginsWith(_fixedSpec, NS_LITERAL_CSTRING("ftp://")))
      _fixedSpec.Cut(0, 6);

    if (StringBeginsWith(_fixedSpec, NS_LITERAL_CSTRING("www.")))
      _fixedSpec.Cut(0, 4);
  }

  /* static */
  bool
  MatchAutoCompleteFunction::findAnywhere(const nsDependentCSubstring &aToken,
                                          const nsACString &aSourceString)
  {
    // We can't use FindInReadable here; it works only for ASCII.

    return findInString(aToken, aSourceString, eFindAnywhere);
  }

  /* static */
  bool
  MatchAutoCompleteFunction::findOnBoundary(const nsDependentCSubstring &aToken,
                                            const nsACString &aSourceString)
  {
    return findInString(aToken, aSourceString, eFindOnBoundary);
  }

  /* static */
  bool
  MatchAutoCompleteFunction::findBeginning(const nsDependentCSubstring &aToken,
                                           const nsACString &aSourceString)
  {
    NS_PRECONDITION(!aToken.IsEmpty(), "Don't search for an empty token!");

    // We can't use StringBeginsWith here, unfortunately.  Although it will
    // happily take a case-insensitive UTF8 comparator, it eventually calls
    // nsACString::Equals, which checks that the two strings contain the same
    // number of bytes before calling the comparator.  This is clearly not what
    // we want.

    const_char_iterator tokenStart(aToken.BeginReading()),
                        tokenEnd(aToken.EndReading()),
                        sourceStart(aSourceString.BeginReading()),
                        sourceEnd(aSourceString.EndReading());

    PRBool dummy;
    while (sourceStart < sourceEnd &&
           CaseInsensitiveUTF8CharsEqual(sourceStart, tokenStart,
                                         sourceEnd, tokenEnd,
                                         &sourceStart, &tokenStart, &dummy)) {

      // We found the token!
      if (tokenStart >= tokenEnd) {
        return true;
      }
    }

    // We don't need to check CaseInsensitiveUTF8CharsEqual's error condition
    // (stored in |dummy|), since the function will return false if it
    // encounters an error.

    return false;
  }

  /* static */
  MatchAutoCompleteFunction::searchFunctionPtr
  MatchAutoCompleteFunction::getSearchFunction(PRInt32 aBehavior)
  {
    switch (aBehavior) {
      case mozIPlacesAutoComplete::MATCH_ANYWHERE:
        return findAnywhere;
      case mozIPlacesAutoComplete::MATCH_BEGINNING:
        return findBeginning;
      case mozIPlacesAutoComplete::MATCH_BOUNDARY:
      default:
        return findOnBoundary;
    };
  }

  NS_IMPL_THREADSAFE_ISUPPORTS1(
    MatchAutoCompleteFunction,
    mozIStorageFunction
  )

  //////////////////////////////////////////////////////////////////////////////
  //// mozIStorageFunction

  NS_IMETHODIMP
  MatchAutoCompleteFunction::OnFunctionCall(mozIStorageValueArray *aArguments,
                                            nsIVariant **_result)
  {
    // Macro to make the code a bit cleaner and easier to read.  Operates on
    // searchBehavior.
    PRInt32 searchBehavior = aArguments->AsInt32(kArgIndexSearchBehavior);
    #define HAS_BEHAVIOR(aBitName) \
      (searchBehavior & mozIPlacesAutoComplete::BEHAVIOR_##aBitName)

    nsCAutoString searchString;
    (void)aArguments->GetUTF8String(kArgSearchString, searchString);
    nsCString url;
    (void)aArguments->GetUTF8String(kArgIndexURL, url);

    // We only want to filter javascript: URLs if we are not supposed to search
    // for them, and the search does not start with "javascript:".
    if (!HAS_BEHAVIOR(JAVASCRIPT) &&
        !StringBeginsWith(searchString, NS_LITERAL_CSTRING("javascript:")) &&
        StringBeginsWith(url, NS_LITERAL_CSTRING("javascript:"))) {
      NS_IF_ADDREF(*_result = new IntegerVariant(0));
      NS_ENSURE_TRUE(*_result, NS_ERROR_OUT_OF_MEMORY);
      return NS_OK;
    }

    PRInt32 visitCount = aArguments->AsInt32(kArgIndexVisitCount);
    bool typed = aArguments->AsInt32(kArgIndexTyped) ? true : false;
    bool bookmark = aArguments->AsInt32(kArgIndexBookmark) ? true : false;
    nsCAutoString tags;
    (void)aArguments->GetUTF8String(kArgIndexTags, tags);
    PRInt32 openPageCount = aArguments->AsInt32(kArgIndexOpenPageCount);

    // Make sure we match all the filter requirements.  If a given restriction
    // is active, make sure the corresponding condition is not true.
    bool matches = !(
      (HAS_BEHAVIOR(HISTORY) && visitCount == 0) ||
      (HAS_BEHAVIOR(TYPED) && !typed) ||
      (HAS_BEHAVIOR(BOOKMARK) && !bookmark) ||
      (HAS_BEHAVIOR(TAG) && tags.IsVoid()) ||
      (HAS_BEHAVIOR(OPENPAGE) && openPageCount == 0)
    );
    if (!matches) {
      NS_IF_ADDREF(*_result = new IntegerVariant(0));
      NS_ENSURE_TRUE(*_result, NS_ERROR_OUT_OF_MEMORY);
      return NS_OK;
    }

    // Clean up our URI spec and prepare it for searching.
    nsCString fixedURI;
    fixupURISpec(url, fixedURI);

    // Obtain our search function.
    PRInt32 matchBehavior = aArguments->AsInt32(kArgIndexMatchBehavior);
    searchFunctionPtr searchFunction = getSearchFunction(matchBehavior);

    nsCAutoString title;
    (void)aArguments->GetUTF8String(kArgIndexTitle, title);

    // Determine if every token matches either the bookmark title, tags, page
    // title, or page URL.
    nsCWhitespaceTokenizer tokenizer(searchString);
    while (matches && tokenizer.hasMoreTokens()) {
      const nsDependentCSubstring &token = tokenizer.nextToken();

      bool matchTags = searchFunction(token, tags);
      bool matchTitle = searchFunction(token, title);

      // Make sure we match something in the title or tags if we have to.
      matches = matchTags || matchTitle;
      if (HAS_BEHAVIOR(TITLE) && !matches)
        break;

      bool matchURL = searchFunction(token, fixedURI);
      // If we do not match the URL when we have to, reset matches to false.
      // Otherwise, keep track that we did match the current search.
      if (HAS_BEHAVIOR(URL) && !matchURL)
        matches = false;
      else
        matches = matches || matchURL;
    }

    NS_IF_ADDREF(*_result = new IntegerVariant(matches ? 1 : 0));
    NS_ENSURE_TRUE(*_result, NS_ERROR_OUT_OF_MEMORY);
    return NS_OK;
    #undef HAS_BEHAVIOR
  }

} // namespace places
} // namespace mozilla
