/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var Ci = Components.interfaces;
var Cc = Components.classes;
var Cr = Components.results;
var Cu = Components.utils;

const FRECENCY_DEFAULT = 10000;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://testing-common/httpd.js");

// Import common head.
{
  let commonFile = do_get_file("../head_common.js", false);
  let uri = Services.io.newFileURI(commonFile);
  Services.scriptloader.loadSubScript(uri.spec, this);
}

// Put any other stuff relative to this test folder below.

const TITLE_SEARCH_ENGINE_SEPARATOR = " \u00B7\u2013\u00B7 ";

function run_test() {
  run_next_test();
}

function* cleanup() {
  Services.prefs.clearUserPref("browser.urlbar.autocomplete.enabled");
  Services.prefs.clearUserPref("browser.urlbar.autoFill");
  Services.prefs.clearUserPref("browser.urlbar.autoFill.typed");
  Services.prefs.clearUserPref("browser.urlbar.autoFill.searchEngines");
  let suggestPrefs = [
    "history",
    "bookmark",
    "history.onlyTyped",
    "openpage",
    "searches",
  ];
  for (let type of suggestPrefs) {
    Services.prefs.clearUserPref("browser.urlbar.suggest." + type);
  }
  Services.prefs.clearUserPref("browser.search.suggest.enabled");
  yield PlacesUtils.bookmarks.eraseEverything();
  yield PlacesTestUtils.clearHistory();
}
do_register_cleanup(cleanup);

/**
 * @param aSearches
 *        Array of AutoCompleteSearch names.
 */
function AutoCompleteInput(aSearches) {
  this.searches = aSearches;
}
AutoCompleteInput.prototype = {
  popup: {
    selectedIndex: -1,
    invalidate: function () {},
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompletePopup])
  },
  popupOpen: false,

  disableAutoComplete: false,
  completeDefaultIndex: true,
  completeSelectedIndex: true,
  forceComplete: false,

  minResultsForPopup: 0,
  maxRows: 0,

  showCommentColumn: false,
  showImageColumn: false,

  timeout: 10,
  searchParam: "",

  get searchCount() {
    return this.searches.length;
  },
  getSearchAt: function(aIndex) {
    return this.searches[aIndex];
  },

  textValue: "",
  // Text selection range
  _selStart: 0,
  _selEnd: 0,
  get selectionStart() {
    return this._selStart;
  },
  get selectionEnd() {
    return this._selEnd;
  },
  selectTextRange: function(aStart, aEnd) {
    this._selStart = aStart;
    this._selEnd = aEnd;
  },

  onSearchBegin: function () {},
  onSearchComplete: function () {},

  onTextEntered: () => false,
  onTextReverted: () => false,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompleteInput])
}

// A helper for check_autocomplete to check a specific match against data from
// the controller.
function _check_autocomplete_matches(match, result) {
  let { uri, title, tags, searchEngine, style } = match;
  if (tags)
    title += " \u2013 " + tags.sort().join(", ");
  if (style)
    style = style.sort();
  else
    style = ["favicon"];

  do_print(`Checking against expected "${uri.spec}", "${title}"`);
  // Got a match on both uri and title?
  if (stripPrefix(uri.spec) != stripPrefix(result.value) || title != result.comment) {
    return false;
  }

  let actualStyle = result.style.split(/\s+/).sort();
  if (style)
    Assert.equal(actualStyle.toString(), style.toString(), "Match should have expected style");
  if (uri.spec.startsWith("moz-action:")) {
    Assert.ok(actualStyle.includes("action"), "moz-action results should always have 'action' in their style");
  }

  if (match.icon)
    Assert.equal(result.image, match.icon, "Match should have expected image");

  return true;
}

function* check_autocomplete(test) {
  // At this point frecency could still be updating due to latest pages
  // updates.
  // This is not a problem in real life, but autocomplete tests should
  // return reliable resultsets, thus we have to wait.
  yield PlacesTestUtils.promiseAsyncUpdates();

  // Make an AutoCompleteInput that uses our searches and confirms results.
  let input = new AutoCompleteInput(["unifiedcomplete"]);
  input.textValue = test.search;

  if (test.searchParam)
    input.searchParam = test.searchParam;

  // Caret must be at the end for autoFill to happen.
  let strLen = test.search.length;
  input.selectTextRange(strLen, strLen);
  Assert.equal(input.selectionStart, strLen, "Selection starts at end");
  Assert.equal(input.selectionEnd, strLen, "Selection ends at the end");

  let controller = Cc["@mozilla.org/autocomplete/controller;1"]
                     .getService(Ci.nsIAutoCompleteController);
  controller.input = input;

  let numSearchesStarted = 0;
  input.onSearchBegin = () => {
    do_print("onSearchBegin received");
    numSearchesStarted++;
  };
  let deferred = Promise.defer();
  input.onSearchComplete = () => {
    do_print("onSearchComplete received");
    deferred.resolve();
  }

  let expectedSearches = 1;
  if (test.incompleteSearch) {
    controller.startSearch(test.incompleteSearch);
    expectedSearches++;
  }
  do_print("Searching for: '" + test.search + "'");
  controller.startSearch(test.search);
  yield deferred.promise;

  Assert.equal(numSearchesStarted, expectedSearches, "All searches started");

  // Check to see the expected uris and titles match up. If 'enable-actions'
  // is specified, we check that the first specified match is the first
  // controller value (as this is the "special" always selected item), but the
  // rest can match in any order.
  // If 'enable-actions' is not specified, they can match in any order.
  if (test.matches) {
    // Do not modify the test original matches.
    let matches = test.matches.slice();

    if (matches.length) {
      let firstIndexToCheck = 0;
      if (test.searchParam && test.searchParam.includes("enable-actions")) {
        firstIndexToCheck = 1;
        do_print("Checking first match is first autocomplete entry")
        let result = {
          value: controller.getValueAt(0),
          comment: controller.getCommentAt(0),
          style: controller.getStyleAt(0),
          image: controller.getImageAt(0),
        }
        do_print(`First match is "${result.value}", "${result.comment}"`);
        Assert.ok(_check_autocomplete_matches(matches[0], result), "first item is correct");
        do_print("Checking rest of the matches");
      }

      for (let i = firstIndexToCheck; i < controller.matchCount; i++) {
        let result = {
          value: controller.getValueAt(i),
          comment: controller.getCommentAt(i),
          style: controller.getStyleAt(i),
          image: controller.getImageAt(i),
        }
        do_print(`Looking for "${result.value}", "${result.comment}" in expected results...`);
        let lowerBound = test.checkSorting ? i : firstIndexToCheck;
        let upperBound = test.checkSorting ? i + 1 : matches.length;
        let found = false;
        for (let j = lowerBound; j < upperBound; ++j) {
          // Skip processed expected results
          if (matches[j] == undefined)
            continue;
          if (_check_autocomplete_matches(matches[j], result)) {
            do_print("Got a match at index " + j + "!");
            // Make it undefined so we don't process it again
            matches[j] = undefined;
            found = true;
            break;
          }
        }

        if (!found)
          do_throw(`Didn't find the current result ("${result.value}", "${result.comment}") in matches`); //' (Emacs syntax highlighting fix)
      }
    }

    Assert.equal(controller.matchCount, matches.length,
                 "Got as many results as expected");

    // If we expect results, make sure we got matches.
    do_check_eq(controller.searchStatus, matches.length ?
                Ci.nsIAutoCompleteController.STATUS_COMPLETE_MATCH :
                Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH);
  }

  if (test.autofilled) {
    // Check the autoFilled result.
    Assert.equal(input.textValue, test.autofilled,
                 "Autofilled value is correct");

    // Now force completion and check correct casing of the result.
    // This ensures the controller is able to do its magic case-preserving
    // stuff and correct replacement of the user's casing with result's one.
    controller.handleEnter(false);
    Assert.equal(input.textValue, test.completed,
                 "Completed value is correct");
  }
}

var addBookmark = Task.async(function* (aBookmarkObj) {
  Assert.ok(!!aBookmarkObj.uri, "Bookmark object contains an uri");
  let parentId = aBookmarkObj.parentId ? aBookmarkObj.parentId
                                       : PlacesUtils.unfiledBookmarksFolderId;

  let bm = yield PlacesUtils.bookmarks.insert({
    parentGuid: (yield PlacesUtils.promiseItemGuid(parentId)),
    title: aBookmarkObj.title || "A bookmark",
    url: aBookmarkObj.uri
  });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);

  if (aBookmarkObj.keyword) {
    yield PlacesUtils.keywords.insert({ keyword: aBookmarkObj.keyword,
                                        url: aBookmarkObj.uri.spec,
                                        postData: aBookmarkObj.postData
                                      });
  }

  if (aBookmarkObj.tags) {
    PlacesUtils.tagging.tagURI(aBookmarkObj.uri, aBookmarkObj.tags);
  }
});

function addOpenPages(aUri, aCount=1) {
  let ac = Cc["@mozilla.org/autocomplete/search;1?name=unifiedcomplete"]
             .getService(Ci.mozIPlacesAutoComplete);
  for (let i = 0; i < aCount; i++) {
    ac.registerOpenPage(aUri);
  }
}

function removeOpenPages(aUri, aCount=1) {
  let ac = Cc["@mozilla.org/autocomplete/search;1?name=unifiedcomplete"]
             .getService(Ci.mozIPlacesAutoComplete);
  for (let i = 0; i < aCount; i++) {
    ac.unregisterOpenPage(aUri);
  }
}

function changeRestrict(aType, aChar) {
  let branch = "browser.urlbar.";
  // "title" and "url" are different from everything else, so special case them.
  if (aType == "title" || aType == "url")
    branch += "match.";
  else
    branch += "restrict.";

  do_print("changing restrict for " + aType + " to '" + aChar + "'");
  Services.prefs.setCharPref(branch + aType, aChar);
}

function resetRestrict(aType) {
  let branch = "browser.urlbar.";
  // "title" and "url" are different from everything else, so special case them.
  if (aType == "title" || aType == "url")
    branch += "match.";
  else
    branch += "restrict.";

  Services.prefs.clearUserPref(branch + aType);
}

/**
 * Strip prefixes from the URI that we don't care about for searching.
 *
 * @param spec
 *        The text to modify.
 * @return the modified spec.
 */
function stripPrefix(spec)
{
  ["http://", "https://", "ftp://"].some(scheme => {
    if (spec.startsWith(scheme)) {
      spec = spec.slice(scheme.length);
      return true;
    }
    return false;
  });

  if (spec.startsWith("www.")) {
    spec = spec.slice(4);
  }
  return spec;
}

function makeActionURI(action, params) {
  let encodedParams = {};
  for (let key in params) {
    encodedParams[key] = encodeURIComponent(params[key]);
  }
  let url = "moz-action:" + action + "," + JSON.stringify(encodedParams);
  return NetUtil.newURI(url);
}

// Creates a full "match" entry for a search result, suitable for passing as
// an entry to check_autocomplete.
function makeSearchMatch(input, extra = {}) {
  // Note that counter-intuitively, the order the object properties are defined
  // in the object passed to makeActionURI is important for check_autocomplete
  // to match them :(
  let params = {
    engineName: extra.engineName || "MozSearch",
    input,
    searchQuery: "searchQuery" in extra ? extra.searchQuery : input,
  };
  if ("alias" in extra) {
    // May be undefined, which is expected, but in that case make sure it's not
    // included in the params of the moz-action URL.
    params.alias = extra.alias;
  }
  let style = [ "action", "searchengine" ];
  if (Array.isArray(extra.style)) {
    style.push(...extra.style);
  }
  if (extra.heuristic) {
    style.push("heuristic");
  }
  return {
    uri: makeActionURI("searchengine", params),
    title: params.engineName,
    style,
  }
}

// Creates a full "match" entry for a search result, suitable for passing as
// an entry to check_autocomplete.
function makeVisitMatch(input, url, extra = {}) {
  // Note that counter-intuitively, the order the object properties are defined
  // in the object passed to makeActionURI is important for check_autocomplete
  // to match them :(
  let params = {
    url,
    input,
  }
  let style = [ "action", "visiturl" ];
  if (extra.heuristic) {
    style.push("heuristic");
  }
  return {
    uri: makeActionURI("visiturl", params),
    title: extra.title || url,
    style,
  }
}

function makeSwitchToTabMatch(url, extra = {}) {
  return {
    uri: makeActionURI("switchtab", {url}),
    title: extra.title || url,
    style: [ "action", "switchtab" ],
  }
}

function setFaviconForHref(href, iconHref) {
  return new Promise(resolve => {
    PlacesUtils.favicons.setAndFetchFaviconForPage(
      NetUtil.newURI(href),
      NetUtil.newURI(iconHref),
      true,
      PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
      resolve,
      Services.scriptSecurityManager.getSystemPrincipal()
    );
  });
}

function makeTestServer(port=-1) {
  let httpServer = new HttpServer();
  httpServer.start(port);
  do_register_cleanup(() => httpServer.stop(() => {}));
  return httpServer;
}

function* addTestEngine(basename, httpServer=undefined) {
  httpServer = httpServer || makeTestServer();
  httpServer.registerDirectory("/", do_get_cwd());
  let dataUrl =
    "http://localhost:" + httpServer.identity.primaryPort + "/data/";

  do_print("Adding engine: " + basename);
  return yield new Promise(resolve => {
    Services.obs.addObserver(function obs(subject, topic, data) {
      let engine = subject.QueryInterface(Ci.nsISearchEngine);
      do_print("Observed " + data + " for " + engine.name);
      if (data != "engine-added" || engine.name != basename) {
        return;
      }

      Services.obs.removeObserver(obs, "browser-search-engine-modified");
      do_register_cleanup(() => Services.search.removeEngine(engine));
      resolve(engine);
    }, "browser-search-engine-modified", false);

    do_print("Adding engine from URL: " + dataUrl + basename);
    Services.search.addEngine(dataUrl + basename, null, null, false);
  });
}

// Ensure we have a default search engine and the keyword.enabled preference
// set.
add_task(function* ensure_search_engine() {
  // keyword.enabled is necessary for the tests to see keyword searches.
  Services.prefs.setBoolPref("keyword.enabled", true);

  // Initialize the search service, but first set this geo IP pref to a dummy
  // string.  When the search service is initialized, it contacts the URI named
  // in this pref, which breaks the test since outside connections aren't
  // allowed.
  let geoPref = "browser.search.geoip.url";
  Services.prefs.setCharPref(geoPref, "");
  do_register_cleanup(() => Services.prefs.clearUserPref(geoPref));
  yield new Promise(resolve => {
    Services.search.init(resolve);
  });

  // Remove any existing engines before adding ours.
  for (let engine of Services.search.getEngines()) {
    Services.search.removeEngine(engine);
  }
  Services.search.addEngineWithDetails("MozSearch", "", "", "", "GET",
                                       "http://s.example.com/search");
  let engine = Services.search.getEngineByName("MozSearch");
  Services.search.currentEngine = engine;
});
