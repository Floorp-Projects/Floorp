/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kSearchEngineID1 = "ignorelist_test_engine1";
const kSearchEngineID2 = "ignorelist_test_engine2";
const kSearchEngineID3 = "ignorelist_test_engine3";
const kSearchEngineURL1 = "http://example.com/?search={searchTerms}&ignore=true";
const kSearchEngineURL2 = "http://example.com/?search={searchTerms}&IGNORE=TRUE";
const kSearchEngineURL3 = "http://example.com/?search={searchTerms}";
const kExtensionID = "searchignore@mozilla.com";

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_ignoreList() {
  Assert.ok(!Services.search.isInitialized,
    "Search service should not be initialized to begin with.");

  let updatePromise = SearchTestUtils.promiseSearchNotification("settings-update-complete");
  await Services.search.addEngineWithDetails(kSearchEngineID1, "", "", "", "get", kSearchEngineURL1);
  await Services.search.addEngineWithDetails(kSearchEngineID2, "", "", "", "get", kSearchEngineURL2);
  await Services.search.addEngineWithDetails(kSearchEngineID3, "", "", "", "get",
                                             kSearchEngineURL3, kExtensionID);

  // Ensure that the initial remote settings update from default values is
  // complete. The defaults do not include the special inclusions inserted below.
  await updatePromise;

  for (let engineName of [kSearchEngineID1, kSearchEngineID2, kSearchEngineID3]) {
    Assert.ok(await Services.search.getEngineByName(engineName),
      `Engine ${engineName} should be present`);
  }

  // Simulate an ignore list update.
  await RemoteSettings("hijack-blocklists").emit("sync", {
    data: {
      current: [{
        "id": "load-paths",
        "schema": 1553857697843,
        "last_modified": 1553859483588,
        "matches": [
          "[other]addEngineWithDetails:searchignore@mozilla.com",
        ],
      }, {
        "id": "submission-urls",
        "schema": 1553857697843,
        "last_modified": 1553859435500,
        "matches": [
          "ignore=true",
        ],
      }],
    },
  });

  for (let engineName of [kSearchEngineID1, kSearchEngineID2, kSearchEngineID3]) {
    Assert.equal(await Services.search.getEngineByName(engineName), null,
      `Engine ${engineName} should not be present`);
  }
});
