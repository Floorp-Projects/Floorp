// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Tests that CRLite filter downloading works correctly.

// The file `test_crlite_filters/20201017-0-filter` can be regenerated using
// the rust-create-cascade program from https://github.com/mozilla/crlite.
//
// The input to this program is a list of known serial numbers and a list of
// revoked serial numbers. The lists are presented as directories of files in
// which each file holds serials for one issuer. The file names are
// urlsafe-base64 encoded SHA256 hashes of issuer SPKIs. The file contents are
// ascii hex encoded serial numbers. The program crlite_key.py in this directory
// can generate these values for you.
//
// The test filter was generated as follows:
//
// $ ./crlite_key.py test_crlite_filters/issuer.pem test_crlite_filters/valid.pem
// 8Rw90Ej3Ttt8RRkrg-WYDS9n7IS03bk5bjP_UXPtaY8=
// 00da4f392bfd8bcea8
//
// $ ./crlite_key.py test_crlite_filters/issuer.pem test_crlite_filters/revoked.pem
// 8Rw90Ej3Ttt8RRkrg-WYDS9n7IS03bk5bjP_UXPtaY8=
// 2d35ca6503fb1ba3
//
// $ mkdir known revoked
// $ echo "00da4f392bfd8bcea8" > known/8Rw90Ej3Ttt8RRkrg-WYDS9n7IS03bk5bjP_UXPtaY8\=
// $ echo "2d35ca6503fb1ba3" >> known/8Rw90Ej3Ttt8RRkrg-WYDS9n7IS03bk5bjP_UXPtaY8\=
// $ echo "2d35ca6503fb1ba3" > revoked/8Rw90Ej3Ttt8RRkrg-WYDS9n7IS03bk5bjP_UXPtaY8\=
//
// $ rust-create-cascade --known ./known/ --revoked ./revoked/
//

"use strict";
do_get_profile(); // must be called before getting nsIX509CertDB

const { RemoteSecuritySettings } = ChromeUtils.import(
  "resource://gre/modules/psm/RemoteSecuritySettings.jsm"
);
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

const { CRLiteFiltersClient } = RemoteSecuritySettings.init();

const CRLITE_FILTERS_ENABLED_PREF =
  "security.remote_settings.crlite_filters.enabled";
const INTERMEDIATES_ENABLED_PREF =
  "security.remote_settings.intermediates.enabled";
const INTERMEDIATES_DL_PER_POLL_PREF =
  "security.remote_settings.intermediates.downloads_per_poll";

// crlite_enrollment_id.py test_crlite_filters/issuer.pem
const ISSUER_PEM_UID = "UbH9/ZAnjuqf79Xhah1mFOWo6ZvgQCgsdheWfjvVUM8=";
// crlite_enrollment_id.py test_crlite_filters/no-sct-issuer.pem
const NO_SCT_ISSUER_PEM_UID = "Myn7EasO1QikOtNmo/UZdh6snCAw0BOY6wgU8OsUeeY=";

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

// Get the name of the file in the test directory to serve as the attachment
// for the given filter.
function getFilenameForFilter(filter) {
  if (filter.type == "full") {
    return "20201017-0-filter";
  }
  if (filter.id == "0001") {
    return "20201017-1-filter.stash";
  }
  // The addition of another stash file was written more than a month after
  // other parts of this test. As such, the second stash file for October 17th,
  // 2020 was not readily available. Since the structure of stash files don't
  // depend on each other, though, any two stash files are compatible, and so
  // this stash from December 1st is used instead.
  return "20201201-3-filter.stash";
}

/**
 * Simulate a Remote Settings synchronization by filling up the local data with
 * fake records.
 *
 * @param {*} filters List of filters for which we will create records.
 * @param {Boolean} clear Whether or not to clear the local DB first. Defaults
 *                        to true.
 */
async function syncAndDownload(filters, clear = true) {
  const localDB = await CRLiteFiltersClient.client.db;
  if (clear) {
    await localDB.clear();
  }

  for (let filter of filters) {
    const filename = getFilenameForFilter(filter);
    const file = do_get_file(`test_crlite_filters/${filename}`);
    const fileBytes = readFile(file);

    const record = {
      details: {
        name: `${filter.timestamp}-${filter.type}`,
      },
      attachment: {
        hash: getHash(fileBytes),
        size: fileBytes.length,
        filename,
        location: `security-state-workspace/cert-revocations/test_crlite_filters/${filename}`,
        mimetype: "application/octet-stream",
      },
      incremental: filter.type == "diff",
      effectiveTimestamp: new Date(filter.timestamp).getTime(),
      parent: filter.type == "diff" ? filter.parent : undefined,
      id: filter.id,
      coverage: filter.type == "full" ? filter.coverage : undefined,
      enrolledIssuers:
        filter.type == "full" ? filter.enrolledIssuers : undefined,
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

add_task(async function test_crlite_filters_disabled() {
  Services.prefs.setBoolPref(CRLITE_FILTERS_ENABLED_PREF, false);

  let result = await syncAndDownload([
    {
      timestamp: "2019-01-01T00:00:00Z",
      type: "full",
      id: "0000",
      coverage: [
        {
          logID: "9lyUL9F3MCIUVBgIMJRWjuNNExkzv98MLyALzE7xZOM=",
          minTimestamp: 0,
          maxTimestamp: 9999999999999,
        },
      ],
    },
  ]);
  equal(result, "disabled", "CRLite filter download should not have run");
});

add_task(async function test_crlite_no_filters() {
  Services.prefs.setBoolPref(CRLITE_FILTERS_ENABLED_PREF, true);

  let result = await syncAndDownload([]);
  equal(
    result,
    "unavailable",
    "CRLite filter download should have run, but nothing was available"
  );
});

add_task(async function test_crlite_only_incremental_filters() {
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
});

add_task(async function test_crlite_incremental_filters_with_wrong_parent() {
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
});

add_task(async function test_crlite_incremental_filter_too_early() {
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
});

add_task(async function test_crlite_filters_basic() {
  Services.prefs.setBoolPref(CRLITE_FILTERS_ENABLED_PREF, true);

  let result = await syncAndDownload([
    { timestamp: "2019-01-01T00:00:00Z", type: "full", id: "0000" },
  ]);
  equal(
    result,
    "finished;2019-01-01T00:00:00Z-full",
    "CRLite filter download should have run"
  );
});

add_task(async function test_crlite_filters_full_and_incremental() {
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
});

add_task(async function test_crlite_filters_multiple_days() {
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
});

add_task(async function test_crlite_confirm_revocations_mode() {
  Services.prefs.setBoolPref(CRLITE_FILTERS_ENABLED_PREF, true);
  Services.prefs.setIntPref(
    "security.pki.crlite_mode",
    CRLiteModeConfirmRevocationsValue
  );
  Services.prefs.setBoolPref(INTERMEDIATES_ENABLED_PREF, true);

  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "test_crlite_filters/issuer.pem", ",,");
  addCertFromFile(certdb, "test_crlite_filters/no-sct-issuer.pem", ",,");

  let result = await syncAndDownload([
    {
      timestamp: "2020-10-17T00:00:00Z",
      type: "full",
      id: "0000",
      coverage: [
        {
          logID: "9lyUL9F3MCIUVBgIMJRWjuNNExkzv98MLyALzE7xZOM=",
          minTimestamp: 0,
          maxTimestamp: 9999999999999,
        },
        {
          logID: "pLkJkLQYWBSHuxOizGdwCjw1mAT5G9+443fNDsgN3BA=",
          minTimestamp: 0,
          maxTimestamp: 9999999999999,
        },
      ],
      enrolledIssuers: [ISSUER_PEM_UID, NO_SCT_ISSUER_PEM_UID],
    },
  ]);
  equal(
    result,
    "finished;2020-10-17T00:00:00Z-full",
    "CRLite filter download should have run"
  );

  // The CRLite result should be enforced for this certificate and
  // OCSP should not be consulted.
  let validCert = constructCertFromFile("test_crlite_filters/valid.pem");
  await checkCertErrorGenericAtTime(
    certdb,
    validCert,
    PRErrorCodeSuccess,
    certificateUsageSSLServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    undefined,
    "vpn.worldofspeed.org",
    0
  );

  // OCSP should be consulted for this certificate, but OCSP is disabled by
  // Ci.nsIX509CertDB.FLAG_LOCAL_ONLY so this will be treated as a soft-failure
  // and the CRLite result will be used.
  let revokedCert = constructCertFromFile("test_crlite_filters/revoked.pem");
  await checkCertErrorGenericAtTime(
    certdb,
    revokedCert,
    SEC_ERROR_REVOKED_CERTIFICATE,
    certificateUsageSSLServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    undefined,
    "us-datarecovery.com",
    Ci.nsIX509CertDB.FLAG_LOCAL_ONLY
  );

  // Reload the filter w/o coverage and enrollment metadata.
  result = await syncAndDownload([
    {
      timestamp: "2020-10-17T00:00:00Z",
      type: "full",
      id: "0000",
      coverage: [],
      enrolledIssuers: [],
    },
  ]);
  equal(
    result,
    "finished;2020-10-17T00:00:00Z-full",
    "CRLite filter download should have run"
  );

  // OCSP will be consulted for the revoked certificate, but a soft-failure
  // should now result in a Success return.
  await checkCertErrorGenericAtTime(
    certdb,
    revokedCert,
    PRErrorCodeSuccess,
    certificateUsageSSLServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    undefined,
    "us-datarecovery.com",
    Ci.nsIX509CertDB.FLAG_LOCAL_ONLY
  );
});

add_task(async function test_crlite_filters_and_check_revocation() {
  Services.prefs.setBoolPref(CRLITE_FILTERS_ENABLED_PREF, true);
  Services.prefs.setIntPref(
    "security.pki.crlite_mode",
    CRLiteModeEnforcePrefValue
  );
  Services.prefs.setBoolPref(INTERMEDIATES_ENABLED_PREF, true);

  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "test_crlite_filters/issuer.pem", ",,");
  addCertFromFile(certdb, "test_crlite_filters/no-sct-issuer.pem", ",,");

  let result = await syncAndDownload([
    {
      timestamp: "2020-10-17T00:00:00Z",
      type: "full",
      id: "0000",
      coverage: [
        {
          logID: "9lyUL9F3MCIUVBgIMJRWjuNNExkzv98MLyALzE7xZOM=",
          minTimestamp: 0,
          maxTimestamp: 9999999999999,
        },
        {
          logID: "pLkJkLQYWBSHuxOizGdwCjw1mAT5G9+443fNDsgN3BA=",
          minTimestamp: 0,
          maxTimestamp: 9999999999999,
        },
      ],
      enrolledIssuers: [ISSUER_PEM_UID, NO_SCT_ISSUER_PEM_UID],
    },
  ]);
  equal(
    result,
    "finished;2020-10-17T00:00:00Z-full",
    "CRLite filter download should have run"
  );

  let validCert = constructCertFromFile("test_crlite_filters/valid.pem");
  // NB: by not specifying Ci.nsIX509CertDB.FLAG_LOCAL_ONLY, this tests that
  // the implementation does not fall back to OCSP fetching, because if it
  // did, the implementation would attempt to connect to a server outside the
  // test infrastructure, which would result in a crash in the test
  // environment, which would be treated as a test failure.
  await checkCertErrorGenericAtTime(
    certdb,
    validCert,
    PRErrorCodeSuccess,
    certificateUsageSSLServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    false,
    "vpn.worldofspeed.org",
    0
  );

  let revokedCert = constructCertFromFile("test_crlite_filters/revoked.pem");
  await checkCertErrorGenericAtTime(
    certdb,
    revokedCert,
    SEC_ERROR_REVOKED_CERTIFICATE,
    certificateUsageSSLServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    false,
    "us-datarecovery.com",
    0
  );

  // Before any stashes are downloaded, this should verify successfully.
  let revokedInStashCert = constructCertFromFile(
    "test_crlite_filters/revoked-in-stash.pem"
  );
  await checkCertErrorGenericAtTime(
    certdb,
    revokedInStashCert,
    PRErrorCodeSuccess,
    certificateUsageSSLServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    false,
    "stokedmoto.com",
    0
  );

  result = await syncAndDownload(
    [
      {
        timestamp: "2020-10-17T03:00:00Z",
        type: "diff",
        id: "0001",
        parent: "0000",
      },
    ],
    false
  );
  equal(
    result,
    "finished;2020-10-17T03:00:00Z-diff",
    "Should have downloaded the expected CRLite filters"
  );

  // After downloading the first stash, this should be revoked.
  await checkCertErrorGenericAtTime(
    certdb,
    revokedInStashCert,
    SEC_ERROR_REVOKED_CERTIFICATE,
    certificateUsageSSLServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    false,
    "stokedmoto.com",
    0
  );

  // Before downloading the second stash, this should not be revoked.
  let revokedInStash2Cert = constructCertFromFile(
    "test_crlite_filters/revoked-in-stash-2.pem"
  );
  await checkCertErrorGenericAtTime(
    certdb,
    revokedInStash2Cert,
    PRErrorCodeSuccess,
    certificateUsageSSLServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    false,
    "icsreps.com",
    0
  );

  result = await syncAndDownload(
    [
      {
        timestamp: "2020-10-17T06:00:00Z",
        type: "diff",
        id: "0002",
        parent: "0001",
      },
    ],
    false
  );
  equal(
    result,
    "finished;2020-10-17T06:00:00Z-diff",
    "Should have downloaded the expected CRLite filters"
  );

  // After downloading the second stash, this should be revoked.
  await checkCertErrorGenericAtTime(
    certdb,
    revokedInStash2Cert,
    SEC_ERROR_REVOKED_CERTIFICATE,
    certificateUsageSSLServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    false,
    "icsreps.com",
    0
  );

  // The other certificates should still get the same results as they did before.
  await checkCertErrorGenericAtTime(
    certdb,
    validCert,
    PRErrorCodeSuccess,
    certificateUsageSSLServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    false,
    "vpn.worldofspeed.org",
    0
  );

  await checkCertErrorGenericAtTime(
    certdb,
    revokedCert,
    SEC_ERROR_REVOKED_CERTIFICATE,
    certificateUsageSSLServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    false,
    "us-datarecovery.com",
    0
  );

  await checkCertErrorGenericAtTime(
    certdb,
    revokedInStashCert,
    SEC_ERROR_REVOKED_CERTIFICATE,
    certificateUsageSSLServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    false,
    "stokedmoto.com",
    0
  );

  // This certificate has no embedded SCTs, so it is not guaranteed to be in
  // CT, so CRLite can't be guaranteed to give the correct answer, so it is
  // not consulted, and the implementation falls back to OCSP. Since the real
  // OCSP responder can't be reached, this results in a
  // SEC_ERROR_OCSP_SERVER_ERROR.
  let noSCTCert = constructCertFromFile("test_crlite_filters/no-sct.pem");
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
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    false,
    "mail233.messagelabs.com",
    0
  );
  Services.prefs.clearUserPref("network.dns.localDomains");
  Services.prefs.clearUserPref("security.OCSP.require");
  Services.prefs.clearUserPref("security.OCSP.enabled");

  // The revoked certificate example has one SCT from the log with ID "9ly...="
  // at time 1598140096613 and another from the log with ID "XNx...=" at time
  // 1598140096917. The filter we construct here fails to cover it by one
  // millisecond in each case. The implementation will fall back to OCSP
  // fetching. Since this would result in a crash and test failure, the
  // Ci.nsIX509CertDB.FLAG_LOCAL_ONLY is used.
  result = await syncAndDownload([
    {
      timestamp: "2020-10-17T00:00:00Z",
      type: "full",
      id: "0000",
      coverage: [
        {
          logID: "9lyUL9F3MCIUVBgIMJRWjuNNExkzv98MLyALzE7xZOM=",
          minTimestamp: 0,
          maxTimestamp: 1598140096612,
        },
        {
          logID: "XNxDkv7mq0VEsV6a1FbmEDf71fpH3KFzlLJe5vbHDso=",
          minTimestamp: 1598140096917,
          maxTimestamp: 9999999999999,
        },
      ],
      enrolledIssuers: [ISSUER_PEM_UID, NO_SCT_ISSUER_PEM_UID],
    },
  ]);
  equal(
    result,
    "finished;2020-10-17T00:00:00Z-full",
    "CRLite filter download should have run"
  );
  await checkCertErrorGenericAtTime(
    certdb,
    revokedCert,
    PRErrorCodeSuccess,
    certificateUsageSSLServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    false,
    "us-datarecovery.com",
    Ci.nsIX509CertDB.FLAG_LOCAL_ONLY
  );
});

add_task(async function test_crlite_filters_avoid_reprocessing_filters() {
  Services.prefs.setBoolPref(CRLITE_FILTERS_ENABLED_PREF, true);

  let result = await syncAndDownload([
    {
      timestamp: "2019-01-01T00:00:00Z",
      type: "full",
      id: "0000",
      coverage: [
        {
          logID: "9lyUL9F3MCIUVBgIMJRWjuNNExkzv98MLyALzE7xZOM=",
          minTimestamp: 0,
          maxTimestamp: 9999999999999,
        },
      ],
      enrolledIssuers: [ISSUER_PEM_UID, NO_SCT_ISSUER_PEM_UID],
    },
    {
      timestamp: "2019-01-01T06:00:00Z",
      type: "diff",
      id: "0001",
      parent: "0000",
    },
    {
      timestamp: "2019-01-01T12:00:00Z",
      type: "diff",
      id: "0002",
      parent: "0001",
    },
    {
      timestamp: "2019-01-01T18:00:00Z",
      type: "diff",
      id: "0003",
      parent: "0002",
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
  // This simulates another poll without clearing the database first. The
  // filter and stashes should not be re-downloaded.
  result = await syncAndDownload([], false);
  equal(result, "finished;");

  // If a new stash is added, only it should be downloaded.
  result = await syncAndDownload(
    [
      {
        timestamp: "2019-01-02T00:00:00Z",
        type: "diff",
        id: "0004",
        parent: "0003",
      },
    ],
    false
  );
  equal(result, "finished;2019-01-02T00:00:00Z-diff");
});

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
