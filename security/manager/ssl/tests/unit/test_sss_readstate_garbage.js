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
    ok(gSSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS,
                              Services.io.newURI(host),
                              0),
       `${host} should be HSTS enabled`);
  }

  const HPKP_HOSTS = [
    "https://keys.with.securitypropertyset.example.com",
    "https://starts.with.number.key.example.com",
    "https://starts.with.symbol.key.example.com",
    "https://multiple.keys.example.com",
  ];
  for (let host of HPKP_HOSTS) {
    ok(gSSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HPKP,
                              Services.io.newURI(host),
                              0),
       `${host} should be HPKP enabled`);
  }

  const NOT_HSTS_OR_HPKP_HOSTS = [
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
  for (let host of NOT_HSTS_OR_HPKP_HOSTS) {
    ok(!gSSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS,
                               Services.io.newURI(host),
                               0),
       `${host} should not be HSTS enabled`);
    ok(!gSSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HPKP,
                               Services.io.newURI(host),
                               0),
       `${host} should not be HPKP enabled`);
  }

  const NOT_HPKP_HOSTS = [
    "https://empty.keys.example.com",
    "https://rubbish.after.keys.example.com",
    "https://keys.with.securitypropertyunset.example.com",
    "https://keys.with.securitypropertyknockout.example.com",
    "https://securitypropertynegative.example.com",
    "https://not.sha256.key.example.com",
  ];
  for (let host of NOT_HPKP_HOSTS) {
    ok(!gSSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HPKP,
                               Services.io.newURI(host),
                               0),
       `${host} should not be HPKP enabled`);
  }

  do_test_finished();
}

const PINNING_ROOT_KEY_HASH = "VCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8=";
const BASE64_BUT_NOT_SHA256 = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
const STARTS_WITH_NUMBER = "1ABC23defG/hiJKlmNoP+QRStuVwxYZ9a+bcD/+/EFg=";
const STARTS_WITH_SYMBOL = "+ABC23defG/hiJKlmNoP+QRStuVwxYZ9a+bcD/+/EFg=";
const MULTIPLE_KEYS = PINNING_ROOT_KEY_HASH + STARTS_WITH_NUMBER +
                      STARTS_WITH_SYMBOL;

function run_test() {
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
    "example.com:HPKP\t\t\t\t\t\t\t",
    "example3.example.com:HSTS\t0\t\t\t\t\t\t",
    "example3.example.com:HPKP\t0\t\t\t\t\t\t",
    `example2.example.com:HSTS\t0\t0\t${expiryTime},1,0`,
    // HSTS and HPKP state string parsing tests
    `extra.comma.example.com:HSTS\t0\t0\t${expiryTime},,1,0`,
    `extra.comma.example.com:HPKP\t0\t0\t${expiryTime},,1,0,${PINNING_ROOT_KEY_HASH}`,
    "empty.statestring.example.com:HSTS\t0\t0\t",
    "empty.statestring.example.com:HPKP\t0\t0\t",
    "rubbish.statestring.example.com:HSTS\t0\t0\tfoobar",
    "rubbish.statestring.example.com:HPKP\t0\t0\tfoobar",
    `spaces.statestring.example.com:HSTS\t0\t0\t${expiryTime}, 1,0 `,
    `spaces.statestring.example.com:HPKP\t0\t0\t${expiryTime}, 1,0, ${PINNING_ROOT_KEY_HASH}`,
    `invalid.expirytime.example.com:HSTS\t0\t0\t${expiryTime}foo123,1,0`,
    `invalid.expirytime.example.com:HPKP\t0\t0\t${expiryTime}foo123,1,0,${PINNING_ROOT_KEY_HASH}`,
    `text.securitypropertystate.example.com:HSTS\t0\t0\t${expiryTime},1foo,0`,
    `text.securitypropertystate.example.com:HPKP\t0\t0\t${expiryTime},1foo,0,${PINNING_ROOT_KEY_HASH}`,
    `invalid.securitypropertystate.example.com:HSTS\t0\t0\t${expiryTime},999,0`,
    `invalid.securitypropertystate.example.com:HPKP\t0\t0\t${expiryTime},999,0,${PINNING_ROOT_KEY_HASH}`,
    `text.includesubdomains.example.com:HSTS\t0\t0\t${expiryTime},1,1foo`,
    `text.includesubdomains.example.com:HPKP\t0\t0\t${expiryTime},1,1foo,${PINNING_ROOT_KEY_HASH}`,
    `invalid.includesubdomains.example.com:HSTS\t0\t0\t${expiryTime},1,0foo`,
    `invalid.includesubdomains.example.com:HPKP\t0\t0\t${expiryTime},1,0foo,${PINNING_ROOT_KEY_HASH}`,
    // HPKP specific state string parsing tests
    `empty.keys.example.com:HPKP\t0\t0\t${expiryTime},1,0,`,
    `rubbish.after.keys.example.com:HPKP\t0\t0\t${expiryTime},1,0,${PINNING_ROOT_KEY_HASH}foo`,
    `keys.with.securitypropertyunset.example.com:HPKP\t0\t0\t${expiryTime},0,0,${PINNING_ROOT_KEY_HASH}`,
    `keys.with.securitypropertyset.example.com:HPKP\t0\t0\t${expiryTime},1,0,${PINNING_ROOT_KEY_HASH}`,
    `keys.with.securitypropertyknockout.example.com:HPKP\t0\t0\t${expiryTime},2,0,${PINNING_ROOT_KEY_HASH}`,
    `securitypropertynegative.example.com:HPKP\t0\t0\t${expiryTime},3,0,${PINNING_ROOT_KEY_HASH}`,
    `not.sha256.key.example.com:HPKP\t0\t0\t${expiryTime},1,0,${BASE64_BUT_NOT_SHA256}`,
    `starts.with.number.key.example.com:HPKP\t0\t0\t${expiryTime},1,0,${STARTS_WITH_NUMBER}`,
    `starts.with.symbol.key.example.com:HPKP\t0\t0\t${expiryTime},1,0,${STARTS_WITH_SYMBOL}`,
    `multiple.keys.example.com:HPKP\t0\t0\t${expiryTime},1,0,${MULTIPLE_KEYS}`,
  ];
  writeLinesAndClose(lines, outputStream);
  Services.obs.addObserver(checkStateRead, "data-storage-ready");
  do_test_pending();
  gSSService = Cc["@mozilla.org/ssservice;1"]
                 .getService(Ci.nsISiteSecurityService);
  notEqual(gSSService, null);

  Services.prefs.setIntPref("security.cert_pinning.enforcement_level", 2);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("security.cert_pinning.enforcement_level");
  });
}
