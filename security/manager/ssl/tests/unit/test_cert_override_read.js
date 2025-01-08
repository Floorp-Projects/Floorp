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
      "B6:9F:87:57:A0:83:EF:E0:5F:2D:4D:81:2A:E2:04:A0:A7:E5:B2:F8:2D:44:E2:BC:FB:56:A5:41:F2:7E:D4:7A",
  };
  // bad_certs/selfsigned.pem
  let cert2 = {
    sha256Fingerprint:
      "79:38:FB:FE:A9:98:85:02:C4:36:C2:3D:9C:59:15:46:36:6A:29:84:96:83:1D:53:A0:68:3F:D9:01:01:61:6E",
  };
  // bad_certs/noValidNames.pem
  let cert3 = {
    sha256Fingerprint:
      "D2:75:19:5B:97:84:40:A8:34:AB:A4:FE:85:94:6F:7D:43:8D:90:86:7B:5D:41:F4:49:25:73:D1:CE:18:BB:9A",
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
      "\t",
    "test.example.com:443:^privateBrowsingId=2\tOID.2.16.840.1.101.3.4.2.1\t" +
      cert1.sha256Fingerprint +
      "\t",
    "test.example.com:443:^privateBrowsingId=3\tOID.2.16.840.1.101.3.4.2.1\t" + // includes bits and dbKey (now obsolete)
      cert1.sha256Fingerprint +
      "\tM\t" +
      "AAAAAAAAAAAAAAACAAAAFjA5MBQxEjAQBgNVBAMMCWxvY2FsaG9zdA==",
    "example.com:443:\tOID.2.16.840.1.101.3.4.2.1\t" +
      cert2.sha256Fingerprint +
      "\t",
    "[::1]:443:\tOID.2.16.840.1.101.3.4.2.1\t" + // IPv6
      cert2.sha256Fingerprint +
      "\t",
    "old.example.com:443\tOID.2.16.840.1.101.3.4.2.1\t" + // missing attributes (defaulted)
      cert1.sha256Fingerprint +
      "\t",
    ":443:\tOID.2.16.840.1.101.3.4.2.1\t" + // missing host name
      cert3.sha256Fingerprint +
      "\t",
    "example.com::\tOID.2.16.840.1.101.3.4.2.1\t" + // missing port
      cert3.sha256Fingerprint +
      "\t",
    "example.com:443:\tOID.2.16.840.1.101.3.4.2.1\t" + // wrong fingerprint
      cert2.sha256Fingerprint +
      "\t",
    "example.com:443:\tOID.0.00.000.0.000.0.0.0.0\t" + // bad OID
      cert3.sha256Fingerprint +
      "\t",
    "example.com:443:\t.0.0.0.0\t" + // malformed OID
      cert3.sha256Fingerprint +
      "\t",
    "example.com:443:\t\t" + // missing OID
      cert3.sha256Fingerprint +
      "\t",
    "example.com:443:\tOID.2.16.840.1.101.3.4.2.1\t", // missing fingerprint
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
