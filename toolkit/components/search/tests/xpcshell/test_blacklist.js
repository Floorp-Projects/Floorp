/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kSearchEngineID = "blacklist_test_engine";
const kSearchEngineURL = "http://example.com/?search={searchTerms}&blacklist=true";

add_task(async function test_blacklistEngine() {
  Assert.ok(!Services.search.isInitialized);

  await asyncInit();

  Services.search.addEngineWithDetails(kSearchEngineID, "", "", "", "get",
                                       kSearchEngineURL);

  // A blacklisted engine shouldn't be available at all
  let engine = Services.search.getEngineByName(kSearchEngineID);
  Assert.equal(engine, null, "Engine should not exist");
});
