/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// This test checks parsing of the the certificate override file

function run_test() {
  // These are hard-coded to avoid initialization of NSS before setup is complete
  let cert1 = {
    sha256Fingerprint:
      "28:23:D2:C2:12:72:B7:18:7E:16:EF:5E:DC:EF:BC:47:F5:77:97:FE:92:02:B6:93:8F:2B:7E:38:67:D9:B1:C6",
    dbKey:
      "AAAAAAAAAAAAAAAUAAAAGwcen4iQvZOEd92XMz1cNtuzqW/VMBkxFzAVBgNVBAMMDlRlc3QgTUlUTSBSb290",
  };
  let cert2 = {
    sha256Fingerprint:
      "B1:C4:8D:60:81:2E:E6:3A:68:76:21:DE:E3:59:C7:61:D1:3D:18:DA:20:6F:B4:A5:A9:F2:11:F3:15:75:D7:71",
    dbKey:
      "AAAAAAAAAAAAAAAUAAAAKD3l/SUhSJbIduh6dR6xr8hmLYK1MCYxJDAiBgNVBAMMG1NlbGYtc2lnbmVkIFRlc3QgRW5kLWVudGl0eQ==",
  };
  let cert3 = {
    sha256Fingerprint:
      "7C:B3:99:E7:F5:46:C1:6A:B1:32:1C:7B:FB:42:D4:44:3A:21:FC:8B:19:53:18:E5:D6:7B:8E:A2:E2:A0:70:C3",
    dbKey:
      "AAAAAAAAAAAAAAAUAAAAFA9mlHOHAkz8WvHc1Vckzg2BpWeHMBIxEDAOBgNVBAMMB1Rlc3QgQ0E=",
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
      bits: 2,
      attributes: { privateBrowsingId: 1 },
    },
    {
      host: "test.example.com",
      port: 443,
      cert: cert1,
      bits: 2,
      attributes: { privateBrowsingId: 2 },
    },
    {
      host: "example.com",
      port: 443,
      cert: cert2,
      bits: 1,
      attributes: {},
    },
    {
      host: "example.com",
      port: 443,
      cert: cert2,
      bits: 1,
      attributes: { userContextId: 1 }, // only privateBrowsingId is used
    },
    {
      host: "old.example.com",
      port: 443,
      cert: cert1,
      bits: 2,
      attributes: {},
    },
  ];
  const BAD_OVERRIDES = [
    {
      host: "test.example.com",
      port: 443,
      cert: cert1,
      bits: 2,
      attributes: { privateBrowsingId: 3 }, // wrong attributes
    },
    {
      host: "test.example.com",
      port: 443,
      cert: cert3, // wrong certificate
      bits: 1,
      attributes: { privateBrowsingId: 1 },
    },
    {
      host: "example.com",
      port: 443,
      cert: cert3,
      bits: 1,
      attributes: {},
    },
  ];
  const BAD_BIT_OVERRIDES = [
    {
      host: "example.com",
      port: 443,
      cert: cert2,
      bits: 2, // wrong bits
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
