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
      "B6:E6:16:9F:A0:9D:97:AF:B4:8C:AF:94:FD:08:C6:D3:AE:E6:B8:62:B9:B2:81:B5:52:AB:6E:7C:17:25:8E:F8",
    dbKey: "This isn't relevant for this test.",
  };
  // bad_certs/selfsigned.pem
  let cert2 = {
    sha256Fingerprint:
      "7B:23:9E:6C:46:D8:D1:F3:59:DC:E6:05:5C:DB:06:FB:98:21:50:92:9C:B7:EC:3A:A3:B9:A5:4E:25:B2:C3:F8",
    dbKey: "This isn't relevant for this test.",
  };
  // bad_certs/noValidNames.pem
  let cert3 = {
    sha256Fingerprint:
      "CB:E3:D7:05:40:05:22:B4:0D:85:01:01:A6:3F:14:44:C1:AE:C1:1C:FA:77:C2:36:56:1F:2B:AD:6D:94:77:A4",
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
      "\t\t" +
      cert1.dbKey,
    "test.example.com:443:^privateBrowsingId=2\tOID.2.16.840.1.101.3.4.2.1\t" +
      cert1.sha256Fingerprint +
      "\t\t" +
      cert1.dbKey,
    "test.example.com:443:^privateBrowsingId=3\tOID.2.16.840.1.101.3.4.2.1\t" + // includes bits (now obsolete)
      cert1.sha256Fingerprint +
      "\tM\t" +
      cert1.dbKey,
    "example.com:443:\tOID.2.16.840.1.101.3.4.2.1\t" +
      cert2.sha256Fingerprint +
      "\t\t" +
      cert2.dbKey,
    "[::1]:443:\tOID.2.16.840.1.101.3.4.2.1\t" + // IPv6
      cert2.sha256Fingerprint +
      "\t\t" +
      cert2.dbKey,
    "old.example.com:443\tOID.2.16.840.1.101.3.4.2.1\t" + // missing attributes (defaulted)
      cert1.sha256Fingerprint +
      "\t\t" +
      cert1.dbKey,
    ":443:\tOID.2.16.840.1.101.3.4.2.1\t" + // missing host name
      cert3.sha256Fingerprint +
      "\t\t" +
      cert3.dbKey,
    "example.com::\tOID.2.16.840.1.101.3.4.2.1\t" + // missing port
      cert3.sha256Fingerprint +
      "\t\t" +
      cert3.dbKey,
    "example.com:443:\tOID.2.16.840.1.101.3.4.2.1\t" + // wrong fingerprint/dbkey
      cert2.sha256Fingerprint +
      "\t\t" +
      cert3.dbKey,
    "example.com:443:\tOID.0.00.000.0.000.0.0.0.0\t" + // bad OID
      cert3.sha256Fingerprint +
      "\t\t" +
      cert3.dbKey,
    "example.com:443:\t.0.0.0.0\t" + // malformed OID
      cert3.sha256Fingerprint +
      "\t\t" +
      cert3.dbKey,
    "example.com:443:\t\t" + // missing OID
      cert3.sha256Fingerprint +
      "\t\t" +
      cert3.dbKey,
    "example.com:443:\tOID.2.16.840.1.101.3.4.2.1\t" + // missing fingerprint
      "\t\t" +
      cert3.dbKey,
    "example.com:443:\tOID.2.16.840.1.101.3.4.2.1\t" + // missing dbkey
      cert3.sha256Fingerprint +
      "\t\t",
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
      attributes: { privateBrowsingId: 1 },
    },
    {
      host: "test.example.com",
      port: 443,
      cert: cert1,
      attributes: { privateBrowsingId: 2 },
    },
    {
      host: "test.example.com",
      port: 443,
      cert: cert1,
      attributes: { privateBrowsingId: 3 },
    },
    {
      host: "example.com",
      port: 443,
      cert: cert2,
      attributes: {},
    },
    {
      host: "::1",
      port: 443,
      cert: cert2,
      attributes: {},
    },
    {
      host: "example.com",
      port: 443,
      cert: cert2,
      attributes: { userContextId: 1 }, // only privateBrowsingId is used
    },
    {
      host: "old.example.com",
      port: 443,
      cert: cert1,
      attributes: {},
    },
  ];
  const BAD_OVERRIDES = [
    {
      host: "test.example.com",
      port: 443,
      cert: cert1,
      attributes: { privateBrowsingId: 4 }, // wrong attributes
    },
    {
      host: "test.example.com",
      port: 443,
      cert: cert3, // wrong certificate
      attributes: { privateBrowsingId: 1 },
    },
    {
      host: "example.com",
      port: 443,
      cert: cert3,
      attributes: {},
    },
  ];

  for (let override of OVERRIDES) {
    let temp = {};
    ok(
      overrideService.hasMatchingOverride(
        override.host,
        override.port,
        override.attributes,
        override.cert,
        temp
      ),
      `${JSON.stringify(override)} should have an override`
    );
    equal(temp.value, false);
  }

  for (let override of BAD_OVERRIDES) {
    let temp = {};
    ok(
      !overrideService.hasMatchingOverride(
        override.host,
        override.port,
        override.attributes,
        override.cert,
        temp
      ),
      `${override} should not have an override`
    );
  }
}
