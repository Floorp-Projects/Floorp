const header_missing_signature = "header1: content1";
const header_invalid_signature = "header1: content1\r\nmanifest-signature: invalid-signature\r\n";
const header = "manifest-signature: MIIF1AYJKoZIhvcNAQcCoIIFxTCCBcECAQExCzAJBgUrDgMCGgUAMAsGCSqGSIb3DQEHAaCCA54wggOaMIICgqADAgECAgECMA0GCSqGSIb3DQEBCwUAMHMxCzAJBgNVBAYTAlVTMQswCQYDVQQIEwJDQTEWMBQGA1UEBxMNTW91bnRhaW4gVmlldzEkMCIGA1UEChMbRXhhbXBsZSBUcnVzdGVkIENvcnBvcmF0aW9uMRkwFwYDVQQDExBUcnVzdGVkIFZhbGlkIENBMB4XDTE1MDkxMDA4MDQzNVoXDTM1MDkxMDA4MDQzNVowdDELMAkGA1UEBhMCVVMxCzAJBgNVBAgTAkNBMRYwFAYDVQQHEw1Nb3VudGFpbiBWaWV3MSQwIgYDVQQKExtFeGFtcGxlIFRydXN0ZWQgQ29ycG9yYXRpb24xGjAYBgNVBAMTEVRydXN0ZWQgQ29ycCBDZXJ0MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAts8whjOzEbn/w1xkFJ67af7F/JPujBK91oyJekh2schIMzFau9pY8S1AiJQoJCulOJCJfUc8hBLKBZiGAkii+4Gpx6cVqMLe6C22MdD806Soxn8Dg4dQqbIvPuI4eeVKu5CEk80PW/BaFMmRvRHO62C7PILuH6yZeGHC4P7dTKpsk4CLxh/jRGXLC8jV2BCW0X+3BMbHBg53NoI9s1Gs7KGYnfOHbBP5wEFAa00RjHnubUaCdEBlC8Kl4X7p0S4RGb3rsB08wgFe9EmSZHIgcIm+SuVo7N4qqbI85qo2ulU6J8NN7ZtgMPHzrMhzgAgf/KnqPqwDIxnNmRNJmHTUYwIDAQABozgwNjAMBgNVHRMBAf8EAjAAMBYGA1UdJQEB/wQMMAoGCCsGAQUFBwMDMA4GA1UdDwEB/wQEAwIHgDANBgkqhkiG9w0BAQsFAAOCAQEAukH6cJUUj5faa8CuPCqrEa0PoLY4SYNnff9NI+TTAHkB9l+kOcFl5eo2EQOcWmZKYi7QLlWC4jy/KQYattO9FMaxiOQL4FAc6ZIbNyfwWBzZWyr5syYJTTTnkLq8A9pCKarN49+FqhJseycU+8EhJEJyP5pv5hLvDNTTHOQ6SXhASsiX8cjo3AY4bxA5pWeXuTZ459qDxOnQd+GrOe4dIeqflk0hA2xYKe3SfF+QlK8EO370B8Dj8RX230OATM1E3OtYyALe34KW3wM9Qm9rb0eViDnVyDiCWkhhQnw5yPg/XQfloug2itRYuCnfUoRt8xfeHgwz2Ymz8cUADn3KpTGCAf4wggH6AgEBMHgwczELMAkGA1UEBhMCVVMxCzAJBgNVBAgTAkNBMRYwFAYDVQQHEw1Nb3VudGFpbiBWaWV3MSQwIgYDVQQKExtFeGFtcGxlIFRydXN0ZWQgQ29ycG9yYXRpb24xGTAXBgNVBAMTEFRydXN0ZWQgVmFsaWQgQ0ECAQIwCQYFKw4DAhoFAKBdMBgGCSqGSIb3DQEJAzELBgkqhkiG9w0BBwEwHAYJKoZIhvcNAQkFMQ8XDTE1MDkxMDA4MDQ0M1owIwYJKoZIhvcNAQkEMRYEFNg6lGtV9bJbL2hA0c5DdOeuCQ6lMA0GCSqGSIb3DQEBAQUABIIBAKGziwzA5Q38rIvNUDHCjYVTR1FhALGZv677Tc2+pwd82W6O9q5GG9IfkF3ajb1fquUIpGPkf7r0oiO4udC8cSehA+lfhR94A8aCM9UhzvTtRI3tFB+TPSk1UcXlX8tB7dNkx4zC06ujlSaRKkmaZODVXQFEcsF6CKMApsBuUJrwzvbQqVi2KHXUO6oGlMEyt4tY+g2OY/vyxGajfAL49dAYOTtrV0arvJvoTYh+E0iSrsbuiuAxKAVjK/QnLJoV/dTaCqW4t3lzHrpE3+avqMXiewxu84VJSURxoryY89uAZS9+4MKrSOGlGCJy/8xDIAm9pi6lPJBP2pIRjaRt9r0=\r\n";

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
      "integrity": "B5Phw8L1tpyRBkI0gwg/evy1fgtMlMq3BIY3Q8X0rYU="
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
  "moz-uuid": "some-uuid",
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
        do_test_finished();
        run_next_test();
      }
    };
    packagedAppUtils.verifyManifest(aHeader, aManifest, fakeVerifier);
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
