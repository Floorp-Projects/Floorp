/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kSearchEngineID = "addEngineWithDetails_test_engine";
const kSearchEngineURL = "http://example.com/?search={searchTerms}";
const kSearchTerm = "foo";

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_addEngineWithDetails() {
  Assert.ok(!Services.search.isInitialized);

  await Services.search.addEngineWithDetails(kSearchEngineID, {
    method: "get",
    template: kSearchEngineURL,
  });

  // An engine added with addEngineWithDetails should have a load path, even
  // though we can't point to a specific file.
  let engine = Services.search.getEngineByName(kSearchEngineID);
  Assert.equal(engine.wrappedJSObject._loadPath, "[other]addEngineWithDetails");
  Assert.ok(
    !engine.isAppProvided,
    "Should not be shown as an app-provided engine"
  );

  // Set the engine as default; this should set a loadPath verification hash,
  // which should ensure we don't show the search reset prompt.
  await Services.search.setDefault(engine);

  let expectedURL = kSearchEngineURL.replace("{searchTerms}", kSearchTerm);
  let submission = (await Services.search.getDefault()).getSubmission(
    kSearchTerm,
    null,
    "searchbar"
  );
  Assert.equal(submission.uri.spec, expectedURL);
});
