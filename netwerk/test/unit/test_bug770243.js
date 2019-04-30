/* this test does the following:
 Always requests the same resource, while for each request getting:
 1. 200 + ETag: "one"
 2. 401 followed by 200 + ETag: "two"
 3. 401 followed by 304
 4. 407 followed by 200 + ETag: "three"
 5. 407 followed by 304
*/

const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");

var httpserv;

function addCreds(scheme, host)
{
  var authMgr = Cc['@mozilla.org/network/http-auth-manager;1']
                  .getService(Ci.nsIHttpAuthManager);
  authMgr.setAuthIdentity(scheme, host, httpserv.identity.primaryPort,
                          "basic", "secret", "/", "", "user", "pass");
}

function clearCreds()
{
  var authMgr = Cc['@mozilla.org/network/http-auth-manager;1']
                  .getService(Ci.nsIHttpAuthManager);
  authMgr.clearAll();
}

function makeChan() {
  return NetUtil.newChannel({
    uri: "http://localhost:" + httpserv.identity.primaryPort + "/",
    loadUsingSystemPrincipal: true
  }).QueryInterface(Ci.nsIHttpChannel);
}

// Array of handlers that are called one by one in response to expected requests

var handlers = [
  // Test 1
  function(metadata, response) {
    Assert.equal(metadata.hasHeader("Authorization"), false);
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.setHeader("ETag", '"one"', false);
    response.setHeader("Cache-control", 'no-cache', false);
    response.setHeader("Content-type", 'text/plain', false);
    var body = "Response body 1";
    response.bodyOutputStream.write(body, body.length);
  },

  // Test 2
  function(metadata, response) {
    Assert.equal(metadata.hasHeader("Authorization"), false);
    Assert.equal(metadata.getHeader("If-None-Match"), '"one"');
    response.setStatusLine(metadata.httpVersion, 401, "Authenticate");
    response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);
    addCreds("http", "localhost");
  },
  function(metadata, response) {
    Assert.equal(metadata.hasHeader("Authorization"), true);
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.setHeader("ETag", '"two"', false);
    response.setHeader("Cache-control", 'no-cache', false);
    response.setHeader("Content-type", 'text/plain', false);
    var body = "Response body 2";
    response.bodyOutputStream.write(body, body.length);
    clearCreds();
  },

  // Test 3
  function(metadata, response) {
    Assert.equal(metadata.hasHeader("Authorization"), false);
    Assert.equal(metadata.getHeader("If-None-Match"), '"two"');
    response.setStatusLine(metadata.httpVersion, 401, "Authenticate");
    response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);
    addCreds("http", "localhost");
  },
  function(metadata, response) {
    Assert.equal(metadata.hasHeader("Authorization"), true);
    Assert.equal(metadata.getHeader("If-None-Match"), '"two"');
    response.setStatusLine(metadata.httpVersion, 304, "OK");
    response.setHeader("ETag", '"two"', false);
    clearCreds();
  },

  // Test 4
  function(metadata, response) {
    Assert.equal(metadata.hasHeader("Authorization"), false);
    Assert.equal(metadata.getHeader("If-None-Match"), '"two"');
    response.setStatusLine(metadata.httpVersion, 407, "Proxy Authenticate");
    response.setHeader("Proxy-Authenticate", 'Basic realm="secret"', false);
    addCreds("http", "localhost");
  },
  function(metadata, response) {
    Assert.equal(metadata.hasHeader("Proxy-Authorization"), true);
    Assert.equal(metadata.getHeader("If-None-Match"), '"two"');
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.setHeader("ETag", '"three"', false);
    response.setHeader("Cache-control", 'no-cache', false);
    response.setHeader("Content-type", 'text/plain', false);
    var body = "Response body 3";
    response.bodyOutputStream.write(body, body.length);
    clearCreds();
  },

  // Test 5
  function(metadata, response) {
    Assert.equal(metadata.hasHeader("Proxy-Authorization"), false);
    Assert.equal(metadata.getHeader("If-None-Match"), '"three"');
    response.setStatusLine(metadata.httpVersion, 407, "Proxy Authenticate");
    response.setHeader("Proxy-Authenticate", 'Basic realm="secret"', false);
    addCreds("http", "localhost");
  },
  function(metadata, response) {
    Assert.equal(metadata.hasHeader("Proxy-Authorization"), true);
    Assert.equal(metadata.getHeader("If-None-Match"), '"three"');
    response.setStatusLine(metadata.httpVersion, 304, "OK");
    response.setHeader("ETag", '"three"', false);
    response.setHeader("Cache-control", 'no-cache', false);
    clearCreds();
  }
];

function handler(metadata, response)
{
  handlers.shift()(metadata, response);
}

// Array of tests to run, self-driven

function sync_and_run_next_test()
{
  syncWithCacheIOThread(function() {
    tests.shift()();
  });
}

var tests = [
  // Test 1: 200 (cacheable)
  function() {
    var ch = makeChan();
    ch.asyncOpen(new ChannelListener(function(req, body) {
      Assert.equal(body, "Response body 1");
      sync_and_run_next_test();
    }, null, CL_NOT_FROM_CACHE));
  },

  // Test 2: 401 and 200 + new content
  function() {
    var ch = makeChan();
    ch.asyncOpen(new ChannelListener(function(req, body) {
      Assert.equal(body, "Response body 2");
      sync_and_run_next_test();
    }, null, CL_NOT_FROM_CACHE));
  },

  // Test 3: 401 and 304
  function() {
    var ch = makeChan();
    ch.asyncOpen(new ChannelListener(function(req, body) {
      Assert.equal(body, "Response body 2");
      sync_and_run_next_test();
    }, null, CL_FROM_CACHE));
  },

  // Test 4: 407 and 200 + new content
  function() {
    var ch = makeChan();
    ch.asyncOpen(new ChannelListener(function(req, body) {
      Assert.equal(body, "Response body 3");
      sync_and_run_next_test();
    }, null, CL_NOT_FROM_CACHE));
  },

  // Test 5: 407 and 304
  function() {
    var ch = makeChan();
    ch.asyncOpen(new ChannelListener(function(req, body) {
      Assert.equal(body, "Response body 3");
      sync_and_run_next_test();
    }, null, CL_FROM_CACHE));
  },

  // End of test run
  function() {
    httpserv.stop(do_test_finished);
  }
];

function run_test()
{
  do_get_profile();

  httpserv = new HttpServer();
  httpserv.registerPathHandler("/", handler);
  httpserv.start(-1);

  const prefs = Cc["@mozilla.org/preferences-service;1"]
                         .getService(Ci.nsIPrefBranch);
  prefs.setCharPref("network.proxy.http", "localhost");
  prefs.setIntPref("network.proxy.http_port", httpserv.identity.primaryPort);
  prefs.setBoolPref("network.proxy.allow_hijacking_localhost", true);
  prefs.setIntPref("network.proxy.type", 1);
  prefs.setBoolPref("network.http.rcwn.enabled", false);

  tests.shift()();
  do_test_pending();
}
