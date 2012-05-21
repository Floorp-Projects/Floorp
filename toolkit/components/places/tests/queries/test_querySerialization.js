/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests Places query serialization.  Associated bug is
 * https://bugzilla.mozilla.org/show_bug.cgi?id=370197
 *
 * The simple idea behind this test is to try out different combinations of
 * query switches and ensure that queries are the same before serialization
 * as they are after de-serialization.
 *
 * In the code below, "switch" refers to a query option -- "option" in a broad
 * sense, not nsINavHistoryQueryOptions specifically (which is why we refer to
 * them as switches, not options).  Both nsINavHistoryQuery and
 * nsINavHistoryQueryOptions allow you to specify switches that affect query
 * strings.  nsINavHistoryQuery instances have attributes hasBeginTime,
 * hasEndTime, hasSearchTerms, and so on.  nsINavHistoryQueryOptions instances
 * have attributes sortingMode, resultType, excludeItems, etc.
 *
 * Ideally we would like to test all 2^N subsets of switches, where N is the
 * total number of switches; switches might interact in erroneous or other ways
 * we do not expect.  However, since N is large (21 at this time), that's
 * impractical for a single test in a suite.
 *
 * Instead we choose all possible subsets of a certain, smaller size.  In fact
 * we begin by choosing CHOOSE_HOW_MANY_SWITCHES_LO and ramp up to
 * CHOOSE_HOW_MANY_SWITCHES_HI.
 *
 * There are two more wrinkles.  First, for some switches we'd like to be able to
 * test multiple values.  For example, it seems like a good idea to test both an
 * empty string and a non-empty string for switch nsINavHistoryQuery.searchTerms.
 * When switches have more than one value for a test run, we use the Cartesian
 * product of their values to generate all possible combinations of values.
 *
 * Second, we need to also test serialization of multiple nsINavHistoryQuery
 * objects at once.  To do this, we remember the previous NUM_MULTIPLE_QUERIES
 * queries we tested individually and then serialize them together.  We do this
 * each time we test an individual query.  Thus the set of queries we test
 * together loses one query and gains another each time.
 *
 * To summarize, here's how this test works:
 *
 * - For n = CHOOSE_HOW_MANY_SWITCHES_LO to CHOOSE_HOW_MANY_SWITCHES_HI:
 *   - From the total set of switches choose all possible subsets of size n.
 *     For each of those subsets s:
 *     - Collect the test runs of each switch in subset s and take their
 *       Cartesian product.  For each sequence in the product:
 *       - Create nsINavHistoryQuery and nsINavHistoryQueryOptions objects
 *         with the chosen switches and test run values.
 *       - Serialize the query.
 *       - De-serialize and ensure that the de-serialized query objects equal
 *         the originals.
 *       - For each of the previous NUM_MULTIPLE_QUERIES
 *         nsINavHistoryQueryOptions objects o we created:
 *         - Serialize the previous NUM_MULTIPLE_QUERIES nsINavHistoryQuery
 *           objects together with o.
 *         - De-serialize and ensure that the de-serialized query objects
 *           equal the originals.
 */

const CHOOSE_HOW_MANY_SWITCHES_LO = 1;
const CHOOSE_HOW_MANY_SWITCHES_HI = 2;

const NUM_MULTIPLE_QUERIES        = 2;

// The switches are represented by objects below, in arrays querySwitches and
// queryOptionSwitches.  Use them to set up test runs.
//
// Some switches have special properties (where noted), but all switches must
// have the following properties:
//
//   matches: A function that takes two nsINavHistoryQuery objects (in the case
//            of nsINavHistoryQuery switches) or two nsINavHistoryQueryOptions
//            objects (for nsINavHistoryQueryOptions switches) and returns true
//            if the values of the switch in the two objects are equal.  This is
//            the foundation of how we determine if two queries are equal.
//   runs:    An array of functions.  Each function takes an nsINavHistoryQuery
//            object and an nsINavHistoryQueryOptions object.  The functions
//            should set the attributes of one of the two objects as appropriate
//            to their switches.  This is how switch values are set for each test
//            run.
//
// The following properties are optional:
//
//   desc:    An informational string to print out during runs when the switch
//            is chosen.  Hopefully helpful if the test fails.

// nsINavHistoryQuery switches
const querySwitches = [
  // hasBeginTime
  {
    // flag and subswitches are used by the flagSwitchMatches function.  Several
    // of the nsINavHistoryQuery switches (like this one) are really guard flags
    // that indicate if other "subswitches" are enabled.
    flag:        "hasBeginTime",
    subswitches: ["beginTime", "beginTimeReference", "absoluteBeginTime"],
    desc:        "nsINavHistoryQuery.hasBeginTime",
    matches:     flagSwitchMatches,
    runs:        [
      function (aQuery, aQueryOptions) {
        aQuery.beginTime = Date.now() * 1000;
        aQuery.beginTimeReference = Ci.nsINavHistoryQuery.TIME_RELATIVE_EPOCH;
      },
      function (aQuery, aQueryOptions) {
        aQuery.beginTime = Date.now() * 1000;
        aQuery.beginTimeReference = Ci.nsINavHistoryQuery.TIME_RELATIVE_TODAY;
      }
    ]
  },
  // hasEndTime
  {
    flag:        "hasEndTime",
    subswitches: ["endTime", "endTimeReference", "absoluteEndTime"],
    desc:        "nsINavHistoryQuery.hasEndTime",
    matches:     flagSwitchMatches,
    runs:        [
      function (aQuery, aQueryOptions) {
        aQuery.endTime = Date.now() * 1000;
        aQuery.endTimeReference = Ci.nsINavHistoryQuery.TIME_RELATIVE_EPOCH;
      },
      function (aQuery, aQueryOptions) {
        aQuery.endTime = Date.now() * 1000;
        aQuery.endTimeReference = Ci.nsINavHistoryQuery.TIME_RELATIVE_TODAY;
      }
    ]
  },
  // hasSearchTerms
  {
    flag:        "hasSearchTerms",
    subswitches: ["searchTerms"],
    desc:        "nsINavHistoryQuery.hasSearchTerms",
    matches:     flagSwitchMatches,
    runs:        [
      function (aQuery, aQueryOptions) {
        aQuery.searchTerms = "shrimp and white wine";
      },
      function (aQuery, aQueryOptions) {
        aQuery.searchTerms = "";
      }
    ]
  },
  // hasDomain
  {
    flag:        "hasDomain",
    subswitches: ["domain", "domainIsHost"],
    desc:        "nsINavHistoryQuery.hasDomain",
    matches:     flagSwitchMatches,
    runs:        [
      function (aQuery, aQueryOptions) {
        aQuery.domain = "mozilla.com";
        aQuery.domainIsHost = false;
      },
      function (aQuery, aQueryOptions) {
        aQuery.domain = "www.mozilla.com";
        aQuery.domainIsHost = true;
      },
      function (aQuery, aQueryOptions) {
        aQuery.domain = "";
      }
    ]
  },
  // hasUri
  {
    flag:        "hasUri",
    subswitches: ["uri", "uriIsPrefix"],
    desc:        "nsINavHistoryQuery.hasUri",
    matches:     flagSwitchMatches,
    runs:        [
      function (aQuery, aQueryOptions) {
        aQuery.uri = uri("http://mozilla.com");
        aQuery.uriIsPrefix = false;
      },
      function (aQuery, aQueryOptions) {
        aQuery.uri = uri("http://mozilla.com");
        aQuery.uriIsPrefix = true;
      }
    ]
  },
  // hasAnnotation
  {
    flag:        "hasAnnotation",
    subswitches: ["annotation", "annotationIsNot"],
    desc:        "nsINavHistoryQuery.hasAnnotation",
    matches:     flagSwitchMatches,
    runs:        [
      function (aQuery, aQueryOptions) {
        aQuery.annotation = "bookmarks/toolbarFolder";
        aQuery.annotationIsNot = false;
      },
      function (aQuery, aQueryOptions) {
        aQuery.annotation = "bookmarks/toolbarFolder";
        aQuery.annotationIsNot = true;
      }
    ]
  },
  // minVisits
  {
    // property is used by function simplePropertyMatches.
    property: "minVisits",
    desc:     "nsINavHistoryQuery.minVisits",
    matches:  simplePropertyMatches,
    runs:     [
      function (aQuery, aQueryOptions) {
        aQuery.minVisits = 0x7fffffff; // 2^31 - 1
      }
    ]
  },
  // maxVisits
  {
    property: "maxVisits",
    desc:     "nsINavHistoryQuery.maxVisits",
    matches:  simplePropertyMatches,
    runs:     [
      function (aQuery, aQueryOptions) {
        aQuery.maxVisits = 0x7fffffff; // 2^31 - 1
      }
    ]
  },
  // onlyBookmarked
  {
    property: "onlyBookmarked",
    desc:     "nsINavHistoryQuery.onlyBookmarked",
    matches:  simplePropertyMatches,
    runs:     [
      function (aQuery, aQueryOptions) {
        aQuery.onlyBookmarked = true;
      }
    ]
  },
  // getFolders
  {
    desc:    "nsINavHistoryQuery.getFolders",
    matches: function (aQuery1, aQuery2) {
      var q1Folders = aQuery1.getFolders();
      var q2Folders = aQuery2.getFolders();
      if (q1Folders.length !== q2Folders.length)
        return false;
      for (let i = 0; i < q1Folders.length; i++) {
        if (q2Folders.indexOf(q1Folders[i]) < 0)
          return false;
      }
      for (let i = 0; i < q2Folders.length; i++) {
        if (q1Folders.indexOf(q2Folders[i]) < 0)
          return false;
      }
      return true;
    },
    runs: [
      function (aQuery, aQueryOptions) {
        aQuery.setFolders([], 0);
      },
      function (aQuery, aQueryOptions) {
        aQuery.setFolders([PlacesUtils.placesRootId], 1);
      },
      function (aQuery, aQueryOptions) {
        aQuery.setFolders([PlacesUtils.placesRootId, PlacesUtils.tagsFolderId], 2);
      }
    ]
  },
  // tags
  {
    desc:    "nsINavHistoryQuery.getTags",
    matches: function (aQuery1, aQuery2) {
      if (aQuery1.tagsAreNot !== aQuery2.tagsAreNot)
        return false;
      var q1Tags = aQuery1.tags;
      var q2Tags = aQuery2.tags;
      if (q1Tags.length !== q2Tags.length)
        return false;
      for (let i = 0; i < q1Tags.length; i++) {
        if (q2Tags.indexOf(q1Tags[i]) < 0)
          return false;
      }
      for (let i = 0; i < q2Tags.length; i++) {
        if (q1Tags.indexOf(q2Tags[i]) < 0)
          return false;
      }
      return true;
    },
    runs: [
      function (aQuery, aQueryOptions) {
        aQuery.tags = [];
      },
      function (aQuery, aQueryOptions) {
        aQuery.tags = [""];
      },
      function (aQuery, aQueryOptions) {
        aQuery.tags = [
          "foo",
          "七難",
          "",
          "いっぱいおっぱい",
          "Abracadabra",
          "１２３",
          "Here's a pretty long tag name with some = signs and 1 2 3s and spaces oh jeez will it work I hope so!",
          "アスキーでございません",
          "あいうえお",
        ];
      },
      function (aQuery, aQueryOptions) {
        aQuery.tags = [
          "foo",
          "七難",
          "",
          "いっぱいおっぱい",
          "Abracadabra",
          "１２３",
          "Here's a pretty long tag name with some = signs and 1 2 3s and spaces oh jeez will it work I hope so!",
          "アスキーでございません",
          "あいうえお",
        ];
        aQuery.tagsAreNot =  true;
      }
    ]
  },
  // transitions
  {
    desc: "tests nsINavHistoryQuery.getTransitions",
    matches: function (aQuery1, aQuery2) {
      var q1Trans = aQuery1.getTransitions();
      var q2Trans = aQuery2.getTransitions();
      if (q1Trans.length !== q2Trans.length)
        return false;
      for (let i = 0; i < q1Trans.length; i++) {
        if (q2Trans.indexOf(q1Trans[i]) < 0)
          return false;
      }
      for (let i = 0; i < q2Trans.length; i++) {
        if (q1Trans.indexOf(q2Trans[i]) < 0)
          return false;
      }
      return true;
    },
    runs: [
      function (aQuery, aQueryOptions) {
        aQuery.setTransitions([], 0);
      },
      function (aQuery, aQueryOptions) {
        aQuery.setTransitions([Ci.nsINavHistoryService.TRANSITION_DOWNLOAD],
                              1);
      },
      function (aQuery, aQueryOptions) {
        aQuery.setTransitions([Ci.nsINavHistoryService.TRANSITION_TYPED,
                               Ci.nsINavHistoryService.TRANSITION_BOOKMARK], 2);
      }
    ]
  },
];

// nsINavHistoryQueryOptions switches
const queryOptionSwitches = [
  // sortingMode
  {
    desc:    "nsINavHistoryQueryOptions.sortingMode",
    matches: function (aOptions1, aOptions2) {
      if (aOptions1.sortingMode === aOptions2.sortingMode) {
        switch (aOptions1.sortingMode) {
          case aOptions1.SORT_BY_ANNOTATION_ASCENDING:
          case aOptions1.SORT_BY_ANNOTATION_DESCENDING:
            return aOptions1.sortingAnnotation === aOptions2.sortingAnnotation;
            break;
        }
        return true;
      }
      return false;
    },
    runs: [
      function (aQuery, aQueryOptions) {
        aQueryOptions.sortingMode = aQueryOptions.SORT_BY_DATE_ASCENDING;
      },
      function (aQuery, aQueryOptions) {
        aQueryOptions.sortingMode = aQueryOptions.SORT_BY_ANNOTATION_ASCENDING;
        aQueryOptions.sortingAnnotation = "bookmarks/toolbarFolder";
      }
    ]
  },
  // resultType
  {
    // property is used by function simplePropertyMatches.
    property: "resultType",
    desc:     "nsINavHistoryQueryOptions.resultType",
    matches:  simplePropertyMatches,
    runs:     [
      function (aQuery, aQueryOptions) {
        aQueryOptions.resultType = aQueryOptions.RESULTS_AS_URI;
      },
      function (aQuery, aQueryOptions) {
        aQueryOptions.resultType = aQueryOptions.RESULTS_AS_FULL_VISIT;
      }
    ]
  },
  // excludeItems
  {
    property: "excludeItems",
    desc:     "nsINavHistoryQueryOptions.excludeItems",
    matches:  simplePropertyMatches,
    runs:     [
      function (aQuery, aQueryOptions) {
        aQueryOptions.excludeItems = true;
      }
    ]
  },
  // excludeQueries
  {
    property: "excludeQueries",
    desc:     "nsINavHistoryQueryOptions.excludeQueries",
    matches:  simplePropertyMatches,
    runs:     [
      function (aQuery, aQueryOptions) {
        aQueryOptions.excludeQueries = true;
      }
    ]
  },
  // excludeReadOnlyFolders
  {
    property: "excludeReadOnlyFolders",
    desc:     "nsINavHistoryQueryOptions.excludeReadOnlyFolders",
    matches:  simplePropertyMatches,
    runs:     [
      function (aQuery, aQueryOptions) {
        aQueryOptions.excludeReadOnlyFolders = true;
      }
    ]
  },
  // expandQueries
  {
    property: "expandQueries",
    desc:     "nsINavHistoryQueryOptions.expandQueries",
    matches:  simplePropertyMatches,
    runs:     [
      function (aQuery, aQueryOptions) {
        aQueryOptions.expandQueries = true;
      }
    ]
  },
  // includeHidden
  {
    property: "includeHidden",
    desc:     "nsINavHistoryQueryOptions.includeHidden",
    matches:  simplePropertyMatches,
    runs:     [
      function (aQuery, aQueryOptions) {
        aQueryOptions.includeHidden = true;
      }
    ]
  },
  // maxResults
  {
    property: "maxResults",
    desc:     "nsINavHistoryQueryOptions.maxResults",
    matches:  simplePropertyMatches,
    runs:     [
      function (aQuery, aQueryOptions) {
        aQueryOptions.maxResults = 0xffffffff; // 2^32 - 1
      }
    ]
  },
  // queryType
  {
    property: "queryType",
    desc:     "nsINavHistoryQueryOptions.queryType",
    matches:  simplePropertyMatches,
    runs:     [
      function (aQuery, aQueryOptions) {
        aQueryOptions.queryType = aQueryOptions.QUERY_TYPE_HISTORY;
      },
      function (aQuery, aQueryOptions) {
        aQueryOptions.queryType = aQueryOptions.QUERY_TYPE_UNIFIED;
      }
    ]
  },
];

///////////////////////////////////////////////////////////////////////////////

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
  var seqEltPtrs = aSequences.map(function (i) 0);

  var numProds = 0;
  var done = false;
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
 * Enumerates all the subsets in aSet of size aHowMany.  There are
 * C(aSet.length, aHowMany) such subsets.  aCallback will be passed each subset
 * as it is generated.  Note that aSet and the subsets enumerated are -- even
 * though they're arrays -- not sequences; the ordering of their elements is not
 * important.  Example:
 *
 *   choose([1, 2, 3, 4], 2, callback);
 *   // callback is called C(4, 2) = 6 times with the following sets (arrays):
 *   // [1, 2], [1, 3], [1, 4], [2, 3], [2, 4], [3, 4]
 *
 * @param  aSet
 *         an array from which to choose elements, aSet.length > 0
 * @param  aHowMany
 *         the number of elements to choose, > 0 and <= aSet.length
 * @return the total number of sets chosen
 */
function choose(aSet, aHowMany, aCallback)
{
  // ptrs = indices of the elements in aSet we're currently choosing
  var ptrs = [];
  for (let i = 0; i < aHowMany; i++) {
    ptrs.push(i);
  }

  var numFound = 0;
  var done = false;
  while (!done) {
    numFound++;
    aCallback(ptrs.map(function (p) aSet[p]));

    // The next subset to be chosen differs from the current one by just a
    // single element.  Determine which element that is.  Advance the "rightmost"
    // pointer to the "right" by one.  If we move past the end of set, move the
    // next non-adjacent rightmost pointer to the right by one, and reset all
    // succeeding pointers so that they're adjacent to it.  When all pointers
    // are clustered all the way to the right, we're done.

    // Advance the rightmost pointer.
    ptrs[ptrs.length - 1]++;

    // The rightmost pointer has gone past the end of set.
    if (ptrs[ptrs.length - 1] >= aSet.length) {
      // Find the next rightmost pointer that is not adjacent to the current one.
      let si = aSet.length - 2; // aSet index
      let pi = ptrs.length - 2; // ptrs index
      while (pi >= 0 && ptrs[pi] === si) {
        pi--;
        si--;
      }

      // All pointers are adjacent and clustered all the way to the right.
      if (pi < 0)
        done = true;
      else {
        // pi = index of rightmost pointer with a gap between it and its
        // succeeding pointer.  Move it right and reset all succeeding pointers
        // so that they're adjacent to it.
        ptrs[pi]++;
        for (let i = 0; i < ptrs.length - pi - 1; i++) {
          ptrs[i + pi + 1] = ptrs[pi] + i + 1;
        }
      }
    }
  }
  return numFound;
}

/**
 * Convenience function for nsINavHistoryQuery switches that act as flags.  This
 * is attached to switch objects.  See querySwitches array above.
 *
 * @param  aQuery1
 *         an nsINavHistoryQuery object
 * @param  aQuery2
 *         another nsINavHistoryQuery object
 * @return true if this switch is the same in both aQuery1 and aQuery2
 */
function flagSwitchMatches(aQuery1, aQuery2)
{
  if (aQuery1[this.flag] && aQuery2[this.flag]) {
    for (let p in this.subswitches) {
      if (p in aQuery1 && p in aQuery2) {
        if (aQuery1[p] instanceof Ci.nsIURI) {
          if (!aQuery1[p].equals(aQuery2[p]))
            return false;
        }
        else if (aQuery1[p] !== aQuery2[p])
          return false;
      }
    }
  }
  else if (aQuery1[this.flag] || aQuery2[this.flag])
    return false;

  return true;
}

/**
 * Tests if aObj1 and aObj2 are equal.  This function is general and may be used
 * for either nsINavHistoryQuery or nsINavHistoryQueryOptions objects.  aSwitches
 * determines which set of switches is used for comparison.  Pass in either
 * querySwitches or queryOptionSwitches.
 *
 * @param  aSwitches
 *         determines which set of switches applies to aObj1 and aObj2, either
 *         querySwitches or queryOptionSwitches
 * @param  aObj1
 *         an nsINavHistoryQuery or nsINavHistoryQueryOptions object
 * @param  aObj2
 *         another nsINavHistoryQuery or nsINavHistoryQueryOptions object
 * @return true if aObj1 and aObj2 are equal
 */
function queryObjsEqual(aSwitches, aObj1, aObj2)
{
  for (let i = 0; i < aSwitches.length; i++) {
    if (!aSwitches[i].matches(aObj1, aObj2))
      return false;
  }
  return true;
}

/**
 * This drives the test runs.  See the comment at the top of this file.
 *
 * @param aHowManyLo
 *        the size of the switch subsets to start with
 * @param aHowManyHi
 *        the size of the switch subsets to end with (inclusive)
 */
function runQuerySequences(aHowManyLo, aHowManyHi)
{
  var allSwitches = querySwitches.concat(queryOptionSwitches);
  var prevQueries = [];
  var prevOpts = [];

  // Choose aHowManyLo switches up to aHowManyHi switches.
  for (let howMany = aHowManyLo; howMany <= aHowManyHi; howMany++) {
    let numIters = 0;
    print("CHOOSING " + howMany + " SWITCHES");

    // Choose all subsets of size howMany from allSwitches.
    choose(allSwitches, howMany, function (chosenSwitches) {
      print(numIters);
      numIters++;

      // Collect the runs.
      // runs = [ [runs from switch 1], ..., [runs from switch howMany] ]
      var runs = chosenSwitches.map(function (s) {
        if (s.desc)
          print("  " + s.desc);
        return s.runs;
      });

      // cartProd(runs) => [
      //   [switch 1 run 1, switch 2 run 1, ..., switch howMany run 1 ],
      //   ...,
      //   [switch 1 run 1, switch 2 run 1, ..., switch howMany run N ],
      //   ..., ...,
      //   [switch 1 run N, switch 2 run N, ..., switch howMany run 1 ],
      //   ...,
      //   [switch 1 run N, switch 2 run N, ..., switch howMany run N ],
      // ]
      cartProd(runs, function (runSet) {
        // Create a new query, apply the switches in runSet, and test it.
        var query = PlacesUtils.history.getNewQuery();
        var opts = PlacesUtils.history.getNewQueryOptions();
        for (let i = 0; i < runSet.length; i++) {
          runSet[i](query, opts);
        }
        serializeDeserialize([query], opts);

        // Test the previous NUM_MULTIPLE_QUERIES queries together.
        prevQueries.push(query);
        prevOpts.push(opts);
        if (prevQueries.length >= NUM_MULTIPLE_QUERIES) {
          // We can serialize multiple nsINavHistoryQuery objects together but
          // only one nsINavHistoryQueryOptions object with them.  So, test each
          // of the previous NUM_MULTIPLE_QUERIES nsINavHistoryQueryOptions.
          for (let i = 0; i < prevOpts.length; i++) {
            serializeDeserialize(prevQueries, prevOpts[i]);
          }
          prevQueries.shift();
          prevOpts.shift();
        }
      });
    });
  }
  print("\n");
}

/**
 * Serializes the nsINavHistoryQuery objects in aQueryArr and the
 * nsINavHistoryQueryOptions object aQueryOptions, de-serializes the
 * serialization, and ensures (using do_check_* functions) that the
 * de-serialized objects equal the originals.
 *
 * @param aQueryArr
 *        an array containing nsINavHistoryQuery objects
 * @param aQueryOptions
 *        an nsINavHistoryQueryOptions object
 */
function serializeDeserialize(aQueryArr, aQueryOptions)
{
  var queryStr = PlacesUtils.history.queriesToQueryString(aQueryArr,
                                                        aQueryArr.length,
                                                        aQueryOptions);
  print("  " + queryStr);
  var queryArr2 = {};
  var opts2 = {};
  PlacesUtils.history.queryStringToQueries(queryStr, queryArr2, {}, opts2);
  queryArr2 = queryArr2.value;
  opts2 = opts2.value;

  // The two sets of queries cannot be the same if their lengths differ.
  do_check_eq(aQueryArr.length, queryArr2.length);

  // Although the query serialization code as it is written now practically
  // ensures that queries appear in the query string in the same order they
  // appear in both the array to be serialized and the array resulting from
  // de-serialization, the interface does not guarantee any ordering.  So, for
  // each query in aQueryArr, find its equivalent in queryArr2 and delete it
  // from queryArr2.  If queryArr2 is empty after looping through aQueryArr,
  // the two sets of queries are equal.
  for (let i = 0; i < aQueryArr.length; i++) {
    let j = 0;
    for (; j < queryArr2.length; j++) {
      if (queryObjsEqual(querySwitches, aQueryArr[i], queryArr2[j]))
        break;
    }
    if (j < queryArr2.length)
      queryArr2.splice(j, 1);
  }
  do_check_eq(queryArr2.length, 0);

  // Finally check the query options objects.
  do_check_true(queryObjsEqual(queryOptionSwitches, aQueryOptions, opts2));
}

/**
 * Convenience function for switches that have simple values.  This is attached
 * to switch objects.  See querySwitches and queryOptionSwitches arrays above.
 *
 * @param  aObj1
 *         an nsINavHistoryQuery or nsINavHistoryQueryOptions object
 * @param  aObj2
 *         another nsINavHistoryQuery or nsINavHistoryQueryOptions object
 * @return true if this switch is the same in both aObj1 and aObj2
 */
function simplePropertyMatches(aObj1, aObj2)
{
  return aObj1[this.property] === aObj2[this.property];
}

///////////////////////////////////////////////////////////////////////////////

function run_test()
{
  runQuerySequences(CHOOSE_HOW_MANY_SWITCHES_LO, CHOOSE_HOW_MANY_SWITCHES_HI);
}
