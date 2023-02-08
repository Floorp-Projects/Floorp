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
      "E9:3A:91:F6:15:11:FB:DD:02:76:DD:45:8C:4B:F4:9B:D1:14:13:91:2E:96:4B:EC:D2:4F:90:D5:F4:BB:29:5C",
    dbKey: "This isn't relevant for this test.",
  };
  // bad_certs/selfsigned.pem
  let cert2 = {
    sha256Fingerprint:
      "51:BC:41:90:C1:FD:6E:73:18:19:B0:60:08:DD:A3:3D:59:B2:5B:FB:D0:3D:DD:89:19:A5:BB:C6:2B:5A:72:A7",
    dbKey: "This isn't relevant for this test.",
  };
  // bad_certs/noValidNames.pem
  let cert3 = {
    sha256Fingerprint:
      "C3:A3:61:02:CA:64:CC:EC:45:1D:24:B6:A0:69:DB:DB:F0:D8:58:76:FC:50:36:52:5A:E8:40:4C:55:72:08:F4",
    dbKey: "This isn't relevant for this test.",
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
    "[::1]:443:\tOID.2.16.840.1.101.3.4.2.1\t" + // IPv6
      cert2.sha256Fingerprint +
      "\tM\t" +
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
  info(
    `if this test fails, try updating cert1.sha256Fingerprint to "${cert1.sha256Fingerprint}"`
  );
  cert2 = constructCertFromFile("bad_certs/selfsigned.pem");
  info(
    `if this test fails, try updating cert2.sha256Fingerprint to "${cert2.sha256Fingerprint}"`
  );
  cert3 = constructCertFromFile("bad_certs/noValidNames.pem");
  info(
    `if this test fails, try updating cert3.sha256Fingerprint to "${cert3.sha256Fingerprint}"`
  );

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
      host: "::1",
      port: 443,
      cert: cert2,
      bits: Ci.nsICertOverrideService.ERROR_MISMATCH,
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
