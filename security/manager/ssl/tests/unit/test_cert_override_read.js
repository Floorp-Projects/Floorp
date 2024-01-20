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
      "E3:E3:56:4C:6D:81:DA:29:E4:52:20:A1:7A:31:E2:03:F1:82:A6:D5:B1:5B:6A:86:D6:10:CF:AE:BA:3B:35:2A",
  };
  // bad_certs/selfsigned.pem
  let cert2 = {
    sha256Fingerprint:
      "9A:C8:37:86:6F:1A:20:A2:31:6F:FE:92:68:CE:05:D2:8C:72:F3:A3:E0:23:3B:AD:8A:28:19:93:82:E8:AE:24",
  };
  // bad_certs/noValidNames.pem
  let cert3 = {
    sha256Fingerprint:
      "67:7C:84:51:32:B5:0B:63:E4:40:B4:1A:33:FD:20:34:0A:B3:1D:61:24:F1:7A:40:14:39:05:66:42:FD:C2:EA",
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
