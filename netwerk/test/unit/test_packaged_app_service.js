//
// This file tests the packaged app service - nsIPackagedAppService
// NOTE: The order in which tests are run is important
//       If you need to add more tests, it's best to define them at the end
//       of the file and to add them at the end of run_test
//
// ----------------------------------------------------------------------------
//
// test_bad_args
//     - checks that calls to nsIPackagedAppService::GetResource do not accept a null argument
// test_callback_gets_called
//     - checks the regular use case -> requesting a resource should asynchronously return an entry
// test_same_content
//     - makes another request for the same file, and checks that the same content is returned
// test_request_number
//     - this test does not make a request, but checks that the package has only
//       been requested once. The entry returned by the call to getResource in
//       test_same_content should be returned from the cache.
//
// test_package_does_not_exist
//     - checks that requesting a file from a <package that does not exist>
//       calls the listener with an error code
// test_file_does_not_exist
//     - checks that requesting a <subresource that doesn't exist> inside a
//       package calls the listener with an error code
//
// test_bad_package
//    - tests that a package with missing headers for some of the files
//      will still return files that are correct
// test_bad_package_404
//    - tests that a request for a missing subresource doesn't hang if
//      if the last file in the package is missing some headers
//
// test_worse_package
//    - tests that a request for a missing/existing resource doesn't
//      break the service and the async verifier.
//

Cu.import('resource://gre/modules/LoadContextInfo.jsm');
Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

var gPrefs = Cc["@mozilla.org/preferences-service;1"]
               .getService(Components.interfaces.nsIPrefBranch);

// The number of times this package has been requested
// This number might be reset by tests that use it
var packagedAppRequestsMade = 0;
// The default content handler. It just responds by sending the package data
// with an application/package content type
function packagedAppContentHandler(metadata, response)
{
  packagedAppRequestsMade++;
  if (packagedAppRequestsMade == 2) {
    // The second request returns a 304 not modified response
    response.setStatusLine(metadata.httpVersion, 304, "Not Modified");
    response.bodyOutputStream.write("", 0);
    return;
  }
  response.setHeader("Content-Type", 'application/package');
  var body = testData.getData();

  if (packagedAppRequestsMade == 3) {
    // The third request returns a 200 OK response with a slightly different content
    body = body.replace(/\.\.\./g, 'xxx');
  }
  response.bodyOutputStream.write(body, body.length);
}

function getChannelForURL(url, notificationCallbacks) {
  let uri = createURI(url);
  let ssm = Cc["@mozilla.org/scriptsecuritymanager;1"]
              .getService(Ci.nsIScriptSecurityManager);
  let principal = ssm.createCodebasePrincipal(uri, {});
  let tmpChannel =
    NetUtil.newChannel({
      uri: url,
      loadingPrincipal: principal,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER
    });

  if (notificationCallbacks) {
    // Use custom notificationCallbacks if any.
    tmpChannel.notificationCallbacks = notificationCallbacks;
  } else {
    tmpChannel.notificationCallbacks =
      new LoadContextCallback(principal.appId,
                              principal.isInBrowserElement,
                              false,
                              false);

  }
  return tmpChannel;
}

// The package content
// getData formats it as described at http://www.w3.org/TR/web-packaging/#streamable-package-format
var testData = {
  packageHeader: 'manifest-signature: dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk\r\n',
  content: [
   { headers: ["Content-Location: /index.html", "Content-Type: text/html"], data: "<html>\r\n  <head>\r\n    <script src=\"/scripts/app.js\"></script>\r\n    ...\r\n  </head>\r\n  ...\r\n</html>\r\n", type: "text/html" },
   { headers: ["Content-Location: /scripts/app.js", "Content-Type: text/javascript"], data: "module Math from '/scripts/helpers/math.js';\r\n...\r\n", type: "text/javascript" },
   { headers: ["Content-Location: /scripts/helpers/math.js", "Content-Type: text/javascript"], data: "export function sum(nums) { ... }\r\n...\r\n", type: "text/javascript" }
  ],
  token : "gc0pJq0M:08jU534c0p",
  getData: function() {
    var str = "";
    for (var i in this.content) {
      str += "--" + this.token + "\r\n";
      for (var j in this.content[i].headers) {
        str += this.content[i].headers[j] + "\r\n";
      }
      str += "\r\n";
      str += this.content[i].data + "\r\n";
    }

    str += "--" + this.token + "--";
    return str;
  }
}

var signedPackage = [
  "manifest-signature: MIIF1AYJKoZIhvcNAQcCoIIFxTCCBcECAQExCzAJBgUrDgMCGgUAMAsGCSqGSIb3DQEHAaCCA54wggOaMIICgqADAgECAgECMA0GCSqGSIb3DQEBCwUAMHMxCzAJBgNVBAYTAlVTMQswCQYDVQQIEwJDQTEWMBQGA1UEBxMNTW91bnRhaW4gVmlldzEkMCIGA1UEChMbRXhhbXBsZSBUcnVzdGVkIENvcnBvcmF0aW9uMRkwFwYDVQQDExBUcnVzdGVkIFZhbGlkIENBMB4XDTE1MDkxMDA4MDQzNVoXDTM1MDkxMDA4MDQzNVowdDELMAkGA1UEBhMCVVMxCzAJBgNVBAgTAkNBMRYwFAYDVQQHEw1Nb3VudGFpbiBWaWV3MSQwIgYDVQQKExtFeGFtcGxlIFRydXN0ZWQgQ29ycG9yYXRpb24xGjAYBgNVBAMTEVRydXN0ZWQgQ29ycCBDZXJ0MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAts8whjOzEbn/w1xkFJ67af7F/JPujBK91oyJekh2schIMzFau9pY8S1AiJQoJCulOJCJfUc8hBLKBZiGAkii+4Gpx6cVqMLe6C22MdD806Soxn8Dg4dQqbIvPuI4eeVKu5CEk80PW/BaFMmRvRHO62C7PILuH6yZeGHC4P7dTKpsk4CLxh/jRGXLC8jV2BCW0X+3BMbHBg53NoI9s1Gs7KGYnfOHbBP5wEFAa00RjHnubUaCdEBlC8Kl4X7p0S4RGb3rsB08wgFe9EmSZHIgcIm+SuVo7N4qqbI85qo2ulU6J8NN7ZtgMPHzrMhzgAgf/KnqPqwDIxnNmRNJmHTUYwIDAQABozgwNjAMBgNVHRMBAf8EAjAAMBYGA1UdJQEB/wQMMAoGCCsGAQUFBwMDMA4GA1UdDwEB/wQEAwIHgDANBgkqhkiG9w0BAQsFAAOCAQEAukH6cJUUj5faa8CuPCqrEa0PoLY4SYNnff9NI+TTAHkB9l+kOcFl5eo2EQOcWmZKYi7QLlWC4jy/KQYattO9FMaxiOQL4FAc6ZIbNyfwWBzZWyr5syYJTTTnkLq8A9pCKarN49+FqhJseycU+8EhJEJyP5pv5hLvDNTTHOQ6SXhASsiX8cjo3AY4bxA5pWeXuTZ459qDxOnQd+GrOe4dIeqflk0hA2xYKe3SfF+QlK8EO370B8Dj8RX230OATM1E3OtYyALe34KW3wM9Qm9rb0eViDnVyDiCWkhhQnw5yPg/XQfloug2itRYuCnfUoRt8xfeHgwz2Ymz8cUADn3KpTGCAf4wggH6AgEBMHgwczELMAkGA1UEBhMCVVMxCzAJBgNVBAgTAkNBMRYwFAYDVQQHEw1Nb3VudGFpbiBWaWV3MSQwIgYDVQQKExtFeGFtcGxlIFRydXN0ZWQgQ29ycG9yYXRpb24xGTAXBgNVBAMTEFRydXN0ZWQgVmFsaWQgQ0ECAQIwCQYFKw4DAhoFAKBdMBgGCSqGSIb3DQEJAzELBgkqhkiG9w0BBwEwHAYJKoZIhvcNAQkFMQ8XDTE1MTAwMTIxMTEwNlowIwYJKoZIhvcNAQkEMRYEFHAisUYrrt+gBxYFhZ5KQQusOmN3MA0GCSqGSIb3DQEBAQUABIIBACHW4V0BsPWOvWrGOTRj6mPpNbH/JI1bN2oyqQZrpUQoaBY+BbYxO7TY4Uwe+aeIR/TTPJznOMF/dl3Bna6TPabezU4ylg7TVFI6W7zC5f5DZKp+Xv6uTX6knUzbbW1fkJqMtE8hGUzYXc3/C++Ci6kuOzrpWOhk6DpJHeUO/ioV56H0+QK/oMAjYpEsOohaPqvTPNOBhMQ0OQP3bmuJ6HcjZ/oz96PpzXUPKT1tDe6VykIYkV5NvtC8Tu2lDbYvp9ug3gyDgdyNSV47y5i/iWkzEhsAJB+9Z50wKhplnkxxVHEXkB/6tmfvExvQ28gLd/VbaEGDX2ljCaTSUjhD0o0=\r",
  "--7B0MKBI3UH\r",
  "Content-Location: manifest.webapp\r",
  "Content-Type: application/x-web-app-manifest+json\r",
  "\r",
  "{",
  "  \"name\": \"My App\",",
  "  \"moz-resources\": [",
  "    {",
  "      \"src\": \"page2.html\",",
  "      \"integrity\": \"JREF3JbXGvZ+I1KHtoz3f46ZkeIPrvXtG4VyFQrJ7II=\"",
  "    },",
  "    {",
  "      \"src\": \"index.html\",",
  "      \"integrity\": \"zEubR310nePwd30NThIuoCxKJdnz7Mf5z+dZHUbH1SE=\"",
  "    },",
  "    {",
  "      \"src\": \"scripts/script.js\",",
  "      \"integrity\": \"6TqtNArQKrrsXEQWu3D9ZD8xvDRIkhyV6zVdTcmsT5Q=\"",
  "    },",
  "    {",
  "      \"src\": \"scripts/library.js\",",
  "      \"integrity\": \"TN2ByXZiaBiBCvS4MeZ02UyNi44vED+KjdjLInUl4o8=\"",
  "    }",
  "  ],",
  "  \"moz-permissions\": [",
  "    {",
  "      \"systemXHR\": {",
  "        \"description\": \"Needed to download stuff\"",
  "      },",
  "      \"devicestorage:pictures\": {",
  "        \"description\": \"Need to load pictures\"",
  "      }",
  "    }",
  "  ],",
  "  \"package-identifier\": \"611FC2FE-491D-4A47-B3B3-43FBDF6F404F\",",
  "  \"moz-package-location\": \"https://example.com/myapp/app.pak\",",
  "  \"description\": \"A great app!\"",
  "}\r",
  "--7B0MKBI3UH\r",
  "Content-Location: page2.html\r",
  "Content-Type: text/html\r",
  "\r",
  "<html>",
  "  page2.html",
  "</html>",
  "\r",
  "--7B0MKBI3UH\r",
  "Content-Location: index.html\r",
  "Content-Type: text/html\r",
  "\r",
  "<html>",
  "  Last updated: 2015/10/01 14:10 PST",
  "</html>",
  "\r",
  "--7B0MKBI3UH\r",
  "Content-Location: scripts/script.js\r",
  "Content-Type: text/javascript\r",
  "\r",
  "// script.js",
  "\r",
  "--7B0MKBI3UH\r",
  "Content-Location: scripts/library.js\r",
  "Content-Type: text/javascript\r",
  "\r",
  "// library.js",
  "\r",
  "--7B0MKBI3UH--"
].join("\n");

XPCOMUtils.defineLazyGetter(this, "uri", function() {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

// The active http server initialized in run_test
var httpserver = null;
// The packaged app service initialized in run_test
var paservice = null;
// This variable is set before getResource is called. The listener uses this variable
// to check the correct resource path for the returned entry
var packagePath = null;

function run_test()
{
  // setup test
  httpserver = new HttpServer();
  httpserver.registerPathHandler("/package", packagedAppContentHandler);
  httpserver.registerPathHandler("/304Package", packagedAppContentHandler);
  httpserver.registerPathHandler("/badPackage", packagedAppBadContentHandler);

  let worsePackageNum = 6;
  for (let i = 0; i < worsePackageNum; i++) {
    httpserver.registerPathHandler("/worsePackage_" + i,
                                   packagedAppWorseContentHandler.bind(null, i));
  }

  httpserver.registerPathHandler("/signedPackage", signedPackagedAppContentHandler);
  httpserver.start(-1);

  // We will enable the developer mode in 'test_signed_package_callback'.
  // So restore it after testing.
  //
  // TODO: To be removed in Bug 1178518.
  do_register_cleanup(function() {
    gPrefs.clearUserPref("network.http.packaged-apps-developer-mode");
    gPrefs.clearUserPref("network.http.packaged-signed-apps-enabled");
  });

  paservice = Cc["@mozilla.org/network/packaged-app-service;1"]
                     .getService(Ci.nsIPackagedAppService);
  ok(!!paservice, "test service exists");

  gPrefs.setBoolPref("network.http.packaged-signed-apps-enabled", true);

  add_test(test_bad_args);

  add_test(test_callback_gets_called);
  add_test(test_same_content);
  add_test(test_request_number);
  add_test(test_updated_package);

  add_test(test_package_does_not_exist);
  add_test(test_file_does_not_exist);

  add_test(test_bad_package);
  add_test(test_bad_package_404);

  add_test(test_signed_package_callback);
  add_test(test_unsigned_package_callback);

  // Channels created by addons could have no load info.
  // In debug mode this triggers an assertion, but we still want to test that
  // it works in optimized mode. See bug 1196021 comment 17
  if (Components.classes["@mozilla.org/xpcom/debug;1"]
                .getService(Components.interfaces.nsIDebug2)
                .isDebugBuild == false) {
    add_test(test_channel_no_loadinfo);
  }

  add_test(test_worse_package_0);
  add_test(test_worse_package_1);
  add_test(test_worse_package_2);
  add_test(test_worse_package_3);
  add_test(test_worse_package_4);
  add_test(test_worse_package_5);

  // run tests
  run_next_test();
}

// This checks the proper metadata is on the entry
var metadataListener = {
  onMetaDataElement: function(key, value) {
    if (key == 'response-head') {
      var kExpectedResponseHead = "HTTP/1.1 200 \r\nContent-Location: /index.html\r\nContent-Type: text/html\r\n";
      ok(0 === value.indexOf(kExpectedResponseHead), 'The cached response header not matched');
    }
    else if (key == 'request-method')
      equal(value, "GET");
    else
      ok(false, "unexpected metadata key")
  }
}

// A listener we use to check the proper cache entry is returned by the service
function packagedResourceListener(content) {
  this.content = content;
}

packagedResourceListener.prototype = {
  QueryInterface: function (iid) {
    if (iid.equals(Ci.nsICacheEntryOpenCallback) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
  onCacheEntryCheck: function() { return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED; },
  onCacheEntryAvailable: function (entry, isnew, appcache, status) {
    equal(status, Cr.NS_OK, "status is NS_OK");
    ok(!!entry, "Needs to have an entry");
    equal(entry.key, uri + packagePath + "!//index.html", "Check entry has correct name");
    entry.visitMetaData(metadataListener);
    var inputStream = entry.openInputStream(0);
    pumpReadStream(inputStream, (read) => {
        inputStream.close();
        equal(read, this.content); // not using do_check_eq since logger will fail for the 1/4MB string
        run_next_test();
    });
  }
};

var cacheListener = new packagedResourceListener(testData.content[0].data);
// ----------------------------------------------------------------------------

// These calls should fail, since one of the arguments is invalid or null
function test_bad_args() {
  Assert.throws(() => { paservice.getResource(getChannelForURL("http://test.com"), cacheListener); }, "url's with no !// aren't allowed");
  Assert.throws(() => { paservice.getResource(getChannelForURL("http://test.com/package!//test"), null); }, "should have a callback");
  Assert.throws(() => { paservice.getResource(null, cacheListener); }, "should have a channel");

  run_next_test();
}

// ----------------------------------------------------------------------------

// This tests that the callback gets called, and the cacheListener gets the proper content.
function test_callback_gets_called() {
  packagePath = "/package";
  let url = uri + packagePath + "!//index.html";
  paservice.getResource(getChannelForURL(url), cacheListener);
}

// Tests that requesting the same resource returns the same content
function test_same_content() {
  packagePath = "/package";
  let url = uri + packagePath + "!//index.html";
  paservice.getResource(getChannelForURL(url), cacheListener);
}

// Check the content handler has been called the expected number of times.
function test_request_number() {
  equal(packagedAppRequestsMade, 2, "2 requests are expected. First with content, second is a 304 not modified.");
  run_next_test();
}

// This tests that new content is returned if the package has been updated
function test_updated_package() {
  packagePath = "/package";
  let url = uri + packagePath + "!//index.html";
  paservice.getResource(getChannelForURL(url),
    new packagedResourceListener(testData.content[0].data.replace(/\.\.\./g, 'xxx')));
}

// ----------------------------------------------------------------------------

// This listener checks that the requested resources are not returned
// either because the package does not exist, or because the requested resource
// is not contained in the package.
var listener404 = {
  onCacheEntryCheck: function() { return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED; },
  onCacheEntryAvailable: function (entry, isnew, appcache, status) {
    // XXX: it returns NS_ERROR_FAILURE for a missing package
    // and NS_ERROR_FILE_NOT_FOUND for a missing file from the package.
    // Maybe make them both return NS_ERROR_FILE_NOT_FOUND?
    notEqual(status, Cr.NS_OK, "NOT FOUND");
    ok(!entry, "There should be no entry");
    run_next_test();
  }
};

// Tests that an error is returned for a non existing package
function test_package_does_not_exist() {
  packagePath = "/package_non_existent";
  let url = uri + packagePath + "!//index.html";
  paservice.getResource(getChannelForURL(url), listener404);
}

// Tests that an error is returned for a non existing resource in a package
function test_file_does_not_exist() {
  packagePath = "/package"; // This package exists
  let url = uri + packagePath + "!//file_non_existent.html";
  paservice.getResource(getChannelForURL(url), listener404);
}

// ----------------------------------------------------------------------------

// Broken package. The first and last resources do not contain a "Content-Location" header
// and should be ignored.
var badTestData = {
  content: [
   { headers: ["Content-Type: text/javascript"], data: "module Math from '/scripts/helpers/math.js';\r\n...\r\n", type: "text/javascript" },
   { headers: ["Content-Location: /index.html", "Content-Type: text/html"], data: "<html>\r\n  <head>\r\n    <script src=\"/scripts/app.js\"></script>\r\n    ...\r\n  </head>\r\n  ...\r\n</html>\r\n", type: "text/html" },
   { headers: ["Content-Type: text/javascript"], data: "export function sum(nums) { ... }\r\n...\r\n", type: "text/javascript" }
  ],
  token : "gc0pJq0M:08jU534c0p",
  getData: function() {
    var str = "";
    for (var i in this.content) {
      str += "--" + this.token + "\r\n";
      for (var j in this.content[i].headers) {
        str += this.content[i].headers[j] + "\r\n";
      }
      str += "\r\n";
      str += this.content[i].data + "\r\n";
    }

    str += "--" + this.token + "--";
    return str;
  }
}

// Returns the content of the package with "Content-Location" headers missing for the first and last resource
function packagedAppBadContentHandler(metadata, response)
{
  response.setHeader("Content-Type", 'application/package');
  var body = badTestData.getData();
  response.bodyOutputStream.write(body, body.length);
}

// Checks that the resource with the proper headers inside the bad package is still returned
function test_bad_package() {
  packagePath = "/badPackage";
  let url = uri + packagePath + "!//index.html";
  paservice.getResource(getChannelForURL(url), cacheListener);
}

// Checks that the request for a non-existent resource doesn't hang for a bad package
function test_bad_package_404() {
  packagePath = "/badPackage";
  let url = uri + packagePath + "!//file_non_existent.html";
  paservice.getResource(getChannelForURL(url), listener404);
}

// ----------------------------------------------------------------------------

// NOTE: This test only runs in NON-DEBUG mode.
function test_channel_no_loadinfo() {
  packagePath = "/package";
  let url = uri + packagePath + "!//index.html";
  let channel = getChannelForURL(url);
  channel.loadInfo = null;
  paservice.getResource(channel, cacheListener);
}

// ----------------------------------------------------------------------------

// Worse package testing to ensure the async PackagedAppVerifier working good.

function getData(aTestingData) {
  var str = "";
  for (var i in aTestingData.content) {
    str += "--" + aTestingData.token + "\r\n";
    for (var j in aTestingData.content[i].headers) {
      str += aTestingData.content[i].headers[j] + "\r\n";
    }
    str += "\r\n";
    str += aTestingData.content[i].data + "\r\n";
  }

  str += "--" + aTestingData.token + "--";
  return str;
}

var worseTestData = [
  // 0. Only one broken resource.
  { content: [
     { headers: ["Content-Type: text/javascript"], data: "module Math from '/scripts/helpers/math.js';\r\n...\r\n", type: "text/javascript" },
    ],
    token : "gc0pJq0M:08jU534c0p",
  },

  // 1. Only one valid resource.
  { content: [
     { headers: ["Content-Location: /index.html", "Content-Type: text/html"], data: "<html>\r\n  <head>\r\n    <script src=\"/scripts/app.js\"></script>\r\n    ...\r\n  </head>\r\n  ...\r\n</html>\r\n", type: "text/html" },
    ],
    token : "gc0pJq0M:08jU534c0p",
  },

  // 2. All broken resources.
  { content: [
     { headers: ["Content-Type: text/javascript"], data: "module Math from '/scripts/helpers/math.js';\r\n...\r\n", type: "text/javascript" },
     { headers: ["Content-Type: text/javascript"], data: "<html>\r\n  <head>\r\n    <script src=\"/scripts/app.js\"></script>\r\n    ...\r\n  </head>\r\n  ...\r\n</html>\r\n", type: "text/html" },
     { headers: ["Content-Type: text/javascript"], data: "export function sum(nums) { ... }\r\n...\r\n", type: "text/javascript" }
    ],
    token : "gc0pJq0M:08jU534c0p",
  },

  // 3. All broken resources except the first one.
  { content: [
     { headers: ["Content-Location: /index.html", "Content-Type: text/html"], data: "<html>\r\n  <head>\r\n    <script src=\"/scripts/app.js\"></script>\r\n    ...\r\n  </head>\r\n  ...\r\n</html>\r\n", type: "text/html" },
     { headers: ["Content-Type: text/javascript"], data: "module Math from '/scripts/helpers/math.js';\r\n...\r\n", type: "text/javascript" },
     { headers: ["Content-Type: text/javascript"], data: "<html>\r\n  <head>\r\n    <script src=\"/scripts/app.js\"></script>\r\n    ...\r\n  </head>\r\n  ...\r\n</html>\r\n", type: "text/html" },
     { headers: ["Content-Type: text/javascript"], data: "export function sum(nums) { ... }\r\n...\r\n", type: "text/javascript" }
    ],
    token : "gc0pJq0M:08jU534c0p",
  },

  // 4. All broken resources except the last one.
  { content: [
     { headers: ["Content-Type: text/javascript"], data: "module Math from '/scripts/helpers/math.js';\r\n...\r\n", type: "text/javascript" },
     { headers: ["Content-Type: text/javascript"], data: "<html>\r\n  <head>\r\n    <script src=\"/scripts/app.js\"></script>\r\n    ...\r\n  </head>\r\n  ...\r\n</html>\r\n", type: "text/html" },
     { headers: ["Content-Type: text/javascript"], data: "export function sum(nums) { ... }\r\n...\r\n", type: "text/javascript" },
     { headers: ["Content-Location: /index.html", "Content-Type: text/html"], data: "<html>\r\n  <head>\r\n    <script src=\"/scripts/app.js\"></script>\r\n    ...\r\n  </head>\r\n  ...\r\n</html>\r\n", type: "text/html" },
    ],
    token : "gc0pJq0M:08jU534c0p",
  },

  // 5. All broken resources except the first and the last one.
  { content: [
     { headers: ["Content-Location: /whatever.html", "Content-Type: text/html"], data: "<html>\r\n  <head>\r\n    <script src=\"/scripts/app.js\"></script>\r\n    ...\r\n  </head>\r\n  ...\r\n</html>\r\n", type: "text/html" },
     { headers: ["Content-Type: text/javascript"], data: "module Math from '/scripts/helpers/math.js';\r\n...\r\n", type: "text/javascript" },
     { headers: ["Content-Type: text/javascript"], data: "<html>\r\n  <head>\r\n    <script src=\"/scripts/app.js\"></script>\r\n    ...\r\n  </head>\r\n  ...\r\n</html>\r\n", type: "text/html" },
     { headers: ["Content-Type: text/javascript"], data: "export function sum(nums) { ... }\r\n...\r\n", type: "text/javascript" },
     { headers: ["Content-Location: /index.html", "Content-Type: text/html"], data: "<html>\r\n  <head>\r\n    <script src=\"/scripts/app.js\"></script>\r\n    ...\r\n  </head>\r\n  ...\r\n</html>\r\n", type: "text/html" },
    ],
    token : "gc0pJq0M:08jU534c0p",
  },
];

function packagedAppWorseContentHandler(index, metadata, response)
{
  response.setHeader("Content-Type", 'application/package');
  var body = getData(worseTestData[index]);
  response.bodyOutputStream.write(body, body.length);
}

function test_worse_package(index, success) {
  packagePath = "/worsePackage_" + index;
  let url = uri + packagePath + "!//index.html";
  let channel = getChannelForURL(url);
  paservice.getResource(channel, {
    QueryInterface: function (iid) {
      if (iid.equals(Ci.nsICacheEntryOpenCallback) ||
          iid.equals(Ci.nsISupports))
        return this;
      throw Cr.NS_ERROR_NO_INTERFACE;
    },
    onCacheEntryCheck: function() { return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED; },
    onCacheEntryAvailable: function (entry, isnew, appcache, status) {
      let cacheSuccess = (status === Cr.NS_OK);
      equal(success, status === Cr.NS_OK, "Check status");
      run_next_test();
    }
  });
}

function test_worse_package_0() {
  test_worse_package(0, false);
}

function test_worse_package_1() {
  test_worse_package(1, true);
}

function test_worse_package_2() {
  test_worse_package(2, false);
}

function test_worse_package_3() {
  test_worse_package(3, true);
}

function test_worse_package_4() {
  test_worse_package(4, true);
}

function test_worse_package_5() {
  test_worse_package(5, true);
}

//-----------------------------------------------------------------------------

function signedPackagedAppContentHandler(metadata, response)
{
  response.setHeader("Content-Type", 'application/package');
  var body = signedPackage;
  response.bodyOutputStream.write(body, body.length);
}

// Used as a stub when the cache listener is not important.
var dummyCacheListener = {
  QueryInterface: function (iid) {
    if (iid.equals(Ci.nsICacheEntryOpenCallback) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
  onCacheEntryCheck: function() { return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED; },
  onCacheEntryAvailable: function () {}
};

function test_signed_package_callback()
{
  packagePath = "/signedPackage";
  let url = uri + packagePath + "!//index.html";
  let channel = getChannelForURL(url, {
    onStartSignedPackageRequest: function(aPackageId) {
      ok(true, "onStartSignedPackageRequest is notifited as expected");
      run_next_test();
    },

    getInterface: function (iid) {
      return this.QueryInterface(iid);
    },

    QueryInterface: function (iid) {
      if (iid.equals(Ci.nsISupports) ||
          iid.equals(Ci.nsIInterfaceRequestor) ||
          iid.equals(Ci.nsIPackagedAppChannelListener)) {
        return this;
      }
      if (iid.equals(Ci.nsILoadContext)) {
        return new LoadContextCallback(1024, false, false, false);
      }
      throw Cr.NS_ERROR_NO_INTERFACE;
    },
  });

  paservice.getResource(channel, dummyCacheListener);
}

function test_unsigned_package_callback()
{
  packagePath = "/package";
  let url = uri + packagePath + "!//index.html";
  let channel = getChannelForURL(url, {
    onStartSignedPackageRequest: function(aPackageId) {
      ok(false, "Unsigned package shouldn't be called.");
    },

    getInterface: function (iid) {
      return this.QueryInterface(iid);
    },

    QueryInterface: function (iid) {
      if (iid.equals(Ci.nsISupports) ||
          iid.equals(Ci.nsIInterfaceRequestor) ||
          iid.equals(Ci.nsIPackagedAppChannelListener)) {
        return this;
      }
      if (iid.equals(Ci.nsILoadContext)) {
        return new LoadContextCallback(1024, false, false, false);
      }
      throw Cr.NS_ERROR_NO_INTERFACE;
    },
  });

  // Pass cacheListener since we rely on 'run_next_test' in it.
  paservice.getResource(channel, cacheListener);
}