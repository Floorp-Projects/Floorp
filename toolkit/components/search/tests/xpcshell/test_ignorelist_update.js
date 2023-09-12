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
  await SearchTestUtils.installSearchExtension({
    name: kSearchEngineID2,
    search_url: kSearchEngineURL2,
  });
  await SearchTestUtils.installSearchExtension({
    id: kExtensionID,
    name: kSearchEngineID3,
    search_url: kSearchEngineURL3,
  });

  // Ensure that the initial remote settings update from default values is
  // complete. The defaults do not include the special inclusions inserted below.
  await updatePromise;

  for (let engineName of [
    kSearchEngineID1,
    kSearchEngineID2,
    kSearchEngineID3,
  ]) {
    Assert.ok(
      await Services.search.getEngineByName(engineName),
      `Engine ${engineName} should be present`
    );
  }

  // Simulate an ignore list update.
  await RemoteSettings("hijack-blocklists").emit("sync", {
    data: {
      current: [
        {
          id: "load-paths",
          schema: 1553857697843,
          last_modified: 1553859483588,
          matches: ["[addon]searchignore@mozilla.com"],
        },
        {
          id: "submission-urls",
          schema: 1553857697843,
          last_modified: 1553859435500,
          matches: ["ignore=true"],
        },
      ],
    },
  });

  for (let engineName of [
    kSearchEngineID1,
    kSearchEngineID2,
    kSearchEngineID3,
  ]) {
    Assert.equal(
      await Services.search.getEngineByName(engineName),
      null,
      `Engine ${engineName} should not be present`
    );
  }
});
