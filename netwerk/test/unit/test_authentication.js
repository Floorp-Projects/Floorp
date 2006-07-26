// This file tests authentication prompt callbacks

function AuthPrompt1() {
}

// Also ought to test:
// - return false from authprompt
// - return wrong username from authprompt

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

    user.value = this.user;
    pw.value = this.pass;
    return true;
  },

  promptPassword: function ap1_promptPW(title, text, realm, save, pwd) {
    do_throw("unexpected promptPassword call");
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
    if (iid.equals(Components.interfaces.nsIAuthPrompt))
      return new AuthPrompt1();
    throw Components.results.NS_ERROR_NO_INTERFACE;
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
      current_test++;
      tests[current_test]();
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

var tests = [test_noauth, test_prompt1];
var current_test = 0;

function run_test() {
  start_server(4444);
  tests[0]();
}

function test_noauth() {
  var chan = makeChan("http://localhost:4444/auth");

  chan.asyncOpen(listener, null);

  do_test_pending();
}

function test_prompt1() {
  var chan = makeChan("http://localhost:4444/auth");

  chan.notificationCallbacks = new Requestor();
  listener.expectedCode = 200; // OK
  chan.asyncOpen(listener, null);

  do_test_pending();
}

