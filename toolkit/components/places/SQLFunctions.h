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
 * the Mozilla Foundation.
 *
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

#ifndef mozilla_places_SQLFunctions_h_
#define mozilla_places_SQLFunctions_h_

/**
 * This file contains functions that Places adds to the database handle that can
 * be accessed by SQL queries.
 */

#include "mozIStorageFunction.h"

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
class MatchAutoCompleteFunction : public mozIStorageFunction
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION

  /**
   * Registers the function with the specified database connection.
   *
   * @param aDBConn
   *        The database connection to register with.
   */
  static nsresult create(mozIStorageConnection *aDBConn);

private:
  /**
   * Argument Indexes
   */
  static const PRUint32 kArgSearchString = 0;
  static const PRUint32 kArgIndexURL = 1;
  static const PRUint32 kArgIndexTitle = 2;
  static const PRUint32 kArgIndexTags = 3;
  static const PRUint32 kArgIndexVisitCount = 4;
  static const PRUint32 kArgIndexTyped = 5;
  static const PRUint32 kArgIndexBookmark = 6;
  static const PRUint32 kArgIndexOpenPageCount = 7;
  static const PRUint32 kArgIndexMatchBehavior = 8;
  static const PRUint32 kArgIndexSearchBehavior = 9;
  static const PRUint32 kArgIndexLength = 10;

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
  static searchFunctionPtr getSearchFunction(PRInt32 aBehavior);

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
   * @param _fixedSpec
   *        An out parameter that is the fixed up string.
   */
  static void fixupURISpec(const nsCString &aURISpec, PRInt32 aMatchBehavior,
                           nsCString &_fixedSpec);
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
class CalculateFrecencyFunction : public mozIStorageFunction
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION

  /**
   * Registers the function with the specified database connection.
   *
   * @param aDBConn
   *        The database connection to register with.
   */
  static nsresult create(mozIStorageConnection *aDBConn);
};

/**
 * SQL function to generate a GUID for a place or bookmark item.  This is just
 * a wrapper around GenerateGUID in Helpers.h.
 *
 * @return a guid for the item.
 */
class GenerateGUIDFunction : public mozIStorageFunction
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION

  /**
   * Registers the function with the specified database connection.
   *
   * @param aDBConn
   *        The database connection to register with.
   */
  static nsresult create(mozIStorageConnection *aDBConn);
};

/**
 * SQL function to unreverse the rev_host of a page.
 *
 * @param rev_host
 *        The rev_host value of the page.
 *
 * @return the unreversed host of the page.
 */
class GetUnreversedHostFunction : public mozIStorageFunction
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION

  /**
   * Registers the function with the specified database connection.
   *
   * @param aDBConn
   *        The database connection to register with.
   */
  static nsresult create(mozIStorageConnection *aDBConn);
};

} // namespace places
} // namespace storage

#endif // mozilla_places_SQLFunctions_h_
