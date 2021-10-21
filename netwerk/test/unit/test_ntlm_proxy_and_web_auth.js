// Unit tests for a NTLM authenticated proxy, proxying for a NTLM authenticated
// web server.
//
// Currently the tests do not determine whether the Authentication dialogs have
// been displayed.
//
Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
    return "http://localhost:" + httpserver.identity.primaryPort;
});

const nsIAuthPrompt2 = Components.interfaces.nsIAuthPrompt2;
const nsIAuthInformation = Components.interfaces.nsIAuthInformation;


function AuthPrompt() {
}

AuthPrompt.prototype = {
  user: "guest",
  pass: "guest",

  QueryInterface: function authprompt_qi(iid) {
    if (iid.equals(Components.interfaces.nsISupports) ||
        iid.equals(Components.interfaces.nsIAuthPrompt2))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  promptAuth: function ap_promptAuth(channel, level, authInfo) {
    authInfo.username = this.user;
    authInfo.password = this.pass;

    return true;
  },

  asyncPromptAuth: function ap_async(chan, cb, ctx, lvl, info) {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  }
};

function Requestor() {
}

Requestor.prototype = {
    QueryInterface: function requestor_qi(iid) {
        if (iid.equals(Components.interfaces.nsISupports) ||
            iid.equals(Components.interfaces.nsIInterfaceRequestor))
                return this;
        throw Components.results.NS_ERROR_NO_INTERFACE;
    },

    getInterface: function requestor_gi(iid) {
        if ( iid.equals(Components.interfaces.nsIAuthPrompt2)) {
            // Allow the prompt to store state by caching it here
            if (!this.prompt)
                this.prompt = new AuthPrompt();
            return this.prompt;
        }

        throw Components.results.NS_ERROR_NO_INTERFACE;
    },

    prompt: null
};

function makeChan(url, loadingUrl) {
    var ios = Cc["@mozilla.org/network/io-service;1"]
                .getService(Ci.nsIIOService);
    var ssm = Cc["@mozilla.org/scriptsecuritymanager;1"]
                .getService(Ci.nsIScriptSecurityManager);
    var principal = ssm.createCodebasePrincipal(ios.newURI(loadingUrl, null, null), {});
    return NetUtil.newChannel(
        { uri: url
        , loadingPrincipal: principal
        , securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL
        , contentPolicyType: Components.interfaces.nsIContentPolicy.TYPE_OTHER
        });
}

function TestListener() {
}
TestListener.prototype.onStartRequest = function(request, context) {

    // Need to do the instanceof to allow request.responseStatus
    // to be read.
    if (!(request instanceof Components.interfaces.nsIHttpChannel))
        do_print("Expecting an HTTP channel");

    Assert.equal( expectedResponse, request.responseStatus,"HTTP Status code") ;
}
TestListener.prototype.onStopRequest = function(request, context, status) {

    Assert.equal(expectedRequests,   requestsMade,   "Number of requests made ");

    if (current_test < (tests.length - 1)) {
        current_test++;
        tests[current_test]();
    } else {
        do_test_pending();
        httpserver.stop(do_test_finished);
    }

    do_test_finished();

}
TestListener.prototype.onDataAvaiable = function(request, context, stream, offset, count) {
    var data = read_stream(stream, count);
}

// NTLM Messages, for the received type 1 and 3 messages only check that they
// are of the expected type.
const NTLM_TYPE1_PREFIX = "NTLM TlRMTVNTUAABAAAA";
const NTLM_TYPE2_PREFIX = "NTLM TlRMTVNTUAACAAAA";
const NTLM_TYPE3_PREFIX = "NTLM TlRMTVNTUAADAAAA";
const NTLM_PREFIX_LEN   = 21;

const NTLM_CHALLENGE    = NTLM_TYPE2_PREFIX +
                          "DAAMADAAAAABAoEAASNFZ4mrze8AAAAAAAAAAGIAYgA8AAAAR" +
                          "ABPAE0AQQBJAE4AAgAMAEQATwBNAEEASQBOAAEADABTAEUAUg" +
                          "BWAEUAUgAEABQAZABvAG0AYQBpAG4ALgBjAG8AbQADACIAcwB" +
                          "lAHIAdgBlAHIALgBkAG8AbQBhAGkAbgAuAGMAbwBtAAAAAAA=";

const PROXY_CHALLENGE    = NTLM_TYPE2_PREFIX +
                           "DgAOADgAAAAFgooCqLNOPe2aZOAAAAAAAAAAAFAAUABGAAAA" +
                           "BgEAAAAAAA9HAFcATAAtAE0ATwBaAAIADgBHAFcATAAtAE0A" +
                           "TwBaAAEADgBHAFcATAAtAE0ATwBaAAQAAgAAAAMAEgBsAG8A" +
                           "YwBhAGwAaABvAHMAdAAHAAgAOKEwGEZL0gEAAAAA";



// Proxy and Web server responses for the happy path scenario.
// i.e. successful proxy auth and successful web server auth
//
function authHandler( metadata, response) {
    switch (requestsMade) {
    case 0:
        // Proxy - First request to the Proxy resppond with a 407 to start auth
        response.setStatusLine( metadata.httpVersion, 407, "Unauthorized");
        response.setHeader( "Proxy-Authenticate", "NTLM", false);
        break;
    case 1:
        // Proxy - Expecting a type 1 negotiate message from the client
        var authorization = metadata.getHeader( "Proxy-Authorization");
        var authPrefix    = authorization.substring(0, NTLM_PREFIX_LEN);
        Assert.equal( NTLM_TYPE1_PREFIX, authPrefix, "Expecting a Type 1 message");
        response.setStatusLine( metadata.httpVersion, 407, "Unauthorized");
        response.setHeader( "Proxy-Authenticate", PROXY_CHALLENGE, false);
        break;
    case 2:
        // Proxy - Expecting a type 3 Authenticate message from the client
        // Will respond with a 401 to start web server auth sequence
        var authorization = metadata.getHeader( "Proxy-Authorization");
        var authPrefix    = authorization.substring(0, NTLM_PREFIX_LEN);
        Assert.equal( NTLM_TYPE3_PREFIX, authPrefix, "Expecting a Type 3 message");
        response.setStatusLine( metadata.httpVersion, 401, "Unauthorized");
        response.setHeader( "WWW-Authenticate", "NTLM", false);
        break;
    case 3:
        // Web Server - Expecting a type 1 negotiate message from the client
        var authorization = metadata.getHeader( "Authorization");
        var authPrefix    = authorization.substring(0, NTLM_PREFIX_LEN);
        Assert.equal( NTLM_TYPE1_PREFIX, authPrefix, "Expecting a Type 1 message");
        response.setStatusLine( metadata.httpVersion, 401, "Unauthorized");
        response.setHeader( "WWW-Authenticate", NTLM_CHALLENGE, false);
        break;
    case 4:
        // Web Server - Expecting a type 3 Authenticate message from the client
        var authorization = metadata.getHeader( "Authorization");
        var authPrefix    = authorization.substring(0, NTLM_PREFIX_LEN);
        Assert.equal( NTLM_TYPE3_PREFIX, authPrefix, "Expecting a Type 3 message");
        response.setStatusLine( metadata.httpVersion, 200, "Successful");
        break;
    default:
        // We should be authenticated and further requests are permitted
        var authorization = metadata.getHeader( "Authorization");
        var authorization = metadata.getHeader( "Proxy-Authorization");
        Assert.isnull( authorization);
        response.setStatusLine( metadata.httpVersion, 200, "Successful");
    }
    requestsMade++;
}

// Proxy responses simulating an invalid proxy password
// Note: that the connection should not be reused after the
//       proxy auth fails.
//
function authHandlerInvalidProxyPassword( metadata, response) {
    switch (requestsMade) {
    case 0:
        // Proxy - First request respond with a 407 to initiate auth sequence
        response.setStatusLine( metadata.httpVersion, 407, "Unauthorized");
        response.setHeader( "Proxy-Authenticate", "NTLM", false);
        break;
    case 1:
        // Proxy - Expecting a type 1 negotiate message from the client
        var authorization = metadata.getHeader( "Proxy-Authorization");
        var authPrefix    = authorization.substring(0, NTLM_PREFIX_LEN);
        Assert.equal( NTLM_TYPE1_PREFIX, authPrefix, "Expecting a Type 1 message");
        response.setStatusLine( metadata.httpVersion, 407, "Unauthorized");
        response.setHeader( "Proxy-Authenticate", PROXY_CHALLENGE, false);
        break;
    case 2:
        // Proxy - Expecting a type 3 Authenticate message from the client
        // Respond with a 407 to indicate invalid credentials
        //
        var authorization = metadata.getHeader( "Proxy-Authorization");
        var authPrefix    = authorization.substring(0, NTLM_PREFIX_LEN);
        Assert.equal( NTLM_TYPE3_PREFIX, authPrefix, "Expecting a Type 3 message");
        response.setStatusLine( metadata.httpVersion, 407, "Unauthorized");
        response.setHeader( "Proxy-Authenticate", "NTLM", false);
        break;
    default:
        // Strictly speaking the connection should not be reused at this point
        // and reaching here should be an error, but have commented out for now
        //do_print( "ERROR: NTLM Proxy Authentication, connection should not be reused");
        //Assert.fail();
        response.setStatusLine( metadata.httpVersion, 407, "Unauthorized");
    }
    requestsMade++;
}

// Proxy and web server responses simulating a successful Proxy auth
// and a failed web server auth
// Note: the connection should not be reused once the password failure is
//       detected
function authHandlerInvalidWebPassword( metadata, response) {
    switch (requestsMade) {
    case 0:
        // Proxy - First request return a 407 to start Proxy auth
        response.setStatusLine( metadata.httpVersion, 407, "Unauthorized");
        response.setHeader( "Proxy-Authenticate", "NTLM", false);
        break;
    case 1:
        // Proxy - Expecting a type 1 negotiate message from the client
        var authorization = metadata.getHeader( "Proxy-Authorization");
        var authPrefix    = authorization.substring(0, NTLM_PREFIX_LEN);
        Assert.equal( NTLM_TYPE1_PREFIX, authPrefix, "Expecting a Type 1 message");
        response.setStatusLine( metadata.httpVersion, 407, "Unauthorized");
        response.setHeader( "Proxy-Authenticate", NTLM_CHALLENGE, false);
        break;
    case 2:
        // Proxy - Expecting a type 3 Authenticate message from the client
        // Responds with a 401 to start web server auth
        var authorization = metadata.getHeader( "Proxy-Authorization");
        var authPrefix    = authorization.substring(0, NTLM_PREFIX_LEN);
        Assert.equal( NTLM_TYPE3_PREFIX, authPrefix, "Expecting a Type 3 message");
        response.setStatusLine( metadata.httpVersion, 401, "Unauthorized");
        response.setHeader( "WWW-Authenticate", "NTLM", false);
        break;
    case 3:
        // Web Server - Expecting a type 1 negotiate message from the client
        var authorization = metadata.getHeader( "Authorization");
        var authPrefix    = authorization.substring(0, NTLM_PREFIX_LEN);
        Assert.equal( NTLM_TYPE1_PREFIX, authPrefix, "Expecting a Type 1 message");
        response.setStatusLine( metadata.httpVersion, 401, "Unauthorized");
        response.setHeader( "WWW-Authenticate", NTLM_CHALLENGE, false);
        break;
    case 4:
        // Web Server - Expecting a type 3 Authenticate message from the client
        // Respond with a 401 to restart the auth sequence.
        var authorization = metadata.getHeader( "Authorization");
        var authPrefix    = authorization.substring(0, NTLM_PREFIX_LEN);
        Assert.equal( NTLM_TYPE3_PREFIX, authPrefix, "Expecting a Type 1 message");
        response.setStatusLine( metadata.httpVersion, 401, "Unauthorized");
        break;
    default:
        // We should not get called past step 4
        do_print( "ERROR: NTLM Auth failed connection should not be reused");
        Assert.fail();
    }
    requestsMade++;
}

// Tests to run test_bad_proxy_pass and test_bad_web_pass are split into two stages
// so that once we determine how detect password dialog displays we can check
// that the are displayed correctly, i.e. proxy password should not be prompted
// for when retrying the web server password
var tests = [
    test_happy_path,
    test_bad_proxy_pass_stage01,
    test_bad_proxy_pass_stage02,
    test_bad_web_pass_stage01,
    test_bad_web_pass_stage02
];
var current_test = 0;


var httpserver = null;
function run_test() {

    httpserver = new HttpServer()
    httpserver.start(-1);

    const prefs = Cc["@mozilla.org/preferences-service;1"]
                           .getService(Ci.nsIPrefBranch);
    prefs.setCharPref("network.proxy.http", "localhost");
    prefs.setIntPref("network.proxy.http_port", httpserver.identity.primaryPort);
    prefs.setCharPref("network.proxy.no_proxies_on", "");
    prefs.setIntPref("network.proxy.type", 1);

    tests[0]();
}

var expectedRequests   = 0;  // Number of HTTP requests that are expected
var requestsMade       = 0;  // The number of requests that were made
var expectedResponse   = 0;  // The response code
                             // Note that any test failures in the HTTP handler
                             // will manifest as a 500 response code

// Common test setup
// Parameters:
//    path       - path component of the URL
//    handler    - http handler function for the httpserver
//    requests   - expected number oh http requests
//    response   - expected http response code
//    clearCache - clear the authentication cache before running the test
function setupTest( path, handler, requests, response, clearCache) {

    requestsMade       = 0;
    expectedRequests   = requests;
    expectedResponse   = response;

    // clear the auth cache if requested
    if (clearCache) {
        do_print( "Clearing auth cache");
         Cc["@mozilla.org/network/http-auth-manager;1"]
           .getService(Ci.nsIHttpAuthManager)
           .clearAll();
    }

    var chan = makeChan( URL + path, URL);
    httpserver.registerPathHandler( path, handler );
    chan.notificationCallbacks = new Requestor();
    chan.asyncOpen2(new TestListener());

    return chan;
}

// Happy code path
// Succesful proxy and web server auth.
function test_happy_path() {
    do_print("RUNNING TEST: test_happy_path");
    var chan = setupTest( "/auth", authHandler, 5, 200, 1);

    do_test_pending();
}

// Failed proxy authentication
function test_bad_proxy_pass_stage01() {
    do_print("RUNNING TEST: test_bad_proxy_pass_stage01");
    var chan = setupTest( "/auth", authHandlerInvalidProxyPassword, 4, 407, 1);

    do_test_pending();
}
// Successful logon after failed proxy auth
function test_bad_proxy_pass_stage02() {
    do_print("RUNNING TEST: test_bad_proxy_pass_stage02");
    var chan = setupTest( "/auth", authHandler, 5, 200, 0);

    do_test_pending();
}

// successful proxy logon, unsuccessful web server sign on
function test_bad_web_pass_stage01() {
    do_print("RUNNING TEST: test_bad_web_pass_stage01");
    var chan = setupTest( "/auth", authHandlerInvalidWebPassword, 5, 401, 1);

    do_test_pending();
}
// successful logon after failed web server auth.
function test_bad_web_pass_stage02() {
    do_print("RUNNING TEST: test_bad_web_pass_stage02");
    var chan = setupTest( "/auth", authHandler, 5, 200, 0);

    do_test_pending();
}
