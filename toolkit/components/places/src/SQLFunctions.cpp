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

using namespace mozilla::storage;

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
                                          nsString &_fixedSpec)
  {
    nsCString unescapedSpec;
    (void)NS_UnescapeURL(aURISpec, esc_SkipControl | esc_AlwaysCopy,
                         unescapedSpec);

    // If this unescaped string is valid UTF-8, we'll convert it.  Otherwise,
    // we will simply convert our original string.
    NS_ASSERTION(_fixedSpec.IsEmpty(),
                 "Passing a non-empty string as an out parameter!");
    if (IsUTF8(unescapedSpec))
      CopyUTF8toUTF16(unescapedSpec, _fixedSpec);
    else
      CopyUTF8toUTF16(aURISpec, _fixedSpec);

    if (StringBeginsWith(_fixedSpec, NS_LITERAL_STRING("http://")))
      _fixedSpec.Cut(0, 7);
    else if (StringBeginsWith(_fixedSpec, NS_LITERAL_STRING("https://")))
      _fixedSpec.Cut(0, 8);
    else if (StringBeginsWith(_fixedSpec, NS_LITERAL_STRING("ftp://")))
      _fixedSpec.Cut(0, 6);
  }

  /* static */
  bool
  MatchAutoCompleteFunction::findAnywhere(const nsDependentSubstring &aToken,
                                          const nsAString &aSourceString)
  {
    return !!CaseInsensitiveFindInReadable(aToken, aSourceString);
  }

  /* static */
  bool
  MatchAutoCompleteFunction::findBeginning(const nsDependentSubstring &aToken,
                                           const nsAString &aSourceString)
  {
    return !!StringBeginsWith(aSourceString, aToken,
                              nsCaseInsensitiveStringComparator());
  }

  /* static */
  bool
  MatchAutoCompleteFunction::findOnBoundary(const nsDependentSubstring &aToken,
                                            const nsAString &aSourceString)
  {
    // We cannot match anything if there is nothing to search.
    if (aSourceString.IsEmpty())
      return false;

    // Define a const instance of this class so it is created once.
    const nsCaseInsensitiveStringComparator caseInsensitiveCompare;

    const_wchar_iterator tokenStart(aToken.BeginReading()),
                         tokenEnd(aToken.EndReading()),
                         sourceStart(aSourceString.BeginReading()),
                         sourceEnd(aSourceString.EndReading());

    // The start of aSourceString is considered a word boundary, so start there.
    do {
      // We are on a word boundary, so start by copying the iterators.
      const_wchar_iterator testTokenItr(tokenStart),
                           testSourceItr(sourceStart);

      // Keep trying to match the token one by one until it doesn't match.
      while (!caseInsensitiveCompare(*testTokenItr, *testSourceItr)) {
        // We matched something, so move down one.
        testTokenItr++;
        testSourceItr++;

        // Matched the full token, so we are done!
        if (testTokenItr == tokenEnd)
          return true;

        // However, if we ran into the end of the source while matching the
        // token, we will not find it.
        if (testSourceItr == sourceEnd)
          return false;
      }

      // Always advance our starting iterator, and if we are not currently on a
      // word boundary, advance to the next word boundary.
      if (!isWordBoundary(ToLowerCase(*sourceStart++)))
        sourceStart = nextWordBoundary(sourceStart, sourceEnd);
    } while (sourceStart != sourceEnd);

    return false;
  }

  /* static */
  MatchAutoCompleteFunction::const_wchar_iterator
  MatchAutoCompleteFunction::nextWordBoundary(const_wchar_iterator aStart,
                                              const_wchar_iterator aEnd)
  {
    while (aStart != aEnd && !isWordBoundary(*aStart))
      aStart++;
    return aStart;
  }

  /* static */
  bool
  MatchAutoCompleteFunction::isWordBoundary(const PRUnichar &aChar)
  {
    // Only check lowercase alphabetic characters so we can match CamelCase
    // words.  This means that matches will happen after an upper-case
    // character.
    return !(PRUnichar('a') <= aChar && aChar <= PRUnichar('z'));
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

    nsAutoString searchString;
    (void)aArguments->GetString(kArgSearchString, searchString);
    nsCString url;
    (void)aArguments->GetUTF8String(kArgIndexURL, url);

    // We only want to filter javascript: URLs if we are not supposed to search
    // for them, and the search does not start with "javascript:".
    if (!HAS_BEHAVIOR(JAVASCRIPT) &&
        !StringBeginsWith(searchString, NS_LITERAL_STRING("javascript:")) &&
        StringBeginsWith(url, NS_LITERAL_CSTRING("javascript:"))) {
      NS_IF_ADDREF(*_result = new IntegerVariant(0));
      NS_ENSURE_TRUE(*_result, NS_ERROR_OUT_OF_MEMORY);
      return NS_OK;
    }

    PRInt32 visitCount = aArguments->AsInt32(kArgIndexVisitCount);
    bool typed = aArguments->AsInt32(kArgIndexTyped) ? true : false;
    bool bookmark = aArguments->AsInt32(kArgIndexBookmark) ? true : false;
    nsAutoString tags;
    (void)aArguments->GetString(kArgIndexTags, tags);

    // Make sure we match all the filter requirements.  If a given restriction
    // is active, make sure the corresponding condition is not true.
    bool matches = !(
      (HAS_BEHAVIOR(HISTORY) && visitCount == 0) ||
      (HAS_BEHAVIOR(TYPED) && !typed) ||
      (HAS_BEHAVIOR(BOOKMARK) && !bookmark) ||
      (HAS_BEHAVIOR(TAG) && tags.IsVoid())
    );
    if (!matches) {
      NS_IF_ADDREF(*_result = new IntegerVariant(0));
      NS_ENSURE_TRUE(*_result, NS_ERROR_OUT_OF_MEMORY);
      return NS_OK;
    }

    // Clean up our URI spec and prepare it for searching.
    nsString fixedURI;
    fixupURISpec(url, fixedURI);

    // Obtain our search function.
    PRInt32 matchBehavior = aArguments->AsInt32(kArgIndexMatchBehavior);
    searchFunctionPtr searchFunction = getSearchFunction(matchBehavior);

    nsAutoString title;
    (void)aArguments->GetString(kArgIndexTitle, title);

    // Determine if every token matches either the bookmark title, tags, page
    // title, or page URL.
    nsWhitespaceTokenizer tokenizer(searchString);
    while (matches && tokenizer.hasMoreTokens()) {
      const nsDependentSubstring &token = tokenizer.nextToken();

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
