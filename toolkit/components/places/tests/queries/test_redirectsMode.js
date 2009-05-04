/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
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
 * The Original Code is Places Test Code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net> (Original Author)
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

// Array of visits we will add to the database, will be populated later
// in the test.
let visits = [];

/**
 * Takes a sequence of query options, and compare query results obtained through
 * them with a custom filtered array of visits, based on the values we are
 * expecting from the query.
 *
 * @param  aSequence
 *         an array that contains query options in the form:
 *         [includeHidden, redirectsMode, maxResults, sortingMode]
 */
function check_results_callback(aSequence) {
  // Sanity check: we should receive 3 parameters.
  do_check_eq(aSequence.length, 4);
  let includeHidden = aSequence[0];
  let redirectsMode = aSequence[1];
  let maxResults = aSequence[2];
  let sortingMode = aSequence[3];
  print("\nTESTING: includeHidden(" + includeHidden + ")," +
                  " redirectsMode(" + redirectsMode + ")," +
                  " maxResults("    + maxResults    + ")," +
                  " sortingMode("   + sortingMode   + ").");

  // Build expectedData array.
  let expectedData = visits.filter(function(aVisit, aIndex, aArray) {
    switch (aVisit.transType) {
      case Ci.nsINavHistoryService.TRANSITION_TYPED:
      case Ci.nsINavHistoryService.TRANSITION_LINK:
      case Ci.nsINavHistoryService.TRANSITION_BOOKMARK:
      case Ci.nsINavHistoryService.TRANSITION_DOWNLOAD:
        return redirectsMode != Ci.nsINavHistoryQueryOptions.REDIRECTS_MODE_TARGET;
      case Ci.nsINavHistoryService.TRANSITION_EMBED:
        return includeHidden && redirectsMode != Ci.nsINavHistoryQueryOptions.REDIRECTS_MODE_TARGET;
      case Ci.nsINavHistoryService.TRANSITION_REDIRECT_TEMPORARY:
      case Ci.nsINavHistoryService.TRANSITION_REDIRECT_PERMANENT:
        if (redirectsMode == Ci.nsINavHistoryQueryOptions.REDIRECTS_MODE_ALL)
          return true;
        if (redirectsMode == Ci.nsINavHistoryQueryOptions.REDIRECTS_MODE_TARGET) {
           // We must check if this redirect will redirect again.
          return aArray.filter(function(refVisit) {
                                return refVisit.referrer == aVisit.uri;
                               }).length == 0;
        }
        return false;
      default:
        return false;
    }
  });

  // Sort expectedData.
  if (sortingMode > 0) {
    function comparator(a, b) {
      if (sortingMode == Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING)
        return b.lastVisit - a.lastVisit;
      else if (sortingMode == Ci.nsINavHistoryQueryOptions.SORT_BY_VISITCOUNT_DESCENDING)
        return b.visitCount - a.visitCount;
      else
        return 0;
    }
    expectedData.sort(comparator);
  }

  // Crop results to maxResults if it's defined.
  if (maxResults) {
    expectedData = expectedData.filter(function(aVisit, aIndex) {
                                        return aIndex < maxResults;
                                       });
  }

  // Create a new query with required options.
  let query = histsvc.getNewQuery();
  let options = histsvc.getNewQueryOptions();
  options.includeHidden = includeHidden;
  options.redirectsMode = redirectsMode;
  options.sortingMode = sortingMode;
  if (maxResults)
    options.maxResults = maxResults;

  // Compare resultset with expectedData.
  let result = histsvc.executeQuery(query, options);
  let root = result.root;
  root.containerOpen = true;
  compareArrayToResult(expectedData, root);
  root.containerOpen = false;
}

/**
 * Enumerates all the sequences of the cartesian product of the arrays contained
 * in aSequences.  Examples:
 *
 *   cartProd([[1, 2, 3], ["a", "b"]], callback);
 *   // callback is called 3 * 2 = 6 times with the following arrays:
 *   // [1, "a"], [1, "b"], [2, "a"], [2, "b"], [3, "a"], [3, "b"]
 *
 *   cartProd([["a"], [1, 2, 3], ["X", "Y"]], callback);
 *   // callback is called 1 * 3 * 2 = 6 times with the following arrays:
 *   // ["a", 1, "X"], ["a", 1, "Y"], ["a", 2, "X"], ["a", 2, "Y"],
 *   // ["a", 3, "X"], ["a", 3, "Y"]
 *
 *   cartProd([[1], [2], [3], [4]], callback);
 *   // callback is called 1 * 1 * 1 * 1 = 1 time with the following array:
 *   // [1, 2, 3, 4]
 *
 *   cartProd([], callback);
 *   // callback is 0 times
 *
 *   cartProd([[1, 2, 3, 4]], callback);
 *   // callback is called 4 times with the following arrays:
 *   // [1], [2], [3], [4]
 *
 * @param  aSequences
 *         an array that contains an arbitrary number of arrays
 * @param  aCallback
 *         a function that is passed each sequence of the product as it's
 *         computed
 * @return the total number of sequences in the product
 */
function cartProd(aSequences, aCallback)
{
  if (aSequences.length === 0)
    return 0;

  // For each sequence in aSequences, we maintain a pointer (an array index,
  // really) to the element we're currently enumerating in that sequence
  let seqEltPtrs = aSequences.map(function (i) 0);

  let numProds = 0;
  let done = false;
  while (!done) {
    numProds++;

    // prod = sequence in product we're currently enumerating
    let prod = [];
    for (let i = 0; i < aSequences.length; i++) {
      prod.push(aSequences[i][seqEltPtrs[i]]);
    }
    aCallback(prod);

    // The next sequence in the product differs from the current one by just a
    // single element.  Determine which element that is.  We advance the
    // "rightmost" element pointer to the "right" by one.  If we move past the
    // end of that pointer's sequence, reset the pointer to the first element
    // in its sequence and then try the sequence to the "left", and so on.

    // seqPtr = index of rightmost input sequence whose element pointer is not
    // past the end of the sequence
    let seqPtr = aSequences.length - 1;
    while (!done) {
      // Advance the rightmost element pointer.
      seqEltPtrs[seqPtr]++;

      // The rightmost element pointer is past the end of its sequence.
      if (seqEltPtrs[seqPtr] >= aSequences[seqPtr].length) {
        seqEltPtrs[seqPtr] = 0;
        seqPtr--;

        // All element pointers are past the ends of their sequences.
        if (seqPtr < 0)
          done = true;
      }
      else break;
    }
  }
  return numProds;
}

/**
 * Populate the visits array and add visits to the database.
 * We will generate visit-chains like:
 *   visit -> redirect_temp -> redirect_perm
 */
function add_visits_to_database() {
  // Clean up the database.
  bhistsvc.removeAllPages();
  remove_all_bookmarks();

  // We don't really bother on this, but we need a time to add visits.
  let timeInMicroseconds = Date.now() * 1000;
  let visitCount = 1;

  // Array of all possible transition types we could be redirected from.
  let t = [
    Ci.nsINavHistoryService.TRANSITION_LINK,
    Ci.nsINavHistoryService.TRANSITION_TYPED,
    Ci.nsINavHistoryService.TRANSITION_BOOKMARK,
    Ci.nsINavHistoryService.TRANSITION_EMBED,
    // Would make hard sorting by visit date because last_visit_date is actually
    // calculated excluding download transitions, but the query includes
    // downloads.
    // TODO: Bug 488966 could fix this behavior.
    //Ci.nsINavHistoryService.TRANSITION_DOWNLOAD,
  ];

  // we add a visit for each of the above transition types.
  t.forEach(function (transition) visits.push(
    { isVisit: true,
      transType: transition,
      uri: "http://" + transition + ".example.com/",
      title: transition + "-example",
      lastVisit: timeInMicroseconds--,
      visitCount: transition == Ci.nsINavHistoryService.TRANSITION_EMBED ? 0 : visitCount++,
      isInQuery: true }));

  // Add a REDIRECT_TEMPORARY layer of visits for each of the above visits.
  t.forEach(function (transition) visits.push(
    { isVisit: true,
      transType: Ci.nsINavHistoryService.TRANSITION_REDIRECT_TEMPORARY,
      uri: "http://" + transition + ".redirect.temp.example.com/",
      title: transition + "-redirect-temp-example",
      lastVisit: timeInMicroseconds--,
      isRedirect: true,
      referrer: "http://" + transition + ".example.com/",
      visitCount: visitCount++,
      isInQuery: true }));

  // Add a REDIRECT_PERMANENT layer of visits for each of the above redirects.
  t.forEach(function (transition) visits.push(
    { isVisit: true,
      transType: Ci.nsINavHistoryService.TRANSITION_REDIRECT_PERMANENT,
      uri: "http://" + transition + ".redirect.perm.example.com/",
      title: transition + "-redirect-perm-example",
      lastVisit: timeInMicroseconds--,
      isRedirect: true,
      referrer: "http://" + transition + ".redirect.temp.example.com/",
      visitCount: visitCount++,
      isInQuery: true }));

  // Put visits in the database.
  populateDB(visits);
}

// Main
function run_test() {
  // Populate the database.
  add_visits_to_database();

  // This array will be used by cartProd to generate a matrix of all possible
  // combinations.
  includeHidden_options = [true, false];
  redirectsMode_options =  [Ci.nsINavHistoryQueryOptions.REDIRECTS_MODE_ALL,
                            Ci.nsINavHistoryQueryOptions.REDIRECTS_MODE_SOURCE,
                            Ci.nsINavHistoryQueryOptions.REDIRECTS_MODE_TARGET];
  maxResults_options = [5, 10, 20, null];
  // These sortingMode are choosen to toggle using special queries for history
  // menu and most visited smart bookmark.
  sorting_options = [Ci.nsINavHistoryQueryOptions.SORT_BY_NONE,
                     Ci.nsINavHistoryQueryOptions.SORT_BY_VISITCOUNT_DESCENDING,
                     Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING];
  // Will execute check_results_callback() for each generated combination.
  cartProd([includeHidden_options, redirectsMode_options, maxResults_options, sorting_options],
           check_results_callback);

  // Clean up so we can't pollute next tests.
  bhistsvc.removeAllPages();
  remove_all_bookmarks();
}
