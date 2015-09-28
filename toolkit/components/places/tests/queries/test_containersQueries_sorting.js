/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Testing behavior of bug 473157
 * "Want to sort history in container view without sorting the containers"
 * and regression bug 488783
 * Tags list no longer sorted (alphabetized).
 * This test is for global testing sorting containers queries.
 */

////////////////////////////////////////////////////////////////////////////////
//// Globals and Constants

var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
var bh = hs.QueryInterface(Ci.nsIBrowserHistory);
var tagging = Cc["@mozilla.org/browser/tagging-service;1"].
              getService(Ci.nsITaggingService);

var resultTypes = [
  {value: Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_QUERY, name: "RESULTS_AS_DATE_QUERY"},
  {value: Ci.nsINavHistoryQueryOptions.RESULTS_AS_SITE_QUERY, name: "RESULTS_AS_SITE_QUERY"},
  {value: Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_SITE_QUERY, name: "RESULTS_AS_DATE_SITE_QUERY"},
  {value: Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_QUERY, name: "RESULTS_AS_TAG_QUERY"},
];

var sortingModes = [
  {value: Ci.nsINavHistoryQueryOptions.SORT_BY_TITLE_ASCENDING, name: "SORT_BY_TITLE_ASCENDING"},
  {value: Ci.nsINavHistoryQueryOptions.SORT_BY_TITLE_DESCENDING, name: "SORT_BY_TITLE_DESCENDING"},
  {value: Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_ASCENDING, name: "SORT_BY_DATE_ASCENDING"},
  {value: Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING, name: "SORT_BY_DATE_DESCENDING"},
  {value: Ci.nsINavHistoryQueryOptions.SORT_BY_DATEADDED_ASCENDING, name: "SORT_BY_DATEADDED_ASCENDING"},
  {value: Ci.nsINavHistoryQueryOptions.SORT_BY_DATEADDED_DESCENDING, name: "SORT_BY_DATEADDED_DESCENDING"},
];

// These pages will be added from newest to oldest and from less visited to most
// visited.
var pages = [
  "http://www.mozilla.org/c/",
  "http://www.mozilla.org/a/",
  "http://www.mozilla.org/b/",
  "http://www.mozilla.com/c/",
  "http://www.mozilla.com/a/",
  "http://www.mozilla.com/b/",
];

var tags = [
  "mozilla",
  "Development",
  "test",
];

////////////////////////////////////////////////////////////////////////////////
//// Test Runner

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
  var seqEltPtrs = aSequences.map(i => 0);

  var numProds = 0;
  var done = false;
  while (!done) {
    numProds++;

    // prod = sequence in product we're currently enumerating
    var prod = [];
    for (var i = 0; i < aSequences.length; i++) {
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
    var seqPtr = aSequences.length - 1;
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
 * Test a query based on passed-in options.
 *
 * @param aSequence
 *        array of options we will use to query.
 */
function test_query_callback(aSequence) {
  do_check_eq(aSequence.length, 2);
  var resultType = aSequence[0];
  var sortingMode = aSequence[1];
  print("\n\n*** Testing default sorting for resultType (" + resultType.name + ") and sortingMode (" + sortingMode.name + ")");

  // Skip invalid combinations sorting queries by none.
  if (resultType.value == Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_QUERY &&
      (sortingMode.value == Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_ASCENDING ||
       sortingMode.value == Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING)) {
    // This is a bookmark query, we can't sort by visit date.
    sortingMode.value = Ci.nsINavHistoryQueryOptions.SORT_BY_NONE;
  }
  if (resultType.value == Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_QUERY ||
      resultType.value == Ci.nsINavHistoryQueryOptions.RESULTS_AS_SITE_QUERY ||
      resultType.value == Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_SITE_QUERY) {
    // This is an history query, we can't sort by date added.
    if (sortingMode.value == Ci.nsINavHistoryQueryOptions.SORT_BY_DATEADDED_ASCENDING ||
       sortingMode.value == Ci.nsINavHistoryQueryOptions.SORT_BY_DATEADDED_DESCENDING)
    sortingMode.value = Ci.nsINavHistoryQueryOptions.SORT_BY_NONE;
  }

  // Create a new query with required options.
  var query = PlacesUtils.history.getNewQuery();
  var options = PlacesUtils.history.getNewQueryOptions();
  options.resultType = resultType.value;
  options.sortingMode = sortingMode.value;

  // Compare resultset with expectedData.
  var result = PlacesUtils.history.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;

  if (resultType.value == Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_QUERY ||
      resultType.value == Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_SITE_QUERY) {
    // Date containers are always sorted by date descending.
    check_children_sorting(root,
                           Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING);
  }
  else
    check_children_sorting(root, sortingMode.value);

  // Now Check sorting of the first child container.
  var container = root.getChild(0)
                      .QueryInterface(Ci.nsINavHistoryContainerResultNode);
  container.containerOpen = true;

  if (resultType.value == Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_SITE_QUERY) {
    // Has more than one level of containers, first we check the sorting of
    // the first level (site containers), those won't inherit sorting...
    check_children_sorting(container,
                           Ci.nsINavHistoryQueryOptions.SORT_BY_TITLE_ASCENDING);
    // ...then we check sorting of the contained urls, we can't inherit sorting
    // since the above level does not inherit it, so they will be sorted by
    // title ascending.
    let innerContainer = container.getChild(0)
                                  .QueryInterface(Ci.nsINavHistoryContainerResultNode);
    innerContainer.containerOpen = true;
    check_children_sorting(innerContainer,
                           Ci.nsINavHistoryQueryOptions.SORT_BY_TITLE_ASCENDING);
    innerContainer.containerOpen = false;
  }
  else if (resultType.value == Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_QUERY) {
    // Sorting mode for tag contents is hardcoded for now, to allow for faster
    // duplicates filtering.
    check_children_sorting(container,
                           Ci.nsINavHistoryQueryOptions.SORT_BY_NONE);
  }
  else
    check_children_sorting(container, sortingMode.value);

  container.containerOpen = false;
  root.containerOpen = false;

  test_result_sortingMode_change(result, resultType, sortingMode);
}

/**
 * Sets sortingMode on aResult and checks for correct sorting of children.
 * Containers should not change their sorting, while contained uri nodes should.
 *
 * @param aResult
 *        nsINavHistoryResult generated by our query.
 * @param aResultType
 *        required result type.
 * @param aOriginalSortingMode
 *        the sorting mode from query's options.
 */
function test_result_sortingMode_change(aResult, aResultType, aOriginalSortingMode) {
  var root = aResult.root;
  // Now we set sortingMode on the result and check that containers are not
  // sorted while children are.
  sortingModes.forEach(function sortingModeChecker(aForcedSortingMode) {
    print("\n* Test setting sortingMode (" + aForcedSortingMode.name + ") " +
          "on result with resultType (" + aResultType.name + ") " +
          "currently sorted as (" + aOriginalSortingMode.name + ")");

    aResult.sortingMode = aForcedSortingMode.value;
    root.containerOpen = true;

    if (aResultType.value == Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_QUERY ||
        aResultType.value == Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_SITE_QUERY) {
      // Date containers are always sorted by date descending.
      check_children_sorting(root,
                             Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING);
    }
    else if (aResultType.value == Ci.nsINavHistoryQueryOptions.RESULTS_AS_SITE_QUERY &&
             (aOriginalSortingMode.value == Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_ASCENDING ||
              aOriginalSortingMode.value == Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING)) {
      // Site containers don't have a good time property to sort by.
      check_children_sorting(root,
                             Ci.nsINavHistoryQueryOptions.SORT_BY_NONE);
    }
    else
      check_children_sorting(root, aOriginalSortingMode.value);

    // Now Check sorting of the first child container.
    var container = root.getChild(0)
                        .QueryInterface(Ci.nsINavHistoryContainerResultNode);
    container.containerOpen = true;

    if (aResultType.value == Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_SITE_QUERY) {
      // Has more than one level of containers, first we check the sorting of
      // the first level (site containers), those won't be sorted...
      check_children_sorting(container,
                             Ci.nsINavHistoryQueryOptions.SORT_BY_TITLE_ASCENDING);
      // ...then we check sorting of the second level of containers, result
      // will sort them through recursiveSort.
      let innerContainer = container.getChild(0)
                                    .QueryInterface(Ci.nsINavHistoryContainerResultNode);
      innerContainer.containerOpen = true;
      check_children_sorting(innerContainer, aForcedSortingMode.value);
      innerContainer.containerOpen = false;
    }
    else {
      if (aResultType.value == Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_QUERY ||
          aResultType.value == Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_SITE_QUERY ||
          aResultType.value == Ci.nsINavHistoryQueryOptions.RESULTS_AS_SITE_QUERY) {
        // Date containers are always sorted by date descending.
        check_children_sorting(root, Ci.nsINavHistoryQueryOptions.SORT_BY_NONE);
      }
      else if (aResultType.value == Ci.nsINavHistoryQueryOptions.RESULTS_AS_SITE_QUERY &&
             (aOriginalSortingMode.value == Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_ASCENDING ||
              aOriginalSortingMode.value == Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING)) {
        // Site containers don't have a good time property to sort by.
        check_children_sorting(root, Ci.nsINavHistoryQueryOptions.SORT_BY_NONE);
      }
      else
        check_children_sorting(root, aOriginalSortingMode.value);

      // Children should always be sorted.
      check_children_sorting(container, aForcedSortingMode.value);
    }

    container.containerOpen = false;
    root.containerOpen = false;
  });
}

/**
 * Test if children of aRootNode are correctly sorted.
 * @param aRootNode
 *        already opened root node from our query's result.
 * @param aExpectedSortingMode
 *        The sortingMode we expect results to be.
 */
function check_children_sorting(aRootNode, aExpectedSortingMode) {
  var results = [];
  print("Found children:");
  for (var i = 0; i < aRootNode.childCount; i++) {
    results[i] = aRootNode.getChild(i);
    print(i + " " + results[i].title);
  }

  // Helper for case insensitive string comparison.
  function caseInsensitiveStringComparator(a, b) {
    var aLC = a.toLowerCase();
    var bLC = b.toLowerCase();
    if (aLC < bLC)
      return -1;
    if (aLC > bLC)
      return 1;
    return 0;
  }

  // Get a comparator based on expected sortingMode.
  var comparator;
  switch(aExpectedSortingMode) {
    case Ci.nsINavHistoryQueryOptions.SORT_BY_NONE:
      comparator = function (a, b) {
        return 0;
      }
      break;
    case Ci.nsINavHistoryQueryOptions.SORT_BY_TITLE_ASCENDING:
      comparator = function (a, b) {
        return caseInsensitiveStringComparator(a.title, b.title);
      }
      break;
    case Ci.nsINavHistoryQueryOptions.SORT_BY_TITLE_DESCENDING:
      comparator = function (a, b) {
        return -caseInsensitiveStringComparator(a.title, b.title);
      }
      break;
    case Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_ASCENDING:
      comparator = function (a, b) {
        return a.time - b.time;
      }
      break;
    case Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING:
      comparator = function (a, b) {
        return b.time - a.time;
      }
    case Ci.nsINavHistoryQueryOptions.SORT_BY_DATEADDED_ASCENDING:
      comparator = function (a, b) {
        return a.dateAdded - b.dateAdded;
      }
      break;
    case Ci.nsINavHistoryQueryOptions.SORT_BY_DATEADDED_DESCENDING:
      comparator = function (a, b) {
        return b.dateAdded - a.dateAdded;
      }
      break;
    default:
      do_throw("Unknown sorting type: " + aExpectedSortingMode);
  }

  // Make an independent copy of the results array and sort it.
  var sortedResults = results.slice();
  sortedResults.sort(comparator);
  // Actually compare returned children with our sorted array.
  for (var i = 0; i < sortedResults.length; i++) {
    if (sortedResults[i].title != results[i].title)
      print(i + " index wrong, expected " + sortedResults[i].title +
            " found " + results[i].title);
    do_check_eq(sortedResults[i].title, results[i].title);
  }
}

////////////////////////////////////////////////////////////////////////////////
//// Main

function run_test()
{
  run_next_test();
}

add_task(function test_containersQueries_sorting()
{
  // Add visits, bookmarks and tags to our database.
  var timeInMilliseconds = Date.now();
  var visitCount = 0;
  var dayOffset = 0;
  var visits = [];
  pages.forEach(aPageUrl => visits.push(
    { isVisit: true,
      isBookmark: true,
      transType: Ci.nsINavHistoryService.TRANSITION_TYPED,
      uri: aPageUrl,
      title: aPageUrl,
      // subtract 5 hours per iteration, to expose more than one day container.
      lastVisit: (timeInMilliseconds - (18000 * 1000 * dayOffset++)) * 1000,
      visitCount: visitCount++,
      isTag: true,
      tagArray: tags,
      isInQuery: true }));
  yield task_populateDB(visits);

  cartProd([resultTypes, sortingModes], test_query_callback);
});
