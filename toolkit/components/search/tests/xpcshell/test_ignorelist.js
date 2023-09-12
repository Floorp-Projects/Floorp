/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kSearchEngineID1 = "ignorelist_test_engine1";
const kSearchEngineID2 = "ignorelist_test_engine2";
const kSearchEngineID3 = "ignorelist_test_engine3";
const kSearchEngineURL1 =
  "https://example.com/?search={searchTerms}&ignore=true";
const kSearchEngineURL2 =
  "https://example.com/?search={searchTerms}&IGNORE=TRUE";
const kSearchEngineURL3 = "https://example.com/?search={searchTerms}";
const kExtensionID = "searchignore@mozilla.com";

add_setup(async function () {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_ignoreList() {
  await setupRemoteSettings();

  Assert.ok(
    !Services.search.isInitialized,
    "Search service should not be initialized to begin with."
  );

  let updatePromise = SearchTestUtils.promiseSearchNotification(
    "settings-update-complete"
  );

  await SearchTestUtils.installSearchExtension({
    name: kSearchEngineID1,
    search_url: kSearchEngineURL1,
  });

  await updatePromise;

  let engine = Services.search.getEngineByName(kSearchEngineID1);
  Assert.equal(
    engine,
    null,
    "Engine with ignored search params should not exist"
  );

  await SearchTestUtils.installSearchExtension({
    name: kSearchEngineID2,
    search_url: kSearchEngineURL2,
  });

  // An ignored engine shouldn't be available at all
  engine = Services.search.getEngineByName(kSearchEngineID2);
  Assert.equal(
    engine,
    null,
    "Engine with ignored search params of a different case should not exist"
  );

  await SearchTestUtils.installSearchExtension({
    id: kExtensionID,
    name: kSearchEngineID3,
    search_url: kSearchEngineURL3,
  });

  // An ignored engine shouldn't be available at all
  engine = Services.search.getEngineByName(kSearchEngineID3);
  Assert.equal(
    engine,
    null,
    "Engine with ignored extension id should not exist"
  );
});
