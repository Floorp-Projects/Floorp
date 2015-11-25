const header_missing_signature = "header1: content1";
const header_invalid_signature = "header1: content1\r\nmanifest-signature: invalid-signature\r\n";
const header = "manifest-signature: MIIF1AYJKoZIhvcNAQcCoIIFxTCCBcECAQExCzAJBgUrDgMCGgUAMAsGCSqGSIb3DQEHAaCCA54wggOaMIICgqADAgECAgEEMA0GCSqGSIb3DQEBCwUAMHMxCzAJBgNVBAYTAlVTMQswCQYDVQQIEwJDQTEWMBQGA1UEBxMNTW91bnRhaW4gVmlldzEkMCIGA1UEChMbRXhhbXBsZSBUcnVzdGVkIENvcnBvcmF0aW9uMRkwFwYDVQQDExBUcnVzdGVkIFZhbGlkIENBMB4XDTE1MTExOTAzMDEwNVoXDTM1MTExOTAzMDEwNVowdDELMAkGA1UEBhMCVVMxCzAJBgNVBAgTAkNBMRYwFAYDVQQHEw1Nb3VudGFpbiBWaWV3MSQwIgYDVQQKExtFeGFtcGxlIFRydXN0ZWQgQ29ycG9yYXRpb24xGjAYBgNVBAMTEVRydXN0ZWQgQ29ycCBDZXJ0MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAzPback9X7RRxKTc3/5o2vm9Ro6XNiSM9NPsN3djjCIVz50bY0rJkP98zsqpFjnLwqHeJigxyYoqFexRhRLgKrG10CxNl4rxP6CEPENjvj5FfbX/HUZpT/DelNR18F498yD95vSHcSrCc3JrjV3bKA+wgt11E4a0Ba95S1RuwtehZw1+Y4hO8nHpbSGfjD0BpluFY2nDoYAm+aWSrsmLuJsKLO8Xn2I1brZFJUynR3q1ujuDE9EJk1niDLfOeVgXM4AavJS5C0ZBHhAhR2W+K9NN97jpkpmHFqecTwDXB7rEhsyB3185cI7anaaQfHHfH5+4SD+cMDNtYIOSgLO06ZwIDAQABozgwNjAMBgNVHRMBAf8EAjAAMBYGA1UdJQEB/wQMMAoGCCsGAQUFBwMDMA4GA1UdDwEB/wQEAwIHgDANBgkqhkiG9w0BAQsFAAOCAQEAlnVyLz5dPhS0ZhZD6qJOUzSo6nFwMxNX1m0oS37mevtuh0b0o1gmEuMw3mVxiAVkC2vPsoxBL2wLlAkcEdBPxGEqhBmtiBY3F3DgvEkf+/sOY1rnr6O1qLZuBAnPzA1Vnco8Jwf0DYF0PxaRd8yT5XSl5qGpM2DItEldZwuKKaL94UEgIeC2c+Uv/IOyrv+EyftX96vcmRwr8ghPFLQ+36H5nuAKEpDD170EvfWl1zs0dUPiqSI6l+hy5V14gl63Woi34L727+FKx8oatbyZtdvbeeOmenfTLifLomnZdx+3WMLkp3TLlHa5xDLwifvZtBP2d3c6zHp7gdrGU1u2WTGCAf4wggH6AgEBMHgwczELMAkGA1UEBhMCVVMxCzAJBgNVBAgTAkNBMRYwFAYDVQQHEw1Nb3VudGFpbiBWaWV3MSQwIgYDVQQKExtFeGFtcGxlIFRydXN0ZWQgQ29ycG9yYXRpb24xGTAXBgNVBAMTEFRydXN0ZWQgVmFsaWQgQ0ECAQQwCQYFKw4DAhoFAKBdMBgGCSqGSIb3DQEJAzELBgkqhkiG9w0BBwEwHAYJKoZIhvcNAQkFMQ8XDTE1MTExOTAzMDEyM1owIwYJKoZIhvcNAQkEMRYEFJ7c42bSAcIZOL+r8PIsMNAXByYiMA0GCSqGSIb3DQEBAQUABIIBABPrgGwvi9bwgUEqiL9I0UpQsEEjzNvID9iSt9cicB3exQ9z3H2b47P7HOWicdYI4OQ1Gb+1OjGWJWoBbOuF+ty4HNIMlLiq+Mhm82J2fTA9h8v9BohjvjwMSECJHFQR1pp6veCrWqIvkhHLX/GcGZIr5vBmkpkcMpWKT/N8VmuNu/DKHtKiTianAWnFguAruOeL02MfKlg8mNLCQysd6qj5mbLhTvR7mtn6XaRFrHB4VSfYPerzP71/AzhVqlPRLIYQocSkRoAE+eNeMcP6RcEinKRqK0SxqmRWeSiJpkDm+5Hlx7Xs8Qamqiteyr8FN7VjkGByM0IcpH6Bd5h3trQ=\r\n";

const manifest = "Content-Location: manifest.webapp\r\n" +
  "Content-Type: application/x-web-app-manifest+json\r\n\r\n" +
`{
  "moz-package-origin": "http://mochi.test:8888",
  "name": "My App",
  "moz-resources": [
    {
      "src": "page2.html",
      "integrity": "JREF3JbXGvZ+I1KHtoz3f46ZkeIPrvXtG4VyFQrJ7II="
    },
    {
      "src": "index.html",
      "integrity": "IjQ2S/V9qsC7wW5uv/Niq40M1aivvqH5+1GKRwUnyRg="
    },
    {
      "src": "scripts/script.js",
      "integrity": "6TqtNArQKrrsXEQWu3D9ZD8xvDRIkhyV6zVdTcmsT5Q="
    },
    {
      "src": "scripts/library.js",
      "integrity": "TN2ByXZiaBiBCvS4MeZ02UyNi44vED+KjdjLInUl4o8="
    }
  ],
  "moz-permissions": [
    {
      "systemXHR": {
        "description": "Needed to download stuff"
      },
      "devicestorage:pictures": {
        "description": "Need to load pictures"
      }
    }
  ],
  "package-identifier": "611FC2FE-491D-4A47-B3B3-43FBDF6F404F",
  "description": "A great app!"
}`;

const manifest_missing_moz_resources = "Content-Location: manifest.webapp\r\n" +
  "Content-Type: application/x-web-app-manifest+json\r\n\r\n" +
`{
  "name": "My App",
  "moz-permissions": [
    {
      "systemXHR": {
        "description": "Needed to download stuff"
      },
      "devicestorage:pictures": {
        "description": "Need to load pictures"
      }
    }
  ],
  "moz-uuid": "some-uuid",
  "moz-package-location": "https://example.com/myapp/app.pak",
  "description": "A great app!"
}`;

const manifest_malformed_json = "}";

var packagedAppUtils;

function run_test() {
  add_test(test_verify_manifest(header_missing_signature, manifest, false));
  add_test(test_verify_manifest(header_invalid_signature, manifest, false));
  add_test(test_verify_manifest(header, manifest_malformed_json, false));
  add_test(test_verify_manifest(header, manifest_missing_moz_resources, false));
  add_test(test_verify_manifest(header, manifest, true));

  // The last verification must succeed, because check_integrity use that object;
  add_test(test_check_integrity_success);
  add_test(test_check_integrity_filename_not_matched);
  add_test(test_check_integrity_hashvalue_not_matched);

  run_next_test();
}

function test_verify_manifest(aHeader, aManifest, aShouldSucceed) {
  return function() {
    do_test_pending();
    packagedAppUtils = Cc["@mozilla.org/network/packaged-app-utils;1"].
                       createInstance(Ci.nsIPackagedAppUtils);
    let fakeVerifier = {
      fireVerifiedEvent: function(aForManifest, aSuccess) {
        ok(aForManifest, "aForManifest should be true");
        equal(aSuccess, aShouldSucceed, "Expected verification result: " + aShouldSucceed);

        // Verify packageIdentifier if it's a successful verification.
        if (aShouldSucceed) {
           equal(packagedAppUtils.packageIdentifier,
                 '611FC2FE-491D-4A47-B3B3-43FBDF6F404F',
                 'package identifier');
        }

        do_test_finished();
        run_next_test();
      }
    };
    packagedAppUtils.verifyManifest(aHeader, aManifest, fakeVerifier, false);
  }
}

function test_check_integrity_success() {
  let manifestBody = manifest.substr(manifest.indexOf('\r\n\r\n') + 4);
  fakeVerifier = {
    fireVerifiedEvent: function(aForManifest, aSuccess) {
      ok(!aForManifest && aSuccess, "checkIntegrity should succeed");
      do_test_finished();
      run_next_test();
    }
  };
  for (let resource of JSON.parse(manifestBody)["moz-resources"]) {
    do_test_pending();
    packagedAppUtils.checkIntegrity(resource.src, resource.integrity, fakeVerifier);
  }
}

function test_check_integrity_filename_not_matched() {
  fakeVerifier = {
    fireVerifiedEvent: function(aForManifest, aSuccess) {
      ok(!aForManifest && !aSuccess, "checkIntegrity should fail");
      do_test_finished();
      run_next_test();
    }
  };
  do_test_pending();
  packagedAppUtils.checkIntegrity("/nosuchfile.html", "sha256-kass...eoirW-e", fakeVerifier);
  run_next_test();
}

function test_check_integrity_hashvalue_not_matched() {
  fakeVerifier = {
    fireVerifiedEvent: function(aForManifest, aSuccess) {
      ok(!aForManifest && !aSuccess, "checkIntegrity should fail");
      do_test_finished();
      run_next_test();
    }
  };
  do_test_pending();
  packagedAppUtils.checkIntegrity("/index.html", "kass...eoirW-e", fakeVerifier);
}
