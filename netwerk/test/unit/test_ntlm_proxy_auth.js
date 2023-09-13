// Unit tests for a NTLM authenticated proxy
//
// Currently the tests do not determine whether the Authentication dialogs have
// been displayed.
//

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

ChromeUtils.defineLazyGetter(this, "URL", function () {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

function AuthPrompt() {}

AuthPrompt.prototype = {
  user: "guest",
  pass: "guest",

  QueryInterface: ChromeUtils.generateQI(["nsIAuthPrompt2"]),

  promptAuth: function ap_promptAuth(channel, level, authInfo) {
    authInfo.username = this.user;
    authInfo.password = this.pass;

    return true;
  },

  asyncPromptAuth: function ap_async(chan, cb, ctx, lvl, info) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  },
};

function Requestor() {}

Requestor.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIInterfaceRequestor"]),

  getInterface: function requestor_gi(iid) {
    if (iid.equals(Ci.nsIAuthPrompt2)) {
      // Allow the prompt to store state by caching it here
      if (!this.prompt) {
        this.prompt = new AuthPrompt();
      }
      return this.prompt;
    }

    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
  },

  prompt: null,
};

function makeChan(url, loadingUrl) {
  var principal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI(loadingUrl),
    {}
  );
  return NetUtil.newChannel({
    uri: url,
    loadingPrincipal: principal,
    securityFlags: Ci.nsILoadInfo.SEC_REQUIRE_SAME_ORIGIN_INHERITS_SEC_CONTEXT,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
  });
}

function TestListener(resolve) {
  this.resolve = resolve;
}
TestListener.prototype.onStartRequest = function (request, context) {
  // Need to do the instanceof to allow request.responseStatus
  // to be read.
  if (!(request instanceof Ci.nsIHttpChannel)) {
    dump("Expecting an HTTP channel");
  }

  Assert.equal(expectedResponse, request.responseStatus, "HTTP Status code");
};
TestListener.prototype.onStopRequest = function (request, context, status) {
  Assert.equal(expectedRequests, requestsMade, "Number of requests made ");
  Assert.equal(
    exptTypeOneCount,
    ntlmTypeOneCount,
    "Number of type one messages received"
  );
  Assert.equal(
    exptTypeTwoCount,
    ntlmTypeTwoCount,
    "Number of type two messages received"
  );

  this.resolve();
};
TestListener.prototype.onDataAvaiable = function (
  request,
  context,
  stream,
  offset,
  count
) {
  read_stream(stream, count);
};

// NTLM Messages, for the received type 1 and 3 messages only check that they
// are of the expected type.
const NTLM_TYPE1_PREFIX = "NTLM TlRMTVNTUAABAAAA";
const NTLM_TYPE2_PREFIX = "NTLM TlRMTVNTUAACAAAA";
const NTLM_TYPE3_PREFIX = "NTLM TlRMTVNTUAADAAAA";
const NTLM_PREFIX_LEN = 21;

const PROXY_CHALLENGE =
  NTLM_TYPE2_PREFIX +
  "DgAOADgAAAAFgooCqLNOPe2aZOAAAAAAAAAAAFAAUABGAAAA" +
  "BgEAAAAAAA9HAFcATAAtAE0ATwBaAAIADgBHAFcATAAtAE0A" +
  "TwBaAAEADgBHAFcATAAtAE0ATwBaAAQAAgAAAAMAEgBsAG8A" +
  "YwBhAGwAaABvAHMAdAAHAAgAOKEwGEZL0gEAAAAA";

// Proxy responses for the happy path scenario.
// i.e. successful proxy auth
//
function successfulAuth(metadata, response) {
  let authorization;
  let authPrefix;
  switch (requestsMade) {
    case 0:
      // Proxy - First request to the Proxy resppond with a 407 to start auth
      response.setStatusLine(metadata.httpVersion, 407, "Unauthorized");
      response.setHeader("Proxy-Authenticate", "NTLM", false);
      break;
    case 1:
      // Proxy - Expecting a type 1 negotiate message from the client
      authorization = metadata.getHeader("Proxy-Authorization");
      authPrefix = authorization.substring(0, NTLM_PREFIX_LEN);
      Assert.equal(NTLM_TYPE1_PREFIX, authPrefix, "Expecting a Type 1 message");
      response.setStatusLine(metadata.httpVersion, 407, "Unauthorized");
      response.setHeader("Proxy-Authenticate", PROXY_CHALLENGE, false);
      break;
    case 2:
      // Proxy - Expecting a type 3 Authenticate message from the client
      // Will respond with a 401 to start web server auth sequence
      authorization = metadata.getHeader("Proxy-Authorization");
      authPrefix = authorization.substring(0, NTLM_PREFIX_LEN);
      Assert.equal(NTLM_TYPE3_PREFIX, authPrefix, "Expecting a Type 3 message");
      response.setStatusLine(metadata.httpVersion, 200, "Successful");
      break;
    default:
      // We should be authenticated and further requests are permitted
      authorization = metadata.getHeader("Proxy-Authorization");
      Assert.isnull(authorization);
      response.setStatusLine(metadata.httpVersion, 200, "Successful");
  }
  requestsMade++;
}

// Proxy responses simulating an invalid proxy password
// Note: that the connection should not be reused after the
//       proxy auth fails.
//
function failedAuth(metadata, response) {
  let authorization;
  let authPrefix;
  switch (requestsMade) {
    case 0:
      // Proxy - First request respond with a 407 to initiate auth sequence
      response.setStatusLine(metadata.httpVersion, 407, "Unauthorized");
      response.setHeader("Proxy-Authenticate", "NTLM", false);
      break;
    case 1:
      // Proxy - Expecting a type 1 negotiate message from the client
      authorization = metadata.getHeader("Proxy-Authorization");
      authPrefix = authorization.substring(0, NTLM_PREFIX_LEN);
      Assert.equal(NTLM_TYPE1_PREFIX, authPrefix, "Expecting a Type 1 message");
      response.setStatusLine(metadata.httpVersion, 407, "Unauthorized");
      response.setHeader("Proxy-Authenticate", PROXY_CHALLENGE, false);
      break;
    case 2:
      // Proxy - Expecting a type 3 Authenticate message from the client
      // Respond with a 407 to indicate invalid credentials
      authorization = metadata.getHeader("Proxy-Authorization");
      authPrefix = authorization.substring(0, NTLM_PREFIX_LEN);
      Assert.equal(NTLM_TYPE3_PREFIX, authPrefix, "Expecting a Type 3 message");
      response.setStatusLine(metadata.httpVersion, 407, "Unauthorized");
      response.setHeader("Proxy-Authenticate", "NTLM", false);
      break;
    default:
      // Strictly speaking the connection should not be reused at this point
      // commenting out for now.
      dump("ERROR: NTLM Proxy Authentication, connection should not be reused");
      // assert.fail();
      response.setStatusLine(metadata.httpVersion, 407, "Unauthorized");
  }
  requestsMade++;
}
//
// Simulate a connection reset once the connection has been authenticated
// Detects bug 486508
//
function connectionReset(metadata, response) {
  let authorization;
  let authPrefix;
  switch (requestsMade) {
    case 0:
      // Proxy - First request to the Proxy resppond with a 407 to start auth
      response.setStatusLine(metadata.httpVersion, 407, "Unauthorized");
      response.setHeader("Proxy-Authenticate", "NTLM", false);
      break;
    case 1:
      // Proxy - Expecting a type 1 negotiate message from the client
      authorization = metadata.getHeader("Proxy-Authorization");
      authPrefix = authorization.substring(0, NTLM_PREFIX_LEN);
      Assert.equal(NTLM_TYPE1_PREFIX, authPrefix, "Expecting a Type 1 message");
      ntlmTypeOneCount++;
      response.setStatusLine(metadata.httpVersion, 407, "Unauthorized");
      response.setHeader("Proxy-Authenticate", PROXY_CHALLENGE, false);
      break;
    case 2:
      authorization = metadata.getHeader("Proxy-Authorization");
      authPrefix = authorization.substring(0, NTLM_PREFIX_LEN);
      Assert.equal(NTLM_TYPE3_PREFIX, authPrefix, "Expecting a Type 3 message");
      ntlmTypeTwoCount++;
      try {
        response.seizePower();
        response.finish();
      } catch (e) {
        Assert.ok(false, "unexpected exception" + e);
      }
      break;
    default:
      // Should not get any further requests on this channel
      dump("ERROR: NTLM Proxy Authentication, connection should not be reused");
      Assert.ok(false);
  }
  requestsMade++;
}

//
// Reset the connection after a negotiate message has been received
//
function connectionReset02(metadata, response) {
  var connectionNumber = httpserver.connectionNumber;
  switch (requestsMade) {
    case 0:
      // Proxy - First request to the Proxy respond with a 407 to start auth
      response.setStatusLine(metadata.httpVersion, 407, "Unauthorized");
      response.setHeader("Proxy-Authenticate", "NTLM", false);
      Assert.equal(connectionNumber, httpserver.connectionNumber);
      break;
    case 1:
    // eslint-disable-next-line no-fallthrough
    default:
      // Proxy - Expecting a type 1 negotiate message from the client
      Assert.equal(connectionNumber, httpserver.connectionNumber);
      var authorization = metadata.getHeader("Proxy-Authorization");
      var authPrefix = authorization.substring(0, NTLM_PREFIX_LEN);
      Assert.equal(NTLM_TYPE1_PREFIX, authPrefix, "Expecting a Type 1 message");
      ntlmTypeOneCount++;
      try {
        response.seizePower();
        response.finish();
      } catch (e) {
        Assert.ok(false, "unexpected exception" + e);
      }
  }
  requestsMade++;
}

var httpserver = null;
function setup() {
  httpserver = new HttpServer();
  httpserver.start(-1);

  Services.prefs.setCharPref("network.proxy.http", "localhost");
  Services.prefs.setIntPref(
    "network.proxy.http_port",
    httpserver.identity.primaryPort
  );
  Services.prefs.setCharPref("network.proxy.no_proxies_on", "");
  Services.prefs.setIntPref("network.proxy.type", 1);
  Services.prefs.setBoolPref("network.proxy.allow_hijacking_localhost", true);

  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("network.proxy.http");
    Services.prefs.clearUserPref("network.proxy.http_port");
    Services.prefs.clearUserPref("network.proxy.no_proxies_on");
    Services.prefs.clearUserPref("network.proxy.type");
    Services.prefs.clearUserPref("network.proxy.allow_hijacking_localhost");

    await httpserver.stop();
  });
}
setup();

var expectedRequests = 0; // Number of HTTP requests that are expected
var requestsMade = 0; // The number of requests that were made
var expectedResponse = 0; // The response code
// Note that any test failures in the HTTP handler
// will manifest as a 500 response code

// Common test setup
// Parameters:
//    path       - path component of the URL
//    handler    - http handler function for the httpserver
//    requests   - expected number oh http requests
//    response   - expected http response code
//    clearCache - clear the authentication cache before running the test
function setupTest(path, handler, requests, response, clearCache) {
  requestsMade = 0;
  expectedRequests = requests;
  expectedResponse = response;

  // clear the auth cache if requested
  if (clearCache) {
    dump("Clearing auth cache");
    Cc["@mozilla.org/network/http-auth-manager;1"]
      .getService(Ci.nsIHttpAuthManager)
      .clearAll();
  }

  return new Promise(resolve => {
    var chan = makeChan(URL + path, URL);
    httpserver.registerPathHandler(path, handler);
    chan.notificationCallbacks = new Requestor();
    chan.asyncOpen(new TestListener(resolve));
  });
}

let ntlmTypeOneCount = 0; // The number of NTLM type one messages received
let exptTypeOneCount = 0; // The number of NTLM type one messages that should be received
let ntlmTypeTwoCount = 0; // The number of NTLM type two messages received
let exptTypeTwoCount = 0; // The number of NTLM type two messages that should received

// Happy code path
// Successful proxy auth.
async function test_happy_path() {
  dump("RUNNING TEST: test_happy_path");
  await setupTest("/auth", successfulAuth, 3, 200, 1);
}

// Failed proxy authentication
async function test_failed_auth() {
  dump("RUNNING TEST:failed auth ");
  await setupTest("/auth", failedAuth, 4, 407, 1);
}

// Test connection reset, after successful auth
async function test_connection_reset() {
  dump("RUNNING TEST:connection reset ");
  ntlmTypeOneCount = 0;
  ntlmTypeTwoCount = 0;
  exptTypeOneCount = 1;
  exptTypeTwoCount = 1;
  await setupTest("/auth", connectionReset, 3, 500, 1);
}

// Test connection reset after sending a negotiate.
// Client should retry request using the same connection
async function test_connection_reset02() {
  dump("RUNNING TEST:connection reset ");
  ntlmTypeOneCount = 0;
  ntlmTypeTwoCount = 0;
  let maxRetryAttempt = 5;
  exptTypeOneCount = maxRetryAttempt;
  exptTypeTwoCount = 0;

  Services.prefs.setIntPref(
    "network.http.request.max-attempts",
    maxRetryAttempt
  );

  await setupTest("/auth", connectionReset02, maxRetryAttempt + 1, 500, 1);
}

add_task(
  { pref_set: [["network.auth.use_redirect_for_retries", false]] },
  test_happy_path
);
add_task(
  { pref_set: [["network.auth.use_redirect_for_retries", false]] },
  test_failed_auth
);
add_task(
  { pref_set: [["network.auth.use_redirect_for_retries", false]] },
  test_connection_reset
);
add_task(
  { pref_set: [["network.auth.use_redirect_for_retries", false]] },
  test_connection_reset02
);

add_task(
  { pref_set: [["network.auth.use_redirect_for_retries", true]] },
  test_happy_path
);
add_task(
  { pref_set: [["network.auth.use_redirect_for_retries", true]] },
  test_failed_auth
);
add_task(
  { pref_set: [["network.auth.use_redirect_for_retries", true]] },
  test_connection_reset
);
add_task(
  { pref_set: [["network.auth.use_redirect_for_retries", true]] },
  test_connection_reset02
);
