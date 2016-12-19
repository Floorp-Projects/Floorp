/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// We require signature checks for this test
Services.prefs.setBoolPref(PREF_XPI_SIGNATURES_REQUIRED, true);
gUseRealCertChecks = true;

const CERT = `MIIDITCCAgmgAwIBAgIJALAv8fydd6nBMA0GCSqGSIb3DQEBBQUAMCcxJTAjBgNV
BAMMHGJvb3RzdHJhcDFAdGVzdHMubW96aWxsYS5vcmcwHhcNMTYwMjAyMjMxNjUy
WhcNMjYwMTMwMjMxNjUyWjAnMSUwIwYDVQQDDBxib290c3RyYXAxQHRlc3RzLm1v
emlsbGEub3JnMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA5caNuLTu
H8dEqNntLlhKi4y09hrgcF3cb6n5Xx9DIHA8CKiZxt9qGXKeeiDwEiiQ8ibJYzdc
jLkbzJUyPVUaH9ygrWynSpSTOvv/Ys3+ERrCo9W7Zuzwdmzt6TTEjFMS4lVx06us
3uUqkdp3JMgCqCEbOFZiztICiSKrp8QFJkAfApZzBqmJOPOWH0yZ2CRRzvbQZ6af
hqQDUalJQjWfsenyUWphhbREqExetxHJFR3OrmJt/shXVyz6dD7TBuE3PPUh1RpE
3ejVufcTzjV3XmK79PxsKLM9V2+ww9e9V3OET57kyvn+bpSWdUYm3X4DA8dxNW6+
kTFWRnQNZ+zQVQIDAQABo1AwTjAdBgNVHQ4EFgQUac36ccv+99N5HxYa8dCDYRaF
HNQwHwYDVR0jBBgwFoAUac36ccv+99N5HxYa8dCDYRaFHNQwDAYDVR0TBAUwAwEB
/zANBgkqhkiG9w0BAQUFAAOCAQEAFfu3MN8EtY5wcxOFdGShOmGQPm2MJJVE6MG+
p4RqHrukHZSgKOyWjkRk7t6NXzNcnHco9HFv7FQRAXSJ5zObmyu+TMZlu4jHHCav
GMcV3C/4SUGtlipZbgNe00UAIm6tM3Wh8dr38W7VYg4KGAwXou5XhQ9gCAnSn90o
H/42NqHTjJsR4v18izX2aO25ARQdMby7Lsr5j9RqweHywiSlPusFcKRseqOnIP0d
JT3+qh78LeMbNBO2mYD3SP/zu0TAmkAVNcj2KPw0+a0kVZ15rvslPC/K3xn9msMk
fQthv3rDAcsWvi9YO7T+vylgZBgJfn1ZqpQqy58xN96uh6nPOw==`;

function overrideCertDB() {
  // Unregister the real database.
  let registrar = Components.manager.QueryInterface(AM_Ci.nsIComponentRegistrar);
  let factory = registrar.getClassObject(CERTDB_CID, AM_Ci.nsIFactory);
  registrar.unregisterFactory(CERTDB_CID, factory);

  // Get the real DB
  let realCertDB = factory.createInstance(null, AM_Ci.nsIX509CertDB);

  let fakeCert = realCertDB.constructX509FromBase64(CERT.replace(/\n/g, ""));

  let fakeCertDB = {
    openSignedAppFileAsync(root, file, callback) {
      callback.openSignedAppFileFinished(Components.results.NS_OK, null, fakeCert);
    },

    verifySignedDirectoryAsync(root, dir, callback) {
      callback.verifySignedDirectoryFinished(Components.results.NS_OK, fakeCert);
    },

    QueryInterface: XPCOMUtils.generateQI([AM_Ci.nsIX509CertDB])
  };

  for (let property of Object.keys(realCertDB)) {
    if (property in fakeCertDB) {
      continue;
    }

    if (typeof realCertDB[property] == "function") {
      fakeCertDB[property] = realCertDB[property].bind(realCertDB);
    }
  }

  let certDBFactory = {
    createInstance: function(outer, iid) {
      if (outer != null) {
        throw Components.results.NS_ERROR_NO_AGGREGATION;
      }
      return fakeCertDB.QueryInterface(iid);
    }
  };
  registrar.registerFactory(CERTDB_CID, "CertDB",
                            CERTDB_CONTRACTID, certDBFactory);
}

add_task(function*() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  startupManager();

  // Once the application is started we shouldn't be able to replace the
  // certificate database
  overrideCertDB();

  let install = yield AddonManager.getInstallForFile(do_get_addon("test_bootstrap1_1"));
  do_check_eq(install.state, AddonManager.STATE_DOWNLOAD_FAILED);
});
