/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kSearchEngineID = "addEngineWithDetails_test_engine";
const kSearchEngineURL = "http://example.com/?search={searchTerms}";
const kSearchTerm = "foo";
const kExtensionID = "extension@mozilla.com";

add_task(async function test_addEngineWithExtensionID() {
  do_check_false(Services.search.isInitialized);

  await asyncInit();

  Services.search.addEngineWithDetails(kSearchEngineID, "", "", "", "get",
                                       kSearchEngineURL, kExtensionID);

  let engine = Services.search.getEngineByName(kSearchEngineID);
  do_check_neq(engine, null);

  let engines = Services.search.getEnginesByExtensionID(kExtensionID);
  do_check_eq(engines.length, 1);
  do_check_eq(engines[0], engine);
});
