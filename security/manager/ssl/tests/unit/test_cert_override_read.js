/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// This test checks parsing of the the certificate override file

function run_test() {
  // These are hard-coded to avoid initialization of NSS before setup is complete
  // bad_certs/mitm.pem
  let cert1 = {
    sha256Fingerprint:
      "AF:89:F3:E8:0A:AD:58:96:05:C4:AC:D7:A2:A3:07:42:E6:F9:85:FA:9D:D7:D4:43:EC:9F:87:52:94:9C:4D:A6",
    dbKey:
      "AAAAAAAAAAAAAAAUAAAAG0uHZ2GoTSZsZNE9WdB/lvAPubXIMBkxFzAVBgNVBAMMDlRlc3QgTUlUTSBSb290",
  };
  // bad_certs/selfsigned.pem
  let cert2 = {
    sha256Fingerprint:
      "5D:13:3E:90:DF:34:C4:E8:27:E8:88:4A:28:12:84:1D:1B:E8:0C:73:20:C4:90:8A:A7:AC:A5:8D:7E:42:7E:6E",
    dbKey:
      "AAAAAAAAAAAAAAAUAAAAKEdUzTa/lL+mUeJpdBfMepsMAP5RMCYxJDAiBgNVBAMMG1NlbGYtc2lnbmVkIFRlc3QgRW5kLWVudGl0eQ==",
  };
  // bad_certs/noValidNames.pem
  let cert3 = {
    sha256Fingerprint:
      "40:56:30:2B:C3:AE:DA:22:40:8A:2D:C5:45:00:5E:EC:9B:AA:38:99:D6:4E:29:05:6B:4E:CB:E8:F9:10:30:D6",
    dbKey:
      "AAAAAAAAAAAAAAAUAAAAFHPQYJXEeVUul+u7/ZQOjaI3fYD1MBIxEDAOBgNVBAMMB1Rlc3QgQ0E=",
  };

  let profileDir = do_get_profile();
  let overrideFile = profileDir.clone();
  overrideFile.append(CERT_OVERRIDE_FILE_NAME);
  // Assuming we're working with a clean slate, the file shouldn't exist
  // until we create it.
  ok(!overrideFile.exists());
  let outputStream = FileUtils.openFileOutputStream(overrideFile);
  let lines = [
    "# PSM Certificate Override Settings file",
    "# This is a generated file!  Do not edit.",
    "test.example.com:443:^privateBrowsingId=1\tOID.2.16.840.1.101.3.4.2.1\t" +
      cert1.sha256Fingerprint +
      "\tM\t" +
      cert1.dbKey,
    "test.example.com:443:^privateBrowsingId=2\tOID.2.16.840.1.101.3.4.2.1\t" +
      cert1.sha256Fingerprint +
      "\tM\t" +
      cert1.dbKey,
    "example.com:443:\tOID.2.16.840.1.101.3.4.2.1\t" +
      cert2.sha256Fingerprint +
      "\tU\t" +
      cert2.dbKey,
    "old.example.com:443\tOID.2.16.840.1.101.3.4.2.1\t" + // missing attributes (defaulted)
      cert1.sha256Fingerprint +
      "\tM\t" +
      cert1.dbKey,
    ":443:\tOID.2.16.840.1.101.3.4.2.1\t" + // missing host name
      cert3.sha256Fingerprint +
      "\tU\t" +
      cert3.dbKey,
    "example.com::\tOID.2.16.840.1.101.3.4.2.1\t" + // missing port
      cert3.sha256Fingerprint +
      "\tU\t" +
      cert3.dbKey,
    "example.com:443:\tOID.2.16.840.1.101.3.4.2.1\t" + // wrong fingerprint/dbkey
      cert2.sha256Fingerprint +
      "\tU\t" +
      cert3.dbKey,
    "example.com:443:\tOID.0.00.000.0.000.0.0.0.0\t" + // bad OID
      cert3.sha256Fingerprint +
      "\tU\t" +
      cert3.dbKey,
    "example.com:443:\t.0.0.0.0\t" + // malformed OID
      cert3.sha256Fingerprint +
      "\tU\t" +
      cert3.dbKey,
    "example.com:443:\t\t" + // missing OID
      cert3.sha256Fingerprint +
      "\tU\t" +
      cert3.dbKey,
    "example.com:443:\tOID.2.16.840.1.101.3.4.2.1\t" + // missing fingerprint
      "\tU\t" +
      cert3.dbKey,
    "example.com:443:\tOID.2.16.840.1.101.3.4.2.1\t" + // missing override bits
      cert3.sha256Fingerprint +
      "\t\t" +
      cert3.dbKey,
    "example.com:443:\tOID.2.16.840.1.101.3.4.2.1\t" + // missing dbkey
      cert3.sha256Fingerprint +
      "\tU\t",
  ];
  writeLinesAndClose(lines, outputStream);
  let overrideService = Cc["@mozilla.org/security/certoverride;1"].getService(
    Ci.nsICertOverrideService
  );
  notEqual(overrideService, null);

  // Now that the override service is initialized we can actually read the certificates
  cert1 = constructCertFromFile("bad_certs/mitm.pem");
  cert2 = constructCertFromFile("bad_certs/selfsigned.pem");
  cert3 = constructCertFromFile("bad_certs/noValidNames.pem");

  const OVERRIDES = [
    {
      host: "test.example.com",
      port: 443,
      cert: cert1,
      bits: Ci.nsICertOverrideService.ERROR_MISMATCH,
      attributes: { privateBrowsingId: 1 },
    },
    {
      host: "test.example.com",
      port: 443,
      cert: cert1,
      bits: Ci.nsICertOverrideService.ERROR_MISMATCH,
      attributes: { privateBrowsingId: 2 },
    },
    {
      host: "example.com",
      port: 443,
      cert: cert2,
      bits: Ci.nsICertOverrideService.ERROR_UNTRUSTED,
      attributes: {},
    },
    {
      host: "example.com",
      port: 443,
      cert: cert2,
      bits: Ci.nsICertOverrideService.ERROR_UNTRUSTED,
      attributes: { userContextId: 1 }, // only privateBrowsingId is used
    },
    {
      host: "old.example.com",
      port: 443,
      cert: cert1,
      bits: Ci.nsICertOverrideService.ERROR_MISMATCH,
      attributes: {},
    },
  ];
  const BAD_OVERRIDES = [
    {
      host: "test.example.com",
      port: 443,
      cert: cert1,
      bits: Ci.nsICertOverrideService.ERROR_MISMATCH,
      attributes: { privateBrowsingId: 3 }, // wrong attributes
    },
    {
      host: "test.example.com",
      port: 443,
      cert: cert3, // wrong certificate
      bits: Ci.nsICertOverrideService.ERROR_UNTRUSTED,
      attributes: { privateBrowsingId: 1 },
    },
    {
      host: "example.com",
      port: 443,
      cert: cert3,
      bits: Ci.nsICertOverrideService.ERROR_UNTRUSTED,
      attributes: {},
    },
  ];
  const BAD_BIT_OVERRIDES = [
    {
      host: "example.com",
      port: 443,
      cert: cert2,
      bits: Ci.nsICertOverrideService.ERROR_MISMATCH, // wrong bits
      attributes: {},
    },
  ];

  for (let override of OVERRIDES) {
    let actualBits = {};
    let temp = {};
    ok(
      overrideService.hasMatchingOverride(
        override.host,
        override.port,
        override.attributes,
        override.cert,
        actualBits,
        temp
      ),
      `${JSON.stringify(override)} should have an override`
    );
    equal(actualBits.value, override.bits);
    equal(temp.value, false);
  }

  for (let override of BAD_OVERRIDES) {
    let actualBits = {};
    let temp = {};
    ok(
      !overrideService.hasMatchingOverride(
        override.host,
        override.port,
        override.attributes,
        override.cert,
        actualBits,
        temp
      ),
      `${override} should not have an override`
    );
  }

  for (let override of BAD_BIT_OVERRIDES) {
    let actualBits = {};
    let temp = {};
    ok(
      overrideService.hasMatchingOverride(
        override.host,
        override.port,
        override.attributes,
        override.cert,
        actualBits,
        temp
      ),
      `${override} should have an override`
    );
    notEqual(actualBits.value, override.bits);
  }
}
