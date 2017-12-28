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
add_task(async function test_store_keeps_unknown_properties() {
  // Create a new nsIHandlerInfo instance before loading the test data.
  await deleteHandlerStore();
  let handlerInfo =
      HandlerServiceTestUtils.getHandlerInfo("example/type.handleinternally");

  await copyTestDataToHandlerStore();
  gHandlerService.store(handlerInfo);

  await unloadHandlerStore();
  let data = JSON.parse(new TextDecoder().decode(await OS.File.read(jsonPath)));
  Assert.equal(data.mimeTypes["example/type.handleinternally"].unknownProperty,
               "preserved");
});

/**
 * Runs the asyncInit method, ensuring that it successfully inits the store
 * and calls the handlersvc-store-initialized topic.
 */
add_task(async function test_async_init() {
  await deleteHandlerStore();
  await copyTestDataToHandlerStore();
  gHandlerService.asyncInit();
  await TestUtils.topicObserved("handlersvc-store-initialized");
  await assertAllHandlerInfosMatchTestData();

  await unloadHandlerStore();
});

/**
 * Races the asyncInit method against the sync init (implicit in enumerate),
 * to ensure that the store will be synchronously initialized without any
 * ill effects.
 */
add_task(async function test_race_async_init() {
  await deleteHandlerStore();
  await copyTestDataToHandlerStore();
  let storeInitialized = false;
  // Pass a callback to synchronously observe the topic, as a promise would
  // resolve asynchronously
  TestUtils.topicObserved("handlersvc-store-initialized", () => {
    storeInitialized = true;
    return true;
  });
  gHandlerService.asyncInit();
  Assert.ok(!storeInitialized);
  gHandlerService.enumerate();
  Assert.ok(storeInitialized);
  await assertAllHandlerInfosMatchTestData();

  await unloadHandlerStore();
});

/**
 * Tests the migration from an existing RDF data source.
 */
add_task(async function test_migration_rdf_present() {
  // Perform the most common migration, with the JSON file missing.
  await deleteHandlerStore();
  await copyTestDataToHandlerStoreRDF();
  Services.prefs.setBoolPref("gecko.handlerService.migrated", false);
  await assertAllHandlerInfosMatchTestData();
  Assert.ok(Services.prefs.getBoolPref("gecko.handlerService.migrated"));

  // Repeat the migration with the JSON file present.
  await unloadHandlerStore();
  await unloadHandlerStoreRDF();
  Services.prefs.setBoolPref("gecko.handlerService.migrated", false);
  await assertAllHandlerInfosMatchTestData();
  Assert.ok(Services.prefs.getBoolPref("gecko.handlerService.migrated"));
});

/**
 * Tests that new entries are preserved if migration is triggered manually.
 */
add_task(async function test_migration_rdf_present_keeps_new_data() {
  await deleteHandlerStore();

  let handlerInfo = getKnownHandlerInfo("example/new");
  gHandlerService.store(handlerInfo);

  // Perform the migration with the JSON file present.
  await unloadHandlerStore();
  await copyTestDataToHandlerStoreRDF();
  Services.prefs.setBoolPref("gecko.handlerService.migrated", false);

  let actualHandlerInfo = HandlerServiceTestUtils.getHandlerInfo("example/new");
  HandlerServiceTestUtils.assertHandlerInfoMatches(actualHandlerInfo, {
    type: "example/new",
    preferredAction: Ci.nsIHandlerInfo.saveToDisk,
    alwaysAskBeforeHandling: false,
  });

  Assert.ok(Services.prefs.getBoolPref("gecko.handlerService.migrated"));
});

/**
 * Tests the injection of default protocol handlers when the RDF does not exist.
 */
add_task(async function test_migration_rdf_absent() {
  if (!Services.prefs.getPrefType("gecko.handlerService.defaultHandlersVersion")) {
    info("This platform or locale does not have default handlers.");
    return;
  }

  // Perform the most common migration, with the JSON file missing.
  await deleteHandlerStore();
  await deleteHandlerStoreRDF();
  Services.prefs.setBoolPref("gecko.handlerService.migrated", false);
  await assertAllHandlerInfosMatchDefaultHandlers();
  Assert.ok(Services.prefs.getBoolPref("gecko.handlerService.migrated"));

  // Repeat the migration with the JSON file present.
  await unloadHandlerStore();
  await unloadHandlerStoreRDF();
  Services.prefs.setBoolPref("gecko.handlerService.migrated", false);
  await assertAllHandlerInfosMatchDefaultHandlers();
  Assert.ok(Services.prefs.getBoolPref("gecko.handlerService.migrated"));
});

/**
 * Test saving and reloading an instance of nsIGIOMimeApp.
 */
add_task(async function test_store_gioHandlerApp() {
  if (!("@mozilla.org/gio-service;1" in Cc)) {
    do_print("Skipping test because it does not apply to this platform.");
    return;
  }

  // Create dummy exec file that following won't fail because file not found error
  let dummyHandlerFile = FileUtils.getFile("TmpD", ["dummyHandler"]);
  dummyHandlerFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("777", 8));

  // Set up an nsIGIOMimeApp instance for testing.
  let handlerApp = Cc["@mozilla.org/gio-service;1"]
                     .getService(Ci.nsIGIOService)
                     .createAppFromCommand(dummyHandlerFile.path, "Dummy GIO handler");
  let expectedGIOMimeHandlerApp = {
    name: handlerApp.name,
    command: handlerApp.command,
  };

  await deleteHandlerStore();

  let handlerInfo = getKnownHandlerInfo("example/new");
  handlerInfo.preferredApplicationHandler = handlerApp;
  handlerInfo.possibleApplicationHandlers.appendElement(handlerApp);
  handlerInfo.possibleApplicationHandlers.appendElement(webHandlerApp);
  gHandlerService.store(handlerInfo);

  await unloadHandlerStore();

  let actualHandlerInfo = HandlerServiceTestUtils.getHandlerInfo("example/new");
  HandlerServiceTestUtils.assertHandlerInfoMatches(actualHandlerInfo, {
    type: "example/new",
    preferredAction: Ci.nsIHandlerInfo.saveToDisk,
    alwaysAskBeforeHandling: false,
    preferredApplicationHandler: expectedGIOMimeHandlerApp,
    possibleApplicationHandlers: [expectedGIOMimeHandlerApp, webHandlerApp],
  });

  await OS.File.remove(dummyHandlerFile.path);

  // After removing dummyHandlerFile, the handler should disappear from the
  // list of possibleApplicationHandlers and preferredAppHandler should be null.
  actualHandlerInfo = HandlerServiceTestUtils.getHandlerInfo("example/new");
  HandlerServiceTestUtils.assertHandlerInfoMatches(actualHandlerInfo, {
    type: "example/new",
    preferredAction: Ci.nsIHandlerInfo.saveToDisk,
    alwaysAskBeforeHandling: false,
    preferredApplicationHandler: null,
    possibleApplicationHandlers: [webHandlerApp],
  });

});

