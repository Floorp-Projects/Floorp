// This file tests authentication prompt callbacks
// TODO NIT use do_check_eq(expected, actual) consistently, not sometimes eq(actual, expected)

const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");

Cu.importGlobalProperties(["XMLHttpRequest"]);

// Turn off the authentication dialog blocking for this test.
var prefs = Cc["@mozilla.org/preferences-service;1"].
              getService(Ci.nsIPrefBranch);
prefs.setIntPref("network.auth.subresource-http-auth-allow", 2);

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserv.identity.primaryPort;
});

XPCOMUtils.defineLazyGetter(this, "PORT", function() {
  return httpserv.identity.primaryPort;
});

const FLAG_RETURN_FALSE   = 1 << 0;
const FLAG_WRONG_PASSWORD = 1 << 1;
const FLAG_BOGUS_USER = 1 << 2;
const FLAG_PREVIOUS_FAILED = 1 << 3;
const CROSS_ORIGIN = 1 << 4;
const FLAG_NO_REALM = 1 << 5;
const FLAG_NON_ASCII_USER_PASSWORD = 1 << 6;

const nsIAuthPrompt2 = Ci.nsIAuthPrompt2;
const nsIAuthInformation = Ci.nsIAuthInformation;


function AuthPrompt1(flags) {
  this.flags = flags;
}

AuthPrompt1.prototype = {
  user: "guest",
  pass: "guest",

  expectedRealm: "secret",

  QueryInterface: function authprompt_qi(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsIAuthPrompt))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  prompt: function ap1_prompt(title, text, realm, save, defaultText, result) {
    do_throw("unexpected prompt call");
  },

  promptUsernameAndPassword:
    function ap1_promptUP(title, text, realm, savePW, user, pw)
  {
    if (this.flags & FLAG_NO_REALM) {
      // Note that the realm here isn't actually the realm. it's a pw mgr key.
      Assert.equal(URL + " (" + this.expectedRealm + ")", realm);
    }
    if (!(this.flags & CROSS_ORIGIN)) {
      if (!text.includes(this.expectedRealm)) {
        do_throw("Text must indicate the realm");
      }
    } else {
      if (text.includes(this.expectedRealm)) {
        do_throw("There should not be realm for cross origin");
      }
    }
    if (!text.includes("localhost"))
      do_throw("Text must indicate the hostname");
    if (!text.includes(String(PORT)))
      do_throw("Text must indicate the port");
    if (text.includes("-1"))
      do_throw("Text must contain negative numbers");

    if (this.flags & FLAG_RETURN_FALSE)
      return false;

    if (this.flags & FLAG_BOGUS_USER) {
      this.user = "foo\nbar";
    } else if (this.flags & FLAG_NON_ASCII_USER_PASSWORD) {
      this.user = "é";
    }

    user.value = this.user;
    if (this.flags & FLAG_WRONG_PASSWORD) {
      pw.value = this.pass + ".wrong";
      // Now clear the flag to avoid an infinite loop
      this.flags &= ~FLAG_WRONG_PASSWORD;
    } else if (this.flags & FLAG_NON_ASCII_USER_PASSWORD) {
      pw.value = "é";
    } else {
      pw.value = this.pass;
    }
    return true;
  },

  promptPassword: function ap1_promptPW(title, text, realm, save, pwd) {
    do_throw("unexpected promptPassword call");
  }

};

function AuthPrompt2(flags) {
  this.flags = flags;
}

AuthPrompt2.prototype = {
  user: "guest",
  pass: "guest",

  expectedRealm: "secret",

  QueryInterface: function authprompt2_qi(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsIAuthPrompt2))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  promptAuth:
    function ap2_promptAuth(channel, level, authInfo)
  {
    var isNTLM = channel.URI.pathQueryRef.includes("ntlm");
    var isDigest = channel.URI.pathQueryRef.includes("digest");

    if (isNTLM || (this.flags & FLAG_NO_REALM)) {
      this.expectedRealm = ""; // NTLM knows no realms
    }

    Assert.equal(this.expectedRealm, authInfo.realm);

    var expectedLevel = (isNTLM || isDigest) ?
                        nsIAuthPrompt2.LEVEL_PW_ENCRYPTED :
                        nsIAuthPrompt2.LEVEL_NONE;
    Assert.equal(expectedLevel, level);

    var expectedFlags = nsIAuthInformation.AUTH_HOST;

    if (this.flags & FLAG_PREVIOUS_FAILED)
      expectedFlags |= nsIAuthInformation.PREVIOUS_FAILED;

    if (this.flags & CROSS_ORIGIN)
      expectedFlags |= nsIAuthInformation.CROSS_ORIGIN_SUB_RESOURCE;

    if (isNTLM)
      expectedFlags |= nsIAuthInformation.NEED_DOMAIN;

    const kAllKnownFlags = 127; // Don't fail test for newly added flags
    Assert.equal(expectedFlags, authInfo.flags & kAllKnownFlags);

    var expectedScheme = isNTLM ? "ntlm" : isDigest ? "digest" : "basic";
    Assert.equal(expectedScheme, authInfo.authenticationScheme);

    // No passwords in the URL -> nothing should be prefilled
    Assert.equal(authInfo.username, "");
    Assert.equal(authInfo.password, "");
    Assert.equal(authInfo.domain, "");

    if (this.flags & FLAG_RETURN_FALSE)
    {
      this.flags |= FLAG_PREVIOUS_FAILED;
      return false;
    }

    if (this.flags & FLAG_BOGUS_USER) {
      this.user = "foo\nbar";
    } else if (this.flags & FLAG_NON_ASCII_USER_PASSWORD) {
      this.user = "é";
    }

    authInfo.username = this.user;
    if (this.flags & FLAG_WRONG_PASSWORD) {
      authInfo.password = this.pass + ".wrong";
      this.flags |= FLAG_PREVIOUS_FAILED;
      // Now clear the flag to avoid an infinite loop
      this.flags &= ~FLAG_WRONG_PASSWORD;
    } else if (this.flags & FLAG_NON_ASCII_USER_PASSWORD) {
      authInfo.password = "é";
    } else {
      authInfo.password = this.pass;
      this.flags &= ~FLAG_PREVIOUS_FAILED;
    }
    return true;
  },

  asyncPromptAuth: function ap2_async(chan, cb, ctx, lvl, info) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  }
};

function Requestor(flags, versions) {
  this.flags = flags;
  this.versions = versions;
}

Requestor.prototype = {
  QueryInterface: function requestor_qi(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsIInterfaceRequestor))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  getInterface: function requestor_gi(iid) {
    if (this.versions & 1 &&
        iid.equals(Ci.nsIAuthPrompt)) {
      // Allow the prompt to store state by caching it here
      if (!this.prompt1)
        this.prompt1 = new AuthPrompt1(this.flags);
      return this.prompt1;
    }
    if (this.versions & 2 &&
        iid.equals(Ci.nsIAuthPrompt2)) {
      // Allow the prompt to store state by caching it here
      if (!this.prompt2)
        this.prompt2 = new AuthPrompt2(this.flags);
      return this.prompt2;
    }

    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  prompt1: null,
  prompt2: null
};

function RealmTestRequestor() {}

RealmTestRequestor.prototype = {
  QueryInterface: function realmtest_qi(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsIInterfaceRequestor) ||
        iid.equals(Ci.nsIAuthPrompt2))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  getInterface: function realmtest_interface(iid) {
    if (iid.equals(Ci.nsIAuthPrompt2))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  promptAuth: function realmtest_checkAuth(channel, level, authInfo) {
    Assert.equal(authInfo.realm, '\"foo_bar');

    return false;
  },

  asyncPromptAuth: function realmtest_async(chan, cb, ctx, lvl, info) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  }
};

var listener = {
  expectedCode: -1, // Uninitialized

  onStartRequest: function test_onStartR(request) {
    try {
      if (!Components.isSuccessCode(request.status))
        do_throw("Channel should have a success code!");

      if (!(request instanceof Ci.nsIHttpChannel))
        do_throw("Expecting an HTTP channel");

      Assert.equal(request.responseStatus, this.expectedCode);
      // The request should be succeeded iff we expect 200
      Assert.equal(request.requestSucceeded, this.expectedCode == 200);

    } catch (e) {
      do_throw("Unexpected exception: " + e);
    }

    throw Cr.NS_ERROR_ABORT;
  },

  onDataAvailable: function test_ODA() {
    do_throw("Should not get any data!");
  },

  onStopRequest: function test_onStopR(request, status) {
    Assert.equal(status, Cr.NS_ERROR_ABORT);

    moveToNextTest();
  }
};

function makeChan(url, loadingUrl) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);
  var ssm = Cc["@mozilla.org/scriptsecuritymanager;1"]
              .getService(Ci.nsIScriptSecurityManager);
  var principal = ssm.createCodebasePrincipal(ios.newURI(loadingUrl), {});
  return NetUtil.newChannel(
    { uri: url, loadingPrincipal: principal,
      securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER
    });
}

var tests = [test_noauth, test_returnfalse1, test_wrongpw1, test_prompt1,
             test_prompt1CrossOrigin, test_prompt2CrossOrigin,
             test_returnfalse2, test_wrongpw2, test_prompt2, test_ntlm,
             test_basicrealm, test_nonascii, test_digest_noauth, test_digest,
             test_digest_bogus_user, test_short_digest, test_large_realm,
             test_large_domain, test_nonascii_xhr];

var current_test = 0;

var httpserv = null;

function moveToNextTest() {
  if (current_test < (tests.length - 1)) {
    // First, gotta clear the auth cache
    Cc["@mozilla.org/network/http-auth-manager;1"]
      .getService(Ci.nsIHttpAuthManager)
      .clearAll();

    current_test++;
    tests[current_test]();
  } else {
    do_test_pending();
    httpserv.stop(do_test_finished);
  }

  do_test_finished();
}

function run_test() {
  httpserv = new HttpServer();

  httpserv.registerPathHandler("/auth", authHandler);
  httpserv.registerPathHandler("/auth/ntlm/simple", authNtlmSimple);
  httpserv.registerPathHandler("/auth/realm", authRealm);
  httpserv.registerPathHandler("/auth/non_ascii", authNonascii);
  httpserv.registerPathHandler("/auth/digest", authDigest);
  httpserv.registerPathHandler("/auth/short_digest", authShortDigest);
  httpserv.registerPathHandler("/largeRealm", largeRealm);
  httpserv.registerPathHandler("/largeDomain", largeDomain);

  httpserv.start(-1);

  tests[0]();
}

function test_noauth() {
  var chan = makeChan(URL + "/auth", URL);

  listener.expectedCode = 401; // Unauthorized
  chan.asyncOpen(listener);

  do_test_pending();
}

function test_returnfalse1() {
  var chan = makeChan(URL + "/auth", URL);

  chan.notificationCallbacks = new Requestor(FLAG_RETURN_FALSE, 1);
  listener.expectedCode = 401; // Unauthorized
  chan.asyncOpen(listener);

  do_test_pending();
}

function test_wrongpw1() {
  var chan = makeChan(URL + "/auth", URL);

  chan.notificationCallbacks = new Requestor(FLAG_WRONG_PASSWORD, 1);
  listener.expectedCode = 200; // OK
  chan.asyncOpen(listener);

  do_test_pending();
}

function test_prompt1() {
  var chan = makeChan(URL + "/auth", URL);

  chan.notificationCallbacks = new Requestor(0, 1);
  listener.expectedCode = 200; // OK
  chan.asyncOpen(listener);

  do_test_pending();
}

function test_prompt1CrossOrigin() {
  var chan = makeChan(URL + "/auth", "http://example.org");

  chan.notificationCallbacks = new Requestor(16, 1);
  listener.expectedCode = 200; // OK
  chan.asyncOpen(listener);

  do_test_pending();
}

function test_prompt2CrossOrigin() {
  var chan = makeChan(URL + "/auth", "http://example.org");

  chan.notificationCallbacks = new Requestor(16, 2);
  listener.expectedCode = 200; // OK
  chan.asyncOpen(listener);

  do_test_pending();
}

function test_returnfalse2() {
  var chan = makeChan(URL + "/auth", URL);

  chan.notificationCallbacks = new Requestor(FLAG_RETURN_FALSE, 2);
  listener.expectedCode = 401; // Unauthorized
  chan.asyncOpen(listener);

  do_test_pending();
}

function test_wrongpw2() {
  var chan = makeChan(URL + "/auth", URL);

  chan.notificationCallbacks = new Requestor(FLAG_WRONG_PASSWORD, 2);
  listener.expectedCode = 200; // OK
  chan.asyncOpen(listener);

  do_test_pending();
}

function test_prompt2() {
  var chan = makeChan(URL + "/auth", URL);

  chan.notificationCallbacks = new Requestor(0, 2);
  listener.expectedCode = 200; // OK
  chan.asyncOpen(listener);

  do_test_pending();
}

function test_ntlm() {
  var chan = makeChan(URL + "/auth/ntlm/simple", URL);

  chan.notificationCallbacks = new Requestor(FLAG_RETURN_FALSE, 2);
  listener.expectedCode = 401; // Unauthorized
  chan.asyncOpen(listener);

  do_test_pending();
}

function test_basicrealm() {
  var chan = makeChan(URL + "/auth/realm", URL);

  chan.notificationCallbacks = new RealmTestRequestor();
  listener.expectedCode = 401; // Unauthorized
  chan.asyncOpen(listener);

  do_test_pending();
}

function test_nonascii() {
  var chan = makeChan(URL + "/auth/non_ascii", URL);

  chan.notificationCallbacks = new Requestor(FLAG_NON_ASCII_USER_PASSWORD, 2);
  listener.expectedCode = 200; // OK
  chan.asyncOpen(listener);

  do_test_pending();
}

function test_nonascii_xhr() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", URL + "/auth/non_ascii", true, "é", "é");
  xhr.onreadystatechange = function(event) {
    if (xhr.readyState == 4) {
      Assert.equal(xhr.status, 200);
      moveToNextTest();
      xhr.onreadystatechange = null;
    }
  };
  xhr.send(null);

  do_test_pending();
}

function test_digest_noauth() {
  var chan = makeChan(URL + "/auth/digest", URL);

  //chan.notificationCallbacks = new Requestor(FLAG_RETURN_FALSE, 2);
  listener.expectedCode = 401; // Unauthorized
  chan.asyncOpen(listener);

  do_test_pending();
}

function test_digest() {
  var chan = makeChan(URL + "/auth/digest", URL);

  chan.notificationCallbacks = new Requestor(0, 2);
  listener.expectedCode = 200; // OK
  chan.asyncOpen(listener);

  do_test_pending();
}

function test_digest_bogus_user() {
  var chan = makeChan(URL + "/auth/digest", URL);
  chan.notificationCallbacks =  new Requestor(FLAG_BOGUS_USER, 2);
  listener.expectedCode = 401; // unauthorized
  chan.asyncOpen(listener);

  do_test_pending();
}

// Test header "WWW-Authenticate: Digest" - bug 1338876.
function test_short_digest() {
  var chan = makeChan(URL + "/auth/short_digest", URL);
  chan.notificationCallbacks =  new Requestor(FLAG_NO_REALM, 2);
  listener.expectedCode = 401; // OK
  chan.asyncOpen(listener);

  do_test_pending();
}

// PATH HANDLERS

// /auth
function authHandler(metadata, response) {
  // btoa("guest:guest"), but that function is not available here
  var expectedHeader = "Basic Z3Vlc3Q6Z3Vlc3Q=";

  var body;
  if (metadata.hasHeader("Authorization") &&
      metadata.getHeader("Authorization") == expectedHeader)
  {
    response.setStatusLine(metadata.httpVersion, 200, "OK, authorized");
    response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);

    body = "success";
  }
  else
  {
    // didn't know guest:guest, failure
    response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
    response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);

    body = "failed";
  }

  response.bodyOutputStream.write(body, body.length);
}

// /auth/ntlm/simple
function authNtlmSimple(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
  response.setHeader("WWW-Authenticate", "NTLM" /* + ' realm="secret"' */, false);

  var body = "NOTE: This just sends an NTLM challenge, it never\n" +
             "accepts the authentication. It also closes\n" +
             "the connection after sending the challenge\n";


  response.bodyOutputStream.write(body, body.length);
}

// /auth/realm
function authRealm(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
  response.setHeader("WWW-Authenticate", 'Basic realm="\\"f\\oo_bar"', false);
  var body = "success";

  response.bodyOutputStream.write(body, body.length);
}

// /auth/nonAscii
function authNonascii(metadata, response) {
  // btoa("é:é"), but that function is not available here
  var expectedHeader = "Basic w6k6w6k=";

  var body;
  if (metadata.hasHeader("Authorization") &&
      metadata.getHeader("Authorization") == expectedHeader)
  {
    response.setStatusLine(metadata.httpVersion, 200, "OK, authorized");
    response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);

    // Use correct XML syntax since this function is also used for testing XHR.
    body = "<?xml version='1.0' ?><root>success</root>";
  }
  else
  {
    // didn't know é:é, failure
    response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
    response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);

    body = "<?xml version='1.0' ?><root>failed</root>";
  }

  response.bodyOutputStream.write(body, body.length);
}

//
// Digest functions
// 
function bytesFromString(str) {
 var converter =
   Cc["@mozilla.org/intl/scriptableunicodeconverter"]
     .createInstance(Ci.nsIScriptableUnicodeConverter);
 converter.charset = "UTF-8";
 var data = converter.convertToByteArray(str);
 return data;
}

// return the two-digit hexadecimal code for a byte
function toHexString(charCode) {
 return ("0" + charCode.toString(16)).slice(-2);
}

function H(str) {
 var data = bytesFromString(str);
 var ch = Cc["@mozilla.org/security/hash;1"]
            .createInstance(Ci.nsICryptoHash);
 ch.init(Ci.nsICryptoHash.MD5);
 ch.update(data, data.length);
 var hash = ch.finish(false);
 return Array.from(hash, (c, i) => toHexString(hash.charCodeAt(i))).join("");
}

//
// Digest handler
//
// /auth/digest
function authDigest(metadata, response) {
 var nonce = "6f93719059cf8d568005727f3250e798";
 var opaque = "1234opaque1234";
 var cnonceRE = /cnonce="(\w+)"/;
 var responseRE = /response="(\w+)"/;
 var usernameRE = /username="(\w+)"/;
 var authenticate = 'Digest realm="secret", domain="/",  qop=auth,' +
                    'algorithm=MD5, nonce="' + nonce+ '" opaque="' + 
                     opaque + '"';
 var body;
 // check creds if we have them
 if (metadata.hasHeader("Authorization")) {
   var auth = metadata.getHeader("Authorization");
   var cnonce = (auth.match(cnonceRE))[1];
   var clientDigest = (auth.match(responseRE))[1];
   var username = (auth.match(usernameRE))[1];
   var nc = "00000001";
   
   if (username != "guest") {
     response.setStatusLine(metadata.httpVersion, 400, "bad request");
     body = "should never get here";
   } else {
     // see RFC2617 for the description of this calculation
     var A1 = "guest:secret:guest";
     var A2 = "GET:/auth/digest";
     var noncebits = [nonce, nc, cnonce, "auth", H(A2)].join(":");
     var digest = H([H(A1), noncebits].join(":"));

     if (clientDigest == digest) {
       response.setStatusLine(metadata.httpVersion, 200, "OK, authorized");
       body = "success";
     } else {
       response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
       response.setHeader("WWW-Authenticate", authenticate, false);
       body = "auth failed";
     }
   }
 } else {
   // no header, send one
   response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
   response.setHeader("WWW-Authenticate", authenticate, false);
   body = "failed, no header";
 }
 
 response.bodyOutputStream.write(body, body.length);
}

function authShortDigest(metadata, response) {
  // no header, send one
  response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
  response.setHeader("WWW-Authenticate", 'Digest', false);
  body = "failed, no header";
}

let buildLargePayload = (function() {
  let size = 33 * 1024;
  let ret = "";
  return function() {
    // Return cached value.
    if (ret.length > 0) {
      return ret;
    }
    for (let i = 0; i < size; i++) {
      ret += "a";
    }
    return ret;
  }
})();

function largeRealm(metadata, response) {
 // test > 32KB realm tokens
  var body;

  response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
  response.setHeader("WWW-Authenticate",
             'Digest realm="' + buildLargePayload() + '", domain="foo"');

  body = "need to authenticate";
  response.bodyOutputStream.write(body, body.length);
}

function largeDomain(metadata, response) {
 // test > 32KB domain tokens
  var body;

  response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
  response.setHeader("WWW-Authenticate",
             'Digest realm="foo", domain="' + buildLargePayload() + '"');

  body = "need to authenticate";
  response.bodyOutputStream.write(body, body.length);
}

function test_large_realm() {
  var chan = makeChan(URL + "/largeRealm", URL);

  listener.expectedCode = 401; // Unauthorized
  chan.asyncOpen(listener);

  do_test_pending();
}

function test_large_domain() {
  var chan = makeChan(URL + "/largeDomain", URL);

  listener.expectedCode = 401; // Unauthorized
  chan.asyncOpen(listener);

  do_test_pending();
}
