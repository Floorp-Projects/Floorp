/* vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/storage.h"
#include "mozilla/dom/URLSearchParams.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsWhitespaceTokenizer.h"
#include "nsEscape.h"
#include "mozIPlacesAutoComplete.h"
#include "SQLFunctions.h"
#include "nsMathUtils.h"
#include "nsUnicodeProperties.h"
#include "nsUTF8Utils.h"
#include "nsINavHistoryService.h"
#include "nsPrintfCString.h"
#include "nsNavHistory.h"
#include "mozilla/Likely.h"
#include "nsVariant.h"

// Maximum number of chars to search through.
// MatchAutoCompleteFunction won't look for matches over this threshold.
#define MAX_CHARS_TO_SEARCH_THROUGH 255

using namespace mozilla::storage;

////////////////////////////////////////////////////////////////////////////////
//// Anonymous Helpers

namespace {

  typedef nsACString::const_char_iterator const_char_iterator;
  typedef nsACString::size_type size_type;
  typedef nsACString::char_type char_type;

  /**
   * Scan forward through UTF-8 text until the next potential character that
   * could match a given codepoint when lower-cased (false positives are okay).
   * This avoids having to actually parse the UTF-8 text, which is slow.
   *
   * @param aStart
   *        An iterator pointing to the first character position considered.
   * @param aEnd
   *        An interator pointing to past-the-end of the string.
   *
   * @return An iterator pointing to the first potential matching character
   *         within the range [aStart, aEnd).
   */
  static
  MOZ_ALWAYS_INLINE const_char_iterator
  nextSearchCandidate(const_char_iterator aStart,
                      const_char_iterator aEnd,
                      uint32_t aSearchFor)
  {
    const_char_iterator cur = aStart;

    // If the character we search for is ASCII, then we can scan until we find
    // it or its ASCII uppercase character, modulo the special cases
    // U+0130 LATIN CAPITAL LETTER I WITH DOT ABOVE and U+212A KELVIN SIGN
    // (which are the only non-ASCII characters that lower-case to ASCII ones).
    // Since false positives are okay, we approximate ASCII lower-casing by
    // bit-ORing with 0x20, for increased performance.
    //
    // If the character we search for is *not* ASCII, we can ignore everything
    // that is, since all ASCII characters lower-case to ASCII.
    //
    // Because of how UTF-8 uses high-order bits, this will never land us
    // in the middle of a codepoint.
    //
    // The assumptions about Unicode made here are verified in the test_casing
    // gtest.
    if (aSearchFor < 128) {
      // When searching for I or K, we pick out the first byte of the UTF-8
      // encoding of the corresponding special case character, and look for it
      // in the loop below.  For other characters we fall back to 0xff, which
      // is not a valid UTF-8 byte.
      unsigned char target = (unsigned char)(aSearchFor | 0x20);
      unsigned char special = 0xff;
      if (target == 'i' || target == 'k') {
        special = (target == 'i' ? 0xc4 : 0xe2);
      }

      while (cur < aEnd && (unsigned char)(*cur | 0x20) != target &&
          (unsigned char)*cur != special) {
        cur++;
      }
    } else {
      const_char_iterator cur = aStart;
      while (cur < aEnd && (unsigned char)(*cur) < 128) {
        cur++;
      }
    }

    return cur;
  }

  /**
   * Check whether a character position is on a word boundary of a UTF-8 string
   * (rather than within a word).  We define "within word" to be any position
   * between [a-zA-Z] and [a-z] -- this lets us match CamelCase words.
   * TODO: support non-latin alphabets.
   *
   * @param aPos
   *        An iterator pointing to the character position considered.  It must
   *        *not* be the first byte of a string.
   *
   * @return true if boundary, false otherwise.
   */
  static
  MOZ_ALWAYS_INLINE bool
  isOnBoundary(const_char_iterator aPos) {
    if ('a' <= *aPos && *aPos <= 'z') {
      char prev = *(aPos - 1) | 0x20;
      return !('a' <= prev && prev <= 'z');
    }
    return true;
  }

  /**
   * Check whether a token string matches a particular position of a source
   * string, case insensitively.
   *
   * @param aTokenStart
   *        An iterator pointing to the start of the token string.
   * @param aTokenEnd
   *        An iterator pointing past-the-end of the token string.
   * @param aSourceStart
   *        An iterator pointing to the position of source string to start
   *        matching at.
   * @param aSourceEnd
   *        An iterator pointing past-the-end of the source string.
   *
   * @return true if the string [aTokenStart, aTokenEnd) matches the start of
   *         the string [aSourceStart, aSourceEnd, false otherwise.
   */
  static
  MOZ_ALWAYS_INLINE bool
  stringMatch(const_char_iterator aTokenStart,
              const_char_iterator aTokenEnd,
              const_char_iterator aSourceStart,
              const_char_iterator aSourceEnd)
  {
    const_char_iterator tokenCur = aTokenStart, sourceCur = aSourceStart;

    while (tokenCur < aTokenEnd) {
      if (sourceCur >= aSourceEnd) {
        return false;
      }

      bool error;
      if (!CaseInsensitiveUTF8CharsEqual(sourceCur, tokenCur,
                                         aSourceEnd, aTokenEnd,
                                         &sourceCur, &tokenCur, &error)) {
        return false;
      }
    }

    return true;
  }

  enum FindInStringBehavior {
    eFindOnBoundary,
    eFindAnywhere
  };

  /**
   * Common implementation for findAnywhere and findOnBoundary.
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
  bool
  findInString(const nsDependentCSubstring &aToken,
               const nsACString &aSourceString,
               FindInStringBehavior aBehavior)
  {
    // GetLowerUTF8Codepoint assumes that there's at least one byte in
    // the string, so don't pass an empty token here.
    MOZ_ASSERT(!aToken.IsEmpty(), "Don't search for an empty token!");

    // We cannot match anything if there is nothing to search.
    if (aSourceString.IsEmpty()) {
      return false;
    }

    const_char_iterator tokenStart(aToken.BeginReading()),
                        tokenEnd(aToken.EndReading()),
                        tokenNext,
                        sourceStart(aSourceString.BeginReading()),
                        sourceEnd(aSourceString.EndReading()),
                        sourceCur(sourceStart),
                        sourceNext;

    uint32_t tokenFirstChar =
      GetLowerUTF8Codepoint(tokenStart, tokenEnd, &tokenNext);
    if (tokenFirstChar == uint32_t(-1)) {
      return false;
    }

    for (;;) {
      // Scan forward to the next viable candidate (if any).
      sourceCur = nextSearchCandidate(sourceCur, sourceEnd, tokenFirstChar);
      if (sourceCur == sourceEnd) {
        break;
      }

      // Check whether the first character in the token matches the character
      // at sourceCur.  At the same time, get a pointer to the next character
      // in the source.
      uint32_t sourceFirstChar =
        GetLowerUTF8Codepoint(sourceCur, sourceEnd, &sourceNext);
      if (sourceFirstChar == uint32_t(-1)) {
        return false;
      }

      if (sourceFirstChar == tokenFirstChar &&
          (aBehavior != eFindOnBoundary || sourceCur == sourceStart ||
              isOnBoundary(sourceCur)) &&
          stringMatch(tokenNext, tokenEnd, sourceNext, sourceEnd))
      {
        return true;
      }

      sourceCur = sourceNext;
    }

    return false;
  }

  static
  MOZ_ALWAYS_INLINE nsDependentCString
  getSharedUTF8String(mozIStorageValueArray* aValues, uint32_t aIndex) {
    uint32_t len;
    const char* str = aValues->AsSharedUTF8String(aIndex, &len);
    if (!str) {
      return nsDependentCString("", (uint32_t)0);
    }
    return nsDependentCString(str, len);
  }

  class MOZ_STACK_CLASS GetQueryParamIterator final :
    public URLParams::ForEachIterator
  {
  public:
    explicit GetQueryParamIterator(const nsCString& aParamName,
                                   nsVariant* aResult)
      : mParamName(aParamName)
      , mResult(aResult)
    {}

    bool URLParamsIterator(const nsAString& aName,
                           const nsAString& aValue) override
    {
      NS_ConvertUTF16toUTF8 name(aName);
      if (!mParamName.Equals(name)) {
        return true;
      }
      mResult->SetAsAString(aValue);
      return false;
    }
  private:
    const nsCString& mParamName;
    nsVariant* mResult;
  };

  /**
   * Gets the length of the prefix in a URI spec.  "Prefix" is defined to be the
   * scheme, colon, and, if present, two slashes.
   *
   * Examples:
   *
   *   http://example.com
   *   ~~~~~~~
   *   => length == 7
   *
   *   foo:example
   *   ~~~~
   *   => length == 4
   *
   *   not a spec
   *   => length == 0
   *
   * @param  aSpec
   *         A URI spec, or a string that may be a URI spec.
   * @return The length of the prefix in the spec.  If there isn't a prefix,
   *         returns 0.
   */
  static
  MOZ_ALWAYS_INLINE size_type
  getPrefixLength(const nsACString &aSpec)
  {
    // To keep the search bounded, look at 64 characters at most.  The longest
    // IANA schemes are ~30, so double that and round up to a nice number.
    size_type length = std::min(static_cast<size_type>(64), aSpec.Length());
    for (size_type i = 0; i < length; ++i) {
      if (aSpec[i] == static_cast<char_type>(':')) {
        // Found the ':'.  Now skip past "//", if present.
        if (i + 2 < aSpec.Length() &&
            aSpec[i + 1] == static_cast<char_type>('/') &&
            aSpec[i + 2] == static_cast<char_type>('/')) {
          i += 2;
        }
        return i + 1;
      }
    }
    return 0;
  }

  /**
   * Gets the index in a URI spec of the host and port substring and optionally
   * its length.
   *
   * Examples:
   *
   *   http://example.com/
   *          ~~~~~~~~~~~
   *   => index == 7, length == 11
   *
   *   http://example.com:8888/
   *          ~~~~~~~~~~~~~~~~
   *   => index == 7, length == 16
   *
   *   http://user:pass@example.com/
   *                    ~~~~~~~~~~~
   *   => index == 17, length == 11
   *
   *   foo:example
   *       ~~~~~~~
   *   => index == 4, length == 7
   *
   *   not a spec
   *   ~~~~~~~~~~
   *   => index == 0, length == 10
   *
   * @param  aSpec
   *         A URI spec, or a string that may be a URI spec.
   * @param  _hostAndPortLength
   *         The length of the host and port substring is returned through this
   *         param.  Pass null if you don't care.
   * @return The length of the host and port substring in the spec.  If aSpec
   *         doesn't look like a URI, then the entire aSpec is assumed to be a
   *         "host and port", and this returns 0, and _hostAndPortLength will be
   *         the length of aSpec.
   */
  static
  MOZ_ALWAYS_INLINE size_type
  indexOfHostAndPort(const nsACString &aSpec,
                     size_type *_hostAndPortLength)
  {
    size_type index = getPrefixLength(aSpec);
    size_type i = index;
    for (; i < aSpec.Length(); ++i) {
      // RFC 3986 (URIs): The origin ("authority") is terminated by '/', '?', or
      // '#' (or the end of the URI).
      if (aSpec[i] == static_cast<char_type>('/') ||
          aSpec[i] == static_cast<char_type>('?') ||
          aSpec[i] == static_cast<char_type>('#')) {
        break;
      }
      // RFC 3986: '@' marks the end of the userinfo component.
      if (aSpec[i] == static_cast<char_type>('@')) {
        index = i + 1;
      }
    }
    if (_hostAndPortLength) {
      *_hostAndPortLength = i - index;
    }
    return index;
  }

} // End anonymous namespace

namespace mozilla {
namespace places {

////////////////////////////////////////////////////////////////////////////////
//// AutoComplete Matching Function

  /* static */
  nsresult
  MatchAutoCompleteFunction::create(mozIStorageConnection *aDBConn)
  {
    RefPtr<MatchAutoCompleteFunction> function =
      new MatchAutoCompleteFunction();

    nsresult rv = aDBConn->CreateFunction(
      NS_LITERAL_CSTRING("autocomplete_match"), kArgIndexLength, function
    );
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  /* static */
  nsDependentCSubstring
  MatchAutoCompleteFunction::fixupURISpec(const nsACString &aURISpec,
                                          int32_t aMatchBehavior,
                                          nsACString &aSpecBuf)
  {
    nsDependentCSubstring fixedSpec;

    // Try to unescape the string.  If that succeeds and yields a different
    // string which is also valid UTF-8, we'll use it.
    // Otherwise, we will simply use our original string.
    bool unescaped = NS_UnescapeURL(aURISpec.BeginReading(),
      aURISpec.Length(), esc_SkipControl, aSpecBuf);
    if (unescaped && IsUTF8(aSpecBuf)) {
      fixedSpec.Rebind(aSpecBuf, 0);
    } else {
      fixedSpec.Rebind(aURISpec, 0);
    }

    if (aMatchBehavior == mozIPlacesAutoComplete::MATCH_ANYWHERE_UNMODIFIED)
      return fixedSpec;

    if (StringBeginsWith(fixedSpec, NS_LITERAL_CSTRING("http://"))) {
      fixedSpec.Rebind(fixedSpec, 7);
    } else if (StringBeginsWith(fixedSpec, NS_LITERAL_CSTRING("https://"))) {
      fixedSpec.Rebind(fixedSpec, 8);
    } else if (StringBeginsWith(fixedSpec, NS_LITERAL_CSTRING("ftp://"))) {
      fixedSpec.Rebind(fixedSpec, 6);
    }

    return fixedSpec;
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
    MOZ_ASSERT(!aToken.IsEmpty(), "Don't search for an empty token!");

    // We can't use StringBeginsWith here, unfortunately.  Although it will
    // happily take a case-insensitive UTF8 comparator, it eventually calls
    // nsACString::Equals, which checks that the two strings contain the same
    // number of bytes before calling the comparator.  Two characters may be
    // case-insensitively equal while taking up different numbers of bytes, so
    // this is not what we want.

    const_char_iterator tokenStart(aToken.BeginReading()),
                        tokenEnd(aToken.EndReading()),
                        sourceStart(aSourceString.BeginReading()),
                        sourceEnd(aSourceString.EndReading());

    bool dummy;
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
  bool
  MatchAutoCompleteFunction::findBeginningCaseSensitive(
    const nsDependentCSubstring &aToken,
    const nsACString &aSourceString)
  {
    MOZ_ASSERT(!aToken.IsEmpty(), "Don't search for an empty token!");

    return StringBeginsWith(aSourceString, aToken);
  }

  /* static */
  MatchAutoCompleteFunction::searchFunctionPtr
  MatchAutoCompleteFunction::getSearchFunction(int32_t aBehavior)
  {
    switch (aBehavior) {
      case mozIPlacesAutoComplete::MATCH_ANYWHERE:
      case mozIPlacesAutoComplete::MATCH_ANYWHERE_UNMODIFIED:
        return findAnywhere;
      case mozIPlacesAutoComplete::MATCH_BEGINNING:
        return findBeginning;
      case mozIPlacesAutoComplete::MATCH_BEGINNING_CASE_SENSITIVE:
        return findBeginningCaseSensitive;
      case mozIPlacesAutoComplete::MATCH_BOUNDARY:
      default:
        return findOnBoundary;
    };
  }

  NS_IMPL_ISUPPORTS(
    MatchAutoCompleteFunction,
    mozIStorageFunction
  )

  MatchAutoCompleteFunction::MatchAutoCompleteFunction()
    : mCachedZero(new IntegerVariant(0))
    , mCachedOne(new IntegerVariant(1))
  {
    static_assert(IntegerVariant::HasThreadSafeRefCnt::value,
        "Caching assumes that variants have thread-safe refcounting");
  }

  NS_IMETHODIMP
  MatchAutoCompleteFunction::OnFunctionCall(mozIStorageValueArray *aArguments,
                                            nsIVariant **_result)
  {
    // Macro to make the code a bit cleaner and easier to read.  Operates on
    // searchBehavior.
    int32_t searchBehavior = aArguments->AsInt32(kArgIndexSearchBehavior);
    #define HAS_BEHAVIOR(aBitName) \
      (searchBehavior & mozIPlacesAutoComplete::BEHAVIOR_##aBitName)

    nsDependentCString searchString =
      getSharedUTF8String(aArguments, kArgSearchString);
    nsDependentCString url =
      getSharedUTF8String(aArguments, kArgIndexURL);

    int32_t matchBehavior = aArguments->AsInt32(kArgIndexMatchBehavior);

    // We only want to filter javascript: URLs if we are not supposed to search
    // for them, and the search does not start with "javascript:".
    if (matchBehavior != mozIPlacesAutoComplete::MATCH_ANYWHERE_UNMODIFIED &&
        StringBeginsWith(url, NS_LITERAL_CSTRING("javascript:")) &&
        !HAS_BEHAVIOR(JAVASCRIPT) &&
        !StringBeginsWith(searchString, NS_LITERAL_CSTRING("javascript:"))) {
      NS_ADDREF(*_result = mCachedZero);
      return NS_OK;
    }

    int32_t visitCount = aArguments->AsInt32(kArgIndexVisitCount);
    bool typed = aArguments->AsInt32(kArgIndexTyped) ? true : false;
    bool bookmark = aArguments->AsInt32(kArgIndexBookmark) ? true : false;
    nsDependentCString tags = getSharedUTF8String(aArguments, kArgIndexTags);
    int32_t openPageCount = aArguments->AsInt32(kArgIndexOpenPageCount);
    bool matches = false;
    if (HAS_BEHAVIOR(RESTRICT)) {
      // Make sure we match all the filter requirements.  If a given restriction
      // is active, make sure the corresponding condition is not true.
      matches = (!HAS_BEHAVIOR(HISTORY) || visitCount > 0) &&
                (!HAS_BEHAVIOR(TYPED) || typed) &&
                (!HAS_BEHAVIOR(BOOKMARK) || bookmark) &&
                (!HAS_BEHAVIOR(TAG) || !tags.IsVoid()) &&
                (!HAS_BEHAVIOR(OPENPAGE) || openPageCount > 0);
    } else {
      // Make sure that we match all the filter requirements and that the
      // corresponding condition is true if at least a given restriction is active.
      matches = (HAS_BEHAVIOR(HISTORY) && visitCount > 0) ||
                (HAS_BEHAVIOR(TYPED) && typed) ||
                (HAS_BEHAVIOR(BOOKMARK) && bookmark) ||
                (HAS_BEHAVIOR(TAG) && !tags.IsVoid()) ||
                (HAS_BEHAVIOR(OPENPAGE) && openPageCount > 0);
    }

    if (!matches) {
      NS_ADDREF(*_result = mCachedZero);
      return NS_OK;
    }

    // Obtain our search function.
    searchFunctionPtr searchFunction = getSearchFunction(matchBehavior);

    // Clean up our URI spec and prepare it for searching.
    nsCString fixedUrlBuf;
    nsDependentCSubstring fixedUrl =
      fixupURISpec(url, matchBehavior, fixedUrlBuf);
    // Limit the number of chars we search through.
    const nsDependentCSubstring& trimmedUrl =
      Substring(fixedUrl, 0, MAX_CHARS_TO_SEARCH_THROUGH);

    nsDependentCString title = getSharedUTF8String(aArguments, kArgIndexTitle);
    // Limit the number of chars we search through.
    const nsDependentCSubstring& trimmedTitle =
      Substring(title, 0, MAX_CHARS_TO_SEARCH_THROUGH);

    // Determine if every token matches either the bookmark title, tags, page
    // title, or page URL.
    nsCWhitespaceTokenizer tokenizer(searchString);
    while (matches && tokenizer.hasMoreTokens()) {
      const nsDependentCSubstring &token = tokenizer.nextToken();

      if (HAS_BEHAVIOR(TITLE) && HAS_BEHAVIOR(URL)) {
        matches = (searchFunction(token, trimmedTitle) ||
                   searchFunction(token, tags)) &&
                  searchFunction(token, trimmedUrl);
      }
      else if (HAS_BEHAVIOR(TITLE)) {
        matches = searchFunction(token, trimmedTitle) ||
                  searchFunction(token, tags);
      }
      else if (HAS_BEHAVIOR(URL)) {
        matches = searchFunction(token, trimmedUrl);
      }
      else {
        matches = searchFunction(token, trimmedTitle) ||
                  searchFunction(token, tags) ||
                  searchFunction(token, trimmedUrl);
      }
    }

    NS_ADDREF(*_result = (matches ? mCachedOne : mCachedZero));
    return NS_OK;
    #undef HAS_BEHAVIOR
  }


////////////////////////////////////////////////////////////////////////////////
//// Frecency Calculation Function

  /* static */
  nsresult
  CalculateFrecencyFunction::create(mozIStorageConnection *aDBConn)
  {
    RefPtr<CalculateFrecencyFunction> function =
      new CalculateFrecencyFunction();

    nsresult rv = aDBConn->CreateFunction(
      NS_LITERAL_CSTRING("calculate_frecency"), -1, function
    );
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMPL_ISUPPORTS(
    CalculateFrecencyFunction,
    mozIStorageFunction
  )

  NS_IMETHODIMP
  CalculateFrecencyFunction::OnFunctionCall(mozIStorageValueArray *aArguments,
                                            nsIVariant **_result)
  {
    // Fetch arguments.  Use default values if they were omitted.
    uint32_t numEntries;
    nsresult rv = aArguments->GetNumEntries(&numEntries);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(numEntries <= 2, "unexpected number of arguments");

    int64_t pageId = aArguments->AsInt64(0);
    MOZ_ASSERT(pageId > 0, "Should always pass a valid page id");
    if (pageId <= 0) {
      NS_ADDREF(*_result = new IntegerVariant(0));
      return NS_OK;
    }

    enum RedirectState {
      eRedirectUnknown,
      eIsRedirect,
      eIsNotRedirect
    };

    RedirectState isRedirect = eRedirectUnknown;

    if (numEntries > 1) {
      isRedirect = aArguments->AsInt32(1) ? eIsRedirect : eIsNotRedirect;
    }

    int32_t typed = 0;
    int32_t visitCount = 0;
    bool hasBookmark = false;
    int32_t isQuery = 0;
    float pointsForSampledVisits = 0.0;
    int32_t numSampledVisits = 0;
    int32_t bonus = 0;

    // This is a const version of the history object for thread-safety.
    const nsNavHistory* history = nsNavHistory::GetConstHistoryService();
    NS_ENSURE_STATE(history);
    RefPtr<Database> DB = Database::GetDatabase();
    NS_ENSURE_STATE(DB);


    // Fetch the page stats from the database.
    {
      nsCOMPtr<mozIStorageStatement> getPageInfo = DB->GetStatement(
        "SELECT typed, visit_count, foreign_count, "
               "(substr(url, 0, 7) = 'place:') "
        "FROM moz_places "
        "WHERE id = :page_id "
      );
      NS_ENSURE_STATE(getPageInfo);
      mozStorageStatementScoper infoScoper(getPageInfo);

      rv = getPageInfo->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), pageId);
      NS_ENSURE_SUCCESS(rv, rv);

      bool hasResult = false;
      rv = getPageInfo->ExecuteStep(&hasResult);
      NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && hasResult, NS_ERROR_UNEXPECTED);

      rv = getPageInfo->GetInt32(0, &typed);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = getPageInfo->GetInt32(1, &visitCount);
      NS_ENSURE_SUCCESS(rv, rv);
      int32_t foreignCount = 0;
      rv = getPageInfo->GetInt32(2, &foreignCount);
      NS_ENSURE_SUCCESS(rv, rv);
      hasBookmark = foreignCount > 0;
      rv = getPageInfo->GetInt32(3, &isQuery);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (visitCount > 0) {
      // Get a sample of the last visits to the page, to calculate its weight.
      // In case of a temporary or permanent redirect, calculate the frecency
      // as if the original page was visited.
      nsCString redirectsTransitionFragment =
        nsPrintfCString("%d AND %d ", nsINavHistoryService::TRANSITION_REDIRECT_PERMANENT,
                                      nsINavHistoryService::TRANSITION_REDIRECT_TEMPORARY);
      nsCOMPtr<mozIStorageStatement> getVisits = DB->GetStatement(
        NS_LITERAL_CSTRING(
          "/* do not warn (bug 659740 - SQLite may ignore index if few visits exist) */"
          "SELECT "
            "ROUND((strftime('%s','now','localtime','utc') - v.visit_date/1000000)/86400), "
            "origin.visit_type, "
            "v.visit_type, "
            "target.id NOTNULL "
          "FROM moz_historyvisits v "
          "LEFT JOIN moz_historyvisits origin ON origin.id = v.from_visit "
                                            "AND v.visit_type BETWEEN "
            ) + redirectsTransitionFragment + NS_LITERAL_CSTRING(
          "LEFT JOIN moz_historyvisits target ON v.id = target.from_visit "
                                            "AND target.visit_type BETWEEN "
            ) + redirectsTransitionFragment + NS_LITERAL_CSTRING(
          "WHERE v.place_id = :page_id "
          "ORDER BY v.visit_date DESC "
        )
      );
      NS_ENSURE_STATE(getVisits);
      mozStorageStatementScoper visitsScoper(getVisits);
      rv = getVisits->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), pageId);
      NS_ENSURE_SUCCESS(rv, rv);

      // Fetch only a limited number of recent visits.
      bool hasResult = false;
      for (int32_t maxVisits = history->GetNumVisitsForFrecency();
           numSampledVisits < maxVisits &&
           NS_SUCCEEDED(getVisits->ExecuteStep(&hasResult)) && hasResult;
           numSampledVisits++) {

        int32_t visitType;
        bool isNull = false;
        rv = getVisits->GetIsNull(1, &isNull);
        NS_ENSURE_SUCCESS(rv, rv);

        if (isRedirect == eIsRedirect || isNull) {
          // Use the main visit_type.
          rv = getVisits->GetInt32(2, &visitType);
          NS_ENSURE_SUCCESS(rv, rv);
        } else {
          // This is a redirect target, so use the origin visit_type.
          rv = getVisits->GetInt32(1, &visitType);
          NS_ENSURE_SUCCESS(rv, rv);
        }

        RedirectState visitIsRedirect = isRedirect;

        // If we don't know if this is a redirect or not, or this is not the
        // most recent visit that we're looking at, then we use the redirect
        // value from the database.
        if (visitIsRedirect == eRedirectUnknown || numSampledVisits >= 1) {
          int32_t redirect;
          rv = getVisits->GetInt32(3, &redirect);
          NS_ENSURE_SUCCESS(rv, rv);
          visitIsRedirect = !!redirect ? eIsRedirect : eIsNotRedirect;
        }

        bonus = history->GetFrecencyTransitionBonus(visitType, true, visitIsRedirect == eIsRedirect);

        // Add the bookmark visit bonus.
        if (hasBookmark) {
          bonus += history->GetFrecencyTransitionBonus(nsINavHistoryService::TRANSITION_BOOKMARK, true);
        }

        // If bonus was zero, we can skip the work to determine the weight.
        if (bonus) {
          int32_t ageInDays = getVisits->AsInt32(0);
          int32_t weight = history->GetFrecencyAgedWeight(ageInDays);
          pointsForSampledVisits += (float)(weight * (bonus / 100.0));
        }
      }
    }

    // If we sampled some visits for this page, use the calculated weight.
    if (numSampledVisits) {
      // We were unable to calculate points, maybe cause all the visits in the
      // sample had a zero bonus. Though, we know the page has some past valid
      // visit, or visit_count would be zero. Thus we set the frecency to
      // -1, so they are still shown in autocomplete.
      if (!pointsForSampledVisits) {
        NS_ADDREF(*_result = new IntegerVariant(-1));
      }
      else {
        // Estimate frecency using the sampled visits.
        // Use ceilf() so that we don't round down to 0, which
        // would cause us to completely ignore the place during autocomplete.
        NS_ADDREF(*_result = new IntegerVariant((int32_t) ceilf(visitCount * ceilf(pointsForSampledVisits) / numSampledVisits)));
      }
      return NS_OK;
    }

    // Otherwise this page has no visits, it may be bookmarked.
    if (!hasBookmark || isQuery) {
      NS_ADDREF(*_result = new IntegerVariant(0));
      return NS_OK;
    }

    // For unvisited bookmarks, produce a non-zero frecency, so that they show
    // up in URL bar autocomplete.
    visitCount = 1;

    // Make it so something bookmarked and typed will have a higher frecency
    // than something just typed or just bookmarked.
    bonus += history->GetFrecencyTransitionBonus(nsINavHistoryService::TRANSITION_BOOKMARK, false);
    if (typed) {
      bonus += history->GetFrecencyTransitionBonus(nsINavHistoryService::TRANSITION_TYPED, false);
    }

    // Assume "now" as our ageInDays, so use the first bucket.
    pointsForSampledVisits = history->GetFrecencyBucketWeight(1) * (bonus / (float)100.0);

    // use ceilf() so that we don't round down to 0, which
    // would cause us to completely ignore the place during autocomplete
    NS_ADDREF(*_result = new IntegerVariant((int32_t) ceilf(visitCount * ceilf(pointsForSampledVisits))));

    return NS_OK;
  }

////////////////////////////////////////////////////////////////////////////////
//// GUID Creation Function

  /* static */
  nsresult
  GenerateGUIDFunction::create(mozIStorageConnection *aDBConn)
  {
    RefPtr<GenerateGUIDFunction> function = new GenerateGUIDFunction();
    nsresult rv = aDBConn->CreateFunction(
      NS_LITERAL_CSTRING("generate_guid"), 0, function
    );
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMPL_ISUPPORTS(
    GenerateGUIDFunction,
    mozIStorageFunction
  )

  NS_IMETHODIMP
  GenerateGUIDFunction::OnFunctionCall(mozIStorageValueArray *aArguments,
                                       nsIVariant **_result)
  {
    nsAutoCString guid;
    nsresult rv = GenerateGUID(guid);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ADDREF(*_result = new UTF8TextVariant(guid));
    return NS_OK;
  }

////////////////////////////////////////////////////////////////////////////////
//// GUID Validation Function

  /* static */
  nsresult
  IsValidGUIDFunction::create(mozIStorageConnection *aDBConn)
  {
    RefPtr<IsValidGUIDFunction> function = new IsValidGUIDFunction();
    return aDBConn->CreateFunction(
      NS_LITERAL_CSTRING("is_valid_guid"), 1, function
    );
  }

  NS_IMPL_ISUPPORTS(
    IsValidGUIDFunction,
    mozIStorageFunction
  )

  NS_IMETHODIMP
  IsValidGUIDFunction::OnFunctionCall(mozIStorageValueArray *aArguments,
                                      nsIVariant **_result)
  {
    // Must have non-null function arguments.
    MOZ_ASSERT(aArguments);

    nsAutoCString guid;
    aArguments->GetUTF8String(0, guid);

    RefPtr<nsVariant> result = new nsVariant();
    result->SetAsBool(IsValidGUID(guid));
    result.forget(_result);
    return NS_OK;
  }

////////////////////////////////////////////////////////////////////////////////
//// Get Unreversed Host Function

  /* static */
  nsresult
  GetUnreversedHostFunction::create(mozIStorageConnection *aDBConn)
  {
    RefPtr<GetUnreversedHostFunction> function = new GetUnreversedHostFunction();
    nsresult rv = aDBConn->CreateFunction(
      NS_LITERAL_CSTRING("get_unreversed_host"), 1, function
    );
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMPL_ISUPPORTS(
    GetUnreversedHostFunction,
    mozIStorageFunction
  )

  NS_IMETHODIMP
  GetUnreversedHostFunction::OnFunctionCall(mozIStorageValueArray *aArguments,
                                            nsIVariant **_result)
  {
    // Must have non-null function arguments.
    MOZ_ASSERT(aArguments);

    nsAutoString src;
    aArguments->GetString(0, src);

    RefPtr<nsVariant> result = new nsVariant();

    if (src.Length()>1) {
      src.Truncate(src.Length() - 1);
      nsAutoString dest;
      ReverseString(src, dest);
      result->SetAsAString(dest);
    }
    else {
      result->SetAsAString(EmptyString());
    }
    result.forget(_result);
    return NS_OK;
  }

////////////////////////////////////////////////////////////////////////////////
//// Fixup URL Function

  /* static */
  nsresult
  FixupURLFunction::create(mozIStorageConnection *aDBConn)
  {
    RefPtr<FixupURLFunction> function = new FixupURLFunction();
    nsresult rv = aDBConn->CreateFunction(
      NS_LITERAL_CSTRING("fixup_url"), 1, function
    );
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMPL_ISUPPORTS(
    FixupURLFunction,
    mozIStorageFunction
  )

  NS_IMETHODIMP
  FixupURLFunction::OnFunctionCall(mozIStorageValueArray *aArguments,
                                   nsIVariant **_result)
  {
    // Must have non-null function arguments.
    MOZ_ASSERT(aArguments);

    nsAutoString src;
    aArguments->GetString(0, src);

    RefPtr<nsVariant> result = new nsVariant();

    if (StringBeginsWith(src, NS_LITERAL_STRING("http://")))
      src.Cut(0, 7);
    else if (StringBeginsWith(src, NS_LITERAL_STRING("https://")))
      src.Cut(0, 8);
    else if (StringBeginsWith(src, NS_LITERAL_STRING("ftp://")))
      src.Cut(0, 6);

    // Remove common URL hostname prefixes
    if (StringBeginsWith(src, NS_LITERAL_STRING("www."))) {
      src.Cut(0, 4);
    }

    result->SetAsAString(src);
    result.forget(_result);
    return NS_OK;
  }

////////////////////////////////////////////////////////////////////////////////
//// Frecency Changed Notification Function

  /* static */
  nsresult
  FrecencyNotificationFunction::create(mozIStorageConnection *aDBConn)
  {
    RefPtr<FrecencyNotificationFunction> function =
      new FrecencyNotificationFunction();
    nsresult rv = aDBConn->CreateFunction(
      NS_LITERAL_CSTRING("notify_frecency"), 5, function
    );
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMPL_ISUPPORTS(
    FrecencyNotificationFunction,
    mozIStorageFunction
  )

  NS_IMETHODIMP
  FrecencyNotificationFunction::OnFunctionCall(mozIStorageValueArray *aArgs,
                                               nsIVariant **_result)
  {
    uint32_t numArgs;
    nsresult rv = aArgs->GetNumEntries(&numArgs);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(numArgs == 5);

    int32_t newFrecency = aArgs->AsInt32(0);

    nsAutoCString spec;
    rv = aArgs->GetUTF8String(1, spec);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString guid;
    rv = aArgs->GetUTF8String(2, guid);
    NS_ENSURE_SUCCESS(rv, rv);

    bool hidden = static_cast<bool>(aArgs->AsInt32(3));
    PRTime lastVisitDate = static_cast<PRTime>(aArgs->AsInt64(4));

    const nsNavHistory* navHistory = nsNavHistory::GetConstHistoryService();
    NS_ENSURE_STATE(navHistory);
    navHistory->DispatchFrecencyChangedNotification(spec, newFrecency, guid,
                                                    hidden, lastVisitDate);

    RefPtr<nsVariant> result = new nsVariant();
    rv = result->SetAsInt32(newFrecency);
    NS_ENSURE_SUCCESS(rv, rv);
    result.forget(_result);
    return NS_OK;
  }

////////////////////////////////////////////////////////////////////////////////
//// Store Last Inserted Id Function

  /* static */
  nsresult
  StoreLastInsertedIdFunction::create(mozIStorageConnection *aDBConn)
  {
    RefPtr<StoreLastInsertedIdFunction> function =
      new StoreLastInsertedIdFunction();
    nsresult rv = aDBConn->CreateFunction(
      NS_LITERAL_CSTRING("store_last_inserted_id"), 2, function
    );
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMPL_ISUPPORTS(
    StoreLastInsertedIdFunction,
    mozIStorageFunction
  )

  NS_IMETHODIMP
  StoreLastInsertedIdFunction::OnFunctionCall(mozIStorageValueArray *aArgs,
                                              nsIVariant **_result)
  {
    uint32_t numArgs;
    nsresult rv = aArgs->GetNumEntries(&numArgs);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(numArgs == 2);

    nsAutoCString table;
    rv = aArgs->GetUTF8String(0, table);
    NS_ENSURE_SUCCESS(rv, rv);

    int64_t lastInsertedId = aArgs->AsInt64(1);

    MOZ_ASSERT(table.EqualsLiteral("moz_places") ||
               table.EqualsLiteral("moz_historyvisits") ||
               table.EqualsLiteral("moz_bookmarks") ||
               table.EqualsLiteral("moz_icons"));

    if (table.EqualsLiteral("moz_bookmarks")) {
      nsNavBookmarks::StoreLastInsertedId(table, lastInsertedId);
    } else if (table.EqualsLiteral("moz_icons")) {
      nsFaviconService::StoreLastInsertedId(table, lastInsertedId);
    } else {
      nsNavHistory::StoreLastInsertedId(table, lastInsertedId);
    }

    RefPtr<nsVariant> result = new nsVariant();
    rv = result->SetAsInt64(lastInsertedId);
    NS_ENSURE_SUCCESS(rv, rv);
    result.forget(_result);
    return NS_OK;
  }

////////////////////////////////////////////////////////////////////////////////
//// Get Query Param Function

  /* static */
  nsresult
  GetQueryParamFunction::create(mozIStorageConnection *aDBConn)
  {
    RefPtr<GetQueryParamFunction> function = new GetQueryParamFunction();
    return aDBConn->CreateFunction(
      NS_LITERAL_CSTRING("get_query_param"), 2, function
    );
  }

  NS_IMPL_ISUPPORTS(
    GetQueryParamFunction,
    mozIStorageFunction
  )

  NS_IMETHODIMP
  GetQueryParamFunction::OnFunctionCall(mozIStorageValueArray *aArguments,
                                        nsIVariant **_result)
  {
    // Must have non-null function arguments.
    MOZ_ASSERT(aArguments);

    nsDependentCString queryString = getSharedUTF8String(aArguments, 0);
    nsDependentCString paramName = getSharedUTF8String(aArguments, 1);

    RefPtr<nsVariant> result = new nsVariant();
    if (!queryString.IsEmpty() && !paramName.IsEmpty()) {
      GetQueryParamIterator iterator(paramName, result);
      URLParams::Parse(queryString, iterator);
    }

    result.forget(_result);
    return NS_OK;
  }

////////////////////////////////////////////////////////////////////////////////
//// Hash Function

  /* static */
  nsresult
  HashFunction::create(mozIStorageConnection *aDBConn)
  {
    RefPtr<HashFunction> function = new HashFunction();
    return aDBConn->CreateFunction(
      NS_LITERAL_CSTRING("hash"), -1, function
    );
  }

  NS_IMPL_ISUPPORTS(
    HashFunction,
    mozIStorageFunction
  )

  NS_IMETHODIMP
  HashFunction::OnFunctionCall(mozIStorageValueArray *aArguments,
                               nsIVariant **_result)
  {
    // Must have non-null function arguments.
    MOZ_ASSERT(aArguments);

    // Fetch arguments.  Use default values if they were omitted.
    uint32_t numEntries;
    nsresult rv = aArguments->GetNumEntries(&numEntries);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(numEntries >= 1  && numEntries <= 2, NS_ERROR_FAILURE);

    nsDependentCString str = getSharedUTF8String(aArguments, 0);
    nsAutoCString mode;
    if (numEntries > 1) {
      aArguments->GetUTF8String(1, mode);
    }

    RefPtr<nsVariant> result = new nsVariant();
    uint64_t hash;
    rv = HashURL(str, mode, &hash);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = result->SetAsInt64(hash);
    NS_ENSURE_SUCCESS(rv, rv);

    result.forget(_result);
    return NS_OK;
  }

////////////////////////////////////////////////////////////////////////////////
//// Get prefix function

  /* static */
  nsresult
  GetPrefixFunction::create(mozIStorageConnection *aDBConn)
  {
    RefPtr<GetPrefixFunction> function = new GetPrefixFunction();
    nsresult rv = aDBConn->CreateFunction(
      NS_LITERAL_CSTRING("get_prefix"), 1, function
    );
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMPL_ISUPPORTS(
    GetPrefixFunction,
    mozIStorageFunction
  )

  NS_IMETHODIMP
  GetPrefixFunction::OnFunctionCall(mozIStorageValueArray *aArgs,
                                    nsIVariant **_result)
  {
    MOZ_ASSERT(aArgs);

    uint32_t numArgs;
    nsresult rv = aArgs->GetNumEntries(&numArgs);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(numArgs == 1);

    nsDependentCString spec(getSharedUTF8String(aArgs, 0));

    RefPtr<nsVariant> result = new nsVariant();
    result->SetAsACString(Substring(spec, 0, getPrefixLength(spec)));
    result.forget(_result);
    return NS_OK;
  }

////////////////////////////////////////////////////////////////////////////////
//// Get host and port function

  /* static */
  nsresult
  GetHostAndPortFunction::create(mozIStorageConnection *aDBConn)
  {
    RefPtr<GetHostAndPortFunction> function = new GetHostAndPortFunction();
    nsresult rv = aDBConn->CreateFunction(
      NS_LITERAL_CSTRING("get_host_and_port"), 1, function
    );
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMPL_ISUPPORTS(
    GetHostAndPortFunction,
    mozIStorageFunction
  )

  NS_IMETHODIMP
  GetHostAndPortFunction::OnFunctionCall(mozIStorageValueArray *aArgs,
                                         nsIVariant **_result)
  {
    MOZ_ASSERT(aArgs);

    uint32_t numArgs;
    nsresult rv = aArgs->GetNumEntries(&numArgs);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(numArgs == 1);

    nsDependentCString spec(getSharedUTF8String(aArgs, 0));

    RefPtr<nsVariant> result = new nsVariant();

    size_type length;
    size_type index = indexOfHostAndPort(spec, &length);
    result->SetAsACString(Substring(spec, index, length));
    result.forget(_result);
    return NS_OK;
  }

////////////////////////////////////////////////////////////////////////////////
//// Strip prefix and userinfo function

  /* static */
  nsresult
  StripPrefixAndUserinfoFunction::create(mozIStorageConnection *aDBConn)
  {
    RefPtr<StripPrefixAndUserinfoFunction> function =
      new StripPrefixAndUserinfoFunction();
    nsresult rv = aDBConn->CreateFunction(
      NS_LITERAL_CSTRING("strip_prefix_and_userinfo"), 1, function
    );
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMPL_ISUPPORTS(
    StripPrefixAndUserinfoFunction,
    mozIStorageFunction
  )

  NS_IMETHODIMP
  StripPrefixAndUserinfoFunction::OnFunctionCall(mozIStorageValueArray *aArgs,
                                                 nsIVariant **_result)
  {
    MOZ_ASSERT(aArgs);

    uint32_t numArgs;
    nsresult rv = aArgs->GetNumEntries(&numArgs);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(numArgs == 1);

    nsDependentCString spec(getSharedUTF8String(aArgs, 0));

    RefPtr<nsVariant> result = new nsVariant();

    size_type index = indexOfHostAndPort(spec, NULL);
    result->SetAsACString(Substring(spec, index, spec.Length() - index));
    result.forget(_result);
    return NS_OK;
  }

////////////////////////////////////////////////////////////////////////////////
//// Is frecency decaying function

  /* static */
  nsresult
  IsFrecencyDecayingFunction::create(mozIStorageConnection *aDBConn)
  {
    RefPtr<IsFrecencyDecayingFunction> function =
      new IsFrecencyDecayingFunction();
    nsresult rv = aDBConn->CreateFunction(
      NS_LITERAL_CSTRING("is_frecency_decaying"), 0, function
    );
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMPL_ISUPPORTS(
    IsFrecencyDecayingFunction,
    mozIStorageFunction
  )

  NS_IMETHODIMP
  IsFrecencyDecayingFunction::OnFunctionCall(mozIStorageValueArray *aArgs,
                                             nsIVariant **_result)
  {
    MOZ_ASSERT(aArgs);

    uint32_t numArgs;
    nsresult rv = aArgs->GetNumEntries(&numArgs);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(numArgs == 0);

    const nsNavHistory *navHistory = nsNavHistory::GetConstHistoryService();
    NS_ENSURE_STATE(navHistory);

    RefPtr<nsVariant> result = new nsVariant();
    rv = result->SetAsBool(navHistory->IsFrecencyDecaying());
    NS_ENSURE_SUCCESS(rv, rv);
    result.forget(_result);
    return NS_OK;
  }

////////////////////////////////////////////////////////////////////////////////
//// Update frecency stats function

  /* static */
  nsresult
  UpdateFrecencyStatsFunction::create(mozIStorageConnection *aDBConn)
  {
    RefPtr<UpdateFrecencyStatsFunction> function =
      new UpdateFrecencyStatsFunction();
    nsresult rv = aDBConn->CreateFunction(
      NS_LITERAL_CSTRING("update_frecency_stats"), 3, function
    );
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMPL_ISUPPORTS(
    UpdateFrecencyStatsFunction,
    mozIStorageFunction
  )

  NS_IMETHODIMP
  UpdateFrecencyStatsFunction::OnFunctionCall(mozIStorageValueArray *aArgs,
                                              nsIVariant **_result)
  {
    MOZ_ASSERT(aArgs);

    uint32_t numArgs;
    nsresult rv = aArgs->GetNumEntries(&numArgs);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(numArgs == 3);

    int64_t placeId = aArgs->AsInt64(0);
    int32_t oldFrecency = aArgs->AsInt32(1);
    int32_t newFrecency = aArgs->AsInt32(2);

    const nsNavHistory* navHistory = nsNavHistory::GetConstHistoryService();
    NS_ENSURE_STATE(navHistory);
    navHistory->DispatchFrecencyStatsUpdate(placeId, oldFrecency, newFrecency);

    RefPtr<nsVariant> result = new nsVariant();
    rv = result->SetAsVoid();
    NS_ENSURE_SUCCESS(rv, rv);
    result.forget(_result);
    return NS_OK;
  }

} // namespace places
} // namespace mozilla
