/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kSearchEngineID = "addEngineWithDetails_test_engine";
const kSearchEngineURL = "http://example.com/?search={searchTerms}";
const kSearchTerm = "foo";

add_task(function* test_addEngineWithDetails() {
  do_check_false(Services.search.isInitialized);

  Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF)
          .setBoolPref("reset.enabled", true);

  yield asyncInit();

  Services.search.addEngineWithDetails(kSearchEngineID, "", "", "", "get",
                                       kSearchEngineURL);

  // An engine added with addEngineWithDetails should have a load path, even
  // though we can't point to a specific file.
  let engine = Services.search.getEngineByName(kSearchEngineID);
  do_check_eq(engine.wrappedJSObject._loadPath, "[other]addEngineWithDetails");

  // Set the engine as default; this should set a loadPath verification hash,
  // which should ensure we don't show the search reset prompt.
  Services.search.currentEngine = engine;

  let expectedURL = kSearchEngineURL.replace("{searchTerms}", kSearchTerm);
  let submission =
    Services.search.currentEngine.getSubmission(kSearchTerm, null, "searchbar");
  do_check_eq(submission.uri.spec, expectedURL);
});
