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

add_task(async function test_ignorelistEngineLowerCase() {
  Assert.ok(!Services.search.isInitialized);

  await asyncInit();

  Services.search.addEngineWithDetails(kSearchEngineID1, "", "", "", "get",
                                       kSearchEngineURL1);

  // An ignored engine shouldn't be available at all
  let engine = Services.search.getEngineByName(kSearchEngineID1);
  Assert.equal(engine, null, "Engine should not exist");

  Services.search.addEngineWithDetails(kSearchEngineID2, "", "", "", "get",
                                       kSearchEngineURL2);

  // An ignored engine shouldn't be available at all
  engine = Services.search.getEngineByName(kSearchEngineID2);
  Assert.equal(engine, null, "Engine should not exist");

  Services.search.addEngineWithDetails(kSearchEngineID3, "", "", "", "get",
                                       kSearchEngineURL3, kExtensionID);

  // An ignored engine shouldn't be available at all
  engine = Services.search.getEngineByName(kSearchEngineID3);
  Assert.equal(engine, null, "Engine should not exist");
});
