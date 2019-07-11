// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Tests that CRLite filter downloading works correctly.

"use strict";
do_get_profile(); // must be called before getting nsIX509CertDB

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
const { RemoteSecuritySettings } = ChromeUtils.import(
  "resource://gre/modules/psm/RemoteSecuritySettings.jsm"
);
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

const { CRLiteFiltersClient } = RemoteSecuritySettings.init();

const CRLITE_FILTERS_ENABLED_PREF =
  "security.remote_settings.crlite_filters.enabled";

function getHashCommon(aStr, useBase64) {
  let hasher = Cc["@mozilla.org/security/hash;1"].createInstance(
    Ci.nsICryptoHash
  );
  hasher.init(Ci.nsICryptoHash.SHA256);
  let stringStream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
    Ci.nsIStringInputStream
  );
  stringStream.data = aStr;
  hasher.updateFromStream(stringStream, -1);

  return hasher.finish(useBase64);
}

// Get a hexified SHA-256 hash of the given string.
function getHash(aStr) {
  return hexify(getHashCommon(aStr, false));
}

/**
 * Simulate a Remote Settings synchronization by filling up the
 * local data with fake records.
 *
 * @param {*} filters List of filters for which we will create
 *                    records.
 */
async function syncAndDownload(filters) {
  const localDB = await CRLiteFiltersClient.client.openCollection();
  await localDB.clear();

  for (let filter of filters) {
    const file = do_get_file("test_crlite_filters.js");
    const fileBytes = readFile(file);

    const record = {
      details: {
        name: `${filter.timestamp}-${filter.type}`,
      },
      attachment: {
        hash: getHash(fileBytes),
        size: fileBytes.length,
        // This test ensures we're downloading the most recent full filter and any subsequent
        // incremental filters, so the actual contents doesn't matter right now.
        filename: "totally_a_crlite_filter",
        location:
          "security-state-workspace/cert-revocations/test_crlite_filters.js",
        mimetype: "application/octet-stream",
      },
      incremental: filter.type == "diff",
    };

    await localDB.create(record);
  }
  // This promise will wait for the end of downloading.
  let promise = TestUtils.topicObserved(
    "remote-security-settings:crlite-filters-downloaded"
  );
  // Simulate polling for changes, trigger the download of attachments.
  Services.obs.notifyObservers(null, "remote-settings:changes-poll-end");
  let results = await promise;
  return results[1]; // topicObserved gives back a 2-array
}

add_task(
  {
    skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
  },
  async function test_crlite_filters_disabled() {
    Services.prefs.setBoolPref(CRLITE_FILTERS_ENABLED_PREF, false);

    let result = await syncAndDownload([
      { timestamp: "2019-01-01T00:00:00Z", type: "full" },
    ]);
    equal(result, "disabled", "CRLite filter download should not have run");
  }
);

add_task(
  {
    skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
  },
  async function test_crlite_no_filters() {
    Services.prefs.setBoolPref(CRLITE_FILTERS_ENABLED_PREF, true);

    let result = await syncAndDownload([]);
    equal(
      result,
      "unavailable",
      "CRLite filter download should have run, but nothing was available"
    );
  }
);

add_task(
  {
    skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
  },
  async function test_crlite_only_incremental_filters() {
    Services.prefs.setBoolPref(CRLITE_FILTERS_ENABLED_PREF, true);

    let result = await syncAndDownload([
      { timestamp: "2019-01-01T06:00:00Z", type: "diff" },
      { timestamp: "2019-01-01T18:00:00Z", type: "diff" },
      { timestamp: "2019-01-01T12:00:00Z", type: "diff" },
    ]);
    equal(
      result,
      "unavailable",
      "CRLite filter download should have run, but no full filters were available"
    );
  }
);

add_task(
  {
    skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
  },
  async function test_crlite_filters_basic() {
    Services.prefs.setBoolPref(CRLITE_FILTERS_ENABLED_PREF, true);

    let result = await syncAndDownload([
      { timestamp: "2019-01-01T00:00:00Z", type: "full" },
    ]);
    equal(
      result,
      "finished;2019-01-01T00:00:00Z-full",
      "CRLite filter download should have run"
    );
  }
);

add_task(
  {
    skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
  },
  async function test_crlite_filters_full_and_incremental() {
    Services.prefs.setBoolPref(CRLITE_FILTERS_ENABLED_PREF, true);

    let result = await syncAndDownload([
      // These are deliberately listed out of order.
      { timestamp: "2019-01-01T06:00:00Z", type: "diff" },
      { timestamp: "2019-01-01T00:00:00Z", type: "full" },
      { timestamp: "2019-01-01T18:00:00Z", type: "diff" },
      { timestamp: "2019-01-01T12:00:00Z", type: "diff" },
    ]);
    let [status, filters] = result.split(";");
    equal(status, "finished", "CRLite filter download should have run");
    let filtersSplit = filters.split(",");
    deepEqual(
      filtersSplit,
      [
        "2019-01-01T00:00:00Z-full",
        "2019-01-01T06:00:00Z-diff",
        "2019-01-01T12:00:00Z-diff",
        "2019-01-01T18:00:00Z-diff",
      ],
      "Should have downloaded the expected CRLite filters"
    );
  }
);

add_task(
  {
    skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
  },
  async function test_crlite_filters_multiple_days() {
    Services.prefs.setBoolPref(CRLITE_FILTERS_ENABLED_PREF, true);

    let result = await syncAndDownload([
      // These are deliberately listed out of order.
      { timestamp: "2019-01-02T06:00:00Z", type: "diff" },
      { timestamp: "2019-01-03T12:00:00Z", type: "diff" },
      { timestamp: "2019-01-02T12:00:00Z", type: "diff" },
      { timestamp: "2019-01-03T18:00:00Z", type: "diff" },
      { timestamp: "2019-01-02T18:00:00Z", type: "diff" },
      { timestamp: "2019-01-02T00:00:00Z", type: "full" },
      { timestamp: "2019-01-03T00:00:00Z", type: "full" },
      { timestamp: "2019-01-01T06:00:00Z", type: "diff" },
      { timestamp: "2019-01-01T18:00:00Z", type: "diff" },
      { timestamp: "2019-01-01T12:00:00Z", type: "diff" },
      { timestamp: "2019-01-01T00:00:00Z", type: "full" },
      { timestamp: "2019-01-03T06:00:00Z", type: "diff" },
    ]);
    let [status, filters] = result.split(";");
    equal(status, "finished", "CRLite filter download should have run");
    let filtersSplit = filters.split(",");
    deepEqual(
      filtersSplit,
      [
        "2019-01-03T00:00:00Z-full",
        "2019-01-03T06:00:00Z-diff",
        "2019-01-03T12:00:00Z-diff",
        "2019-01-03T18:00:00Z-diff",
      ],
      "Should have downloaded the expected CRLite filters"
    );
  }
);

let server;

function run_test() {
  server = new HttpServer();
  server.start(-1);
  registerCleanupFunction(() => server.stop(() => {}));

  server.registerDirectory(
    "/cdn/security-state-workspace/cert-revocations/",
    do_get_file(".")
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

  Services.prefs.setCharPref("browser.policies.loglevel", "debug");

  run_next_test();
}
