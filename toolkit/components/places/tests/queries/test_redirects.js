/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Array of visits we will add to the database, will be populated later
// in the test.
var visits = [];

/**
 * Takes a sequence of query options, and compare query results obtained through
 * them with a custom filtered array of visits, based on the values we are
 * expecting from the query.
 *
 * @param  aSequence
 *         an array that contains query options in the form:
 *         [includeHidden, maxResults, sortingMode]
 */
function check_results_callback(aSequence) {
  // Sanity check: we should receive 3 parameters.
  do_check_eq(aSequence.length, 3);
  let includeHidden = aSequence[0];
  let maxResults = aSequence[1];
  let sortingMode = aSequence[2];
  print("\nTESTING: includeHidden(" + includeHidden + ")," +
                  " maxResults(" + maxResults + ")," +
                  " sortingMode(" + sortingMode + ").");

  function isHidden(aVisit) {
    return aVisit.transType == Ci.nsINavHistoryService.TRANSITION_FRAMED_LINK ||
           aVisit.isRedirect;
  }

  // Build expectedData array.
  let expectedData = visits.filter(function(aVisit, aIndex, aArray) {
    // Embed visits never appear in results.
    if (aVisit.transType == Ci.nsINavHistoryService.TRANSITION_EMBED)
      return false;

    if (!includeHidden && isHidden(aVisit)) {
      // If the page has any non-hidden visit, then it's visible.
      if (visits.filter(function(refVisit) {
        return refVisit.uri == aVisit.uri && !isHidden(refVisit);
          }).length == 0)
        return false;
    }

    return true;
  });

  // Remove duplicates, since queries are RESULTS_AS_URI (unique pages).
  let seen = [];
  expectedData = expectedData.filter(function(aData) {
    if (seen.includes(aData.uri)) {
      return false;
    }
    seen.push(aData.uri);
    return true;
  });

  // Sort expectedData.
  function getFirstIndexFor(aEntry) {
    for (let i = 0; i < visits.length; i++) {
      if (visits[i].uri == aEntry.uri)
        return i;
    }
    return undefined;
  }
  function comparator(a, b) {
    if (sortingMode == Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING) {
      return b.lastVisit - a.lastVisit;
    }
    if (sortingMode == Ci.nsINavHistoryQueryOptions.SORT_BY_VISITCOUNT_DESCENDING) {
      return b.visitCount - a.visitCount;
    }
    return getFirstIndexFor(a) - getFirstIndexFor(b);
  }
  expectedData.sort(comparator);

  // Crop results to maxResults if it's defined.
  if (maxResults) {
    expectedData = expectedData.slice(0, maxResults);
  }

  // Create a new query with required options.
  let query = PlacesUtils.history.getNewQuery();
  let options = PlacesUtils.history.getNewQueryOptions();
  options.includeHidden = includeHidden;
  options.sortingMode = sortingMode;
  if (maxResults)
    options.maxResults = maxResults;

  // Compare resultset with expectedData.
  let result = PlacesUtils.history.executeQuery(query, options);
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
function cartProd(aSequences, aCallback) {
  if (aSequences.length === 0)
    return 0;

  // For each sequence in aSequences, we maintain a pointer (an array index,
  // really) to the element we're currently enumerating in that sequence
  let seqEltPtrs = aSequences.map(i => 0);

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
      } else break;
    }
  }
  return numProds;
}

/**
 * Populate the visits array and add visits to the database.
 * We will generate visit-chains like:
 *   visit -> redirect_temp -> redirect_perm
 */
add_task(async function test_add_visits_to_database() {
  await PlacesUtils.bookmarks.eraseEverything();

  // We don't really bother on this, but we need a time to add visits.
  let timeInMicroseconds = Date.now() * 1000;
  let visitCount = 1;

  // Array of all possible transition types we could be redirected from.
  let t = [
    Ci.nsINavHistoryService.TRANSITION_LINK,
    Ci.nsINavHistoryService.TRANSITION_TYPED,
    Ci.nsINavHistoryService.TRANSITION_BOOKMARK,
    // Embed visits are not added to the database and we don't want redirects
    // to them, thus just avoid addition.
    // Ci.nsINavHistoryService.TRANSITION_EMBED,
    Ci.nsINavHistoryService.TRANSITION_FRAMED_LINK,
    // Would make hard sorting by visit date because last_visit_date is actually
    // calculated excluding download transitions, but the query includes
    // downloads.
    // TODO: Bug 488966 could fix this behavior.
    //Ci.nsINavHistoryService.TRANSITION_DOWNLOAD,
  ];

  function newTimeInMicroseconds() {
    timeInMicroseconds = timeInMicroseconds - 1000;
    return timeInMicroseconds;
  }

  // we add a visit for each of the above transition types.
  t.forEach(transition => visits.push(
    { isVisit: true,
      transType: transition,
      uri: "http://" + transition + ".example.com/",
      title: transition + "-example",
      isRedirect: true,
      lastVisit: newTimeInMicroseconds(),
      visitCount: (transition == Ci.nsINavHistoryService.TRANSITION_EMBED ||
                   transition == Ci.nsINavHistoryService.TRANSITION_FRAMED_LINK) ? 0 : visitCount++,
      isInQuery: true }));

  // Add a REDIRECT_TEMPORARY layer of visits for each of the above visits.
  t.forEach(transition => visits.push(
    { isVisit: true,
      transType: Ci.nsINavHistoryService.TRANSITION_REDIRECT_TEMPORARY,
      uri: "http://" + transition + ".redirect.temp.example.com/",
      title: transition + "-redirect-temp-example",
      lastVisit: newTimeInMicroseconds(),
      isRedirect: true,
      referrer: "http://" + transition + ".example.com/",
      visitCount: visitCount++,
      isInQuery: true }));

  // Add a REDIRECT_PERMANENT layer of visits for each of the above redirects.
  t.forEach(transition => visits.push(
    { isVisit: true,
      transType: Ci.nsINavHistoryService.TRANSITION_REDIRECT_PERMANENT,
      uri: "http://" + transition + ".redirect.perm.example.com/",
      title: transition + "-redirect-perm-example",
      lastVisit: newTimeInMicroseconds(),
      isRedirect: true,
      referrer: "http://" + transition + ".redirect.temp.example.com/",
      visitCount: visitCount++,
      isInQuery: true }));

  // Add a REDIRECT_PERMANENT layer of visits that loop to the first visit.
  // These entries should not change visitCount or lastVisit, otherwise
  // guessing an order would be a nightmare.
  function getLastValue(aURI, aProperty) {
    for (let i = 0; i < visits.length; i++) {
      if (visits[i].uri == aURI) {
        return visits[i][aProperty];
      }
    }
    do_throw("Unknown uri.");
    return null;
  }
  t.forEach(transition => visits.push(
    { isVisit: true,
      transType: Ci.nsINavHistoryService.TRANSITION_REDIRECT_PERMANENT,
      uri: "http://" + transition + ".example.com/",
      title: getLastValue("http://" + transition + ".example.com/", "title"),
      lastVisit: getLastValue("http://" + transition + ".example.com/", "lastVisit"),
      isRedirect: true,
      referrer: "http://" + transition + ".redirect.perm.example.com/",
      visitCount: getLastValue("http://" + transition + ".example.com/", "visitCount"),
      isInQuery: true }));

  // Add an unvisited bookmark in the database, it should never appear.
  visits.push({ isBookmark: true,
    uri: "http://unvisited.bookmark.com/",
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    title: "Unvisited Bookmark",
    isInQuery: false });

  // Put visits in the database.
  await task_populateDB(visits);
});

add_task(async function test_redirects() {
  // Frecency and hidden are updated asynchronously, wait for them.
  await PlacesTestUtils.promiseAsyncUpdates();

  // This array will be used by cartProd to generate a matrix of all possible
  // combinations.
  let includeHidden_options = [true, false];
  let maxResults_options = [5, 10, 20, null];
  // These sortingMode are choosen to toggle using special queries for history
  // menu and most visited smart bookmark.
  let sorting_options = [Ci.nsINavHistoryQueryOptions.SORT_BY_NONE,
                         Ci.nsINavHistoryQueryOptions.SORT_BY_VISITCOUNT_DESCENDING,
                         Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING];
  // Will execute check_results_callback() for each generated combination.
  cartProd([includeHidden_options, maxResults_options, sorting_options],
           check_results_callback);

  await PlacesUtils.bookmarks.eraseEverything();

  await PlacesTestUtils.clearHistory();
});
