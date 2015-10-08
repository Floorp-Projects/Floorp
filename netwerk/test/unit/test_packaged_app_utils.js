const header_missing_signature = "header1: content1";
const header_invalid_signature = "header1: content1\r\nmanifest-signature: invalid-signature\r\n";
const header = "manifest-signature: MIIF1AYJKoZIhvcNAQcCoIIFxTCCBcECAQExCzAJBgUrDgMCGgUAMAsGCSqGSIb3DQEHAaCCA54wggOaMIICgqADAgECAgECMA0GCSqGSIb3DQEBCwUAMHMxCzAJBgNVBAYTAlVTMQswCQYDVQQIEwJDQTEWMBQGA1UEBxMNTW91bnRhaW4gVmlldzEkMCIGA1UEChMbRXhhbXBsZSBUcnVzdGVkIENvcnBvcmF0aW9uMRkwFwYDVQQDExBUcnVzdGVkIFZhbGlkIENBMB4XDTE1MDkxMDA4MDQzNVoXDTM1MDkxMDA4MDQzNVowdDELMAkGA1UEBhMCVVMxCzAJBgNVBAgTAkNBMRYwFAYDVQQHEw1Nb3VudGFpbiBWaWV3MSQwIgYDVQQKExtFeGFtcGxlIFRydXN0ZWQgQ29ycG9yYXRpb24xGjAYBgNVBAMTEVRydXN0ZWQgQ29ycCBDZXJ0MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAts8whjOzEbn/w1xkFJ67af7F/JPujBK91oyJekh2schIMzFau9pY8S1AiJQoJCulOJCJfUc8hBLKBZiGAkii+4Gpx6cVqMLe6C22MdD806Soxn8Dg4dQqbIvPuI4eeVKu5CEk80PW/BaFMmRvRHO62C7PILuH6yZeGHC4P7dTKpsk4CLxh/jRGXLC8jV2BCW0X+3BMbHBg53NoI9s1Gs7KGYnfOHbBP5wEFAa00RjHnubUaCdEBlC8Kl4X7p0S4RGb3rsB08wgFe9EmSZHIgcIm+SuVo7N4qqbI85qo2ulU6J8NN7ZtgMPHzrMhzgAgf/KnqPqwDIxnNmRNJmHTUYwIDAQABozgwNjAMBgNVHRMBAf8EAjAAMBYGA1UdJQEB/wQMMAoGCCsGAQUFBwMDMA4GA1UdDwEB/wQEAwIHgDANBgkqhkiG9w0BAQsFAAOCAQEAukH6cJUUj5faa8CuPCqrEa0PoLY4SYNnff9NI+TTAHkB9l+kOcFl5eo2EQOcWmZKYi7QLlWC4jy/KQYattO9FMaxiOQL4FAc6ZIbNyfwWBzZWyr5syYJTTTnkLq8A9pCKarN49+FqhJseycU+8EhJEJyP5pv5hLvDNTTHOQ6SXhASsiX8cjo3AY4bxA5pWeXuTZ459qDxOnQd+GrOe4dIeqflk0hA2xYKe3SfF+QlK8EO370B8Dj8RX230OATM1E3OtYyALe34KW3wM9Qm9rb0eViDnVyDiCWkhhQnw5yPg/XQfloug2itRYuCnfUoRt8xfeHgwz2Ymz8cUADn3KpTGCAf4wggH6AgEBMHgwczELMAkGA1UEBhMCVVMxCzAJBgNVBAgTAkNBMRYwFAYDVQQHEw1Nb3VudGFpbiBWaWV3MSQwIgYDVQQKExtFeGFtcGxlIFRydXN0ZWQgQ29ycG9yYXRpb24xGTAXBgNVBAMTEFRydXN0ZWQgVmFsaWQgQ0ECAQIwCQYFKw4DAhoFAKBdMBgGCSqGSIb3DQEJAzELBgkqhkiG9w0BBwEwHAYJKoZIhvcNAQkFMQ8XDTE1MTAwMTIxMTEwNlowIwYJKoZIhvcNAQkEMRYEFHAisUYrrt+gBxYFhZ5KQQusOmN3MA0GCSqGSIb3DQEBAQUABIIBACHW4V0BsPWOvWrGOTRj6mPpNbH/JI1bN2oyqQZrpUQoaBY+BbYxO7TY4Uwe+aeIR/TTPJznOMF/dl3Bna6TPabezU4ylg7TVFI6W7zC5f5DZKp+Xv6uTX6knUzbbW1fkJqMtE8hGUzYXc3/C++Ci6kuOzrpWOhk6DpJHeUO/ioV56H0+QK/oMAjYpEsOohaPqvTPNOBhMQ0OQP3bmuJ6HcjZ/oz96PpzXUPKT1tDe6VykIYkV5NvtC8Tu2lDbYvp9ug3gyDgdyNSV47y5i/iWkzEhsAJB+9Z50wKhplnkxxVHEXkB/6tmfvExvQ28gLd/VbaEGDX2ljCaTSUjhD0o0=\r\n";

const manifest = "Content-Location: manifest.webapp\r\n" +
  "Content-Type: application/x-web-app-manifest+json\r\n\r\n" +
`{
  "name": "My App",
  "moz-resources": [
    {
      "src": "page2.html",
      "integrity": "JREF3JbXGvZ+I1KHtoz3f46ZkeIPrvXtG4VyFQrJ7II="
    },
    {
      "src": "index.html",
      "integrity": "zEubR310nePwd30NThIuoCxKJdnz7Mf5z+dZHUbH1SE="
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
  "moz-package-location": "https://example.com/myapp/app.pak",
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
