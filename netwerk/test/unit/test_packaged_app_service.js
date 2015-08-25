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

Cu.import('resource://gre/modules/LoadContextInfo.jsm');
Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

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

function getChannelForURL(url) {
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

  tmpChannel.notificationCallbacks =
    new LoadContextCallback(principal.appId,
                            principal.isInBrowserElement,
                            false,
                            false);
  return tmpChannel;
}

// The package content
// getData formats it as described at http://www.w3.org/TR/web-packaging/#streamable-package-format
var testData = {
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
  httpserver.start(-1);

  paservice = Cc["@mozilla.org/network/packaged-app-service;1"]
                     .getService(Ci.nsIPackagedAppService);
  ok(!!paservice, "test service exists");

  add_test(test_bad_args);

  add_test(test_callback_gets_called);
  add_test(test_same_content);
  add_test(test_request_number);
  add_test(test_updated_package);

  add_test(test_package_does_not_exist);
  add_test(test_file_does_not_exist);

  add_test(test_bad_package);
  add_test(test_bad_package_404);

  // Channels created by addons could have no load info.
  // In debug mode this triggers an assertion, but we still want to test that
  // it works in optimized mode. See bug 1196021 comment 17
  if (Components.classes["@mozilla.org/xpcom/debug;1"]
                .getService(Components.interfaces.nsIDebug2)
                .isDebugBuild == false) {
    add_test(test_channel_no_loadinfo);
  }

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
