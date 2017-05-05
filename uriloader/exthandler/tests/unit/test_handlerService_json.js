/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests the nsIHandlerService interface using the JSON backend.
 */

let gHandlerService = gHandlerServiceJSON;
let unloadHandlerStore = unloadHandlerStoreJSON;
let deleteHandlerStore = deleteHandlerStoreJSON;
let copyTestDataToHandlerStore = copyTestDataToHandlerStoreJSON;

var scriptFile = do_get_file("common_test_handlerService.js");
Services.scriptloader.loadSubScript(NetUtil.newURI(scriptFile).spec);

/**
 * Ensures forward compatibility by checking that the "store" method preserves
 * unknown properties in the test data. This is specific to the JSON back-end.
 */
add_task(function* test_store_keeps_unknown_properties() {
  // Create a new nsIHandlerInfo instance before loading the test data.
  yield deleteHandlerStore();
  let handlerInfo =
      HandlerServiceTestUtils.getHandlerInfo("example/type.handleinternally");

  yield copyTestDataToHandlerStore();
  gHandlerService.store(handlerInfo);

  yield unloadHandlerStore();
  let data = JSON.parse(new TextDecoder().decode(yield OS.File.read(jsonPath)));
  do_check_eq(data.mimeTypes["example/type.handleinternally"].unknownProperty,
              "preserved");
});

/**
 * Tests the migration from an existing RDF data source.
 */
add_task(function* test_migration_rdf_present() {
  // Perform the most common migration, with the JSON file missing.
  yield deleteHandlerStore();
  yield copyTestDataToHandlerStoreRDF();
  Services.prefs.setBoolPref("gecko.handlerService.migrated", false);
  yield assertAllHandlerInfosMatchTestData();
  do_check_true(Services.prefs.getBoolPref("gecko.handlerService.migrated"));

  // Repeat the migration with the JSON file present.
  yield unloadHandlerStore();
  yield unloadHandlerStoreRDF();
  Services.prefs.setBoolPref("gecko.handlerService.migrated", false);
  yield assertAllHandlerInfosMatchTestData();
  do_check_true(Services.prefs.getBoolPref("gecko.handlerService.migrated"));
});

/**
 * Tests that new entries are preserved if migration is triggered manually.
 */
add_task(function* test_migration_rdf_present_keeps_new_data() {
  yield deleteHandlerStore();

  let handlerInfo = getKnownHandlerInfo("example/new");
  gHandlerService.store(handlerInfo);

  // Perform the migration with the JSON file present.
  yield unloadHandlerStore();
  yield copyTestDataToHandlerStoreRDF();
  Services.prefs.setBoolPref("gecko.handlerService.migrated", false);

  let actualHandlerInfo = HandlerServiceTestUtils.getHandlerInfo("example/new");
  HandlerServiceTestUtils.assertHandlerInfoMatches(actualHandlerInfo, {
    type: "example/new",
    preferredAction: Ci.nsIHandlerInfo.saveToDisk,
    alwaysAskBeforeHandling: false,
  });

  do_check_true(Services.prefs.getBoolPref("gecko.handlerService.migrated"));
});

/**
 * Tests the injection of default protocol handlers when the RDF does not exist.
 */
add_task(function* test_migration_rdf_absent() {
  if (!Services.prefs.getPrefType("gecko.handlerService.defaultHandlersVersion")) {
    do_print("This platform or locale does not have default handlers.");
    return;
  }

  // Perform the most common migration, with the JSON file missing.
  yield deleteHandlerStore();
  yield deleteHandlerStoreRDF();
  Services.prefs.setBoolPref("gecko.handlerService.migrated", false);
  yield assertAllHandlerInfosMatchDefaultHandlers();
  do_check_true(Services.prefs.getBoolPref("gecko.handlerService.migrated"));

  // Repeat the migration with the JSON file present.
  yield unloadHandlerStore();
  yield unloadHandlerStoreRDF();
  Services.prefs.setBoolPref("gecko.handlerService.migrated", false);
  yield assertAllHandlerInfosMatchDefaultHandlers();
  do_check_true(Services.prefs.getBoolPref("gecko.handlerService.migrated"));
});
