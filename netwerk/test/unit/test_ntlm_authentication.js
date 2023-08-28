// This file tests authentication prompt callbacks
// TODO NIT use do_check_eq(expected, actual) consistently, not sometimes eq(actual, expected)

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

// Turn off the authentication dialog blocking for this test.
var prefs = Services.prefs;
prefs.setIntPref("network.auth.subresource-http-auth-allow", 2);

ChromeUtils.defineLazyGetter(this, "URL", function () {
  return "http://localhost:" + httpserv.identity.primaryPort;
});

ChromeUtils.defineLazyGetter(this, "PORT", function () {
  return httpserv.identity.primaryPort;
});

const FLAG_RETURN_FALSE = 1 << 0;
const FLAG_WRONG_PASSWORD = 1 << 1;
const FLAG_BOGUS_USER = 1 << 2;
// const FLAG_PREVIOUS_FAILED = 1 << 3;
const CROSS_ORIGIN = 1 << 4;
const FLAG_NO_REALM = 1 << 5;
const FLAG_NON_ASCII_USER_PASSWORD = 1 << 6;

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

// /auth/ntlm/simple
function authNtlmSimple(metadata, response) {
  if (!metadata.hasHeader("Authorization")) {
    response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
    response.setHeader("WWW-Authenticate", "NTLM", false);
    return;
  }

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

let httpserv;
add_task(async function test_ntlm() {
  Services.prefs.setBoolPref("network.auth.force-generic-ntlm", true);
  Services.prefs.setBoolPref("network.auth.force-generic-ntlm-v1", true);

  httpserv = new HttpServer();
  httpserv.registerPathHandler("/auth/ntlm/simple", authNtlmSimple);
  httpserv.start(-1);

  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("network.auth.force-generic-ntlm");
    Services.prefs.clearUserPref("network.auth.force-generic-ntlm-v1");

    await httpserv.stop();
  });

  var chan = makeChan(URL + "/auth/ntlm/simple", URL);

  chan.notificationCallbacks = new Requestor(FLAG_RETURN_FALSE, 2);
  let [req, buf] = await new Promise(resolve => {
    chan.asyncOpen(
      new ChannelListener((req1, buf1) => resolve([req1, buf1]), null)
    );
  });
  Assert.ok(buf);
  Assert.equal(req.QueryInterface(Ci.nsIHttpChannel).responseStatus, 200);
});
