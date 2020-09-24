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

const {
  CRLiteFiltersClient,
  IntermediatePreloadsClient,
} = RemoteSecuritySettings.init();

const CRLITE_FILTERS_ENABLED_PREF =
  "security.remote_settings.crlite_filters.enabled";
const INTERMEDIATES_ENABLED_PREF =
  "security.remote_settings.intermediates.enabled";
const INTERMEDIATES_DL_PER_POLL_PREF =
  "security.remote_settings.intermediates.downloads_per_poll";

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
  const localDB = await CRLiteFiltersClient.client.db;
  await localDB.clear();

  for (let filter of filters) {
    const filename =
      "test-filter." + (filter.type == "diff" ? "stash" : "crlite");
    const file = do_get_file(`test_cert_storage_direct/${filename}`);
    const fileBytes = readFile(file);

    const record = {
      details: {
        name: `${filter.timestamp}-${filter.type}`,
      },
      attachment: {
        hash: getHash(fileBytes),
        size: fileBytes.length,
        filename,
        location: `security-state-workspace/cert-revocations/test_cert_storage_direct/${filename}`,
        mimetype: "application/octet-stream",
      },
      incremental: filter.type == "diff",
      effectiveTimestamp: new Date(filter.timestamp).getTime(),
      parent: filter.type == "diff" ? filter.parent : undefined,
      id: filter.id,
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
      { timestamp: "2019-01-01T00:00:00Z", type: "full", id: "0000" },
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
      {
        timestamp: "2019-01-01T06:00:00Z",
        type: "diff",
        id: "0001",
        parent: "0000",
      },
      {
        timestamp: "2019-01-01T18:00:00Z",
        type: "diff",
        id: "0002",
        parent: "0001",
      },
      {
        timestamp: "2019-01-01T12:00:00Z",
        type: "diff",
        id: "0003",
        parent: "0002",
      },
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
  async function test_crlite_incremental_filters_with_wrong_parent() {
    Services.prefs.setBoolPref(CRLITE_FILTERS_ENABLED_PREF, true);

    let result = await syncAndDownload([
      { timestamp: "2019-01-01T00:00:00Z", type: "full", id: "0000" },
      {
        timestamp: "2019-01-01T06:00:00Z",
        type: "diff",
        id: "0001",
        parent: "0000",
      },
      {
        timestamp: "2019-01-01T12:00:00Z",
        type: "diff",
        id: "0003",
        parent: "0002",
      },
      {
        timestamp: "2019-01-01T18:00:00Z",
        type: "diff",
        id: "0004",
        parent: "0003",
      },
    ]);
    let [status, filters] = result.split(";");
    equal(status, "finished", "CRLite filter download should have run");
    let filtersSplit = filters.split(",");
    deepEqual(
      filtersSplit,
      ["2019-01-01T00:00:00Z-full", "2019-01-01T06:00:00Z-diff"],
      "Should have downloaded the expected CRLite filters"
    );
  }
);

add_task(
  {
    skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
  },
  async function test_crlite_incremental_filter_too_early() {
    Services.prefs.setBoolPref(CRLITE_FILTERS_ENABLED_PREF, true);

    let result = await syncAndDownload([
      { timestamp: "2019-01-02T00:00:00Z", type: "full", id: "0000" },
      {
        timestamp: "2019-01-01T00:00:00Z",
        type: "diff",
        id: "0001",
        parent: "0000",
      },
    ]);
    equal(
      result,
      "finished;2019-01-02T00:00:00Z-full",
      "CRLite filter download should have run"
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
      { timestamp: "2019-01-01T00:00:00Z", type: "full", id: "0000" },
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
      {
        timestamp: "2019-01-01T06:00:00Z",
        type: "diff",
        id: "0001",
        parent: "0000",
      },
      { timestamp: "2019-01-01T00:00:00Z", type: "full", id: "0000" },
      {
        timestamp: "2019-01-01T18:00:00Z",
        type: "diff",
        id: "0003",
        parent: "0002",
      },
      {
        timestamp: "2019-01-01T12:00:00Z",
        type: "diff",
        id: "0002",
        parent: "0001",
      },
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
      {
        timestamp: "2019-01-02T06:00:00Z",
        type: "diff",
        id: "0011",
        parent: "0010",
      },
      {
        timestamp: "2019-01-03T12:00:00Z",
        type: "diff",
        id: "0022",
        parent: "0021",
      },
      {
        timestamp: "2019-01-02T12:00:00Z",
        type: "diff",
        id: "0012",
        parent: "0011",
      },
      {
        timestamp: "2019-01-03T18:00:00Z",
        type: "diff",
        id: "0023",
        parent: "0022",
      },
      {
        timestamp: "2019-01-02T18:00:00Z",
        type: "diff",
        id: "0013",
        parent: "0012",
      },
      { timestamp: "2019-01-02T00:00:00Z", type: "full", id: "0010" },
      { timestamp: "2019-01-03T00:00:00Z", type: "full", id: "0020" },
      {
        timestamp: "2019-01-01T06:00:00Z",
        type: "diff",
        id: "0001",
        parent: "0000",
      },
      {
        timestamp: "2019-01-01T18:00:00Z",
        type: "diff",
        id: "0003",
        parent: "0002",
      },
      {
        timestamp: "2019-01-01T12:00:00Z",
        type: "diff",
        id: "0002",
        parent: "0001",
      },
      { timestamp: "2019-01-01T00:00:00Z", type: "full", id: "0000" },
      {
        timestamp: "2019-01-03T06:00:00Z",
        type: "diff",
        id: "0021",
        parent: "0020",
      },
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

function getCRLiteEnrollmentRecordFor(nsCert) {
  let { subjectString, spkiHashString } = getSubjectAndSPKIHash(nsCert);
  return {
    subjectDN: btoa(subjectString),
    pubKeyHash: spkiHashString,
    crlite_enrolled: true,
  };
}

add_task(
  {
    skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
  },
  async function test_crlite_filters_and_check_revocation() {
    Services.prefs.setBoolPref(CRLITE_FILTERS_ENABLED_PREF, true);
    Services.prefs.setIntPref(
      "security.pki.crlite_mode",
      CRLiteModeEnforcePrefValue
    );
    Services.prefs.setBoolPref(INTERMEDIATES_ENABLED_PREF, true);

    let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
      Ci.nsIX509CertDB
    );
    let validCertIssuer = constructCertFromFile(
      "test_cert_storage_direct/valid-cert-issuer.pem"
    );
    let revokedCertIssuer = constructCertFromFile(
      "test_cert_storage_direct/revoked-cert-issuer.pem"
    );
    let revokedInStashIssuer = constructCertFromFile(
      "test_cert_storage_direct/revoked-in-stash-issuer.pem"
    );
    let noSCTCertIssuer = constructCertFromFile(
      "test_cert_storage_direct/no-sct-issuer.pem"
    );

    let crliteEnrollmentRecords = [
      getCRLiteEnrollmentRecordFor(validCertIssuer),
      getCRLiteEnrollmentRecordFor(revokedCertIssuer),
      getCRLiteEnrollmentRecordFor(revokedInStashIssuer),
      getCRLiteEnrollmentRecordFor(noSCTCertIssuer),
    ];

    await IntermediatePreloadsClient.onSync({
      data: {
        current: crliteEnrollmentRecords,
        created: crliteEnrollmentRecords,
        updated: [],
        deleted: [],
      },
    });

    let result = await syncAndDownload([
      { timestamp: "2019-11-19T00:00:00Z", type: "full", id: "0000" },
    ]);
    equal(
      result,
      "finished;2019-11-19T00:00:00Z-full",
      "CRLite filter download should have run"
    );

    let validCert = constructCertFromFile(
      "test_cert_storage_direct/valid-cert.pem"
    );
    await checkCertErrorGenericAtTime(
      certdb,
      validCert,
      PRErrorCodeSuccess,
      certificateUsageSSLServer,
      new Date("2019-11-20T00:00:00Z").getTime() / 1000,
      false,
      "skynew.jp",
      Ci.nsIX509CertDB.FLAG_LOCAL_ONLY
    );

    let revokedCert = constructCertFromFile(
      "test_cert_storage_direct/revoked-cert.pem"
    );
    await checkCertErrorGenericAtTime(
      certdb,
      revokedCert,
      SEC_ERROR_REVOKED_CERTIFICATE,
      certificateUsageSSLServer,
      new Date("2019-11-20T00:00:00Z").getTime() / 1000,
      false,
      "schunk-group.com",
      Ci.nsIX509CertDB.FLAG_LOCAL_ONLY
    );

    // Before any stashes are downloaded, this should verify successfully.
    let revokedInStashCert = constructCertFromFile(
      "test_cert_storage_direct/revoked-in-stash-cert.pem"
    );
    await checkCertErrorGenericAtTime(
      certdb,
      revokedInStashCert,
      PRErrorCodeSuccess,
      certificateUsageSSLServer,
      new Date("2020-11-20T00:00:00Z").getTime() / 1000,
      false,
      "gold-g2-valid-cert-demo.swisssign.net",
      Ci.nsIX509CertDB.FLAG_LOCAL_ONLY
    );

    result = await syncAndDownload([
      { timestamp: "2019-11-20T00:00:00Z", type: "full", id: "0000" },
      {
        timestamp: "2019-11-20T06:00:00Z",
        type: "diff",
        id: "0001",
        parent: "0000",
      },
    ]);
    let [status, filters] = result.split(";");
    equal(status, "finished", "CRLite filter download should have run");
    deepEqual(
      filters,
      ["2019-11-20T00:00:00Z-full", "2019-11-20T06:00:00Z-diff"],
      "Should have downloaded the expected CRLite filters"
    );

    // After downloading the stash, this should be revoked.
    await checkCertErrorGenericAtTime(
      certdb,
      revokedInStashCert,
      SEC_ERROR_REVOKED_CERTIFICATE,
      certificateUsageSSLServer,
      new Date("2020-11-20T00:00:00Z").getTime() / 1000,
      false,
      "gold-g2-valid-cert-demo.swisssign.net",
      Ci.nsIX509CertDB.FLAG_LOCAL_ONLY
    );

    // The other certificates should still get the same results as they did before.
    await checkCertErrorGenericAtTime(
      certdb,
      validCert,
      PRErrorCodeSuccess,
      certificateUsageSSLServer,
      new Date("2019-11-20T00:00:00Z").getTime() / 1000,
      false,
      "skynew.jp",
      Ci.nsIX509CertDB.FLAG_LOCAL_ONLY
    );

    await checkCertErrorGenericAtTime(
      certdb,
      revokedCert,
      SEC_ERROR_REVOKED_CERTIFICATE,
      certificateUsageSSLServer,
      new Date("2019-11-20T00:00:00Z").getTime() / 1000,
      false,
      "schunk-group.com",
      Ci.nsIX509CertDB.FLAG_LOCAL_ONLY
    );

    // This certificate has no embedded SCTs, so it is not guaranteed to be in
    // CT, so CRLite can't be guaranteed to give the correct answer, so it is
    // not consulted.
    let noSCTCert = constructCertFromFile(
      "test_cert_storage_direct/no-sct.pem"
    );
    // Currently OCSP will always be consulted for certificates that are not
    // revoked in CRLite, but if/when OCSP gets skipped for all certificates
    // covered by CRLite, this test will ensure that certificates without
    // embedded SCTs will cause OCSP to be consulted.
    // NB: this will cause an OCSP request to be sent to localhost:80, but
    // since an OCSP responder shouldn't be running on that port, this should
    // fail safely.
    Services.prefs.setCharPref("network.dns.localDomains", "ocsp.digicert.com");
    Services.prefs.setBoolPref("security.OCSP.require", true);
    Services.prefs.setIntPref("security.OCSP.enabled", 1);
    await checkCertErrorGenericAtTime(
      certdb,
      noSCTCert,
      SEC_ERROR_OCSP_SERVER_ERROR,
      certificateUsageSSLServer,
      new Date("2020-11-20T00:00:00Z").getTime() / 1000,
      false,
      "mail233.messagelabs.com",
      0
    );
    Services.prefs.clearUserPref("network.dns.localDomains");
    Services.prefs.clearUserPref("security.OCSP.require");
    Services.prefs.clearUserPref("security.OCSP.enabled");
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

  // Set intermediate preloading to download 0 intermediates at a time.
  Services.prefs.setIntPref(INTERMEDIATES_DL_PER_POLL_PREF, 0);

  Services.prefs.setCharPref("browser.policies.loglevel", "debug");

  run_next_test();
}
