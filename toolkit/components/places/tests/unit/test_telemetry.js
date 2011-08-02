/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests common Places telemetry probes by faking the telemetry service.

Components.utils.import("resource://gre/modules/PlacesDBUtils.jsm");

let histograms = {
  PLACES_PAGES_COUNT: function (val) do_check_eq(val, 1),
  PLACES_BOOKMARKS_COUNT: function (val) do_check_eq(val, 1),
  PLACES_TAGS_COUNT: function (val) do_check_eq(val, 1),
  PLACES_FOLDERS_COUNT: function (val) do_check_eq(val, 1),
  PLACES_KEYWORDS_COUNT: function (val) do_check_eq(val, 1),
  PLACES_SORTED_BOOKMARKS_PERC: function (val) do_check_eq(val, 100),
  PLACES_TAGGED_BOOKMARKS_PERC: function (val) do_check_eq(val, 100),
  PLACES_DATABASE_FILESIZE_MB: function (val) do_check_true(val > 0),
  PLACES_DATABASE_JOURNALSIZE_MB: function (val) do_check_true(val > 0),
  PLACES_DATABASE_PAGESIZE_B: function (val) do_check_eq(val, 32768),
  PLACES_DATABASE_SIZE_PER_PAGE_B: function (val) do_check_true(val > 0),
  PLACES_EXPIRATION_STEPS_TO_CLEAN: function (val) do_check_true(val > 1),
  //PLACES_AUTOCOMPLETE_1ST_RESULT_TIME_MS:  function (val) do_check_true(val > 1),
}

// This sucks, but due to nsITelemetry using [implicit_jscontext], it's
// impossible to implement it in js, so no fancy service factory replacements.
// This mock implements only the telemetry methods used by Places.
XPCOMUtils.defineLazyGetter(Services, "telemetry", function () {
  return {
    getHistogramById: function FT_getHistogramById(id) {
      if (id in histograms) {
        return {
          add: function FH_add(val) {
            do_log_info("Testing probe " + id);
            histograms[id](val);
            delete histograms[id];
            if (Object.keys(histograms).length == 0)
              do_test_finished();
          }
        };
      }

      return {
        add: function FH_add(val) {
          do_log_info("Unknown probe " + id);
        }
      };
    },
  };
});

function run_test() {
  do_test_pending();

  // Put some trash in the database.
  const URI = NetUtil.newURI("http://moz.org/");

  let folderId = PlacesUtils.bookmarks.createFolder(PlacesUtils.unfiledBookmarksFolderId,
                                                    "moz test",
                                                    PlacesUtils.bookmarks.DEFAULT_INDEX);
  let itemId = PlacesUtils.bookmarks.insertBookmark(folderId,
                                                    uri,
                                                    PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                    "moz test");
  PlacesUtils.tagging.tagURI(uri, ["tag"]);
  PlacesUtils.bookmarks.setKeywordForBookmark(itemId, "keyword");

  // Test generic database probes.
  PlacesDBUtils._telemetry();

  waitForAsyncUpdates(continue_test);
}

function continue_test() {
  // Test expiration probes.
  for (let i = 0; i < 2; i++) {
    PlacesUtils.history.addVisit(NetUtil.newURI("http://" +  i + ".moz.org/"),
                                 Date.now(), null,
                                 PlacesUtils.history.TRANSITION_TYPED, false, 0);
  }
  Services.prefs.setIntPref("places.history.expiration.max_pages", 0);
  let expire = Cc["@mozilla.org/places/expiration;1"].getService(Ci.nsIObserver);
  expire.observe(null, "places-debug-start-expiration", 1);
  expire.observe(null, "places-debug-start-expiration", -1);

  // Test autocomplete probes.
  /*
  // This is useful for manual testing by changing the minimum time for
  // autocomplete telemetry to 0, but there is no way to artificially delay
  // autocomplete by more than 50ms in a realiable way.
  Services.prefs.setIntPref("browser.urlbar.search.sources", 3);
  Services.prefs.setIntPref("browser.urlbar.default.behavior", 0);
  function AutoCompleteInput(aSearches) {
    this.searches = aSearches;
  }
  AutoCompleteInput.prototype = {
    timeout: 10,
    textValue: "",
    searchParam: "",
    popupOpen: false,
    minResultsForPopup: 0,
    invalidate: function() {},
    disableAutoComplete: false,
    completeDefaultIndex: false,
    get popup() { return this; },
    onSearchBegin: function() {},
    onSearchComplete: function() {},
    setSelectedIndex: function() {},
    get searchCount() { return this.searches.length; },
    getSearchAt: function(aIndex) this.searches[aIndex],
    QueryInterface: XPCOMUtils.generateQI([
      Ci.nsIAutoCompleteInput,
      Ci.nsIAutoCompletePopup,
    ])
  };
  let controller = Cc["@mozilla.org/autocomplete/controller;1"].
                   getService(Ci.nsIAutoCompleteController);
  controller.input = new AutoCompleteInput(["history"]);
  controller.startSearch("moz");
  */
}
