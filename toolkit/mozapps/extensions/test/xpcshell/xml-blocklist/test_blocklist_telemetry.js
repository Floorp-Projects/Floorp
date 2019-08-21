/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Blocklist } = ChromeUtils.import(
  "resource://gre/modules/Blocklist.jsm"
);
const { TelemetryController } = ChromeUtils.import(
  "resource://gre/modules/TelemetryController.jsm"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const PREF_BLOCKLIST_URL = "extensions.blocklist.url";

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "49"
);

const serverTime = new Date();
const lastUpdateTimestamp = serverTime.getTime() - 2000;
const lastUpdateUTCString = new Date(lastUpdateTimestamp).toUTCString();

let blocklistHandler;

const server = AddonTestUtils.createHttpServer({
  hosts: ["example.com"],
});

server.registerPathHandler("/blocklist.xml", (request, response) => {
  if (blocklistHandler) {
    const handler = blocklistHandler;
    blocklistHandler = null;
    handler(request, response);
  } else {
    response.setStatusLine(
      request.httpVersion,
      501,
      "test blocklist handler undefined"
    );
    response.write("");
  }
});

function promiseConsoleMessage(regexp) {
  return new Promise(resolve => {
    const listener = entry => {
      if (regexp.test(entry.message)) {
        Services.console.unregisterListener(listener);
        resolve(entry);
      }
    };
    Services.console.registerListener(listener);
  });
}

async function promiseBlocklistUpdateWithError({
  serverBlocklistHandler,
  expectedConsoleMessage,
}) {
  let onceConsoleMessageLogged = promiseConsoleMessage(expectedConsoleMessage);
  blocklistHandler = serverBlocklistHandler;
  Blocklist.notify();
  await onceConsoleMessageLogged;
}

async function promiseBlocklistUpdateSuccessful({ serverBlocklistHandler }) {
  const promiseBlocklistLoaded = TestUtils.topicObserved(
    "addon-blocklist-updated"
  );
  blocklistHandler = serverBlocklistHandler;
  Blocklist.notify();
  await promiseBlocklistLoaded;
}

function createBlocklistHandler({ lastupdate }) {
  return (request, response) => {
    response.setStatusLine(
      request.httpVersion,
      200,
      "test successfull blocklist update"
    );
    response.setHeader("Last-Modified", serverTime.toUTCString());
    response.write(`<?xml version='1.0' encoding='UTF-8'?>
      <blocklist xmlns="http://www.mozilla.org/2006/addons-blocklist"
                 lastupdate="${lastupdate}">
      </blocklist>
    `);
  };
}

async function assertTelemetryOnBlocklistUpdateError({
  serverBlocklistHandler,
  expectedConsoleMessage,
  expectedTelemetryEvents,
}) {
  blocklistHandler = serverBlocklistHandler;
  let onceConsoleMessageLogged = promiseConsoleMessage(expectedConsoleMessage);
  await Blocklist.notify();
  await onceConsoleMessageLogged;

  TelemetryTestUtils.assertEvents(expectedTelemetryEvents, {
    category: "addonsManager",
    method: "blocklistUpdateError",
  });
}

add_task(async function test_setup() {
  Services.prefs.setCharPref(
    PREF_BLOCKLIST_URL,
    "http://example.com/blocklist.xml"
  );

  // Ensure the telemetry event category is enabled.
  await AddonTestUtils.promiseStartupManager();
  // Ensure that the telemetry event definition is loaded.
  await TelemetryController.testSetup();
});

add_task(async function test_lastModifed_xml_on_blocklist_updates() {
  Services.telemetry.clearScalars();

  info(
    "Verify lastModified_xml scalar is updated after a successfull blocklist update"
  );

  await promiseBlocklistUpdateSuccessful({
    serverBlocklistHandler: createBlocklistHandler({
      lastupdate: lastUpdateTimestamp,
    }),
  });

  let scalars = TelemetryTestUtils.getProcessScalars("parent");
  equal(
    scalars["blocklist.lastModified_xml"],
    lastUpdateUTCString,
    "Got the expected value set on the telemetry scalar"
  );

  info(
    "Verify lastModified_xml scalar is updated after a blocklist update failure"
  );

  await promiseBlocklistUpdateWithError({
    serverBlocklistHandler(request, response) {
      response.setStatusLine(
        request.httpVersion,
        501,
        "test failed blocklist update"
      );
      response.write("");
    },
    expectedConsoleMessage: /^Blocklist::onXMLLoad: there was an error/,
  });

  scalars = TelemetryTestUtils.getProcessScalars("parent");
  equal(
    scalars["blocklist.lastModified_xml"],
    lastUpdateUTCString,
    "Expect the telemetry scalar to be unchanged after a failed blocklist update"
  );
});

add_task(async function test_lastModifed_xml_on_blocklist_loaded_from_disk() {
  Services.telemetry.clearScalars();

  const { BlocklistXML } = ChromeUtils.import(
    "resource://gre/modules/Blocklist.jsm",
    null
  );

  // Force the blocklist to be read from file again.
  BlocklistXML._clear();
  await BlocklistXML.loadBlocklistAsync();

  let scalars = TelemetryTestUtils.getProcessScalars("parent");
  equal(
    scalars["blocklist.lastModified_xml"],
    lastUpdateUTCString,
    "Got the expect the telemetry scalar value"
  );
});

add_task(async function test_lastModified_xml_invalid_lastupdate_attribute() {
  Services.telemetry.clearScalars();

  await promiseBlocklistUpdateSuccessful({
    serverBlocklistHandler: createBlocklistHandler({
      lastupdate: "not a timestamp",
    }),
  });

  let scalars = TelemetryTestUtils.getProcessScalars("parent");
  equal(
    scalars["blocklist.lastModified_xml"],
    "Invalid Date",
    "Expect the telemetry scalar to be unchanged"
  );
});

add_task(async function test_lastModified_xml_on_empty_lastupdate_attribute() {
  Services.telemetry.clearScalars();

  await promiseBlocklistUpdateSuccessful({
    serverBlocklistHandler: createBlocklistHandler({
      lastupdate: "",
    }),
  });

  let scalars = TelemetryTestUtils.getProcessScalars("parent");
  equal(
    scalars["blocklist.lastModified_xml"],
    "Missing Date",
    "Expect the telemetry scalar to be unchanged"
  );
});

add_task(async function test_blocklist_telemetry_event() {
  Services.telemetry.clearEvents();

  info("Verify telemetry event recorded on unexpected HTTP status code");
  await assertTelemetryOnBlocklistUpdateError({
    serverBlocklistHandler: (request, response) => {
      response.setStatusLine(
        request.httpVersion,
        501,
        "unexpected status code"
      );
      response.write("");
    },
    expectedConsoleMessage: /^Blocklist::onXMLLoad: there was an error/,
    expectedTelemetryEvents: [
      {
        category: "addonsManager",
        method: "blocklistUpdateError",
        object: "xml",
        value: "UNEXPECTED_STATUS_CODE",
        extra: { status_code: "501" },
      },
    ],
  });

  info("Verify telemetry event recorded on XML parse error");
  await assertTelemetryOnBlocklistUpdateError({
    serverBlocklistHandler: (request, response) => {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.write("");
    },
    expectedConsoleMessage: /^Blocklist::onXMLLoad: there was an error during load, we got invalid XML/,
    expectedTelemetryEvents: [
      {
        category: "addonsManager",
        method: "blocklistUpdateError",
        object: "xml",
        value: "INVALID_XML_ERROR",
      },
    ],
  });

  info("Verify telemetry event recorded on XHR load error");
  Services.prefs.setCharPref(
    PREF_BLOCKLIST_URL,
    // Use the wrong schema to trigger a XHR load error.
    "https://example.com/blocklist.xml"
  );
  await assertTelemetryOnBlocklistUpdateError({
    expectedConsoleMessage: /^Blocklist:onError/,
    expectedTelemetryEvents: [
      {
        category: "addonsManager",
        method: "blocklistUpdateError",
        object: "xml",
        value: "DOWNLOAD_ERROR",
        extra: { status_code: "0" },
      },
    ],
  });

  info(
    "Verify telemetry event recorded on errors creating the blocklist server url error"
  );
  Services.prefs.setCharPref(PREF_BLOCKLIST_URL, "an invalid url");
  await assertTelemetryOnBlocklistUpdateError({
    expectedConsoleMessage: /^Blocklist::notify: There was an error creating the blocklist URI/,
    expectedTelemetryEvents: [
      {
        category: "addonsManager",
        method: "blocklistUpdateError",
        object: "xml",
        value: "INVALID_BLOCKLIST_URL",
      },
    ],
  });

  info(
    "Verify telemetry event recorded on missing blocklist server url preference"
  );

  // Replace the BlocklistXML._getBlocklistServerURL method to trigger a
  // "missing blocklist url pref" scenario (clearing the pref is not enough because
  // the url from the default pref value just takes its place).
  const { BlocklistXML } = ChromeUtils.import(
    "resource://gre/modules/Blocklist.jsm",
    null
  );
  const _getBlocklistServerURL = BlocklistXML._getBlocklistServerURL;
  BlocklistXML._getBlocklistServerURL = () => {
    BlocklistXML._getBlocklistServerURL = _getBlocklistServerURL;
    throw new Error("Fake missing blocklist url pref");
  };

  await assertTelemetryOnBlocklistUpdateError({
    expectedConsoleMessage: /^Blocklist::notify: The extensions.blocklist.url preference is missing!/,
    expectedTelemetryEvents: [
      {
        category: "addonsManager",
        method: "blocklistUpdateError",
        object: "xml",
        value: "MISSING_BLOCKLIST_SERVER_URL",
      },
    ],
  });
});
