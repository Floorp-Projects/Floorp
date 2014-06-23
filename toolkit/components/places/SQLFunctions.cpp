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
#include "mozilla/Telemetry.h"
#include "mozilla/Likely.h"

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

} // End anonymous namespace

namespace mozilla {
namespace places {

////////////////////////////////////////////////////////////////////////////////
//// AutoComplete Matching Function

  //////////////////////////////////////////////////////////////////////////////
  //// MatchAutoCompleteFunction

  MatchAutoCompleteFunction::~MatchAutoCompleteFunction()
  {
  }

  /* static */
  nsresult
  MatchAutoCompleteFunction::create(mozIStorageConnection *aDBConn)
  {
    nsRefPtr<MatchAutoCompleteFunction> function =
      new MatchAutoCompleteFunction();

    nsresult rv = aDBConn->CreateFunction(
      NS_LITERAL_CSTRING("autocomplete_match"), kArgIndexLength, function
    );
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  /* static */
  void
  MatchAutoCompleteFunction::fixupURISpec(const nsCString &aURISpec,
                                          int32_t aMatchBehavior,
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

    if (aMatchBehavior == mozIPlacesAutoComplete::MATCH_ANYWHERE_UNMODIFIED)
      return;

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

  //////////////////////////////////////////////////////////////////////////////
  //// mozIStorageFunction

  NS_IMETHODIMP
  MatchAutoCompleteFunction::OnFunctionCall(mozIStorageValueArray *aArguments,
                                            nsIVariant **_result)
  {
    // Macro to make the code a bit cleaner and easier to read.  Operates on
    // searchBehavior.
    int32_t searchBehavior = aArguments->AsInt32(kArgIndexSearchBehavior);
    #define HAS_BEHAVIOR(aBitName) \
      (searchBehavior & mozIPlacesAutoComplete::BEHAVIOR_##aBitName)

    nsAutoCString searchString;
    (void)aArguments->GetUTF8String(kArgSearchString, searchString);
    nsCString url;
    (void)aArguments->GetUTF8String(kArgIndexURL, url);

    int32_t matchBehavior = aArguments->AsInt32(kArgIndexMatchBehavior);

    // We only want to filter javascript: URLs if we are not supposed to search
    // for them, and the search does not start with "javascript:".
    if (matchBehavior != mozIPlacesAutoComplete::MATCH_ANYWHERE_UNMODIFIED &&
        !HAS_BEHAVIOR(JAVASCRIPT) &&
        !StringBeginsWith(searchString, NS_LITERAL_CSTRING("javascript:")) &&
        StringBeginsWith(url, NS_LITERAL_CSTRING("javascript:"))) {
      NS_ADDREF(*_result = new IntegerVariant(0));
      return NS_OK;
    }

    int32_t visitCount = aArguments->AsInt32(kArgIndexVisitCount);
    bool typed = aArguments->AsInt32(kArgIndexTyped) ? true : false;
    bool bookmark = aArguments->AsInt32(kArgIndexBookmark) ? true : false;
    nsAutoCString tags;
    (void)aArguments->GetUTF8String(kArgIndexTags, tags);
    int32_t openPageCount = aArguments->AsInt32(kArgIndexOpenPageCount);

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
      NS_ADDREF(*_result = new IntegerVariant(0));
      return NS_OK;
    }

    // Obtain our search function.
    searchFunctionPtr searchFunction = getSearchFunction(matchBehavior);

    // Clean up our URI spec and prepare it for searching.
    nsCString fixedURI;
    fixupURISpec(url, matchBehavior, fixedURI);

    nsAutoCString title;
    (void)aArguments->GetUTF8String(kArgIndexTitle, title);

    // Determine if every token matches either the bookmark title, tags, page
    // title, or page URL.
    nsCWhitespaceTokenizer tokenizer(searchString);
    while (matches && tokenizer.hasMoreTokens()) {
      const nsDependentCSubstring &token = tokenizer.nextToken();

      if (HAS_BEHAVIOR(TITLE) && HAS_BEHAVIOR(URL)) {
        matches = (searchFunction(token, title) || searchFunction(token, tags)) &&
                  searchFunction(token, fixedURI);
      }
      else if (HAS_BEHAVIOR(TITLE)) {
        matches = searchFunction(token, title) || searchFunction(token, tags);
      }
      else if (HAS_BEHAVIOR(URL)) {
        matches = searchFunction(token, fixedURI);
      }
      else {
        matches = searchFunction(token, title) ||
                  searchFunction(token, tags) ||
                  searchFunction(token, fixedURI);
      }
    }

    NS_ADDREF(*_result = new IntegerVariant(matches ? 1 : 0));
    return NS_OK;
    #undef HAS_BEHAVIOR
  }


////////////////////////////////////////////////////////////////////////////////
//// Frecency Calculation Function

  //////////////////////////////////////////////////////////////////////////////
  //// CalculateFrecencyFunction

  CalculateFrecencyFunction::~CalculateFrecencyFunction()
  {
  }

  /* static */
  nsresult
  CalculateFrecencyFunction::create(mozIStorageConnection *aDBConn)
  {
    nsRefPtr<CalculateFrecencyFunction> function =
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

  //////////////////////////////////////////////////////////////////////////////
  //// mozIStorageFunction

  NS_IMETHODIMP
  CalculateFrecencyFunction::OnFunctionCall(mozIStorageValueArray *aArguments,
                                            nsIVariant **_result)
  {
    // Fetch arguments.  Use default values if they were omitted.
    uint32_t numEntries;
    nsresult rv = aArguments->GetNumEntries(&numEntries);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ASSERTION(numEntries > 0, "unexpected number of arguments");

    Telemetry::AutoTimer<Telemetry::PLACES_FRECENCY_CALC_TIME_MS> timer;

    int64_t pageId = aArguments->AsInt64(0);
    int32_t typed = numEntries > 1 ? aArguments->AsInt32(1) : 0;
    int32_t fullVisitCount = numEntries > 2 ? aArguments->AsInt32(2) : 0;
    int64_t bookmarkId = numEntries > 3 ? aArguments->AsInt64(3) : 0;
    int32_t visitCount = 0;
    int32_t hidden = 0;
    int32_t isQuery = 0;
    float pointsForSampledVisits = 0.0;

    // This is a const version of the history object for thread-safety.
    const nsNavHistory* history = nsNavHistory::GetConstHistoryService();
    NS_ENSURE_STATE(history);
    nsRefPtr<Database> DB = Database::GetDatabase();
    NS_ENSURE_STATE(DB);

    if (pageId > 0) {
      // The page is already in the database, and we can fetch current
      // params from the database.
      nsRefPtr<mozIStorageStatement> getPageInfo = DB->GetStatement(
        "SELECT typed, hidden, visit_count, "
          "(SELECT count(*) FROM moz_historyvisits WHERE place_id = :page_id), "
          "EXISTS (SELECT 1 FROM moz_bookmarks WHERE fk = :page_id), "
          "(url > 'place:' AND url < 'place;') "
        "FROM moz_places "
        "WHERE id = :page_id "
      );
      NS_ENSURE_STATE(getPageInfo);
      mozStorageStatementScoper infoScoper(getPageInfo);

      rv = getPageInfo->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), pageId);
      NS_ENSURE_SUCCESS(rv, rv);

      bool hasResult;
      rv = getPageInfo->ExecuteStep(&hasResult);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_TRUE(hasResult, NS_ERROR_UNEXPECTED);
      rv = getPageInfo->GetInt32(0, &typed);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = getPageInfo->GetInt32(1, &hidden);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = getPageInfo->GetInt32(2, &visitCount);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = getPageInfo->GetInt32(3, &fullVisitCount);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = getPageInfo->GetInt64(4, &bookmarkId);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = getPageInfo->GetInt32(5, &isQuery);
      NS_ENSURE_SUCCESS(rv, rv);

      // NOTE: This is not limited to visits with "visit_type NOT IN (0,4,7,8)"
      // because otherwise it would not return any visit for those transitions
      // causing an incorrect frecency, see CalculateFrecencyInternal().
      // In case of a temporary or permanent redirect, calculate the frecency
      // as if the original page was visited.
      // Get a sample of the last visits to the page, to calculate its weight.
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
      int32_t numSampledVisits = 0;
      for (int32_t maxVisits = history->GetNumVisitsForFrecency();
           numSampledVisits < maxVisits &&
           NS_SUCCEEDED(getVisits->ExecuteStep(&hasResult)) && hasResult;
           numSampledVisits++) {
        int32_t visitType;
        rv = getVisits->GetInt32(1, &visitType);
        NS_ENSURE_SUCCESS(rv, rv);
        int32_t bonus = history->GetFrecencyTransitionBonus(visitType, true);

        // Always add the bookmark visit bonus.
        if (bookmarkId) {
          bonus += history->GetFrecencyTransitionBonus(nsINavHistoryService::TRANSITION_BOOKMARK, true);
        }

        // If bonus was zero, we can skip the work to determine the weight.
        if (bonus) {
          int32_t ageInDays = getVisits->AsInt32(0);
          int32_t weight = history->GetFrecencyAgedWeight(ageInDays);
          pointsForSampledVisits += (float)(weight * (bonus / 100.0));
        }
      }

      // If we found some visits for this page, use the calculated weight.
      if (numSampledVisits) {
        // fix for bug #412219
        if (!pointsForSampledVisits) {
          // For URIs with zero points in the sampled recent visits
          // but "browsing" type visits outside the sampling range, set
          // frecency to -visit_count, so they're still shown in autocomplete.
          NS_ADDREF(*_result = new IntegerVariant(-visitCount));
        }
        else {
          // Estimate frecency using the last few visits.
          // Use ceilf() so that we don't round down to 0, which
          // would cause us to completely ignore the place during autocomplete.
          NS_ADDREF(*_result = new IntegerVariant((int32_t) ceilf(fullVisitCount * ceilf(pointsForSampledVisits) / numSampledVisits)));
        }

        return NS_OK;
      }
    }

    // This page is unknown or has no visits.  It could have just been added, so
    // use passed in or default values.

    // The code below works well for guessing the frecency on import, and we'll
    // correct later once we have visits.
    // TODO: What if we don't have visits and we never visit?  We could end up
    // with a really high value that keeps coming up in ac results? Should we
    // only do this on import?  Have to figure it out.
    int32_t bonus = 0;

    // Make it so something bookmarked and typed will have a higher frecency
    // than something just typed or just bookmarked.
    if (bookmarkId && !isQuery) {
      bonus += history->GetFrecencyTransitionBonus(nsINavHistoryService::TRANSITION_BOOKMARK, false);;
      // For unvisited bookmarks, produce a non-zero frecency, so that they show
      // up in URL bar autocomplete.
      fullVisitCount = 1;
    }

    if (typed) {
      bonus += history->GetFrecencyTransitionBonus(nsINavHistoryService::TRANSITION_TYPED, false);
    }

    // Assume "now" as our ageInDays, so use the first bucket.
    pointsForSampledVisits = history->GetFrecencyBucketWeight(1) * (bonus / (float)100.0); 

    // use ceilf() so that we don't round down to 0, which
    // would cause us to completely ignore the place during autocomplete
    NS_ADDREF(*_result = new IntegerVariant((int32_t) ceilf(fullVisitCount * ceilf(pointsForSampledVisits))));

    return NS_OK;
  }

////////////////////////////////////////////////////////////////////////////////
//// GUID Creation Function

  GenerateGUIDFunction::~GenerateGUIDFunction()
  {
  }

  //////////////////////////////////////////////////////////////////////////////
  //// GenerateGUIDFunction

  /* static */
  nsresult
  GenerateGUIDFunction::create(mozIStorageConnection *aDBConn)
  {
    nsRefPtr<GenerateGUIDFunction> function = new GenerateGUIDFunction();
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

  //////////////////////////////////////////////////////////////////////////////
  //// mozIStorageFunction

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

  GetUnreversedHostFunction::~GetUnreversedHostFunction()
  {
  }

  //////////////////////////////////////////////////////////////////////////////
  //// GetUnreversedHostFunction

  /* static */
  nsresult
  GetUnreversedHostFunction::create(mozIStorageConnection *aDBConn)
  {
    nsRefPtr<GetUnreversedHostFunction> function = new GetUnreversedHostFunction();
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

  //////////////////////////////////////////////////////////////////////////////
  //// mozIStorageFunction

  NS_IMETHODIMP
  GetUnreversedHostFunction::OnFunctionCall(mozIStorageValueArray *aArguments,
                                            nsIVariant **_result)
  {
    // Must have non-null function arguments.
    MOZ_ASSERT(aArguments);

    nsAutoString src;
    aArguments->GetString(0, src);

    nsCOMPtr<nsIWritableVariant> result =
      do_CreateInstance("@mozilla.org/variant;1");
    NS_ENSURE_STATE(result);

    if (src.Length()>1) {
      src.Truncate(src.Length() - 1);
      nsAutoString dest;
      ReverseString(src, dest);
      result->SetAsAString(dest);
    }
    else {
      result->SetAsAString(EmptyString());
    }
    NS_ADDREF(*_result = result);
    return NS_OK;
  }

////////////////////////////////////////////////////////////////////////////////
//// Fixup URL Function

  FixupURLFunction::~FixupURLFunction()
  {
  }

  //////////////////////////////////////////////////////////////////////////////
  //// FixupURLFunction

  /* static */
  nsresult
  FixupURLFunction::create(mozIStorageConnection *aDBConn)
  {
    nsRefPtr<FixupURLFunction> function = new FixupURLFunction();
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

  //////////////////////////////////////////////////////////////////////////////
  //// mozIStorageFunction

  NS_IMETHODIMP
  FixupURLFunction::OnFunctionCall(mozIStorageValueArray *aArguments,
                                   nsIVariant **_result)
  {
    // Must have non-null function arguments.
    MOZ_ASSERT(aArguments);

    nsAutoString src;
    aArguments->GetString(0, src);

    nsCOMPtr<nsIWritableVariant> result =
      do_CreateInstance("@mozilla.org/variant;1");
    NS_ENSURE_STATE(result);

    // Remove common URL hostname prefixes
    if (StringBeginsWith(src, NS_LITERAL_STRING("www."))) {
      src.Cut(0, 4);
    }

    result->SetAsAString(src);
    NS_ADDREF(*_result = result);
    return NS_OK;
  }

////////////////////////////////////////////////////////////////////////////////
//// Frecency Changed Notification Function

  FrecencyNotificationFunction::~FrecencyNotificationFunction()
  {
  }

  /* static */
  nsresult
  FrecencyNotificationFunction::create(mozIStorageConnection *aDBConn)
  {
    nsRefPtr<FrecencyNotificationFunction> function =
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

    nsCOMPtr<nsIWritableVariant> result =
      do_CreateInstance("@mozilla.org/variant;1");
    NS_ENSURE_STATE(result);
    rv = result->SetAsInt32(newFrecency);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ADDREF(*_result = result);
    return NS_OK;
  }

} // namespace places
} // namespace mozilla
