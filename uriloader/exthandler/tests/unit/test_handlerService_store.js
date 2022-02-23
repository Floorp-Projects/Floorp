/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests the nsIHandlerService interface.
 */

// Set up an nsIWebHandlerApp instance that can be used in multiple tests.
let webHandlerApp = Cc[
  "@mozilla.org/uriloader/web-handler-app;1"
].createInstance(Ci.nsIWebHandlerApp);
webHandlerApp.name = "Web Handler";
webHandlerApp.uriTemplate = "https://www.example.com/?url=%s";
let expectedWebHandlerApp = {
  name: webHandlerApp.name,
  uriTemplate: webHandlerApp.uriTemplate,
};

// Set up an nsILocalHandlerApp instance that can be used in multiple tests. The
// executable should exist, but it doesn't need to point to an actual file, so
// we simply initialize it to the path of an existing directory.
let localHandlerApp = Cc[
  "@mozilla.org/uriloader/local-handler-app;1"
].createInstance(Ci.nsILocalHandlerApp);
localHandlerApp.name = "Local Handler";
localHandlerApp.executable = FileUtils.getFile("TmpD", []);
let expectedLocalHandlerApp = {
  name: localHandlerApp.name,
  executable: localHandlerApp.executable,
};

/**
 * Returns a new nsIHandlerInfo instance initialized to known values that don't
 * depend on the platform and are easier to verify later.
 *
 * @param type
 *        Because the "preferredAction" is initialized to saveToDisk, this
 *        should represent a MIME type rather than a protocol.
 */
function getKnownHandlerInfo(type) {
  let handlerInfo = HandlerServiceTestUtils.getBlankHandlerInfo(type);
  handlerInfo.preferredAction = Ci.nsIHandlerInfo.saveToDisk;
  handlerInfo.alwaysAskBeforeHandling = false;
  return handlerInfo;
}

/**
 * Checks that the information stored in the handler service instance under
 * testing matches the test data files.
 */
function assertAllHandlerInfosMatchTestData() {
  let handlerInfos = HandlerServiceTestUtils.getAllHandlerInfos();

  // It's important that the MIME types we check here do not exist at the
  // operating system level, otherwise the list of handlers and file extensions
  // will be merged. The current implementation avoids duplicate entries.

  HandlerServiceTestUtils.assertHandlerInfoMatches(handlerInfos.shift(), {
    type: "example/type.handleinternally",
    preferredAction: Ci.nsIHandlerInfo.handleInternally,
    alwaysAskBeforeHandling: false,
    fileExtensions: ["example_one"],
  });

  HandlerServiceTestUtils.assertHandlerInfoMatches(handlerInfos.shift(), {
    type: "example/type.savetodisk",
    preferredAction: Ci.nsIHandlerInfo.saveToDisk,
    alwaysAskBeforeHandling: true,
    preferredApplicationHandler: {
      name: "Example Default Handler",
      uriTemplate: "https://www.example.com/?url=%s",
    },
    possibleApplicationHandlers: [
      {
        name: "Example Default Handler",
        uriTemplate: "https://www.example.com/?url=%s",
      },
    ],
    fileExtensions: ["example_two", "example_three"],
  });

  HandlerServiceTestUtils.assertHandlerInfoMatches(handlerInfos.shift(), {
    type: "example/type.usehelperapp",
    preferredAction: Ci.nsIHandlerInfo.useHelperApp,
    alwaysAskBeforeHandling: true,
    preferredApplicationHandler: {
      name: "Example Default Handler",
      uriTemplate: "https://www.example.com/?url=%s",
    },
    possibleApplicationHandlers: [
      {
        name: "Example Default Handler",
        uriTemplate: "https://www.example.com/?url=%s",
      },
      {
        name: "Example Possible Handler One",
        uriTemplate: "http://www.example.com/?id=1&url=%s",
      },
      {
        name: "Example Possible Handler Two",
        uriTemplate: "http://www.example.com/?id=2&url=%s",
      },
    ],
    fileExtensions: ["example_two", "example_three"],
  });

  HandlerServiceTestUtils.assertHandlerInfoMatches(handlerInfos.shift(), {
    type: "example/type.usesystemdefault",
    preferredAction: Ci.nsIHandlerInfo.useSystemDefault,
    alwaysAskBeforeHandling: false,
    possibleApplicationHandlers: [
      {
        name: "Example Possible Handler",
        uriTemplate: "http://www.example.com/?url=%s",
      },
    ],
  });

  HandlerServiceTestUtils.assertHandlerInfoMatches(handlerInfos.shift(), {
    type: "examplescheme.usehelperapp",
    preferredAction: Ci.nsIHandlerInfo.useHelperApp,
    alwaysAskBeforeHandling: true,
    preferredApplicationHandler: {
      name: "Example Default Handler",
      uriTemplate: "https://www.example.com/?url=%s",
    },
    possibleApplicationHandlers: [
      {
        name: "Example Default Handler",
        uriTemplate: "https://www.example.com/?url=%s",
      },
      {
        name: "Example Possible Handler One",
        uriTemplate: "http://www.example.com/?id=1&url=%s",
      },
      {
        name: "Example Possible Handler Two",
        uriTemplate: "http://www.example.com/?id=2&url=%s",
      },
    ],
  });

  HandlerServiceTestUtils.assertHandlerInfoMatches(handlerInfos.shift(), {
    type: "examplescheme.usesystemdefault",
    preferredAction: Ci.nsIHandlerInfo.useSystemDefault,
    alwaysAskBeforeHandling: false,
    possibleApplicationHandlers: [
      {
        name: "Example Possible Handler",
        uriTemplate: "http://www.example.com/?url=%s",
      },
    ],
  });

  Assert.equal(handlerInfos.length, 0);
}

/**
 * Loads data from a file in a predefined format, verifying that the format is
 * recognized and all the known properties are loaded and saved.
 */
add_task(async function test_store_fillHandlerInfo_predefined() {
  // Test that the file format used in previous versions can be loaded.
  await copyTestDataToHandlerStore();
  await assertAllHandlerInfosMatchTestData();

  // Keep a copy of the nsIHandlerInfo instances, then delete the handler store
  // and populate it with the known data. Since the handler store is empty, the
  // default handlers for the current locale are also injected, so we have to
  // delete them manually before adding the other nsIHandlerInfo instances.
  let testHandlerInfos = HandlerServiceTestUtils.getAllHandlerInfos();
  await deleteHandlerStore();
  for (let handlerInfo of HandlerServiceTestUtils.getAllHandlerInfos()) {
    gHandlerService.remove(handlerInfo);
  }
  for (let handlerInfo of testHandlerInfos) {
    gHandlerService.store(handlerInfo);
  }

  // Test that the known data still matches after saving it and reloading.
  await unloadHandlerStore();
  await assertAllHandlerInfosMatchTestData();
});

/**
 * Check that "store" is able to add new instances, that "remove" and "exists"
 * work, and that "fillHandlerInfo" throws when the instance does not exist.
 */
add_task(async function test_store_remove_exists() {
  // Test both MIME types and protocols.
  for (let type of [
    "example/type.usehelperapp",
    "examplescheme.usehelperapp",
  ]) {
    // Create new nsIHandlerInfo instances before loading the test data.
    await deleteHandlerStore();
    let handlerInfoPresent = HandlerServiceTestUtils.getHandlerInfo(type);
    let handlerInfoAbsent = HandlerServiceTestUtils.getHandlerInfo(type + "2");

    // Set up known properties that we can verify later.
    handlerInfoAbsent.preferredAction = Ci.nsIHandlerInfo.saveToDisk;
    handlerInfoAbsent.alwaysAskBeforeHandling = false;

    await copyTestDataToHandlerStore();

    Assert.ok(gHandlerService.exists(handlerInfoPresent));
    Assert.ok(!gHandlerService.exists(handlerInfoAbsent));

    gHandlerService.store(handlerInfoAbsent);
    gHandlerService.remove(handlerInfoPresent);

    await unloadHandlerStore();

    Assert.ok(!gHandlerService.exists(handlerInfoPresent));
    Assert.ok(gHandlerService.exists(handlerInfoAbsent));

    Assert.throws(
      () => gHandlerService.fillHandlerInfo(handlerInfoPresent, ""),
      ex => ex.result == Cr.NS_ERROR_NOT_AVAILABLE
    );

    let actualHandlerInfo = HandlerServiceTestUtils.getHandlerInfo(type + "2");
    HandlerServiceTestUtils.assertHandlerInfoMatches(actualHandlerInfo, {
      type: type + "2",
      preferredAction: Ci.nsIHandlerInfo.saveToDisk,
      alwaysAskBeforeHandling: false,
    });
  }
});

/**
 * Tests that it is possible to save an nsIHandlerInfo instance with a
 * "preferredAction" that is either a valid or an unknown value, and the
 * action always takes on an appropriate value when reloading.
 */
add_task(async function test_store_preferredAction() {
  await deleteHandlerStore();

  let handlerInfo = getKnownHandlerInfo("example/new");
  // Valid action values should all remain unchanged across a refresh, except
  // for alwaysAsk which may be overridden with useHelperApp depending on prefs.
  // Invalid action values should always convert to useHelperApp.
  const actions = [
    {
      preferred: Ci.nsIHandlerInfo.alwaysAsk,
      expected: Services.prefs.getBoolPref(
        "browser.download.improvements_to_download_panel"
      )
        ? Ci.nsIHandlerInfo.alwaysAsk
        : Ci.nsIHandlerInfo.useHelperApp,
    },
    {
      preferred: Ci.nsIHandlerInfo.handleInternally,
      expected: Ci.nsIHandlerInfo.handleInternally,
    },
    { preferred: 999, expected: Ci.nsIHandlerInfo.useHelperApp },
  ];

  for (let action of actions) {
    handlerInfo.preferredAction = action.preferred;
    gHandlerService.store(handlerInfo);
    gHandlerService.fillHandlerInfo(handlerInfo, "");
    Assert.equal(handlerInfo.preferredAction, action.expected);
  }
});

/**
 * Tests that it is possible to save an nsIHandlerInfo instance containing an
 * nsILocalHandlerApp instance pointing to an executable that doesn't exist, but
 * this entry is ignored when reloading.
 */
add_task(async function test_store_localHandlerApp_missing() {
  if (!("@mozilla.org/uriloader/dbus-handler-app;1" in Cc)) {
    info("Skipping test because it does not apply to this platform.");
    return;
  }

  let missingHandlerApp = Cc[
    "@mozilla.org/uriloader/local-handler-app;1"
  ].createInstance(Ci.nsILocalHandlerApp);
  missingHandlerApp.name = "Non-existing Handler";
  missingHandlerApp.executable = FileUtils.getFile("TmpD", ["nonexisting"]);

  await deleteHandlerStore();

  let handlerInfo = getKnownHandlerInfo("example/new");
  handlerInfo.preferredApplicationHandler = missingHandlerApp;
  handlerInfo.possibleApplicationHandlers.appendElement(missingHandlerApp);
  handlerInfo.possibleApplicationHandlers.appendElement(webHandlerApp);
  gHandlerService.store(handlerInfo);

  await unloadHandlerStore();

  let actualHandlerInfo = HandlerServiceTestUtils.getHandlerInfo("example/new");
  HandlerServiceTestUtils.assertHandlerInfoMatches(actualHandlerInfo, {
    type: "example/new",
    preferredAction: Ci.nsIHandlerInfo.saveToDisk,
    alwaysAskBeforeHandling: false,
    possibleApplicationHandlers: [expectedWebHandlerApp],
  });
});

/**
 * Test saving and reloading an instance of nsIDBusHandlerApp.
 */
add_task(async function test_store_dBusHandlerApp() {
  if (!("@mozilla.org/uriloader/dbus-handler-app;1" in Cc)) {
    info("Skipping test because it does not apply to this platform.");
    return;
  }

  // Set up an nsIDBusHandlerApp instance for testing.
  let dBusHandlerApp = Cc[
    "@mozilla.org/uriloader/dbus-handler-app;1"
  ].createInstance(Ci.nsIDBusHandlerApp);
  dBusHandlerApp.name = "DBus Handler";
  dBusHandlerApp.service = "test.method.server";
  dBusHandlerApp.method = "Method";
  dBusHandlerApp.dBusInterface = "test.method.Type";
  dBusHandlerApp.objectPath = "/test/method/Object";
  let expectedDBusHandlerApp = {
    name: dBusHandlerApp.name,
    service: dBusHandlerApp.service,
    method: dBusHandlerApp.method,
    dBusInterface: dBusHandlerApp.dBusInterface,
    objectPath: dBusHandlerApp.objectPath,
  };

  await deleteHandlerStore();

  let handlerInfo = getKnownHandlerInfo("example/new");
  handlerInfo.preferredApplicationHandler = dBusHandlerApp;
  handlerInfo.possibleApplicationHandlers.appendElement(dBusHandlerApp);
  gHandlerService.store(handlerInfo);

  await unloadHandlerStore();

  let actualHandlerInfo = HandlerServiceTestUtils.getHandlerInfo("example/new");
  HandlerServiceTestUtils.assertHandlerInfoMatches(actualHandlerInfo, {
    type: "example/new",
    preferredAction: Ci.nsIHandlerInfo.saveToDisk,
    alwaysAskBeforeHandling: false,
    preferredApplicationHandler: expectedDBusHandlerApp,
    possibleApplicationHandlers: [expectedDBusHandlerApp],
  });
});

/**
 * Tests that it is possible to save an nsIHandlerInfo instance with a
 * "preferredApplicationHandler" and no "possibleApplicationHandlers", but the
 * former is always included in the latter list when reloading.
 */
add_task(
  async function test_store_possibleApplicationHandlers_includes_preferred() {
    await deleteHandlerStore();

    let handlerInfo = getKnownHandlerInfo("example/new");
    handlerInfo.preferredApplicationHandler = localHandlerApp;
    gHandlerService.store(handlerInfo);

    await unloadHandlerStore();

    let actualHandlerInfo = HandlerServiceTestUtils.getHandlerInfo(
      "example/new"
    );
    HandlerServiceTestUtils.assertHandlerInfoMatches(actualHandlerInfo, {
      type: "example/new",
      preferredAction: Ci.nsIHandlerInfo.saveToDisk,
      alwaysAskBeforeHandling: false,
      preferredApplicationHandler: expectedLocalHandlerApp,
      possibleApplicationHandlers: [expectedLocalHandlerApp],
    });
  }
);

/**
 * Tests that it is possible to save an nsIHandlerInfo instance with a
 * "preferredApplicationHandler" that is not the first element in
 * "possibleApplicationHandlers", but the former is always included as the first
 * element of the latter list when reloading.
 */
add_task(
  async function test_store_possibleApplicationHandlers_preferred_first() {
    await deleteHandlerStore();

    let handlerInfo = getKnownHandlerInfo("example/new");
    handlerInfo.preferredApplicationHandler = webHandlerApp;
    // The preferred handler is appended after the other one.
    handlerInfo.possibleApplicationHandlers.appendElement(localHandlerApp);
    handlerInfo.possibleApplicationHandlers.appendElement(webHandlerApp);
    gHandlerService.store(handlerInfo);

    await unloadHandlerStore();

    let actualHandlerInfo = HandlerServiceTestUtils.getHandlerInfo(
      "example/new"
    );
    HandlerServiceTestUtils.assertHandlerInfoMatches(actualHandlerInfo, {
      type: "example/new",
      preferredAction: Ci.nsIHandlerInfo.saveToDisk,
      alwaysAskBeforeHandling: false,
      preferredApplicationHandler: expectedWebHandlerApp,
      possibleApplicationHandlers: [
        expectedWebHandlerApp,
        expectedLocalHandlerApp,
      ],
    });
  }
);

/**
 * Tests that it is possible to save an nsIHandlerInfo instance with an
 * uppercase file extension, but it is converted to lowercase when reloading.
 */
add_task(async function test_store_fileExtensions_lowercase() {
  await deleteHandlerStore();

  let handlerInfo = getKnownHandlerInfo("example/new");
  handlerInfo.appendExtension("extension_test1");
  handlerInfo.appendExtension("EXTENSION_test2");
  gHandlerService.store(handlerInfo);

  await unloadHandlerStore();

  let actualHandlerInfo = HandlerServiceTestUtils.getHandlerInfo("example/new");
  HandlerServiceTestUtils.assertHandlerInfoMatches(actualHandlerInfo, {
    type: "example/new",
    preferredAction: Ci.nsIHandlerInfo.saveToDisk,
    alwaysAskBeforeHandling: false,
    fileExtensions: ["extension_test1", "extension_test2"],
  });
});

/**
 * Tests that appendExtension doesn't add duplicates, and that anyway duplicates
 * from possibleApplicationHandlers are removed when saving and reloading.
 */
add_task(async function test_store_no_duplicates() {
  await deleteHandlerStore();

  let handlerInfo = getKnownHandlerInfo("example/new");
  handlerInfo.preferredApplicationHandler = webHandlerApp;
  handlerInfo.possibleApplicationHandlers.appendElement(webHandlerApp);
  handlerInfo.possibleApplicationHandlers.appendElement(localHandlerApp);
  handlerInfo.possibleApplicationHandlers.appendElement(localHandlerApp);
  handlerInfo.possibleApplicationHandlers.appendElement(webHandlerApp);
  handlerInfo.appendExtension("extension_test1");
  handlerInfo.appendExtension("extension_test2");
  handlerInfo.appendExtension("extension_test1");
  handlerInfo.appendExtension("EXTENSION_test1");
  Assert.deepEqual(Array.from(handlerInfo.getFileExtensions()), [
    "extension_test1",
    "extension_test2",
  ]);
  gHandlerService.store(handlerInfo);

  await unloadHandlerStore();

  let actualHandlerInfo = HandlerServiceTestUtils.getHandlerInfo("example/new");
  HandlerServiceTestUtils.assertHandlerInfoMatches(actualHandlerInfo, {
    type: "example/new",
    preferredAction: Ci.nsIHandlerInfo.saveToDisk,
    alwaysAskBeforeHandling: false,
    preferredApplicationHandler: expectedWebHandlerApp,
    possibleApplicationHandlers: [
      expectedWebHandlerApp,
      expectedLocalHandlerApp,
    ],
    fileExtensions: ["extension_test1", "extension_test2"],
  });
});

/**
 * Tests that setFileExtensions doesn't add duplicates.
 */
add_task(async function test_setFileExtensions_no_duplicates() {
  await deleteHandlerStore();

  let handlerInfo = getKnownHandlerInfo("example/new");
  handlerInfo.setFileExtensions("a,b,A,b,c,a");
  let expected = ["a", "b", "c"];
  Assert.deepEqual(Array.from(handlerInfo.getFileExtensions()), expected);
  // Test empty extensions, also at begin and end.
  handlerInfo.setFileExtensions(",a,,b,A,c,");
  Assert.deepEqual(Array.from(handlerInfo.getFileExtensions()), expected);
});

/**
 * Tests that "store" deletes properties that have their default values from
 * the data store.
 *
 * File extensions are never deleted once they have been associated.
 */
add_task(async function test_store_deletes_properties_except_extensions() {
  await deleteHandlerStore();

  // Prepare an nsIHandlerInfo instance with all the properties set to values
  // that will result in deletions. The preferredAction is also set to a defined
  // value so we can more easily verify it later.
  let handlerInfo = HandlerServiceTestUtils.getBlankHandlerInfo(
    "example/type.savetodisk"
  );
  handlerInfo.preferredAction = Ci.nsIHandlerInfo.saveToDisk;
  handlerInfo.alwaysAskBeforeHandling = false;

  // All the properties for "example/type.savetodisk" are present in the test
  // data, so we load the data before overwriting their values.
  await copyTestDataToHandlerStore();
  gHandlerService.store(handlerInfo);

  // Now we can reload the data and verify that no extra values have been kept.
  await unloadHandlerStore();
  let actualHandlerInfo = HandlerServiceTestUtils.getHandlerInfo(
    "example/type.savetodisk"
  );
  HandlerServiceTestUtils.assertHandlerInfoMatches(actualHandlerInfo, {
    type: "example/type.savetodisk",
    preferredAction: Ci.nsIHandlerInfo.saveToDisk,
    alwaysAskBeforeHandling: false,
    fileExtensions: ["example_two", "example_three"],
  });
});

/**
 * Tests the "overrideType" argument of "fillHandlerInfo".
 */
add_task(async function test_fillHandlerInfo_overrideType() {
  // Test both MIME types and protocols.
  for (let type of [
    "example/type.usesystemdefault",
    "examplescheme.usesystemdefault",
  ]) {
    await deleteHandlerStore();

    // Create new nsIHandlerInfo instances before loading the test data.
    let handlerInfoAbsent = HandlerServiceTestUtils.getHandlerInfo(type + "2");

    // Fill the nsIHandlerInfo instance using the type that actually exists.
    await copyTestDataToHandlerStore();
    gHandlerService.fillHandlerInfo(handlerInfoAbsent, type);
    HandlerServiceTestUtils.assertHandlerInfoMatches(handlerInfoAbsent, {
      // While the data is populated from another type, the type is unchanged.
      type: type + "2",
      preferredAction: Ci.nsIHandlerInfo.useSystemDefault,
      alwaysAskBeforeHandling: false,
      possibleApplicationHandlers: [
        {
          name: "Example Possible Handler",
          uriTemplate: "http://www.example.com/?url=%s",
        },
      ],
    });
  }
});

/**
 * Tests "getTypeFromExtension" including unknown extensions.
 */
add_task(async function test_getTypeFromExtension() {
  await copyTestDataToHandlerStore();

  Assert.equal(gHandlerService.getTypeFromExtension(""), "");
  Assert.equal(gHandlerService.getTypeFromExtension("example_unknown"), "");
  Assert.equal(
    gHandlerService.getTypeFromExtension("example_one"),
    "example/type.handleinternally"
  );
  Assert.equal(
    gHandlerService.getTypeFromExtension("EXAMPLE_one"),
    "example/type.handleinternally"
  );
});

/**
 * Checks that the information stored in the handler service instance under
 * testing matches the default handlers for the English locale.
 */
function assertAllHandlerInfosMatchDefaultHandlers() {
  let handlerInfos = HandlerServiceTestUtils.getAllHandlerInfos();

  for (let type of ["irc", "ircs"]) {
    HandlerServiceTestUtils.assertHandlerInfoMatches(handlerInfos.shift(), {
      type,
      preferredActionOSDependent: true,
      possibleApplicationHandlers: [
        {
          name: "Mibbit",
          uriTemplate: "https://www.mibbit.com/?url=%s",
        },
      ],
    });
  }

  HandlerServiceTestUtils.assertHandlerInfoMatches(handlerInfos.shift(), {
    type: "mailto",
    preferredActionOSDependent: true,
    possibleApplicationHandlers: [
      {
        name: "Yahoo! Mail",
        uriTemplate: "https://compose.mail.yahoo.com/?To=%s",
      },
      {
        name: "Gmail",
        uriTemplate: "https://mail.google.com/mail/?extsrc=mailto&url=%s",
      },
    ],
  });

  Assert.equal(handlerInfos.length, 0);
}

/**
 * Tests the default protocol handlers imported from the locale-specific data.
 */
add_task(async function test_default_protocol_handlers() {
  if (
    !Services.prefs.getPrefType("gecko.handlerService.defaultHandlersVersion")
  ) {
    info("This platform or locale does not have default handlers.");
    return;
  }

  // This will inject the default protocol handlers for the current locale.
  await deleteHandlerStore();

  await assertAllHandlerInfosMatchDefaultHandlers();
});

/**
 * Tests that the default protocol handlers are not imported again from the
 * locale-specific data if they already exist.
 */
add_task(async function test_default_protocol_handlers_no_duplicates() {
  if (
    !Services.prefs.getPrefType("gecko.handlerService.defaultHandlersVersion")
  ) {
    info("This platform or locale does not have default handlers.");
    return;
  }

  // This will inject the default protocol handlers for the current locale.
  await deleteHandlerStore();

  // Remove the "irc" handler so we can verify that the injection is repeated.
  let ircHandlerInfo = HandlerServiceTestUtils.getHandlerInfo("irc");
  gHandlerService.remove(ircHandlerInfo);

  let originalDefaultHandlersVersion = Services.prefs.getComplexValue(
    "gecko.handlerService.defaultHandlersVersion",
    Ci.nsIPrefLocalizedString
  );

  // Set the preference to an arbitrarily high number to force injecting again.
  Services.prefs.setStringPref(
    "gecko.handlerService.defaultHandlersVersion",
    "999"
  );

  await unloadHandlerStore();

  // Check that "irc" exists to make sure that the injection was repeated.
  Assert.ok(gHandlerService.exists(ircHandlerInfo));

  // There should be no duplicate handlers in the protocols.
  await assertAllHandlerInfosMatchDefaultHandlers();

  Services.prefs.setStringPref(
    "gecko.handlerService.defaultHandlersVersion",
    originalDefaultHandlersVersion
  );
});

/**
 * Ensures forward compatibility by checking that the "store" method preserves
 * unknown properties in the test data.
 */
add_task(async function test_store_keeps_unknown_properties() {
  // Create a new nsIHandlerInfo instance before loading the test data.
  await deleteHandlerStore();
  let handlerInfo = HandlerServiceTestUtils.getHandlerInfo(
    "example/type.handleinternally"
  );

  await copyTestDataToHandlerStore();
  gHandlerService.store(handlerInfo);

  await unloadHandlerStore();
  let data = JSON.parse(new TextDecoder().decode(await OS.File.read(jsonPath)));
  Assert.equal(
    data.mimeTypes["example/type.handleinternally"].unknownProperty,
    "preserved"
  );
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
 * Test saving and reloading an instance of nsIGIOMimeApp.
 */
add_task(async function test_store_gioHandlerApp() {
  if (!("@mozilla.org/gio-service;1" in Cc)) {
    info("Skipping test because it does not apply to this platform.");
    return;
  }

  // Create dummy exec file that following won't fail because file not found error
  let dummyHandlerFile = FileUtils.getFile("TmpD", ["dummyHandler"]);
  dummyHandlerFile.createUnique(
    Ci.nsIFile.NORMAL_FILE_TYPE,
    parseInt("777", 8)
  );

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
