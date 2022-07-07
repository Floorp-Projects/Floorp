/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* Unit tests for the nsIUrlClassifierRemoteSettingsService implementation. */

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
const { SBRS_UPDATE_MINIMUM_DELAY } = ChromeUtils.import(
  "resource://gre/modules/UrlClassifierRemoteSettingsService.jsm"
);

const COLLECTION_NAME = "tracking-protection-lists";

const REMOTE_SETTINGS_DATA = [
  {
    Name: "content-fingerprinting-track-digest256",
    attachment: {
      hash: "96a4a850a1a475001148fa8a3a5efea58951f7176d3624ad7614fbf32732ee48",
      size: 948,
      filename: "content-fingerprinting-track-digest256",
      location:
        "main-workspace/tracking-protection-lists/content-fingerprinting-track-digest256",
      mimetype: "text/plain",
    },
    id: "content-fingerprinting-track-digest256",
    Version: 1597417364,
  },
  {
    Name: "mozplugin-block-digest256",
    attachment: {
      hash: "dd2b800c7e4bad17e1c79f3e530c0b94e0a039adf4566f30bc3c285a547fa4fc",
      size: 3029,
      filename: "mozplugin-block-digest256",
      location:
        "main-workspace/tracking-protection-lists/mozplugin-block-digest256",
      mimetype: "text/plain",
    },
    id: "mozplugin-block-digest256",
    Version: 1575583456,
  },
  // Entry with non-exist attachment
  {
    Name: "social-track-digest256",
    attachment: {
      location: "main-workspace/tracking-protection-lists/not-exist",
    },
    id: "social-track-digest256",
    Version: 1111111111,
  },
  // Entry with corrupted attachment
  {
    Name: "analytic-track-digest256",
    attachment: {
      hash: "644a0662bcf7313570ee68490e3805f5cc7a0503c097f040525c28dc5bfe4c97",
      size: 58,
      filename: "invalid.chunk",
      location: "main-workspace/tracking-protection-lists/invalid.chunk",
      mimetype: "text/plain",
    },
    id: "analytic-track-digest256",
    Version: 1111111111,
  },
];

let gListService = Cc["@mozilla.org/url-classifier/list-service;1"].getService(
  Ci.nsIUrlClassifierRemoteSettingsService
);
let gDbService = Cc["@mozilla.org/url-classifier/dbservice;1"].getService(
  Ci.nsIUrlClassifierDBService
);

class UpdateEvent extends EventTarget {}
function waitForEvent(element, eventName) {
  return new Promise(function(resolve) {
    element.addEventListener(eventName, e => resolve(e.detail), { once: true });
  });
}

function buildPayload(tables) {
  let payload = ``;
  for (let table of tables) {
    payload += table[0];
    if (table[1] != null) {
      payload += `;a:${table[1]}`;
    }
    payload += `\n`;
  }
  return payload;
}

let server;
add_task(async function init() {
  Services.prefs.setCharPref(
    "browser.safebrowsing.provider.mozilla.updateURL",
    `moz-sbrs://tracking-protection-list`
  );
  // Setup HTTP server for remote setting
  server = new HttpServer();
  server.start(-1);
  registerCleanupFunction(() => server.stop(() => {}));

  server.registerDirectory(
    "/cdn/main-workspace/tracking-protection-lists/",
    do_get_file("data")
  );

  server.registerPathHandler("/v1/", (request, response) => {
    response.write(
      JSON.stringify({
        capabilities: {
          attachments: {
            base_url: `http://localhost:${server.identity.primaryPort}/cdn/`,
          },
        },
      })
    );
    response.setHeader("Content-Type", "application/json; charset=UTF-8");
    response.setStatusLine(null, 200, "OK");
  });

  Services.prefs.setCharPref(
    "services.settings.server",
    `http://localhost:${server.identity.primaryPort}/v1`
  );

  // Setup remote setting initial data
  let db = await RemoteSettings(COLLECTION_NAME).db;
  await db.importChanges({}, 42, REMOTE_SETTINGS_DATA);

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(
      "browser.safebrowsing.provider.mozilla.updateURL"
    );
    Services.prefs.clearUserPref("services.settings.server");
  });
});

// Test updates from RemoteSettings when there is no local data
add_task(async function test_empty_update() {
  let updateEvent = new UpdateEvent();
  let promise = waitForEvent(updateEvent, "update");

  const TEST_TABLES = [
    ["mozplugin-block-digest256", null], // empty
    ["content-fingerprinting-track-digest256", null], // empty
  ];

  gListService.fetchList(buildPayload(TEST_TABLES), {
    // nsIStreamListener observer
    onStartRequest(request) {},
    onDataAvailable(aRequest, aStream, aOffset, aCount) {
      let stream = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
        Ci.nsIScriptableInputStream
      );
      stream.init(aStream);
      let event = new CustomEvent("update", {
        detail: stream.readBytes(aCount),
      });
      updateEvent.dispatchEvent(event);
    },
    onStopRequest(request, status) {},
  });

  let expected = "n:" + SBRS_UPDATE_MINIMUM_DELAY + "\n";
  for (const table of TEST_TABLES) {
    expected += `i:${table[0]}\n` + readFileToString(`data/${table[0]}`);
  }

  Assert.equal(
    await promise,
    expected,
    "Receive expected data from onDataAvailable"
  );
  gListService.clear();
});

// Test updates from RemoteSettings when we have an empty table,
// a table with an older version, and a table which is up-to-date.
add_task(async function test_update() {
  let updateEvent = new UpdateEvent();
  let promise = waitForEvent(updateEvent, "update");

  const TEST_TABLES = [
    ["mozplugin-block-digest256", 1575583456], // up-to-date
    ["content-fingerprinting-track-digest256", 1575583456 - 1], // older version
  ];

  gListService.fetchList(buildPayload(TEST_TABLES), {
    // observer
    // nsIStreamListener observer
    onStartRequest(request) {},
    onDataAvailable(aRequest, aStream, aOffset, aCount) {
      let stream = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
        Ci.nsIScriptableInputStream
      );
      stream.init(aStream);
      let event = new CustomEvent("update", {
        detail: stream.readBytes(aCount),
      });
      updateEvent.dispatchEvent(event);
    },
    onStopRequest(request, status) {},
  });

  // Build request with no version
  let expected = "n:" + SBRS_UPDATE_MINIMUM_DELAY + "\n";
  for (const table of TEST_TABLES) {
    if (["content-fingerprinting-track-digest256"].includes(table[0])) {
      expected += `i:${table[0]}\n` + readFileToString(`data/${table[0]}`);
    }
  }

  Assert.equal(
    await promise,
    expected,
    "Receive expected data from onDataAvailable"
  );
  gListService.clear();
});

// Test updates from RemoteSettings service when all tables are up-to-date.
add_task(async function test_no_update() {
  let updateEvent = new UpdateEvent();
  let promise = waitForEvent(updateEvent, "update");

  const TEST_TABLES = [
    ["mozplugin-block-digest256", 1575583456], // up-to-date
    ["content-fingerprinting-track-digest256", 1597417364], // up-to-date
  ];

  gListService.fetchList(buildPayload(TEST_TABLES), {
    // nsIStreamListener observer
    onStartRequest(request) {},
    onDataAvailable(aRequest, aStream, aOffset, aCount) {
      let stream = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
        Ci.nsIScriptableInputStream
      );
      stream.init(aStream);
      let event = new CustomEvent("update", {
        detail: stream.readBytes(aCount),
      });
      updateEvent.dispatchEvent(event);
    },
    onStopRequest(request, status) {},
  });

  // No data is expected
  let expected = "n:" + SBRS_UPDATE_MINIMUM_DELAY + "\n";

  Assert.equal(
    await promise,
    expected,
    "Receive expected data from onDataAvailable"
  );
  gListService.clear();
});

add_test(function test_update() {
  let streamUpdater = Cc[
    "@mozilla.org/url-classifier/streamupdater;1"
  ].getService(Ci.nsIUrlClassifierStreamUpdater);

  // Download some updates, and don't continue until the downloads are done.
  function updateSuccess(aEvent) {
    Assert.equal(SBRS_UPDATE_MINIMUM_DELAY, aEvent);
    info("All data processed");
    run_next_test();
  }
  // Just throw if we ever get an update or download error.
  function handleError(aEvent) {
    do_throw("We didn't download or update correctly: " + aEvent);
  }

  streamUpdater.downloadUpdates(
    "content-fingerprinting-track-digest256",
    "content-fingerprinting-track-digest256;\n",
    true,
    "moz-sbrs://remote-setting",
    updateSuccess,
    handleError,
    handleError
  );
});

add_test(function test_url_not_denylisted() {
  let uri = Services.io.newURI("http://example.com");
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );
  gDbService.lookup(
    principal,
    "content-fingerprinting-track-digest256",
    function handleEvent(aEvent) {
      // This URI is not on any lists.
      Assert.equal("", aEvent);
      run_next_test();
    }
  );
});

add_test(function test_url_denylisted() {
  let uri = Services.io.newURI("https://www.foresee.com");
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );
  gDbService.lookup(
    principal,
    "content-fingerprinting-track-digest256",
    function handleEvent(aEvent) {
      Assert.equal("content-fingerprinting-track-digest256", aEvent);
      run_next_test();
    }
  );
});

add_test(function test_update_download_error() {
  let streamUpdater = Cc[
    "@mozilla.org/url-classifier/streamupdater;1"
  ].getService(Ci.nsIUrlClassifierStreamUpdater);

  // Download some updates, and don't continue until the downloads are done.
  function updateSuccessOrError(aEvent) {
    do_throw("Should be downbload error");
  }
  // Just throw if we ever get an update or download error.
  function downloadError(aEvent) {
    run_next_test();
  }

  streamUpdater.downloadUpdates(
    "social-track-digest256",
    "social-track-digest256;\n",
    true,
    "moz-sbrs://remote-setting",
    updateSuccessOrError,
    updateSuccessOrError,
    downloadError
  );
});

add_test(function test_update_update_error() {
  let streamUpdater = Cc[
    "@mozilla.org/url-classifier/streamupdater;1"
  ].getService(Ci.nsIUrlClassifierStreamUpdater);

  // Download some updates, and don't continue until the downloads are done.
  function updateSuccessOrDownloadError(aEvent) {
    do_throw("Should be update error");
  }
  // Just throw if we ever get an update or download error.
  function updateError(aEvent) {
    run_next_test();
  }

  streamUpdater.downloadUpdates(
    "analytic-track-digest256",
    "analytic-track-digest256;\n",
    true,
    "moz-sbrs://remote-setting",
    updateSuccessOrDownloadError,
    updateError,
    updateSuccessOrDownloadError
  );
});
