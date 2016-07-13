/* vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_places_SQLFunctions_h_
#define mozilla_places_SQLFunctions_h_

/**
 * This file contains functions that Places adds to the database handle that can
 * be accessed by SQL queries.
 *
 * Keep the GUID-related parts of this file in sync with
 * toolkit/downloads/SQLFunctions.[h|cpp]!
 */

#include "mozIStorageFunction.h"
#include "mozilla/Attributes.h"

class mozIStorageConnection;

namespace mozilla {
namespace places {

////////////////////////////////////////////////////////////////////////////////
//// AutoComplete Matching Function

/**
 * This function is used to determine if a given set of data should match an
 * AutoComplete query.
 *
 * In SQL, you'd use it in the WHERE clause like so:
 * WHERE AUTOCOMPLETE_MATCH(aSearchString, aURL, aTitle, aTags, aVisitCount,
 *                          aTyped, aBookmark, aOpenPageCount, aMatchBehavior,
 *                          aSearchBehavior)
 *
 * @param aSearchString
 *        The string to compare against.
 * @param aURL
 *        The URL to test for an AutoComplete match.
 * @param aTitle
 *        The title to test for an AutoComplete match.
 * @param aTags
 *        The tags to test for an AutoComplete match.
 * @param aVisitCount
 *        The number of visits aURL has.
 * @param aTyped
 *        Indicates if aURL is a typed URL or not.  Treated as a boolean.
 * @param aBookmark
 *        Indicates if aURL is a bookmark or not.  Treated as a boolean.
 * @param aOpenPageCount
 *        The number of times aURL has been registered as being open.  (See
 *        mozIPlacesAutoComplete::registerOpenPage.)
 * @param aMatchBehavior
 *        The match behavior to use for this search.
 * @param aSearchBehavior
 *        A bitfield dictating the search behavior.
 */
class MatchAutoCompleteFunction final : public mozIStorageFunction
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION

  /**
   * Registers the function with the specified database connection.
   *
   * @param aDBConn
   *        The database connection to register with.
   */
  static nsresult create(mozIStorageConnection *aDBConn);

private:
  ~MatchAutoCompleteFunction() {}

  /**
   * Argument Indexes
   */
  static const uint32_t kArgSearchString = 0;
  static const uint32_t kArgIndexURL = 1;
  static const uint32_t kArgIndexTitle = 2;
  static const uint32_t kArgIndexTags = 3;
  static const uint32_t kArgIndexVisitCount = 4;
  static const uint32_t kArgIndexTyped = 5;
  static const uint32_t kArgIndexBookmark = 6;
  static const uint32_t kArgIndexOpenPageCount = 7;
  static const uint32_t kArgIndexMatchBehavior = 8;
  static const uint32_t kArgIndexSearchBehavior = 9;
  static const uint32_t kArgIndexLength = 10;

  /**
   * Typedefs
   */
  typedef bool (*searchFunctionPtr)(const nsDependentCSubstring &aToken,
                                    const nsACString &aSourceString);

  typedef nsACString::const_char_iterator const_char_iterator;

  /**
   * Obtains the search function to match on.
   *
   * @param aBehavior
   *        The matching behavior to use defined by one of the
   *        mozIPlacesAutoComplete::MATCH_* values.
   * @return a pointer to the function that will perform the proper search.
   */
  static searchFunctionPtr getSearchFunction(int32_t aBehavior);

  /**
   * Tests if aSourceString starts with aToken.
   *
   * @param aToken
   *        The string to search for.
   * @param aSourceString
   *        The string to search.
   * @return true if found, false otherwise.
   */
  static bool findBeginning(const nsDependentCSubstring &aToken,
                            const nsACString &aSourceString);

  /**
   * Tests if aSourceString starts with aToken in a case sensitive way.
   *
   * @param aToken
   *        The string to search for.
   * @param aSourceString
   *        The string to search.
   * @return true if found, false otherwise.
   */
  static bool findBeginningCaseSensitive(const nsDependentCSubstring &aToken,
                                         const nsACString &aSourceString);

  /**
   * Searches aSourceString for aToken anywhere in the string in a case-
   * insensitive way.
   *
   * @param aToken
   *        The string to search for.
   * @param aSourceString
   *        The string to search.
   * @return true if found, false otherwise.
   */
  static bool findAnywhere(const nsDependentCSubstring &aToken,
                           const nsACString &aSourceString);

  /**
   * Tests if aToken is found on a word boundary in aSourceString.
   *
   * @param aToken
   *        The string to search for.
   * @param aSourceString
   *        The string to search.
   * @return true if found, false otherwise.
   */
  static bool findOnBoundary(const nsDependentCSubstring &aToken,
                             const nsACString &aSourceString);


  /**
   * Fixes a URI's spec such that it is ready to be searched.  This includes
   * unescaping escaped characters and removing certain specs that we do not
   * care to search for.
   *
   * @param aURISpec
   *        The spec of the URI to prepare for searching.
   * @param aMatchBehavior
   *        The matching behavior to use defined by one of the
   *        mozIPlacesAutoComplete::MATCH_* values.
   * @param aSpecBuf
   *        A string buffer that the returned slice can point into, if needed.
   * @return the fixed up string.
   */
  static nsDependentCSubstring fixupURISpec(const nsACString &aURISpec,
                                            int32_t aMatchBehavior,
                                            nsACString &aSpecBuf);
};


////////////////////////////////////////////////////////////////////////////////
//// Frecency Calculation Function

/**
 * This function is used to calculate frecency for a page.
 *
 * In SQL, you'd use it in when setting frecency like:
 * SET frecency = CALCULATE_FRECENCY(place_id).
 * Optional parameters must be passed in if the page is not yet in the database,
 * otherwise they will be fetched from it automatically.
 *
 * @param pageId
 *        The id of the page.  Pass -1 if the page is being added right now.
 * @param [optional] typed
 *        Whether the page has been typed in.  Default is false.
 * @param [optional] fullVisitCount
 *        Count of all the visits (All types).  Default is 0.
 * @param [optional] isBookmarked
 *        Whether the page is bookmarked. Default is false.
 */
class CalculateFrecencyFunction final : public mozIStorageFunction
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION

  /**
   * Registers the function with the specified database connection.
   *
   * @param aDBConn
   *        The database connection to register with.
   */
  static nsresult create(mozIStorageConnection *aDBConn);
private:
  ~CalculateFrecencyFunction() {}
};

/**
 * SQL function to generate a GUID for a place or bookmark item.  This is just
 * a wrapper around GenerateGUID in Helpers.h.
 *
 * @return a guid for the item.
 */
class GenerateGUIDFunction final : public mozIStorageFunction
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION

  /**
   * Registers the function with the specified database connection.
   *
   * @param aDBConn
   *        The database connection to register with.
   */
  static nsresult create(mozIStorageConnection *aDBConn);
private:
  ~GenerateGUIDFunction() {}
};

/**
 * SQL function to unreverse the rev_host of a page.
 *
 * @param rev_host
 *        The rev_host value of the page.
 *
 * @return the unreversed host of the page.
 */
class GetUnreversedHostFunction final : public mozIStorageFunction
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION

  /**
   * Registers the function with the specified database connection.
   *
   * @param aDBConn
   *        The database connection to register with.
   */
  static nsresult create(mozIStorageConnection *aDBConn);
private:
  ~GetUnreversedHostFunction() {}
};


////////////////////////////////////////////////////////////////////////////////
//// Fixup URL Function

/**
 * Make a given URL more suitable for searches, by removing common prefixes
 * such as "www."
 *
 * @param url
 *        A URL.
 * @return
 *        The same URL, with redundant parts removed.
 */
class FixupURLFunction final : public mozIStorageFunction
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION

  /**
   * Registers the function with the specified database connection.
   *
   * @param aDBConn
   *        The database connection to register with.
   */
  static nsresult create(mozIStorageConnection *aDBConn);
private:
  ~FixupURLFunction() {}
};


////////////////////////////////////////////////////////////////////////////////
//// Frecency Changed Notification Function

/**
 * For a given place, posts a runnable to the main thread that calls
 * onFrecencyChanged on nsNavHistory's nsINavHistoryObservers.  The passed-in
 * newFrecency value is returned unchanged.
 *
 * @param newFrecency
 *        The place's new frecency.
 * @param url
 *        The place's URL.
 * @param guid
 *        The place's GUID.
 * @param hidden
 *        The place's hidden boolean.
 * @param lastVisitDate
 *        The place's last visit date.
 * @return newFrecency
 */
class FrecencyNotificationFunction final : public mozIStorageFunction
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION

  /**
   * Registers the function with the specified database connection.
   *
   * @param aDBConn
   *        The database connection to register with.
   */
  static nsresult create(mozIStorageConnection *aDBConn);
private:
  ~FrecencyNotificationFunction() {}
};


////////////////////////////////////////////////////////////////////////////////
//// Store Last Inserted Id Function

/**
 * Store the last inserted id for reference purpose.
 *
 * @param tableName
 *        The table name.
 * @param id
 *        The last inserted id.
 * @return null
 */
class StoreLastInsertedIdFunction final : public mozIStorageFunction
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION

  /**
   * Registers the function with the specified database connection.
   *
   * @param aDBConn
   *        The database connection to register with.
   */
  static nsresult create(mozIStorageConnection *aDBConn);
private:
  ~StoreLastInsertedIdFunction() {}
};


////////////////////////////////////////////////////////////////////////////////
//// Hash Function

/**
 * Calculates hash for a given string using the mfbt AddToHash function.
 *
 * @param string
 *        A string.
 * @return
 *        The hash for the string.
 */
class HashFunction final : public mozIStorageFunction
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION

  /**
   * Registers the function with the specified database connection.
   *
   * @param aDBConn
   *        The database connection to register with.
   */
  static nsresult create(mozIStorageConnection *aDBConn);
private:
  ~HashFunction() {}
};

} // namespace places
} // namespace mozilla

#endif // mozilla_places_SQLFunctions_h_
