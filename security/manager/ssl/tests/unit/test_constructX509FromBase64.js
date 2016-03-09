// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// Checks that ConstructX509FromBase64() accepts valid input and rejects invalid
// input.

do_get_profile(); // Must be called before getting nsIX509CertDB
const certDB = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

function excMessage(e) {
  if (e.message) {
    let msg = e.message;
    if (e.data) {
      msg = msg + ": " + e.data;
    }
    return msg;
  }

  return e.toString();
}

function testGood(data) {
  try {
    let cert = certDB.constructX509FromBase64(data.cert);
    equal(cert.commonName, data.cn,
          "Actual and expected commonName should match");
  } catch (e) {
    do_print(`Exception: ${excMessage(e)}`);
    ok(false, `Should not have gotten an exception for "CN=${data.cn}"`);
  }
}

function testBad(data) {
  throws(() => certDB.constructX509FromBase64(data.input), data.result,
         `Should get "${data.result}" for "${data.input}"`);
}

function run_test() {
  const badCases = [
    // Wrong type or too short
    { input: null, result: /NS_ERROR_ILLEGAL_VALUE/ },
    { input: "", result: /NS_ERROR_ILLEGAL_VALUE/ },
    { input: "=", result: /NS_ERROR_ILLEGAL_VALUE/ },
    { input: "==", result: /NS_ERROR_ILLEGAL_VALUE/ },
    // Not base64
    { input: "forty-four dead stone lions", result: /NS_ERROR_ILLEGAL_VALUE/ },
    // Not a cert
    { input: "Zm9ydHktZm91ciBkZWFkIHN0b25lIGxpb25z",
      result: /NS_ERROR_FAILURE/ },
  ];

  // Real certs with all three padding levels
  const goodCases = [
    { cn: "A", cert: "MIHhMIGcAgEAMA0GCSqGSIb3DQEBBQUAMAwxCjAIBgNVBAMTAUEwHhcNMTEwMzIzMjMyNTE3WhcNMTEwNDIyMjMyNTE3WjAMMQowCAYDVQQDEwFBMEwwDQYJKoZIhvcNAQEBBQADOwAwOAIxANFm7ZCfYNJViaDWTFuMClX3+9u18VFGiyLfM6xJrxir4QVtQC7VUC/WUGoBUs9COQIDAQABMA0GCSqGSIb3DQEBBQUAAzEAx2+gIwmuYjJO5SyabqIm4lB1MandHH1HQc0y0tUFshBOMESTzQRPSVwPn77a6R9t" },
    { cn: "Bo", cert: "MIHjMIGeAgEAMA0GCSqGSIb3DQEBBQUAMA0xCzAJBgNVBAMTAkJvMB4XDTExMDMyMzIzMjYwMloXDTExMDQyMjIzMjYwMlowDTELMAkGA1UEAxMCQm8wTDANBgkqhkiG9w0BAQEFAAM7ADA4AjEA1FoSl9w9HqMqVgk2K0J3OTiRsgHeNsQdPUl6S82ME33gH+E56PcWZA3nse+fpS3NAgMBAAEwDQYJKoZIhvcNAQEFBQADMQAo/e3BvQAmygiATljQ68tWPoWcbMwa1xxAvpWTEc1LOvMqeDBinBUqbAbSmPhGWb4=" },
    { cn: "Cid", cert: "MIHlMIGgAgEAMA0GCSqGSIb3DQEBBQUAMA4xDDAKBgNVBAMTA0NpZDAeFw0xMTAzMjMyMzI2MzJaFw0xMTA0MjIyMzI2MzJaMA4xDDAKBgNVBAMTA0NpZDBMMA0GCSqGSIb3DQEBAQUAAzsAMDgCMQDUUxlF5xKN+8KCSsR83sN+SRwJmZdliXsnBB7PU0OgbmOWN0u8yehRkmu39kN9tzcCAwEAATANBgkqhkiG9w0BAQUFAAMxAJ3UScNqRcjHFrNu4nuwRldZLJlVJvRYXp982V4/kYodQEGN4gJ+Qyj+HTsaXy5x/w==" }
  ];

  for (let badCase of badCases) {
    testBad(badCase);
  }
  for (let goodCase of goodCases) {
    testGood(goodCase);
  }
}
