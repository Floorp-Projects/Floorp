// This file tests authentication prompt callbacks
// TODO NIT use do_check_eq(expected, actual) consistently, not sometimes eq(actual, expected)

"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

// Turn off the authentication dialog blocking for this test.
Services.prefs.setIntPref("network.auth.subresource-http-auth-allow", 2);

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserv.identity.primaryPort;
});

XPCOMUtils.defineLazyGetter(this, "PORT", function() {
  return httpserv.identity.primaryPort;
});

const FLAG_RETURN_FALSE = 1 << 0;
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

  QueryInterface: ChromeUtils.generateQI(["nsIAuthPrompt"]),

  prompt: function ap1_prompt(title, text, realm, save, defaultText, result) {
    do_throw("unexpected prompt call");
  },

  promptUsernameAndPassword: function ap1_promptUP(
    title,
    text,
    realm,
    savePW,
    user,
    pw
  ) {
    if (this.flags & FLAG_NO_REALM) {
      // Note that the realm here isn't actually the realm. it's a pw mgr key.
      Assert.equal(URL + " (" + this.expectedRealm + ")", realm);
    }
    if (!(this.flags & CROSS_ORIGIN)) {
      if (!text.includes(this.expectedRealm)) {
        do_throw("Text must indicate the realm");
      }
    } else if (text.includes(this.expectedRealm)) {
      do_throw("There should not be realm for cross origin");
    }
    if (!text.includes("localhost")) {
      do_throw("Text must indicate the hostname");
    }
    if (!text.includes(String(PORT))) {
      do_throw("Text must indicate the port");
    }
    if (text.includes("-1")) {
      do_throw("Text must contain negative numbers");
    }

    if (this.flags & FLAG_RETURN_FALSE) {
      return false;
    }

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
  },
};

function AuthPrompt2(flags) {
  this.flags = flags;
}

AuthPrompt2.prototype = {
  user: "guest",
  pass: "guest",

  expectedRealm: "secret",

  QueryInterface: ChromeUtils.generateQI(["nsIAuthPrompt2"]),

  promptAuth: function ap2_promptAuth(channel, level, authInfo) {
    var isNTLM = channel.URI.pathQueryRef.includes("ntlm");
    var isDigest = channel.URI.pathQueryRef.includes("digest");

    if (isNTLM || this.flags & FLAG_NO_REALM) {
      this.expectedRealm = ""; // NTLM knows no realms
    }

    Assert.equal(this.expectedRealm, authInfo.realm);

    var expectedLevel =
      isNTLM || isDigest
        ? nsIAuthPrompt2.LEVEL_PW_ENCRYPTED
        : nsIAuthPrompt2.LEVEL_NONE;
    Assert.equal(expectedLevel, level);

    var expectedFlags = nsIAuthInformation.AUTH_HOST;

    if (this.flags & FLAG_PREVIOUS_FAILED) {
      expectedFlags |= nsIAuthInformation.PREVIOUS_FAILED;
    }

    if (this.flags & CROSS_ORIGIN) {
      expectedFlags |= nsIAuthInformation.CROSS_ORIGIN_SUB_RESOURCE;
    }

    if (isNTLM) {
      expectedFlags |= nsIAuthInformation.NEED_DOMAIN;
    }

    const kAllKnownFlags = 127; // Don't fail test for newly added flags
    Assert.equal(expectedFlags, authInfo.flags & kAllKnownFlags);

    // eslint-disable-next-line no-nested-ternary
    var expectedScheme = isNTLM ? "ntlm" : isDigest ? "digest" : "basic";
    Assert.equal(expectedScheme, authInfo.authenticationScheme);

    // No passwords in the URL -> nothing should be prefilled
    Assert.equal(authInfo.username, "");
    Assert.equal(authInfo.password, "");
    Assert.equal(authInfo.domain, "");

    if (this.flags & FLAG_RETURN_FALSE) {
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
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  },
};

function Requestor(flags, versions) {
  this.flags = flags;
  this.versions = versions;
}

Requestor.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIInterfaceRequestor"]),

  getInterface: function requestor_gi(iid) {
    if (this.versions & 1 && iid.equals(Ci.nsIAuthPrompt)) {
      // Allow the prompt to store state by caching it here
      if (!this.prompt1) {
        this.prompt1 = new AuthPrompt1(this.flags);
      }
      return this.prompt1;
    }
    if (this.versions & 2 && iid.equals(Ci.nsIAuthPrompt2)) {
      // Allow the prompt to store state by caching it here
      if (!this.prompt2) {
        this.prompt2 = new AuthPrompt2(this.flags);
      }
      return this.prompt2;
    }

    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
  },

  prompt1: null,
  prompt2: null,
};

function RealmTestRequestor() {
  this.promptRealm = "";
}

RealmTestRequestor.prototype = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIInterfaceRequestor",
    "nsIAuthPrompt2",
  ]),

  getInterface: function realmtest_interface(iid) {
    if (iid.equals(Ci.nsIAuthPrompt2)) {
      return this;
    }

    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
  },

  promptAuth: function realmtest_checkAuth(channel, level, authInfo) {
    this.promptRealm = authInfo.realm;

    return false;
  },

  asyncPromptAuth: function realmtest_async(chan, cb, ctx, lvl, info) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  },
};

var listener = {
  expectedCode: -1, // Uninitialized
  nextTest: undefined,

  onStartRequest: function test_onStartR(request) {
    try {
      if (!Components.isSuccessCode(request.status)) {
        do_throw("Channel should have a success code!");
      }

      if (!(request instanceof Ci.nsIHttpChannel)) {
        do_throw("Expecting an HTTP channel");
      }

      Assert.equal(request.responseStatus, this.expectedCode);
      // The request should be succeeded if we expect 200
      Assert.equal(request.requestSucceeded, this.expectedCode == 200);
    } catch (e) {
      do_throw("Unexpected exception: " + e);
    }

    throw Components.Exception("", Cr.NS_ERROR_ABORT);
  },

  onDataAvailable: function test_ODA() {
    do_throw("Should not get any data!");
  },

  onStopRequest: function test_onStopR(request, status) {
    Assert.equal(status, Cr.NS_ERROR_ABORT);

    this.nextTest();
  },
};

function makeChan(url, loadingUrl) {
  var principal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI(loadingUrl),
    {}
  );
  return NetUtil.newChannel({
    uri: url,
    loadingPrincipal: principal,
    securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
  });
}

var httpserv = null;

function setup() {
  httpserv = new HttpServer();

  httpserv.registerPathHandler("/auth", authHandler);
  httpserv.registerPathHandler("/auth/ntlm/simple", authNtlmSimple);
  httpserv.registerPathHandler("/auth/realm", authRealm);
  httpserv.registerPathHandler("/auth/non_ascii", authNonascii);
  httpserv.registerPathHandler("/auth/digest_md5", authDigestMD5);
  httpserv.registerPathHandler("/auth/digest_md5sess", authDigestMD5sess);
  httpserv.registerPathHandler("/auth/digest_sha256", authDigestSHA256);
  httpserv.registerPathHandler("/auth/digest_sha256sess", authDigestSHA256sess);
  httpserv.registerPathHandler("/auth/digest_sha256_md5", authDigestSHA256_MD5);
  httpserv.registerPathHandler("/auth/digest_md5_sha256", authDigestMD5_SHA256);
  httpserv.registerPathHandler(
    "/auth/digest_md5_sha256_oneline",
    authDigestMD5_SHA256_oneline
  );
  httpserv.registerPathHandler("/auth/short_digest", authShortDigest);
  httpserv.registerPathHandler("/largeRealm", largeRealm);
  httpserv.registerPathHandler("/largeDomain", largeDomain);

  httpserv.start(-1);

  registerCleanupFunction(async () => {
    await httpserv.stop();
  });
}
setup();

async function openAndListen(chan) {
  await new Promise(resolve => {
    listener.nextTest = resolve;
    chan.asyncOpen(listener);
  });
  Cc["@mozilla.org/network/http-auth-manager;1"]
    .getService(Ci.nsIHttpAuthManager)
    .clearAll();
}

add_task(async function test_noauth() {
  var chan = makeChan(URL + "/auth", URL);

  listener.expectedCode = 401; // Unauthorized
  await openAndListen(chan);
});

add_task(async function test_returnfalse1() {
  var chan = makeChan(URL + "/auth", URL);

  chan.notificationCallbacks = new Requestor(FLAG_RETURN_FALSE, 1);
  listener.expectedCode = 401; // Unauthorized
  await openAndListen(chan);
});

add_task(async function test_wrongpw1() {
  var chan = makeChan(URL + "/auth", URL);

  chan.notificationCallbacks = new Requestor(FLAG_WRONG_PASSWORD, 1);
  listener.expectedCode = 200; // OK
  await openAndListen(chan);
});

add_task(async function test_prompt1() {
  var chan = makeChan(URL + "/auth", URL);

  chan.notificationCallbacks = new Requestor(0, 1);
  listener.expectedCode = 200; // OK
  await openAndListen(chan);
});

add_task(async function test_prompt1CrossOrigin() {
  var chan = makeChan(URL + "/auth", "http://example.org");

  chan.notificationCallbacks = new Requestor(16, 1);
  listener.expectedCode = 200; // OK
  await openAndListen(chan);
});

add_task(async function test_prompt2CrossOrigin() {
  var chan = makeChan(URL + "/auth", "http://example.org");

  chan.notificationCallbacks = new Requestor(16, 2);
  listener.expectedCode = 200; // OK
  await openAndListen(chan);
});

add_task(async function test_returnfalse2() {
  var chan = makeChan(URL + "/auth", URL);

  chan.notificationCallbacks = new Requestor(FLAG_RETURN_FALSE, 2);
  listener.expectedCode = 401; // Unauthorized
  await openAndListen(chan);
});

add_task(async function test_wrongpw2() {
  var chan = makeChan(URL + "/auth", URL);

  chan.notificationCallbacks = new Requestor(FLAG_WRONG_PASSWORD, 2);
  listener.expectedCode = 200; // OK
  await openAndListen(chan);
});

add_task(async function test_prompt2() {
  var chan = makeChan(URL + "/auth", URL);

  chan.notificationCallbacks = new Requestor(0, 2);
  listener.expectedCode = 200; // OK
  await openAndListen(chan);
});

add_task(async function test_ntlm() {
  var chan = makeChan(URL + "/auth/ntlm/simple", URL);

  chan.notificationCallbacks = new Requestor(FLAG_RETURN_FALSE, 2);
  listener.expectedCode = 401; // Unauthorized
  await openAndListen(chan);
});

add_task(async function test_basicrealm() {
  var chan = makeChan(URL + "/auth/realm", URL);

  let requestor = new RealmTestRequestor();
  chan.notificationCallbacks = requestor;
  listener.expectedCode = 401; // Unauthorized
  await openAndListen(chan);
  Assert.equal(requestor.promptRealm, '"foo_bar');
});

add_task(async function test_nonascii() {
  var chan = makeChan(URL + "/auth/non_ascii", URL);

  chan.notificationCallbacks = new Requestor(FLAG_NON_ASCII_USER_PASSWORD, 2);
  listener.expectedCode = 200; // OK
  await openAndListen(chan);
});

add_task(async function test_digest_noauth() {
  var chan = makeChan(URL + "/auth/digest_md5", URL);

  // chan.notificationCallbacks = new Requestor(FLAG_RETURN_FALSE, 2);
  listener.expectedCode = 401; // Unauthorized
  await openAndListen(chan);
});

add_task(async function test_digest_md5() {
  var chan = makeChan(URL + "/auth/digest_md5", URL);

  chan.notificationCallbacks = new Requestor(0, 2);
  listener.expectedCode = 200; // OK
  await openAndListen(chan);
});

add_task(async function test_digest_md5sess() {
  var chan = makeChan(URL + "/auth/digest_md5sess", URL);

  chan.notificationCallbacks = new Requestor(0, 2);
  listener.expectedCode = 200; // OK
  await openAndListen(chan);
});

add_task(async function test_digest_sha256() {
  var chan = makeChan(URL + "/auth/digest_sha256", URL);

  chan.notificationCallbacks = new Requestor(0, 2);
  listener.expectedCode = 200; // OK
  await openAndListen(chan);
});

add_task(async function test_digest_sha256sess() {
  var chan = makeChan(URL + "/auth/digest_sha256sess", URL);

  chan.notificationCallbacks = new Requestor(0, 2);
  listener.expectedCode = 200; // OK
  await openAndListen(chan);
});

add_task(async function test_digest_sha256_md5() {
  var chan = makeChan(URL + "/auth/digest_sha256_md5", URL);

  chan.notificationCallbacks = new Requestor(0, 2);
  listener.expectedCode = 200; // OK
  await openAndListen(chan);
});

add_task(async function test_digest_md5_sha256() {
  var chan = makeChan(URL + "/auth/digest_md5_sha256", URL);

  chan.notificationCallbacks = new Requestor(0, 2);
  listener.expectedCode = 200; // OK
  await openAndListen(chan);
});

add_task(async function test_digest_md5_sha256_oneline() {
  var chan = makeChan(URL + "/auth/digest_md5_sha256_oneline", URL);

  chan.notificationCallbacks = new Requestor(0, 2);
  listener.expectedCode = 200; // OK
  await openAndListen(chan);
});

add_task(async function test_digest_bogus_user() {
  var chan = makeChan(URL + "/auth/digest_md5", URL);
  chan.notificationCallbacks = new Requestor(FLAG_BOGUS_USER, 2);
  listener.expectedCode = 401; // unauthorized
  await openAndListen(chan);
});

// Test header "WWW-Authenticate: Digest" - bug 1338876.
add_task(async function test_short_digest() {
  var chan = makeChan(URL + "/auth/short_digest", URL);
  chan.notificationCallbacks = new Requestor(FLAG_NO_REALM, 2);
  listener.expectedCode = 401; // OK
  await openAndListen(chan);
});

// XXX(valentin): this makes tests fail if it's not run last. Why?
add_task(async function test_nonascii_xhr() {
  await new Promise(resolve => {
    let xhr = new XMLHttpRequest();
    xhr.open("GET", URL + "/auth/non_ascii", true, "é", "é");
    xhr.onreadystatechange = function(event) {
      if (xhr.readyState == 4) {
        Assert.equal(xhr.status, 200);
        resolve();
        xhr.onreadystatechange = null;
      }
    };
    xhr.send(null);
  });
});

// PATH HANDLERS

// /auth
function authHandler(metadata, response) {
  // btoa("guest:guest"), but that function is not available here
  var expectedHeader = "Basic Z3Vlc3Q6Z3Vlc3Q=";

  var body;
  if (
    metadata.hasHeader("Authorization") &&
    metadata.getHeader("Authorization") == expectedHeader
  ) {
    response.setStatusLine(metadata.httpVersion, 200, "OK, authorized");
    response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);

    body = "success";
  } else {
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
  response.setHeader(
    "WWW-Authenticate",
    "NTLM" /* + ' realm="secret"' */,
    false
  );

  var body =
    "NOTE: This just sends an NTLM challenge, it never\n" +
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
  if (
    metadata.hasHeader("Authorization") &&
    metadata.getHeader("Authorization") == expectedHeader
  ) {
    response.setStatusLine(metadata.httpVersion, 200, "OK, authorized");
    response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);

    // Use correct XML syntax since this function is also used for testing XHR.
    body = "<?xml version='1.0' ?><root>success</root>";
  } else {
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
  var converter = Cc[
    "@mozilla.org/intl/scriptableunicodeconverter"
  ].createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  var data = converter.convertToByteArray(str);
  return data;
}

// return the two-digit hexadecimal code for a byte
function toHexString(charCode) {
  return ("0" + charCode.toString(16)).slice(-2);
}

function HMD5(str) {
  var data = bytesFromString(str);
  var ch = Cc["@mozilla.org/security/hash;1"].createInstance(Ci.nsICryptoHash);
  ch.init(Ci.nsICryptoHash.MD5);
  ch.update(data, data.length);
  var hash = ch.finish(false);
  return Array.from(hash, (c, i) => toHexString(hash.charCodeAt(i))).join("");
}

function HSHA256(str) {
  var data = bytesFromString(str);
  var ch = Cc["@mozilla.org/security/hash;1"].createInstance(Ci.nsICryptoHash);
  ch.init(Ci.nsICryptoHash.SHA256);
  ch.update(data, data.length);
  var hash = ch.finish(false);
  return Array.from(hash, (c, i) => toHexString(hash.charCodeAt(i))).join("");
}

//
// Digest handler
//
// /auth/digest
function authDigestMD5_helper(metadata, response, test_name) {
  var nonce = "6f93719059cf8d568005727f3250e798";
  var opaque = "1234opaque1234";
  var body;
  var send_401 = 0;
  // check creds if we have them
  if (metadata.hasHeader("Authorization")) {
    var cnonceRE = /cnonce="(\w+)"/;
    var responseRE = /response="(\w+)"/;
    var usernameRE = /username="(\w+)"/;
    var algorithmRE = /algorithm=([\w-]+)/;
    var auth = metadata.getHeader("Authorization");
    var cnonce = auth.match(cnonceRE)[1];
    var clientDigest = auth.match(responseRE)[1];
    var username = auth.match(usernameRE)[1];
    var algorithm = auth.match(algorithmRE)[1];
    var nc = "00000001";

    if (username != "guest") {
      response.setStatusLine(metadata.httpVersion, 400, "bad request");
      body = "should never get here";
    } else if (
      algorithm != null &&
      algorithm != "MD5" &&
      algorithm != "MD5-sess"
    ) {
      response.setStatusLine(metadata.httpVersion, 400, "bad request");
      body = "Algorithm must be same as provided in WWW-Authenticate header";
    } else {
      // see RFC2617 for the description of this calculation
      var A1 = "guest:secret:guest";
      if (algorithm == "MD5-sess") {
        A1 = [HMD5(A1), nonce, cnonce].join(":");
      }
      var A2 = "GET:/auth/" + test_name;
      var noncebits = [nonce, nc, cnonce, "auth", HMD5(A2)].join(":");
      var digest = HMD5([HMD5(A1), noncebits].join(":"));

      if (clientDigest == digest) {
        response.setStatusLine(metadata.httpVersion, 200, "OK, authorized");
        body = "success";
      } else {
        send_401 = 1;
        body = "auth failed";
      }
    }
  } else {
    // no header, send one
    send_401 = 1;
    body = "failed, no header";
  }

  if (send_401) {
    var authenticate_md5 =
      'Digest realm="secret", domain="/",  qop=auth,' +
      'algorithm=MD5, nonce="' +
      nonce +
      '" opaque="' +
      opaque +
      '"';
    var authenticate_md5sess =
      'Digest realm="secret", domain="/",  qop=auth,' +
      'algorithm=MD5, nonce="' +
      nonce +
      '" opaque="' +
      opaque +
      '"';
    if (test_name == "digest_md5") {
      response.setHeader("WWW-Authenticate", authenticate_md5, false);
    } else if (test_name == "digest_md5sess") {
      response.setHeader("WWW-Authenticate", authenticate_md5sess, false);
    }
    response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
  }

  response.bodyOutputStream.write(body, body.length);
}

function authDigestMD5(metadata, response) {
  authDigestMD5_helper(metadata, response, "digest_md5");
}

function authDigestMD5sess(metadata, response) {
  authDigestMD5_helper(metadata, response, "digest_md5sess");
}

function authDigestSHA256_helper(metadata, response, test_name) {
  var nonce = "6f93719059cf8d568005727f3250e798";
  var opaque = "1234opaque1234";
  var body;
  var send_401 = 0;
  // check creds if we have them
  if (metadata.hasHeader("Authorization")) {
    var cnonceRE = /cnonce="(\w+)"/;
    var responseRE = /response="(\w+)"/;
    var usernameRE = /username="(\w+)"/;
    var algorithmRE = /algorithm=([\w-]+)/;
    var auth = metadata.getHeader("Authorization");
    var cnonce = auth.match(cnonceRE)[1];
    var clientDigest = auth.match(responseRE)[1];
    var username = auth.match(usernameRE)[1];
    var algorithm = auth.match(algorithmRE)[1];
    var nc = "00000001";

    if (username != "guest") {
      response.setStatusLine(metadata.httpVersion, 400, "bad request");
      body = "should never get here";
    } else if (algorithm != "SHA-256" && algorithm != "SHA-256-sess") {
      response.setStatusLine(metadata.httpVersion, 400, "bad request");
      body = "Algorithm must be same as provided in WWW-Authenticate header";
    } else {
      // see RFC7616 for the description of this calculation
      var A1 = "guest:secret:guest";
      if (algorithm == "SHA-256-sess") {
        A1 = [HSHA256(A1), nonce, cnonce].join(":");
      }
      var A2 = "GET:/auth/" + test_name;
      var noncebits = [nonce, nc, cnonce, "auth", HSHA256(A2)].join(":");
      var digest = HSHA256([HSHA256(A1), noncebits].join(":"));

      if (clientDigest == digest) {
        response.setStatusLine(metadata.httpVersion, 200, "OK, authorized");
        body = "success";
      } else {
        send_401 = 1;
        body = "auth failed";
      }
    }
  } else {
    // no header, send one
    send_401 = 1;
    body = "failed, no header";
  }

  if (send_401) {
    var authenticate_sha256 =
      'Digest realm="secret", domain="/", qop=auth, ' +
      'algorithm=SHA-256, nonce="' +
      nonce +
      '", opaque="' +
      opaque +
      '"';
    var authenticate_sha256sess =
      'Digest realm="secret", domain="/", qop=auth, ' +
      'algorithm=SHA-256-sess, nonce="' +
      nonce +
      '", opaque="' +
      opaque +
      '"';
    var authenticate_md5 =
      'Digest realm="secret", domain="/", qop=auth, ' +
      'algorithm=MD5, nonce="' +
      nonce +
      '", opaque="' +
      opaque +
      '"';
    if (test_name == "digest_sha256") {
      response.setHeader("WWW-Authenticate", authenticate_sha256, false);
    } else if (test_name == "digest_sha256sess") {
      response.setHeader("WWW-Authenticate", authenticate_sha256sess, false);
    } else if (test_name == "digest_md5_sha256") {
      response.setHeader("WWW-Authenticate", authenticate_md5, false);
      response.setHeader("WWW-Authenticate", authenticate_sha256, true);
    } else if (test_name == "digest_md5_sha256_oneline") {
      response.setHeader(
        "WWW-Authenticate",
        authenticate_md5 + " " + authenticate_sha256,
        false
      );
    } else if (test_name == "digest_sha256_md5") {
      response.setHeader("WWW-Authenticate", authenticate_sha256, false);
      response.setHeader("WWW-Authenticate", authenticate_md5, true);
    }
    response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
  }

  response.bodyOutputStream.write(body, body.length);
}

function authDigestSHA256(metadata, response) {
  authDigestSHA256_helper(metadata, response, "digest_sha256");
}

function authDigestSHA256sess(metadata, response) {
  authDigestSHA256_helper(metadata, response, "digest_sha256sess");
}

function authDigestSHA256_MD5(metadata, response) {
  authDigestSHA256_helper(metadata, response, "digest_sha256_md5");
}

function authDigestMD5_SHA256(metadata, response) {
  authDigestSHA256_helper(metadata, response, "digest_md5_sha256");
}

function authDigestMD5_SHA256_oneline(metadata, response) {
  authDigestSHA256_helper(metadata, response, "digest_md5_sha256_oneline");
}

function authShortDigest(metadata, response) {
  // no header, send one
  response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
  response.setHeader("WWW-Authenticate", "Digest", false);
}

let buildLargePayload = (function() {
  let size = 33 * 1024;
  let ret = "";
  return function() {
    // Return cached value.
    if (ret.length) {
      return ret;
    }
    for (let i = 0; i < size; i++) {
      ret += "a";
    }
    return ret;
  };
})();

function largeRealm(metadata, response) {
  // test > 32KB realm tokens
  var body;

  response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
  response.setHeader(
    "WWW-Authenticate",
    'Digest realm="' + buildLargePayload() + '", domain="foo"'
  );

  body = "need to authenticate";
  response.bodyOutputStream.write(body, body.length);
}

function largeDomain(metadata, response) {
  // test > 32KB domain tokens
  var body;

  response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
  response.setHeader(
    "WWW-Authenticate",
    'Digest realm="foo", domain="' + buildLargePayload() + '"'
  );

  body = "need to authenticate";
  response.bodyOutputStream.write(body, body.length);
}

add_task(async function test_large_realm() {
  var chan = makeChan(URL + "/largeRealm", URL);

  listener.expectedCode = 401; // Unauthorized
  await openAndListen(chan);
});

add_task(async function test_large_domain() {
  var chan = makeChan(URL + "/largeDomain", URL);

  listener.expectedCode = 401; // Unauthorized
  await openAndListen(chan);
});

async function add_parse_realm_testcase(testcase) {
  httpserv.registerPathHandler("/parse_realm", (metadata, response) => {
    response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
    response.setHeader("WWW-Authenticate", testcase.input, false);

    let body = "failed";
    response.bodyOutputStream.write(body, body.length);
  });

  let chan = makeChan(URL + "/parse_realm", URL);
  let requestor = new RealmTestRequestor();
  chan.notificationCallbacks = requestor;

  listener.expectedCode = 401;
  await openAndListen(chan);
  Assert.equal(requestor.promptRealm, testcase.realm);
}

add_task(async function simplebasic() {
  await add_parse_realm_testcase({
    input: `Basic realm="foo"`,
    scheme: `Basic`,
    realm: `foo`,
  });
});

add_task(async function simplebasiclf() {
  await add_parse_realm_testcase({
    input: `Basic\r\n realm="foo"`,
    scheme: `Basic`,
    realm: `foo`,
  });
});

add_task(async function simplebasicucase() {
  await add_parse_realm_testcase({
    input: `BASIC REALM="foo"`,
    scheme: `Basic`,
    realm: `foo`,
  });
});

add_task(async function simplebasictok() {
  await add_parse_realm_testcase({
    input: `Basic realm=foo`,
    scheme: `Basic`,
    realm: `foo`,
  });
});

add_task(async function simplebasictokbs() {
  await add_parse_realm_testcase({
    input: `Basic realm=\\f\\o\\o`,
    scheme: `Basic`,
    realm: `\\foo`,
  });
});

add_task(async function simplebasicsq() {
  await add_parse_realm_testcase({
    input: `Basic realm='foo'`,
    scheme: `Basic`,
    realm: `'foo'`,
  });
});

add_task(async function simplebasicpct() {
  await add_parse_realm_testcase({
    input: `Basic realm="foo%20bar"`,
    scheme: `Basic`,
    realm: `foo%20bar`,
  });
});

add_task(async function simplebasiccomma() {
  await add_parse_realm_testcase({
    input: `Basic , realm="foo"`,
    scheme: `Basic`,
    realm: `foo`,
  });
});

add_task(async function simplebasiccomma2() {
  await add_parse_realm_testcase({
    input: `Basic, realm="foo"`,
    scheme: `Basic`,
    realm: ``,
  });
});

add_task(async function simplebasicnorealm() {
  await add_parse_realm_testcase({
    input: `Basic`,
    scheme: `Basic`,
    realm: ``,
  });
});

add_task(async function simplebasic2realms() {
  await add_parse_realm_testcase({
    input: `Basic realm="foo", realm="bar"`,
    scheme: `Basic`,
    realm: `foo`,
  });
});

add_task(async function simplebasicwsrealm() {
  await add_parse_realm_testcase({
    input: `Basic realm = "foo"`,
    scheme: `Basic`,
    realm: `foo`,
  });
});

add_task(async function simplebasicrealmsqc() {
  await add_parse_realm_testcase({
    input: `Basic realm="\\f\\o\\o"`,
    scheme: `Basic`,
    realm: `foo`,
  });
});

add_task(async function simplebasicrealmsqc2() {
  await add_parse_realm_testcase({
    input: `Basic realm="\\"foo\\""`,
    scheme: `Basic`,
    realm: `"foo"`,
  });
});

add_task(async function simplebasicnewparam1() {
  await add_parse_realm_testcase({
    input: `Basic realm="foo", bar="xyz",, a=b,,,c=d`,
    scheme: `Basic`,
    realm: `foo`,
  });
});

add_task(async function simplebasicnewparam2() {
  await add_parse_realm_testcase({
    input: `Basic bar="xyz", realm="foo"`,
    scheme: `Basic`,
    realm: `foo`,
  });
});

add_task(async function simplebasicrealmiso88591() {
  await add_parse_realm_testcase({
    input: `Basic realm="foo-ä"`,
    scheme: `Basic`,
    realm: `foo-ä`,
  });
});

add_task(async function simplebasicrealmutf8() {
  await add_parse_realm_testcase({
    input: `Basic realm="foo-Ã¤"`,
    scheme: `Basic`,
    realm: `foo-Ã¤`,
  });
});

add_task(async function simplebasicrealmrfc2047() {
  await add_parse_realm_testcase({
    input: `Basic realm="=?ISO-8859-1?Q?foo-=E4?="`,
    scheme: `Basic`,
    realm: `=?ISO-8859-1?Q?foo-=E4?=`,
  });
});

add_task(async function multibasicunknown() {
  await add_parse_realm_testcase({
    input: `Basic realm="basic", Newauth realm="newauth"`,
    scheme: `Basic`,
    realm: `basic`,
  });
});

add_task(async function multibasicunknownnoparam() {
  await add_parse_realm_testcase({
    input: `Basic realm="basic", Newauth`,
    scheme: `Basic`,
    realm: `basic`,
  });
});

add_task(async function multibasicunknown2() {
  await add_parse_realm_testcase({
    input: `Newauth realm="newauth", Basic realm="basic"`,
    scheme: `Basic`,
    realm: `basic`,
  });
});

add_task(async function multibasicunknown2np() {
  await add_parse_realm_testcase({
    input: `Newauth, Basic realm="basic"`,
    scheme: `Basic`,
    realm: `basic`,
  });
});

add_task(async function multibasicunknown2mf() {
  httpserv.registerPathHandler("/parse_realm", (metadata, response) => {
    response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
    response.setHeader("WWW-Authenticate", `Newauth realm="newauth"`, false);
    response.setHeader("WWW-Authenticate", `Basic realm="basic"`, false);

    let body = "failed";
    response.bodyOutputStream.write(body, body.length);
  });

  let chan = makeChan(URL + "/parse_realm", URL);
  let requestor = new RealmTestRequestor();
  chan.notificationCallbacks = requestor;

  listener.expectedCode = 401;
  await openAndListen(chan);
  Assert.equal(requestor.promptRealm, "basic");
});

add_task(async function multibasicempty() {
  await add_parse_realm_testcase({
    input: `,Basic realm="basic"`,
    scheme: `Basic`,
    realm: `basic`,
  });
});

add_task(async function multibasicqs() {
  await add_parse_realm_testcase({
    input: `Newauth realm="apps", type=1, title="Login to \"apps\"", Basic realm="simple"`,
    scheme: `Basic`,
    realm: `simple`,
  });
});

add_task(async function multidisgscheme() {
  await add_parse_realm_testcase({
    input: `Newauth realm="Newauth Realm", basic=foo, Basic realm="Basic Realm"`,
    scheme: `Basic`,
    realm: `Basic Realm`,
  });
});

add_task(async function unknown() {
  await add_parse_realm_testcase({
    input: `Newauth param="value"`,
    scheme: `Basic`,
    realm: ``,
  });
});

add_task(async function parametersnotrequired() {
  await add_parse_realm_testcase({ input: `A, B`, scheme: `Basic`, realm: `` });
});

add_task(async function disguisedrealm() {
  await add_parse_realm_testcase({
    input: `Basic foo="realm=nottherealm", realm="basic"`,
    scheme: `Basic`,
    realm: `basic`,
  });
});

add_task(async function disguisedrealm2() {
  await add_parse_realm_testcase({
    input: `Basic nottherealm="nottherealm", realm="basic"`,
    scheme: `Basic`,
    realm: `basic`,
  });
});

add_task(async function missingquote() {
  await add_parse_realm_testcase({
    input: `Basic realm="basic`,
    scheme: `Basic`,
    realm: `basic`,
  });
});
