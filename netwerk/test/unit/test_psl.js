"use strict";

var etld = Cc["@mozilla.org/network/effective-tld-service;1"].getService(
  Ci.nsIEffectiveTLDService
);

var idna = Cc["@mozilla.org/network/idn-service;1"].getService(
  Ci.nsIIDNService
);

function run_test() {
  var file = do_get_file("data/test_psl.txt");
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var uri = ios.newFileURI(file);
  var scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(
    Ci.mozIJSSubScriptLoader
  );
  var srvScope = {};
  scriptLoader.loadSubScript(uri.spec, srvScope);
}

function checkPublicSuffix(host, expectedSuffix) {
  var actualSuffix = null;
  try {
    actualSuffix = etld.getBaseDomainFromHost(host);
  } catch (e) {
    if (
      e.result != Cr.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS &&
      e.result != Cr.NS_ERROR_ILLEGAL_VALUE
    ) {
      throw e;
    }
  }
  // The EffectiveTLDService always gives back punycoded labels.
  // The test suite wants to get back what it put in.
  if (
    actualSuffix !== null &&
    expectedSuffix !== null &&
    /(^|\.)xn--/.test(actualSuffix) &&
    !/(^|\.)xn--/.test(expectedSuffix)
  ) {
    actualSuffix = idna.convertACEtoUTF8(actualSuffix);
  }
  Assert.equal(actualSuffix, expectedSuffix);
}
