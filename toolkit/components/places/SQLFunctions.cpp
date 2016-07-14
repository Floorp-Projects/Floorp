/* vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/storage.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsWhitespaceTokenizer.h"
#include "nsEscape.h"
#include "mozIPlacesAutoComplete.h"
#include "SQLFunctions.h"
#include "nsMathUtils.h"
#include "nsUTF8Utils.h"
#include "nsINavHistoryService.h"
#include "nsPrintfCString.h"
#include "nsNavHistory.h"
#include "mozilla/Likely.h"
#include "nsVariant.h"
#include "mozilla/HashFunctions.h"

// Maximum number of chars to search through.
// MatchAutoCompleteFunction won't look for matches over this threshold.
#define MAX_CHARS_TO_SEARCH_THROUGH 255

using namespace mozilla::storage;

// Keep the GUID-related parts of this file in sync with toolkit/downloads/SQLFunctions.cpp!

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
  MOZ_ALWAYS_INLINE const_char_iterator
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
   * a bit slower.  Our solution is to define findInString as MOZ_ALWAYS_INLINE
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
  MOZ_ALWAYS_INLINE bool
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
      bool error;
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
      if (MOZ_UNLIKELY(error)) {
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

  static
  MOZ_ALWAYS_INLINE nsDependentCString
  getSharedString(mozIStorageValueArray* aValues, uint32_t aIndex) {
    uint32_t len;
    const char* str = aValues->AsSharedUTF8String(aIndex, &len);
    if (!str) {
      return nsDependentCString("", (uint32_t)0);
    }
    return nsDependentCString(str, len);
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

    if (StringBeginsWith(fixedSpec, NS_LITERAL_CSTRING("www."))) {
      fixedSpec.Rebind(fixedSpec, 4);
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
    NS_PRECONDITION(!aToken.IsEmpty(), "Don't search for an empty token!");

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
    NS_PRECONDITION(!aToken.IsEmpty(), "Don't search for an empty token!");

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
      getSharedString(aArguments, kArgSearchString);
    nsDependentCString url =
      getSharedString(aArguments, kArgIndexURL);

    int32_t matchBehavior = aArguments->AsInt32(kArgIndexMatchBehavior);

    // We only want to filter javascript: URLs if we are not supposed to search
    // for them, and the search does not start with "javascript:".
    if (matchBehavior != mozIPlacesAutoComplete::MATCH_ANYWHERE_UNMODIFIED &&
        StringBeginsWith(url, NS_LITERAL_CSTRING("javascript:")) &&
        !HAS_BEHAVIOR(JAVASCRIPT) &&
        !StringBeginsWith(searchString, NS_LITERAL_CSTRING("javascript:"))) {
      NS_ADDREF(*_result = new IntegerVariant(0));
      return NS_OK;
    }

    int32_t visitCount = aArguments->AsInt32(kArgIndexVisitCount);
    bool typed = aArguments->AsInt32(kArgIndexTyped) ? true : false;
    bool bookmark = aArguments->AsInt32(kArgIndexBookmark) ? true : false;
    nsDependentCString tags = getSharedString(aArguments, kArgIndexTags);
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
      NS_ADDREF(*_result = new IntegerVariant(0));
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

    nsDependentCString title = getSharedString(aArguments, kArgIndexTitle);
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

    NS_ADDREF(*_result = new IntegerVariant(matches ? 1 : 0));
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
      NS_LITERAL_CSTRING("calculate_frecency"), 1, function
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
    MOZ_ASSERT(numEntries == 1, "unexpected number of arguments");

    int64_t pageId = aArguments->AsInt64(0);
    MOZ_ASSERT(pageId > 0, "Should always pass a valid page id");
    if (pageId <= 0) {
      NS_ADDREF(*_result = new IntegerVariant(0));
      return NS_OK;
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
      RefPtr<mozIStorageStatement> getPageInfo = DB->GetStatement(
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
      nsCOMPtr<mozIStorageStatement> getVisits = DB->GetStatement(
        NS_LITERAL_CSTRING(
          "/* do not warn (bug 659740 - SQLite may ignore index if few visits exist) */"
          "SELECT "
            "ROUND((strftime('%s','now','localtime','utc') - v.visit_date/1000000)/86400), "
            "IFNULL(r.visit_type, v.visit_type), "
            "v.visit_date "
          "FROM moz_historyvisits v "
          "LEFT JOIN moz_historyvisits r ON r.id = v.from_visit AND v.visit_type BETWEEN "
        ) + nsPrintfCString("%d AND %d ", nsINavHistoryService::TRANSITION_REDIRECT_PERMANENT,
                                          nsINavHistoryService::TRANSITION_REDIRECT_TEMPORARY) +
        NS_LITERAL_CSTRING(
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
        rv = getVisits->GetInt32(1, &visitType);
        NS_ENSURE_SUCCESS(rv, rv);
        bonus = history->GetFrecencyTransitionBonus(visitType, true);

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

    nsNavHistory::StoreLastInsertedId(table, lastInsertedId);

    RefPtr<nsVariant> result = new nsVariant();
    rv = result->SetAsInt64(lastInsertedId);
    NS_ENSURE_SUCCESS(rv, rv);
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

    nsString str;
    aArguments->GetString(0, str);
    nsAutoCString mode;
    if (numEntries > 1) {
      aArguments->GetUTF8String(1, mode);
    }

    RefPtr<nsVariant> result = new nsVariant();
    if (mode.IsEmpty()) {
      // URI-like strings (having a prefix before a colon), are handled specially,
      // as a 48 bit hash, where first 16 bits are the prefix hash, while the
      // other 32 are the string hash.
      // The 16 bits have been decided based on the fact hashing all of the IANA
      // known schemes, plus "places", does not generate collisions.
      nsAString::const_iterator start, tip, end;
      str.BeginReading(tip);
      start = tip;
      str.EndReading(end);
      if (FindInReadable(NS_LITERAL_STRING(":"), tip, end)) {
        const nsDependentSubstring& prefix = Substring(start, tip);
        uint64_t prefixHash = static_cast<uint64_t>(HashString(prefix) & 0x0000FFFF);
        // The second half of the url is more likely to be unique, so we add it.
        uint32_t srcHash = HashString(str);
        uint64_t hash = (prefixHash << 32) + srcHash;
        result->SetAsInt64(hash);
      } else {
        uint32_t hash = HashString(str);
        result->SetAsInt64(hash);
      }
    } else if (mode.Equals(NS_LITERAL_CSTRING("prefix_lo"))) {
      // Keep only 16 bits.
      uint64_t hash = static_cast<uint64_t>(HashString(str) & 0x0000FFFF) << 32;
      result->SetAsInt64(hash);
    } else if (mode.Equals(NS_LITERAL_CSTRING("prefix_hi"))) {
      // Keep only 16 bits.
      uint64_t hash = static_cast<uint64_t>(HashString(str) & 0x0000FFFF) << 32;
      // Make this a prefix upper bound by filling the lowest 32 bits.
      hash +=  0xFFFFFFFF;
      result->SetAsInt64(hash);
    } else {
      return NS_ERROR_FAILURE;
    }

    result.forget(_result);
    return NS_OK;
  }

} // namespace places
} // namespace mozilla
