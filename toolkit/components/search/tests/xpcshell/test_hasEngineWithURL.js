/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the hasEngineWithURL() method of the nsISearchService.
 */

// Return a discreet, cloned copy of an (engine) object.
function getEngineClone(engine) {
  return JSON.parse(JSON.stringify(engine));
}

// Check whether and engine does or doesn't exist.
function checkEngineState(exists, engine) {
  Assert.equal(exists, Services.search.hasEngineWithURL(engine.method,
                                                        engine.formURL,
                                                        engine.queryParams));
}

// Add a search engine for testing.
async function addEngineWithParams(engine) {
  await Services.search.addEngineWithDetails(engine.name, null, null, null,
                                             engine.method, engine.formURL);

  let addedEngine = Services.search.getEngineByName(engine.name);
  for (let param of engine.queryParams) {
    addedEngine.addParam(param.name, param.value, null);
  }
}

// Main test.
add_task(async function test_hasEngineWithURL() {
  info("Setting up test");

  useHttpServer();

  info("Test starting");
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();

  // Setup various Engine definitions for method tests.
  let UNSORTED_ENGINE = {
    name: "mySearch Engine",
    method: "GET",
    formURL: "https://totallyNotRealSearchEngine.com/",
    queryParams: [
      { name: "DDs", value: "38s" },
      { name: "DCs", value: "39s" },
      { name: "DDs", value: "39s" },
      { name: "DDs", value: "38s" },
      { name: "DDs", value: "37s" },
      { name: "DDs", value: "38s" },
      { name: "DEs", value: "38s" },
      { name: "DCs", value: "38s" },
      { name: "DEs", value: "37s" },
    ],
  };

  // Same as UNSORTED_ENGINE, but sorted.
  let SORTED_ENGINE = {
    name: "mySearch Engine",
    method: "GET",
    formURL: "https://totallyNotRealSearchEngine.com/",
    queryParams: [
      { name: "DCs", value: "38s" },
      { name: "DCs", value: "39s" },
      { name: "DDs", value: "37s" },
      { name: "DDs", value: "38s" },
      { name: "DDs", value: "38s" },
      { name: "DDs", value: "38s" },
      { name: "DDs", value: "39s" },
      { name: "DEs", value: "37s" },
      { name: "DEs", value: "38s" },
    ],
  };

  // Unique variations of the SORTED_ENGINE.
  let SORTED_ENGINE_METHOD_CHANGE = getEngineClone(SORTED_ENGINE);
  SORTED_ENGINE_METHOD_CHANGE.method = "PoST";

  let SORTED_ENGINE_FORMURL_CHANGE = getEngineClone(SORTED_ENGINE);
  SORTED_ENGINE_FORMURL_CHANGE.formURL = "http://www.ahighrpowr.com/";

  let SORTED_ENGINE_QUERYPARM_CHANGE = getEngineClone(SORTED_ENGINE);
  SORTED_ENGINE_QUERYPARM_CHANGE.queryParams = [];

  let SORTED_ENGINE_NAME_CHANGE = getEngineClone(SORTED_ENGINE);
  SORTED_ENGINE_NAME_CHANGE.name += " 2";


  // First ensure neither the unsorted engine, nor the same engine
  // with a pre-sorted list of query parms matches.
  checkEngineState(false, UNSORTED_ENGINE);
  info("The unsorted version of the test engine does not exist.");
  checkEngineState(false, SORTED_ENGINE);
  info("The sorted version of the test engine does not exist.");

  // Ensure variations of the engine definition do not match.
  checkEngineState(false, SORTED_ENGINE_METHOD_CHANGE);
  checkEngineState(false, SORTED_ENGINE_FORMURL_CHANGE);
  checkEngineState(false, SORTED_ENGINE_QUERYPARM_CHANGE);
  info("There are no modified versions of the sorted test engine.");

  // Note that this method doesn't check name variations.
  checkEngineState(false, SORTED_ENGINE_NAME_CHANGE);
  info("There is no NAME modified version of the sorted test engine.");


  // Add the unsorted engine and it's queryParams.
  await addEngineWithParams(UNSORTED_ENGINE);
  info("The unsorted engine has been added.");


  // Then, ensure we find a match for the unsorted engine, and for the
  // same engine with a pre-sorted list of query parms.
  checkEngineState(true, UNSORTED_ENGINE);
  info("The unsorted version of the test engine now exists.");
  checkEngineState(true, SORTED_ENGINE);
  info("The sorted version of the same test engine also now exists.");

  // Ensure variations of the engine definition still do not match.
  checkEngineState(false, SORTED_ENGINE_METHOD_CHANGE);
  checkEngineState(false, SORTED_ENGINE_FORMURL_CHANGE);
  checkEngineState(false, SORTED_ENGINE_QUERYPARM_CHANGE);
  info("There are still no modified versions of the sorted test engine.");

  // Note that this method still doesn't check name variations.
  checkEngineState(true, SORTED_ENGINE_NAME_CHANGE);
  info("There IS now a NAME modified version of the sorted test engine.");
});
