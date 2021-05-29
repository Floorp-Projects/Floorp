/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const certService = Cc["@mozilla.org/security/local-cert-service;1"].getService(
  Ci.nsILocalCertService
);
const overrideService = Cc["@mozilla.org/security/certoverride;1"].getService(
  Ci.nsICertOverrideService
);
const certDB = Cc["@mozilla.org/security/x509certdb;1"].getService(
  Ci.nsIX509CertDB
);

const CERT_TEST =
  "MIHhMIGcAgEAMA0GCSqGSIb3DQEBBQUAMAwxCjAIBgNVBAMTAUEwHhcNMTEwMzIzMjMyNTE3WhcNMTEwNDIyMjMyNTE3WjAMMQowCAYDVQQDEwFBMEwwDQYJKoZIhvcNAQEBBQADOwAwOAIxANFm7ZCfYNJViaDWTFuMClX3+9u18VFGiyLfM6xJrxir4QVtQC7VUC/WUGoBUs9COQIDAQABMA0GCSqGSIb3DQEBBQUAAzEAx2+gIwmuYjJO5SyabqIm4lB1MandHH1HQc0y0tUFshBOMESTzQRPSVwPn77a6R9t";

add_task(async function() {
  Assert.ok(Services.clearData);

  const TEST_URI = Services.io.newURI("http://test.com/");
  const ANOTHER_TEST_URI = Services.io.newURI("https://example.com/");
  const YET_ANOTHER_TEST_URI = Services.io.newURI("https://example.test/");
  let cert = certDB.constructX509FromBase64(CERT_TEST);
  let flags = Ci.nsIClearDataService.CLEAR_CERT_EXCEPTIONS;

  ok(cert, "Cert was created");

  Assert.equal(
    overrideService.isCertUsedForOverrides(cert, true, true),
    0,
    "Cert should not be used for override yet"
  );

  overrideService.rememberValidityOverride(
    TEST_URI.asciiHost,
    TEST_URI.port,
    cert,
    flags,
    false
  );

  Assert.equal(
    overrideService.isCertUsedForOverrides(cert, true, true),
    1,
    "Cert should be used for override now"
  );

  await new Promise(aResolve => {
    Services.clearData.deleteDataFromHost(
      TEST_URI.asciiHostPort,
      true /* user request */,
      flags,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  Assert.equal(
    overrideService.isCertUsedForOverrides(cert, true, true),
    0,
    "Cert should not be used for override now"
  );

  for (let uri of [TEST_URI, ANOTHER_TEST_URI, YET_ANOTHER_TEST_URI]) {
    overrideService.rememberValidityOverride(
      uri.asciiHost,
      uri.port,
      cert,
      flags,
      false
    );
    Assert.ok(
      overrideService.hasMatchingOverride(
        uri.asciiHost,
        uri.port,
        cert,
        {},
        {}
      ),
      `Should have added override for ${uri.asciiHost}:${uri.port}`
    );
  }

  await new Promise(aResolve => {
    Services.clearData.deleteData(flags, value => {
      Assert.equal(value, 0);
      aResolve();
    });
  });

  for (let uri of [TEST_URI, ANOTHER_TEST_URI, YET_ANOTHER_TEST_URI]) {
    Assert.ok(
      !overrideService.hasMatchingOverride(
        uri.asciiHost,
        uri.port,
        cert,
        {},
        {}
      ),
      `Should have removed override for ${uri.asciiHost}:${uri.port}`
    );
  }
});

add_task(async function test_deleteByBaseDomain() {
  let toClear = [
    Services.io.newURI("https://example.com"),
    Services.io.newURI("http://example.com:8080"),
    Services.io.newURI("http://test1.example.com"),
    Services.io.newURI("http://foo.bar.example.com"),
  ];

  let toKeep = [
    Services.io.newURI("https://example.org"),
    Services.io.newURI("http://test1.example.org"),
    Services.io.newURI("http://foo.bar.example.org"),
    Services.io.newURI("http://example.test"),
  ];

  let all = toClear.concat(toKeep);

  let cert = certDB.constructX509FromBase64(CERT_TEST);
  ok(cert, "Cert was created");
  Assert.equal(
    overrideService.isCertUsedForOverrides(cert, true, true),
    0,
    "Cert should not be used for override yet"
  );

  let overrideBits =
    Ci.nsICertOverrideService.ERROR_UNTRUSTED |
    Ci.nsICertOverrideService.ERROR_MISMATCH;

  all.forEach(({ asciiHost, port }) =>
    overrideService.rememberValidityOverride(
      asciiHost,
      port,
      cert,
      overrideBits,
      false
    )
  );

  Assert.equal(
    overrideService.isCertUsedForOverrides(cert, true, true),
    all.length,
    "Cert should be used for override now"
  );

  await new Promise(aResolve => {
    Services.clearData.deleteDataFromBaseDomain(
      "example.com",
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_CERT_EXCEPTIONS,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  Assert.equal(
    overrideService.isCertUsedForOverrides(cert, true, true),
    toKeep.length,
    "Cert should still be used for override"
  );

  toClear.forEach(({ asciiHost, port }) =>
    Assert.ok(
      !overrideService.hasMatchingOverride(asciiHost, port, cert, {}, {}),
      `Should have cleared override for ${asciiHost}:${port}`
    )
  );

  toKeep.forEach(({ asciiHost, port }) =>
    Assert.ok(
      overrideService.hasMatchingOverride(asciiHost, port, cert, {}, {}),
      `Should have kept override for ${asciiHost}:${port}`
    )
  );

  // Cleanup
  overrideService.clearAllOverrides();
});
