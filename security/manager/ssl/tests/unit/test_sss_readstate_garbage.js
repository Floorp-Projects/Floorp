/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// The purpose of this test is to create a mostly bogus site security service
// state file and see that the site security service handles it properly.

var gSSService = null;

function checkStateRead(aSubject, aTopic, aData) {
  if (aData == PRELOAD_STATE_FILE_NAME) {
    return;
  }

  equal(aData, SSS_STATE_FILE_NAME);

  const HSTS_HOSTS = [
    "https://example1.example.com",
    "https://example2.example.com",
  ];
  for (let host of HSTS_HOSTS) {
    ok(
      gSSService.isSecureURI(
        Ci.nsISiteSecurityService.HEADER_HSTS,
        Services.io.newURI(host),
        0
      ),
      `${host} should be HSTS enabled`
    );
  }

  const NOT_HSTS_HOSTS = [
    "https://example.com",
    "https://example3.example.com",
    "https://extra.comma.example.com",
    "https://empty.statestring.example.com",
    "https://rubbish.statestring.example.com",
    "https://spaces.statestring.example.com",
    "https://invalid.expirytime.example.com",
    "https://text.securitypropertystate.example.com",
    "https://invalid.securitypropertystate.example.com",
    "https://text.includesubdomains.example.com",
    "https://invalid.includesubdomains.example.com",
  ];
  for (let host of NOT_HSTS_HOSTS) {
    ok(
      !gSSService.isSecureURI(
        Ci.nsISiteSecurityService.HEADER_HSTS,
        Services.io.newURI(host),
        0
      ),
      `${host} should not be HSTS enabled`
    );
  }

  do_test_finished();
}

const PINNING_ROOT_KEY_HASH = "VCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8=";
const BASE64_BUT_NOT_SHA256 = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
const STARTS_WITH_NUMBER = "1ABC23defG/hiJKlmNoP+QRStuVwxYZ9a+bcD/+/EFg=";
const STARTS_WITH_SYMBOL = "+ABC23defG/hiJKlmNoP+QRStuVwxYZ9a+bcD/+/EFg=";
const MULTIPLE_KEYS =
  PINNING_ROOT_KEY_HASH + STARTS_WITH_NUMBER + STARTS_WITH_SYMBOL;

function run_test() {
  Services.prefs.setBoolPref("security.cert_pinning.hpkp.enabled", true);
  let profileDir = do_get_profile();
  let stateFile = profileDir.clone();
  stateFile.append(SSS_STATE_FILE_NAME);
  // Assuming we're working with a clean slate, the file shouldn't exist
  // until we create it.
  ok(!stateFile.exists());
  let outputStream = FileUtils.openFileOutputStream(stateFile);
  let expiryTime = Date.now() + 100000;
  let lines = [
    // General state file entry tests.
    `example1.example.com:HSTS\t0\t0\t${expiryTime},1,0`,
    "I'm a lumberjack and I'm okay; I work all night and I sleep all day!",
    "This is a totally bogus entry\t",
    "0\t0\t0\t0\t",
    "\t\t\t\t\t\t\t",
    "example.com:HSTS\t\t\t\t\t\t\t",
    "example3.example.com:HSTS\t0\t\t\t\t\t\t",
    `example2.example.com:HSTS\t0\t0\t${expiryTime},1,0`,
    // HSTS state string parsing tests
    `extra.comma.example.com:HSTS\t0\t0\t${expiryTime},,1,0`,
    "empty.statestring.example.com:HSTS\t0\t0\t",
    "rubbish.statestring.example.com:HSTS\t0\t0\tfoobar",
    `spaces.statestring.example.com:HSTS\t0\t0\t${expiryTime}, 1,0 `,
    `invalid.expirytime.example.com:HSTS\t0\t0\t${expiryTime}foo123,1,0`,
    `text.securitypropertystate.example.com:HSTS\t0\t0\t${expiryTime},1foo,0`,
    `invalid.securitypropertystate.example.com:HSTS\t0\t0\t${expiryTime},999,0`,
    `text.includesubdomains.example.com:HSTS\t0\t0\t${expiryTime},1,1foo`,
    `invalid.includesubdomains.example.com:HSTS\t0\t0\t${expiryTime},1,0foo`,
  ];
  writeLinesAndClose(lines, outputStream);
  Services.obs.addObserver(checkStateRead, "data-storage-ready");
  do_test_pending();
  gSSService = Cc["@mozilla.org/ssservice;1"].getService(
    Ci.nsISiteSecurityService
  );
  notEqual(gSSService, null);

  Services.prefs.setIntPref("security.cert_pinning.enforcement_level", 2);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("security.cert_pinning.enforcement_level");
  });
}
