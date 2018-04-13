/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/CertUtils.jsm");

const PREF_PREFIX = "certutils.certs.";

function resetPrefs() {
  var prefs = Services.prefs.getChildList(PREF_PREFIX);
  prefs.forEach(Services.prefs.clearUserPref);
}

function attributes_match(aCert, aExpected) {
  if (Object.keys(aCert).length != Object.keys(aExpected).length)
    return false;

  for (var attribute in aCert) {
    if (!(attribute in aExpected))
      return false;
    if (aCert[attribute] != aExpected[attribute])
      return false;
  }

  return true;
}

function test_results(aCerts, aExpected) {
  Assert.equal(aCerts.length, aExpected.length);

  for (var i = 0; i < aCerts.length; i++) {
    if (!attributes_match(aCerts[i], aExpected[i])) {
      dump("Attributes for certificate " + (i + 1) + " did not match expected attributes\n");
      dump("Saw: " + aCerts[i].toSource() + "\n");
      dump("Expected: " + aExpected[i].toSource() + "\n");
      Assert.ok(false);
    }
  }
}

add_test(function test_singleCert() {
  Services.prefs.setCharPref(PREF_PREFIX + "1.attribute1", "foo");
  Services.prefs.setCharPref(PREF_PREFIX + "1.attribute2", "bar");

  var certs = CertUtils.readCertPrefs(PREF_PREFIX);
  test_results(certs, [{
    attribute1: "foo",
    attribute2: "bar"
  }]);

  resetPrefs();
  run_next_test();
});

add_test(function test_multipleCert() {
  Services.prefs.setCharPref(PREF_PREFIX + "1.md5Fingerprint", "cf84a9a2a804e021f27cb5128fe151f4");
  Services.prefs.setCharPref(PREF_PREFIX + "1.nickname", "1st cert");
  Services.prefs.setCharPref(PREF_PREFIX + "2.md5Fingerprint", "9441051b7eb50e5ca2226095af710c1a");
  Services.prefs.setCharPref(PREF_PREFIX + "2.nickname", "2nd cert");

  var certs = CertUtils.readCertPrefs(PREF_PREFIX);
  test_results(certs, [{
    md5Fingerprint: "cf84a9a2a804e021f27cb5128fe151f4",
    nickname: "1st cert"
  }, {
    md5Fingerprint: "9441051b7eb50e5ca2226095af710c1a",
    nickname: "2nd cert"
  }]);

  resetPrefs();
  run_next_test();
});

add_test(function test_skippedCert() {
  Services.prefs.setCharPref(PREF_PREFIX + "1.issuerName", "Mozilla");
  Services.prefs.setCharPref(PREF_PREFIX + "1.nickname", "1st cert");
  Services.prefs.setCharPref(PREF_PREFIX + "2.issuerName", "Top CA");
  Services.prefs.setCharPref(PREF_PREFIX + "2.nickname", "2nd cert");
  Services.prefs.setCharPref(PREF_PREFIX + "4.issuerName", "Unknown CA");
  Services.prefs.setCharPref(PREF_PREFIX + "4.nickname", "Ignored cert");

  var certs = CertUtils.readCertPrefs(PREF_PREFIX);
  test_results(certs, [{
    issuerName: "Mozilla",
    nickname: "1st cert"
  }, {
    issuerName: "Top CA",
    nickname: "2nd cert"
  }]);

  resetPrefs();
  run_next_test();
});
