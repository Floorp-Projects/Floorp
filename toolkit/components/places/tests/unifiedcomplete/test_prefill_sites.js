/**
 * Test for bug 1211726 - prefill list of top web sites for better
 * autocompletion on empty profiles.
 */

const PREF_FEATURE_ENABLED = "browser.urlbar.usepreloadedtopurls.enabled";
const PREF_FEATURE_EXPIRE_DAYS = "browser.urlbar.usepreloadedtopurls.expire_days";

const autocompleteObject = Cc["@mozilla.org/autocomplete/search;1?name=unifiedcomplete"]
                             .getService(Ci.mozIPlacesAutoComplete);

Cu.importGlobalProperties(["fetch"]);

// With or without trailing slash - no matter. URI.spec does have it always.
// Then, check_autocomplete() doesn't cut it off (uses stripPrefix()).
let yahoooURI = NetUtil.newURI("https://yahooo.com/");
let gooogleURI = NetUtil.newURI("https://gooogle.com/");

autocompleteObject.addPrefillSite(yahoooURI.spec, "Yahooo");
autocompleteObject.addPrefillSite(gooogleURI.spec, "Gooogle");

function *assert_feature_works(condition) {
  do_print("List Results do appear " + condition);
  yield check_autocomplete({
    search: "ooo",
    matches: [
      { uri: yahoooURI, title: "Yahooo",  style: ["prefill-site"] },
      { uri: gooogleURI, title: "Gooogle", style: ["prefill-site"] },
    ],
  });

  do_print("Autofill does appear " + condition);
  yield check_autocomplete({
    search: "gooo",
    autofilled: "gooogle.com/", // Will fail without trailing slash
    completed: "https://gooogle.com/",
  });
}

function *assert_feature_does_not_appear(condition) {
  do_print("List Results don't appear " + condition);
  yield check_autocomplete({
    search: "ooo",
    matches: [],
  });

  do_print("Autofill doesn't appear " + condition);
  // "search" is what you type,
  // "autofilled" is what you get in response in the url bar,
  // "completed" is what you get there when you hit enter.
  // So if they are all equal - it's the proof there was no autofill
  // (knowing we have a suitable prefill site).
  yield check_autocomplete({
    search: "gooo",
    autofilled: "gooo",
    completed: "gooo",
  });
}

add_task(function* test_it_works() {
  // Not expired but OFF
  Services.prefs.setIntPref(PREF_FEATURE_EXPIRE_DAYS, 14);
  Services.prefs.setBoolPref(PREF_FEATURE_ENABLED, false);
  yield assert_feature_does_not_appear("when OFF by prefs");

  // Now turn it ON
  Services.prefs.setBoolPref(PREF_FEATURE_ENABLED, true);
  yield assert_feature_works("when ON by prefs");

  // And expire
  Services.prefs.setIntPref(PREF_FEATURE_EXPIRE_DAYS, 0);
  yield assert_feature_does_not_appear("when expired");

  yield cleanup();
});

add_task(function* test_sorting_against_bookmark() {
  let boookmarkURI = NetUtil.newURI("https://boookmark.com");
  yield addBookmark( { uri: boookmarkURI, title: "Boookmark" } );

  Services.prefs.setBoolPref(PREF_FEATURE_ENABLED, true);
  Services.prefs.setIntPref(PREF_FEATURE_EXPIRE_DAYS, 14);

  do_print("Prefill Sites are placed lower than Bookmarks");
  yield check_autocomplete({
    checkSorting: true,
    search: "ooo",
    matches: [
      { uri: boookmarkURI, title: "Boookmark",  style: ["bookmark"] },
      { uri: yahoooURI, title: "Yahooo",  style: ["prefill-site"] },
      { uri: gooogleURI, title: "Gooogle", style: ["prefill-site"] },
    ],
  });

  yield cleanup();
});

add_task(function* test_sorting_against_history() {
  let histoooryURI = NetUtil.newURI("https://histooory.com");
  yield PlacesTestUtils.addVisits( { uri: histoooryURI, title: "Histooory" } );

  Services.prefs.setBoolPref(PREF_FEATURE_ENABLED, true);
  Services.prefs.setIntPref(PREF_FEATURE_EXPIRE_DAYS, 14);

  do_print("Prefill Sites are placed lower than History entries");
  yield check_autocomplete({
    checkSorting: true,
    search: "ooo",
    matches: [
      { uri: histoooryURI, title: "Histooory" },
      { uri: yahoooURI, title: "Yahooo",  style: ["prefill-site"] },
      { uri: gooogleURI, title: "Gooogle", style: ["prefill-site"] },
    ],
  });

  yield cleanup();
});

add_task(function* test_data_file() {
  let response = yield fetch("chrome://global/content/unifiedcomplete-top-urls.json");

  do_print("Source file is supplied and fetched OK");
  Assert.ok(response.ok);

  do_print("The JSON is parsed");
  let sites = yield response.json();

  do_print("Storage is populated");
  autocompleteObject.populatePrefillSiteStorage(sites);

  let lastSite = sites.pop();
  let uri = NetUtil.newURI(lastSite[0]);
  let title = lastSite[1];

  do_print("Storage is populated from JSON correctly");
  yield check_autocomplete({
    search: uri.host.slice(1), // omit 1st letter to avoid style:"autofill" result
    matches: [ { uri, title,  style: ["prefill-site"] } ],
  });

  yield cleanup();
});
