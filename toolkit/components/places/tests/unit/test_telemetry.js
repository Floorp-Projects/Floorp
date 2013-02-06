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
  // The journal may have been truncated.
  PLACES_DATABASE_JOURNALSIZE_MB: function (val) do_check_true(val >= 0),
  PLACES_DATABASE_PAGESIZE_B: function (val) do_check_eq(val, 32768),
  PLACES_DATABASE_SIZE_PER_PAGE_B: function (val) do_check_true(val > 0),
  PLACES_EXPIRATION_STEPS_TO_CLEAN2: function (val) do_check_true(val > 1),
  //PLACES_AUTOCOMPLETE_1ST_RESULT_TIME_MS:  function (val) do_check_true(val > 1),
  PLACES_IDLE_FRECENCY_DECAY_TIME_MS: function (val) do_check_true(val > 0),
  PLACES_IDLE_MAINTENANCE_TIME_MS: function (val) do_check_true(val > 0),
  PLACES_ANNOS_BOOKMARKS_COUNT: function (val) do_check_eq(val, 1),
  PLACES_ANNOS_BOOKMARKS_SIZE_KB: function (val) do_check_eq(val, 1),
  PLACES_ANNOS_PAGES_COUNT: function (val) do_check_eq(val, 1),
  PLACES_ANNOS_PAGES_SIZE_KB: function (val) do_check_eq(val, 1),
  PLACES_FRECENCY_CALC_TIME_MS: function (val) do_check_true(val >= 0),
}

function run_test()
{
  run_next_test();
}

add_task(function test_execute()
{
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

  // Set a large annotation.
  let content = "";
  while (content.length < 1024) {
    content += "0";
  }
  PlacesUtils.annotations.setItemAnnotation(itemId, "test-anno", content, 0,
                                            PlacesUtils.annotations.EXPIRE_NEVER);
  PlacesUtils.annotations.setPageAnnotation(uri, "test-anno", content, 0,
                                            PlacesUtils.annotations.EXPIRE_NEVER);

  // Request to gather telemetry data.
  Cc["@mozilla.org/places/categoriesStarter;1"]
    .getService(Ci.nsIObserver)
    .observe(null, "gather-telemetry", null);

  yield promiseAsyncUpdates();

  // Test expiration probes.
  for (let i = 0; i < 2; i++) {
    yield promiseAddVisits({
      uri: uri("http://" +  i + ".moz.org/"),
      visitDate: Date.now() // [sic]
    });
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

  // Test idle probes.
  PlacesUtils.history.QueryInterface(Ci.nsIObserver)
                     .observe(null, "idle-daily", null);
  PlacesDBUtils.maintenanceOnIdle();

  yield promiseTopicObserved("places-maintenance-finished");

  for (let histogramId in histograms) {
    do_log_info("checking histogram " + histogramId);
    let validate = histograms[histogramId];
    let snapshot = Services.telemetry.getHistogramById(histogramId).snapshot();
    validate(snapshot.sum);
    do_check_true(snapshot.counts.reduce(function(a, b) a + b) > 0);
  }
});

add_test(function test_healthreport_callback() {
  PlacesDBUtils.telemetry(null, function onResult(data) {
    do_check_neq(data, null);

    do_check_eq(Object.keys(data).length, 2);
    do_check_eq(data.PLACES_PAGES_COUNT, 1);
    do_check_eq(data.PLACES_BOOKMARKS_COUNT, 1);

    run_next_test();
  });
});

