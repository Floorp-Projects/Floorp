"use strict";

/* globals browser */

Cu.import("resource://gre/modules/LegacyExtensionsUtils.jsm");

// Import EmbeddedExtensionManager to be able to check that the
// tacked instances are cleared after the embedded extension shutdown.
const {
  EmbeddedExtensionManager,
} = Cu.import("resource://gre/modules/LegacyExtensionsUtils.jsm", {});

/**
 * This test case ensures that the LegacyExtensionsUtils.EmbeddedExtension:
 *  - load the embedded webextension resources from a "/webextension/" dir
 *    inside the XPI.
 *  - EmbeddedExtension.prototype.api returns an API object which exposes
 *    a working `runtime.onConnect` event object (e.g. the API can receive a port
 *    when the embedded webextension is started  and it can exchange messages
 *    with the background page).
 *  - EmbeddedExtension.prototype.startup/shutdown methods manage the embedded
 *    webextension lifecycle as expected.
 */
add_task(function* test_embedded_webextension_utils() {
  function backgroundScript() {
    let port = browser.runtime.connect();

    port.onMessage.addListener((msg) => {
      if (msg == "legacy_extension -> webextension") {
        port.postMessage("webextension -> legacy_extension");
        port.disconnect();
      }
    });
  }

  const id = "@test.embedded.web.extension";

  // Extensions.generateXPI is used here (and in the other hybrid addons tests in this same
  // test dir) to be able to generate an xpi with the directory layout that we expect from
  // an hybrid legacy+webextension addon (where all the embedded webextension resources are
  // loaded from a 'webextension/' directory).
  let fakeHybridAddonFile = Extension.generateZipFile({
    "webextension/manifest.json": {
      applications: {gecko: {id}},
      name: "embedded webextension name",
      manifest_version: 2,
      version: "1.0",
      background: {
        scripts: ["bg.js"],
      },
    },
    "webextension/bg.js": `new ${backgroundScript}`,
  });

  // Remove the generated xpi file and flush the its jar cache
  // on cleanup.
  do_register_cleanup(() => {
    Services.obs.notifyObservers(fakeHybridAddonFile, "flush-cache-entry", null);
    fakeHybridAddonFile.remove(false);
  });

  let fileURI = Services.io.newFileURI(fakeHybridAddonFile);
  let resourceURI = Services.io.newURI(`jar:${fileURI.spec}!/`);

  let embeddedExtension = LegacyExtensionsUtils.getEmbeddedExtensionFor({
    id, resourceURI,
  });

  ok(embeddedExtension, "Got the embeddedExtension object");

  equal(EmbeddedExtensionManager.embeddedExtensionsByAddonId.size, 1,
        "Got the expected number of tracked embedded extension instances");

  do_print("waiting embeddedExtension.startup");
  let embeddedExtensionAPI = yield embeddedExtension.startup();
  ok(embeddedExtensionAPI, "Got the embeddedExtensionAPI object");

  let waitConnectPort = new Promise(resolve => {
    let {browser} = embeddedExtensionAPI;
    browser.runtime.onConnect.addListener(port => {
      resolve(port);
    });
  });

  let port = yield waitConnectPort;

  ok(port, "Got the Port API object");

  let waitPortMessage = new Promise(resolve => {
    port.onMessage.addListener((msg) => {
      resolve(msg);
    });
  });

  port.postMessage("legacy_extension -> webextension");

  let msg = yield waitPortMessage;

  equal(msg, "webextension -> legacy_extension",
     "LegacyExtensionContext received the expected message from the webextension");

  let waitForDisconnect = new Promise(resolve => {
    port.onDisconnect.addListener(resolve);
  });

  do_print("Wait for the disconnect port event");
  yield waitForDisconnect;
  do_print("Got the disconnect port event");

  yield embeddedExtension.shutdown();

  equal(EmbeddedExtensionManager.embeddedExtensionsByAddonId.size, 0,
        "EmbeddedExtension instances has been untracked from the EmbeddedExtensionManager");
});

function* createManifestErrorTestCase(id, xpi, expectedError) {
  // Remove the generated xpi file and flush the its jar cache
  // on cleanup.
  do_register_cleanup(() => {
    Services.obs.notifyObservers(xpi, "flush-cache-entry", null);
    xpi.remove(false);
  });

  let fileURI = Services.io.newFileURI(xpi);
  let resourceURI = Services.io.newURI(`jar:${fileURI.spec}!/`);

  let embeddedExtension = LegacyExtensionsUtils.getEmbeddedExtensionFor({
    id, resourceURI,
  });

  yield Assert.rejects(embeddedExtension.startup(), expectedError,
                       "embedded extension startup rejected");

  // Shutdown a "never-started" addon with an embedded webextension should not
  // raise any exception, and if it does this test will fail.
  yield embeddedExtension.shutdown();
}

add_task(function* test_startup_error_empty_manifest() {
  const id = "empty-manifest@test.embedded.web.extension";
  const files = {
    "webextension/manifest.json": ``,
  };
  const expectedError = "(NS_BASE_STREAM_CLOSED)";

  let fakeHybridAddonFile = Extension.generateZipFile(files);

  yield createManifestErrorTestCase(id, fakeHybridAddonFile, expectedError);
});

add_task(function* test_startup_error_invalid_json_manifest() {
  const id = "invalid-json-manifest@test.embedded.web.extension";
  const files = {
    "webextension/manifest.json": `{ "name": }`,
  };
  const expectedError = "JSON.parse:";

  let fakeHybridAddonFile = Extension.generateZipFile(files);

  yield createManifestErrorTestCase(id, fakeHybridAddonFile, expectedError);
});

add_task(function* test_startup_error_blocking_validation_errors() {
  const id = "blocking-manifest-validation-error@test.embedded.web.extension";
  const files = {
    "webextension/manifest.json": {
      name: "embedded webextension name",
      manifest_version: 2,
      version: "1.0",
      background: {
        scripts: {},
      },
    },
  };

  function expectedError(actual) {
    if (actual.errors && actual.errors.length == 1 &&
        actual.errors[0].startsWith("Reading manifest:")) {
      return true;
    }

    return false;
  }

  let fakeHybridAddonFile = Extension.generateZipFile(files);

  yield createManifestErrorTestCase(id, fakeHybridAddonFile, expectedError);
});
