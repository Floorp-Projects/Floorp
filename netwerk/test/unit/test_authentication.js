// This file tests authentication prompt callbacks

do_import_script("netwerk/test/httpserver/httpd.js");

const FLAG_RETURN_FALSE   = 1 << 0;
const FLAG_WRONG_PASSWORD = 1 << 1;

const nsIAuthPrompt2 = Components.interfaces.nsIAuthPrompt2;
const nsIAuthInformation = Components.interfaces.nsIAuthInformation;


function AuthPrompt1(flags) {
  this.flags = flags;
}

AuthPrompt1.prototype = {
  user: "guest",
  pass: "guest",

  expectedRealm: "secret",

  QueryInterface: function authprompt_qi(iid) {
    if (iid.equals(Components.interfaces.nsISupports) ||
        iid.equals(Components.interfaces.nsIAuthPrompt))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  prompt: function ap1_prompt(title, text, realm, save, defaultText, result) {
    do_throw("unexpected prompt call");
  },

  promptUsernameAndPassword:
    function ap1_promptUP(title, text, realm, savePW, user, pw)
  {
    // Note that the realm here isn't actually the realm. it's a pw mgr key.
    do_check_eq("localhost:4444 (" + this.expectedRealm + ")", realm);
    if (text.indexOf(this.expectedRealm) == -1)
      do_throw("Text must indicate the realm");
    if (text.indexOf("localhost") == -1)
      do_throw("Text must indicate the hostname");
    if (text.indexOf("4444") == -1)
      do_throw("Text must indicate the port");
    if (text.indexOf("-1") != -1)
      do_throw("Text must contain negative numbers");

    if (this.flags & FLAG_RETURN_FALSE)
      return false;

    user.value = this.user;
    if (this.flags & FLAG_WRONG_PASSWORD) {
      pw.value = this.pass + ".wrong";
      // Now clear the flag to avoid an infinite loop
      this.flags &= ~FLAG_WRONG_PASSWORD;
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
    if (iid.equals(Components.interfaces.nsISupports) ||
        iid.equals(Components.interfaces.nsIAuthPrompt2))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  promptAuth:
    function ap2_promptAuth(channel, level, authInfo)
  {
    var isNTLM = channel.URI.path.indexOf("ntlm") != -1;

    if (isNTLM)
      this.expectedRealm = ""; // NTLM knows no realms

    do_check_eq(this.expectedRealm, authInfo.realm);

    var expectedLevel = isNTLM ?
                        nsIAuthPrompt2.LEVEL_PW_ENCRYPTED :
                        nsIAuthPrompt2.LEVEL_NONE;
    do_check_eq(expectedLevel, level);

    var expectedFlags = nsIAuthInformation.AUTH_HOST;

    if (isNTLM)
      expectedFlags |= nsIAuthInformation.NEED_DOMAIN;

    do_check_eq(expectedFlags, authInfo.flags);

    var expectedScheme = isNTLM ? "ntlm" : "basic";
    do_check_eq(expectedScheme, authInfo.authenticationScheme);

    // No passwords in the URL -> nothing should be prefilled
    do_check_eq(authInfo.username, "");
    do_check_eq(authInfo.password, "");
    do_check_eq(authInfo.domain, "");

    if (this.flags & FLAG_RETURN_FALSE)
      return false;

    authInfo.username = this.user;
    if (this.flags & FLAG_WRONG_PASSWORD) {
      authInfo.password = this.pass + ".wrong";
      // Now clear the flag to avoid an infinite loop
      this.flags &= ~FLAG_WRONG_PASSWORD;
    } else {
      authInfo.password = this.pass;
    }
    return true;
  },

  asyncPromptAuth: function ap2_async(chan, cb, ctx, lvl, info) {
    do_throw("not implemented yet")
  }
};

function Requestor(flags, versions) {
  this.flags = flags;
  this.versions = versions;
}

Requestor.prototype = {
  QueryInterface: function requestor_qi(iid) {
    if (iid.equals(Components.interfaces.nsISupports) ||
        iid.equals(Components.interfaces.nsIInterfaceRequestor))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  getInterface: function requestor_gi(iid) {
    if (this.versions & 1 &&
        iid.equals(Components.interfaces.nsIAuthPrompt)) {
      // Allow the prompt to store state by caching it here
      if (!this.prompt1)
        this.prompt1 = new AuthPrompt1(this.flags); 
      return this.prompt1;
    }
    if (this.versions & 2 &&
        iid.equals(Components.interfaces.nsIAuthPrompt2)) {
      // Allow the prompt to store state by caching it here
      if (!this.prompt2)
        this.prompt2 = new AuthPrompt2(this.flags); 
      return this.prompt2;
    }

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  prompt1: null,
  prompt2: null
};

function RealmTestRequestor() {}

RealmTestRequestor.prototype = {
  QueryInterface: function realmtest_qi(iid) {
    if (iid.equals(Components.interfaces.nsISupports) ||
        iid.equals(Components.interfaces.nsIInterfaceRequestor) ||
        iid.equals(Components.interfaces.nsIAuthPrompt2))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  getInterface: function realmtest_interface(iid) {
    if (iid.equals(Components.interfaces.nsIAuthPrompt2))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  promptAuth: function realmtest_checkAuth(channel, level, authInfo) {
    do_check_eq(authInfo.realm, '\\"foo_bar');

    return false;
  },

  asyncPromptAuth: function realmtest_async(chan, cb, ctx, lvl, info) {
    do_throw("not implemented yet");
  }
};

var listener = {
  expectedCode: 401, // Unauthorized

  onStartRequest: function test_onStartR(request, ctx) {
    try {
      if (!Components.isSuccessCode(request.status))
        do_throw("Channel should have a success code!");

      if (!(request instanceof Components.interfaces.nsIHttpChannel))
        do_throw("Expecting an HTTP channel");

      do_check_eq(request.responseStatus, this.expectedCode);
      // The request should be succeeded iff we expect 200
      do_check_eq(request.requestSucceeded, this.expectedCode == 200);

    } catch (e) {
      do_throw("Unexpected exception: " + e);
    }

    throw Components.results.NS_ERROR_ABORT;
  },

  onDataAvailable: function test_ODA() {
    do_throw("Should not get any data!");
  },

  onStopRequest: function test_onStopR(request, ctx, status) {
    do_check_eq(status, Components.results.NS_ERROR_ABORT);

    if (current_test < (tests.length - 1)) {
      // First, gotta clear the auth cache
      Components.classes["@mozilla.org/network/http-auth-manager;1"]
                .getService(Components.interfaces.nsIHttpAuthManager)
                .clearAll();

      current_test++;
      tests[current_test]();
    } else { 
      httpserv.stop();
    }

    do_test_finished();
  }
};

function makeChan(url) {
  var ios = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);
  var chan = ios.newChannel(url, null, null)
                .QueryInterface(Components.interfaces.nsIHttpChannel);

  return chan;
}

var tests = [test_noauth, test_returnfalse1, test_wrongpw1, test_prompt1,
             test_returnfalse2, test_wrongpw2, test_prompt2, test_ntlm,
             test_auth];
var current_test = 0;

var httpserv = null;

function run_test() {
  httpserv = new nsHttpServer();

  httpserv.registerPathHandler("/auth", authHandler);
  httpserv.registerPathHandler("/auth/ntlm/simple", authNtlmSimple);
  httpserv.registerPathHandler("/auth/realm", authRealm);

  httpserv.start(4444);

  tests[0]();
}

function test_noauth() {
  var chan = makeChan("http://localhost:4444/auth");

  listener.expectedCode = 401; // Unauthorized
  chan.asyncOpen(listener, null);

  do_test_pending();
}

function test_returnfalse1() {
  var chan = makeChan("http://localhost:4444/auth");

  chan.notificationCallbacks = new Requestor(FLAG_RETURN_FALSE, 1);
  listener.expectedCode = 401; // Unauthorized
  chan.asyncOpen(listener, null);

  do_test_pending();
}

function test_wrongpw1() {
  var chan = makeChan("http://localhost:4444/auth");

  chan.notificationCallbacks = new Requestor(FLAG_WRONG_PASSWORD, 1);
  listener.expectedCode = 200; // OK
  chan.asyncOpen(listener, null);

  do_test_pending();
}

function test_prompt1() {
  var chan = makeChan("http://localhost:4444/auth");

  chan.notificationCallbacks = new Requestor(0, 1);
  listener.expectedCode = 200; // OK
  chan.asyncOpen(listener, null);

  do_test_pending();
}

function test_returnfalse2() {
  var chan = makeChan("http://localhost:4444/auth");

  chan.notificationCallbacks = new Requestor(FLAG_RETURN_FALSE, 2);
  listener.expectedCode = 401; // Unauthorized
  chan.asyncOpen(listener, null);

  do_test_pending();
}

function test_wrongpw2() {
  var chan = makeChan("http://localhost:4444/auth");

  chan.notificationCallbacks = new Requestor(FLAG_WRONG_PASSWORD, 2);
  listener.expectedCode = 200; // OK
  chan.asyncOpen(listener, null);

  do_test_pending();
}

function test_prompt2() {
  var chan = makeChan("http://localhost:4444/auth");

  chan.notificationCallbacks = new Requestor(0, 2);
  listener.expectedCode = 200; // OK
  chan.asyncOpen(listener, null);

  do_test_pending();
}

function test_ntlm() {
  var chan = makeChan("http://localhost:4444/auth/ntlm/simple");

  chan.notificationCallbacks = new Requestor(FLAG_RETURN_FALSE, 2);
  listener.expectedCode = 401; // Unauthorized
  chan.asyncOpen(listener, null);

  do_test_pending();
}

function test_auth() {
  var chan = makeChan("http://localhost:4444/auth/realm");

  chan.notificationCallbacks = new RealmTestRequestor();
  listener.expectedCode = 401; // Unauthorized
  chan.asyncOpen(listener, null);

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
  response.setHeader("WWW-Authenticate", "NTLM" /* + ' realm="secret"' */);

  var body = "NOTE: This just sends an NTLM challenge, it never\n" +
             "accepts the authentication. It also closes\n" +
             "the connection after sending the challenge\n";


  response.bodyOutputStream.write(body, body.length);
}

// /auth/realm
function authRealm(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
  response.setHeader("WWW-Authenticate", 'Basic realm="\\"foo_bar"', false);
  var body = "success";

  response.bodyOutputStream.write(body, body.length);
}
