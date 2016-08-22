// This test is to verify Bug 1214079: when we failed to load a signed packaged
// content due to the bad signature, we are able to load in the next time since
// the cache is not cleaned up after signature verification.
//
// In order to verify it, we do two identical requests in a row, where the
// second request will be loaded from cache. The request is made for a signed
// package with bad signature. If the package cache isn't doomed after the
// first load, the second request will get valid data from the cache.

Cu.import('resource://gre/modules/LoadContextInfo.jsm');
Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

var gPrefs = Cc["@mozilla.org/preferences-service;1"]
               .getService(Components.interfaces.nsIPrefBranch);

var badSignature = [
  "manifest-signature: AAAAAAAAAAAAAAAAAAAAA\r",
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

function run_test()
{
  // setup test
  httpserver = new HttpServer();
  httpserver.registerPathHandler("/badSignature", badSignatureHandler);
  httpserver.start(-1);

  do_register_cleanup(function() {
    gPrefs.clearUserPref("network.http.signed-packages.enabled");
  });

  paservice = Cc["@mozilla.org/network/packaged-app-service;1"]
                     .getService(Ci.nsIPackagedAppService);
  ok(!!paservice, "test service exists");

  gPrefs.setBoolPref("network.http.signed-packages.enabled", true);

  add_test(test_badSignature_package);
  add_test(test_badSignature_package);

  // run tests
  run_next_test();
}

//-----------------------------------------------------------------------------

// The number of times this package has been requested
// This number might be reset by tests that use it
var packagedAppRequestsMade = 0;

function badSignatureHandler(metadata, response)
{
  packagedAppRequestsMade++;
  if (packagedAppRequestsMade == 2) {
    // The second request returns a 304 not modified response
    response.setStatusLine(metadata.httpVersion, 304, "Not Modified");
    response.bodyOutputStream.write("", 0);
    return;
  }

  response.setHeader("Content-Type", 'application/package');
  var body = badSignature;
  response.bodyOutputStream.write(body, body.length);
}

function getChannelForURL(url) {
  let uri = createURI(url);
  let ssm = Cc["@mozilla.org/scriptsecuritymanager;1"]
              .getService(Ci.nsIScriptSecurityManager);
  let principal = ssm.createCodebasePrincipal(uri, {});
  let tmpChannel =
    NetUtil.newChannel({
      uri: url,
      loadingPrincipal: principal,
      securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER
    });

  tmpChannel.loadInfo.originAttributes = principal.originAttributes;

  return tmpChannel;
}

function test_badSignature_package()
{
  let packagePath = "/badSignature";
  let url = uri + packagePath + "!//index.html";
  let channel = getChannelForURL(url);

  paservice.getResource(channel, {
    onCacheEntryCheck: function() { return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED; },
    onCacheEntryAvailable: function (entry, isnew, appcache, status) {
      // XXX: it returns NS_ERROR_FAILURE for a missing package
      // and NS_ERROR_FILE_NOT_FOUND for a missing file from the package.
      // Maybe make them both return NS_ERROR_FILE_NOT_FOUND?
      notEqual(status, Cr.NS_OK, "NOT FOUND");
      ok(!entry, "There should be no entry");
      run_next_test();
    }
  });
}
