/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kSearchEngineID = "addEngineWithDetails_test_engine";
const kSearchEngineURL = "http://example.com/?search={searchTerms}";
const kSearchSuggestURL = "http://example.com/?suggest={searchTerms}";
const kIconURL = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPhfDwAChwGA60e6kgAAAABJRU5ErkJggg==";
const kDescription = "Test Description";
const kAlias = "alias_foo";
const kSearchTerm = "foo";
const kExtensionID = "test@example.com";
const URLTYPE_SUGGEST_JSON = "application/x-suggestions+json";

add_task(async function test_addEngineWithDetails() {
  do_check_false(Services.search.isInitialized);

  Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF)
          .setBoolPref("reset.enabled", true);

  await asyncInit();

  Services.search.addEngineWithDetails(kSearchEngineID, {
    template: kSearchEngineURL,
    description: kDescription,
    iconURL: kIconURL,
    suggestURL: kSearchSuggestURL,
    alias: "alias_foo",
    extensionID: kExtensionID,
  });

  // An engine added with addEngineWithDetails should have a load path, even
  // though we can't point to a specific file.
  let engine = Services.search.getEngineByName(kSearchEngineID);
  do_check_eq(engine.wrappedJSObject._loadPath, "[other]addEngineWithDetails:" + kExtensionID);
  do_check_eq(engine.description, kDescription);
  do_check_eq(engine.iconURI.spec, kIconURL);
  do_check_eq(engine.alias, kAlias);

  // Set the engine as default; this should set a loadPath verification hash,
  // which should ensure we don't show the search reset prompt.
  Services.search.currentEngine = engine;

  let expectedURL = kSearchEngineURL.replace("{searchTerms}", kSearchTerm);
  let submission =
    Services.search.currentEngine.getSubmission(kSearchTerm, null, "searchbar");
  do_check_eq(submission.uri.spec, expectedURL);
  let expectedSuggestURL = kSearchSuggestURL.replace("{searchTerms}", kSearchTerm);
  let submissionSuggest =
    Services.search.currentEngine.getSubmission(kSearchTerm, URLTYPE_SUGGEST_JSON);
  do_check_eq(submissionSuggest.uri.spec, expectedSuggestURL);
});
