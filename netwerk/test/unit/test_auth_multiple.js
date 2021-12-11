// This file tests authentication prompt callbacks
// TODO NIT use do_check_eq(expected, actual) consistently, not sometimes eq(actual, expected)

"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

// Turn off the authentication dialog blocking for this test.
var prefs = Cc["@mozilla.org/preferences-service;1"].getService(
  Ci.nsIPrefBranch
);
prefs.setIntPref("network.auth.subresource-http-auth-allow", 2);

function URL(domain, path = "") {
  if (path.startsWith("/")) {
    path = path.substring(1);
  }
  return `http://${domain}:${httpserv.identity.primaryPort}/${path}`;
}

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
    authInfo.username = this.user;
    authInfo.password = this.pass;
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

function RealmTestRequestor() {}

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
    Assert.equal(authInfo.realm, '"foo_bar');

    return false;
  },

  asyncPromptAuth: function realmtest_async(chan, cb, ctx, lvl, info) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  },
};

function makeChan(url) {
  let loadingUrl = Services.io
    .newURI(url)
    .mutate()
    .setPathQueryRef("")
    .finalize();
  var principal = Services.scriptSecurityManager.createContentPrincipal(
    loadingUrl,
    {}
  );
  return NetUtil.newChannel({
    uri: url,
    loadingPrincipal: principal,
    securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
  });
}

function ntlm_auth(metadata, response) {
  let challenge = metadata.getHeader("Authorization");
  if (!challenge.startsWith("NTLM ")) {
    response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
    return;
  }

  let decoded = atob(challenge.substring(5));
  info(decoded);

  if (!decoded.startsWith("NTLMSSP\0")) {
    response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
    return;
  }

  let isNegotiate = decoded.substring(8).startsWith("\x01\x00\x00\x00");
  let isAuthenticate = decoded.substring(8).startsWith("\x03\x00\x00\x00");

  if (isNegotiate) {
    response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
    response.setHeader(
      "WWW-Authenticate",
      "NTLM TlRMTVNTUAACAAAAAAAAAAAoAAABggAAASNFZ4mrze8AAAAAAAAAAAAAAAAAAAAA",
      false
    );
    return;
  }

  if (isAuthenticate) {
    let body = "OK";
    response.bodyOutputStream.write(body, body.length);
    return;
  }

  // Something else went wrong.
  response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
}

function basic_auth(metadata, response) {
  let challenge = metadata.getHeader("Authorization");
  if (!challenge.startsWith("Basic ")) {
    response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
    return;
  }

  if (challenge == "Basic Z3Vlc3Q6Z3Vlc3Q=") {
    response.setStatusLine(metadata.httpVersion, 200, "OK, authorized");
    response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);

    let body = "success";
    response.bodyOutputStream.write(body, body.length);
    return;
  }

  response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
  response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);
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

function H(str) {
  var data = bytesFromString(str);
  var ch = Cc["@mozilla.org/security/hash;1"].createInstance(Ci.nsICryptoHash);
  ch.init(Ci.nsICryptoHash.MD5);
  ch.update(data, data.length);
  var hash = ch.finish(false);
  return Array.from(hash, (c, i) => toHexString(hash.charCodeAt(i))).join("");
}

const nonce = "6f93719059cf8d568005727f3250e798";
const opaque = "1234opaque1234";
const digestChallenge = `Digest realm="secret", domain="/",  qop=auth,algorithm=MD5, nonce="${nonce}" opaque="${opaque}"`;
//
// Digest handler
//
// /auth/digest
function authDigest(metadata, response) {
  var cnonceRE = /cnonce="(\w+)"/;
  var responseRE = /response="(\w+)"/;
  var usernameRE = /username="(\w+)"/;
  var body = "";
  // check creds if we have them
  if (metadata.hasHeader("Authorization")) {
    var auth = metadata.getHeader("Authorization");
    var cnonce = auth.match(cnonceRE)[1];
    var clientDigest = auth.match(responseRE)[1];
    var username = auth.match(usernameRE)[1];
    var nc = "00000001";

    if (username != "guest") {
      response.setStatusLine(metadata.httpVersion, 400, "bad request");
      body = "should never get here";
    } else {
      // see RFC2617 for the description of this calculation
      var A1 = "guest:secret:guest";
      var A2 = "GET:/path";
      var noncebits = [nonce, nc, cnonce, "auth", H(A2)].join(":");
      var digest = H([H(A1), noncebits].join(":"));

      if (clientDigest == digest) {
        response.setStatusLine(metadata.httpVersion, 200, "OK, authorized");
        body = "digest";
      } else {
        info(clientDigest);
        info(digest);
        handle_unauthorized(metadata, response);
        return;
      }
    }
  } else {
    // no header, send one
    handle_unauthorized(metadata, response);
    return;
  }

  response.bodyOutputStream.write(body, body.length);
}

let challenges = ["NTLM", `Basic realm="secret"`, digestChallenge];

function handle_unauthorized(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");

  for (let ch of challenges) {
    response.setHeader("WWW-Authenticate", ch, true);
  }
}

// /path
function auth_handler(metadata, response) {
  if (!metadata.hasHeader("Authorization")) {
    handle_unauthorized(metadata, response);
    return;
  }

  let challenge = metadata.getHeader("Authorization");
  if (challenge.startsWith("NTLM ")) {
    ntlm_auth(metadata, response);
    return;
  }

  if (challenge.startsWith("Basic ")) {
    basic_auth(metadata, response);
    return;
  }

  if (challenge.startsWith("Digest ")) {
    authDigest(metadata, response);
    return;
  }

  handle_unauthorized(metadata, response);
}

let httpserv;
function setup() {
  Services.prefs.setBoolPref("network.auth.force-generic-ntlm", true);
  Services.prefs.setBoolPref("network.auth.force-generic-ntlm-v1", true);
  Services.prefs.setBoolPref("network.dns.native-is-localhost", true);
  Services.prefs.setBoolPref("network.http.sanitize-headers-in-logs", false);

  httpserv = new HttpServer();
  httpserv.registerPathHandler("/path", auth_handler);
  httpserv.start(-1);

  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("network.auth.force-generic-ntlm");
    Services.prefs.clearUserPref("network.auth.force-generic-ntlm-v1");
    Services.prefs.clearUserPref("network.dns.native-is-localhost");
    Services.prefs.clearUserPref("network.http.sanitize-headers-in-logs");

    await httpserv.stop();
  });
}
setup();

add_task(async function test_ntlm_first() {
  challenges = ["NTLM", `Basic realm="secret"`, digestChallenge];
  httpserv.identity.add("http", "ntlm.com", httpserv.identity.primaryPort);
  let chan = makeChan(URL("ntlm.com", "/path"));

  chan.notificationCallbacks = new Requestor(FLAG_RETURN_FALSE, 2);
  let [req, buf] = await new Promise(resolve => {
    chan.asyncOpen(
      new ChannelListener((req, buf) => resolve([req, buf]), null)
    );
  });
  Assert.equal(buf, "OK");
  Assert.equal(req.QueryInterface(Ci.nsIHttpChannel).responseStatus, 200);
});

add_task(async function test_basic_first() {
  challenges = [`Basic realm="secret"`, "NTLM", digestChallenge];
  httpserv.identity.add("http", "basic.com", httpserv.identity.primaryPort);
  let chan = makeChan(URL("basic.com", "/path"));

  chan.notificationCallbacks = new Requestor(FLAG_RETURN_FALSE, 2);
  let [req, buf] = await new Promise(resolve => {
    chan.asyncOpen(
      new ChannelListener((req, buf) => resolve([req, buf]), null)
    );
  });
  Assert.equal(buf, "success");
  Assert.equal(req.QueryInterface(Ci.nsIHttpChannel).responseStatus, 200);
});

add_task(async function test_digest_first() {
  challenges = [digestChallenge, `Basic realm="secret"`, "NTLM"];
  httpserv.identity.add("http", "digest.com", httpserv.identity.primaryPort);
  let chan = makeChan(URL("digest.com", "/path"));

  chan.notificationCallbacks = new Requestor(FLAG_RETURN_FALSE, 2);
  let [req, buf] = await new Promise(resolve => {
    chan.asyncOpen(
      new ChannelListener((req, buf) => resolve([req, buf]), null)
    );
  });
  Assert.equal(req.QueryInterface(Ci.nsIHttpChannel).responseStatus, 200);
  Assert.equal(buf, "digest");
});
