/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kSearchEngineID1 = "blacklist_test_engine1";
const kSearchEngineID2 = "blacklist_test_engine2";
const kSearchEngineURL1 = "http://example.com/?search={searchTerms}&blacklist=true";
const kSearchEngineURL2 = "http://example.com/?search={searchTerms}&BLACKLIST=TRUE";

add_task(async function test_blacklistEngineLowerCase() {
  Assert.ok(!Services.search.isInitialized);

  await asyncInit();

  Services.search.addEngineWithDetails(kSearchEngineID1, "", "", "", "get",
                                       kSearchEngineURL1);

  // A blacklisted engine shouldn't be available at all
  let engine = Services.search.getEngineByName(kSearchEngineID1);
  Assert.equal(engine, null, "Engine should not exist");

  Services.search.addEngineWithDetails(kSearchEngineID2, "", "", "", "get",
                                       kSearchEngineURL2);

  // A blacklisted engine shouldn't be available at all
  engine = Services.search.getEngineByName(kSearchEngineID2);
  Assert.equal(engine, null, "Engine should not exist");
});
