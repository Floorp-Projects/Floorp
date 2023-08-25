// This file ensures that canceling a channel early does not
// send the request to the server (bug 350790)
//
// I've also shoehorned in a test that ENSURE_CALLED_BEFORE_CONNECT works as
// expected: see comments that start with ENSURE_CALLED_BEFORE_CONNECT:
//
// This test also checks that cancelling a channel before asyncOpen, after
// onStopRequest, or during onDataAvailable works as expected.

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);
const reason = "testing";

function inChildProcess() {
  return Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

var ios = Services.io;
var ReferrerInfo = Components.Constructor(
  "@mozilla.org/referrer-info;1",
  "nsIReferrerInfo",
  "init"
);
var observer = {
  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),

  observe(subject, topic, data) {
    subject = subject.QueryInterface(Ci.nsIRequest);
    subject.cancelWithReason(Cr.NS_BINDING_ABORTED, reason);

    // ENSURE_CALLED_BEFORE_CONNECT: setting values should still work
    try {
      subject.QueryInterface(Ci.nsIHttpChannel);
      let currentReferrer = subject.getRequestHeader("Referer");
      Assert.equal(currentReferrer, "http://site1.com/");
      var uri = ios.newURI("http://site2.com");
      subject.referrerInfo = new ReferrerInfo(
        Ci.nsIReferrerInfo.EMPTY,
        true,
        uri
      );
    } catch (ex) {
      do_throw("Exception: " + ex);
    }
  },
};

let cancelDuringOnStartListener = {
  onStartRequest: function test_onStartR(request) {
    Assert.equal(request.status, Cr.NS_BINDING_ABORTED);
    // We didn't sync the reason to child process.
    if (!inChildProcess()) {
      Assert.equal(request.canceledReason, reason);
    }

    // ENSURE_CALLED_BEFORE_CONNECT: setting referrer should now fail
    try {
      request.QueryInterface(Ci.nsIHttpChannel);
      let currentReferrer = request.getRequestHeader("Referer");
      Assert.equal(currentReferrer, "http://site2.com/");
      var uri = ios.newURI("http://site3.com/");

      // Need to set NECKO_ERRORS_ARE_FATAL=0 else we'll abort process
      Services.env.set("NECKO_ERRORS_ARE_FATAL", "0");
      // we expect setting referrer to fail
      try {
        request.referrerInfo = new ReferrerInfo(
          Ci.nsIReferrerInfo.EMPTY,
          true,
          uri
        );
        do_throw("Error should have been thrown before getting here");
      } catch (ex) {}
    } catch (ex) {
      do_throw("Exception: " + ex);
    }
  },

  onDataAvailable: function test_ODA() {
    do_throw("Should not get any data!");
  },

  onStopRequest: function test_onStopR(request, status) {
    this.resolved();
  },
};

var cancelDuringOnDataListener = {
  data: "",
  channel: null,
  receivedSomeData: null,
  onStartRequest: function test_onStartR(request, ctx) {
    Assert.equal(request.status, Cr.NS_OK);
  },

  onDataAvailable: function test_ODA(request, stream, offset, count) {
    let string = NetUtil.readInputStreamToString(stream, count);
    Assert.ok(!string.includes("b"));
    this.data += string;
    this.channel.cancel(Cr.NS_BINDING_ABORTED);
    if (this.receivedSomeData) {
      this.receivedSomeData();
    }
  },

  onStopRequest: function test_onStopR(request, ctx, status) {
    Assert.ok(this.data.includes("a"), `data: ${this.data}`);
    Assert.equal(request.status, Cr.NS_BINDING_ABORTED);
    this.resolved();
  },
};

function makeChan(url) {
  var chan = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);

  // ENSURE_CALLED_BEFORE_CONNECT: set original value
  var uri = ios.newURI("http://site1.com");
  chan.referrerInfo = new ReferrerInfo(Ci.nsIReferrerInfo.EMPTY, true, uri);
  return chan;
}

var httpserv = null;

add_task(async function setup() {
  httpserv = new HttpServer();
  httpserv.registerPathHandler("/failtest", failtest);
  httpserv.registerPathHandler("/cancel_middle", cancel_middle);
  httpserv.registerPathHandler("/normal_response", normal_response);
  httpserv.start(-1);

  registerCleanupFunction(async () => {
    await new Promise(resolve => httpserv.stop(resolve));
  });
});

add_task(async function test_cancel_during_onModifyRequest() {
  var chan = makeChan(
    "http://localhost:" + httpserv.identity.primaryPort + "/failtest"
  );

  if (!inChildProcess()) {
    Services.obs.addObserver(observer, "http-on-modify-request");
  } else {
    do_send_remote_message("register-observer");
    await do_await_remote_message("register-observer-done");
  }

  await new Promise(resolve => {
    cancelDuringOnStartListener.resolved = resolve;
    chan.asyncOpen(cancelDuringOnStartListener);
  });

  if (!inChildProcess()) {
    Services.obs.removeObserver(observer, "http-on-modify-request");
  } else {
    do_send_remote_message("unregister-observer");
    await do_await_remote_message("unregister-observer-done");
  }
});

add_task(async function test_cancel_before_asyncOpen() {
  var chan = makeChan(
    "http://localhost:" + httpserv.identity.primaryPort + "/failtest"
  );

  chan.cancel(Cr.NS_BINDING_ABORTED);

  Assert.throws(
    () => {
      chan.asyncOpen(cancelDuringOnStartListener);
    },
    /NS_BINDING_ABORTED/,
    "cannot open if already cancelled"
  );
});

add_task(async function test_cancel_during_onData() {
  var chan = makeChan(
    "http://localhost:" + httpserv.identity.primaryPort + "/cancel_middle"
  );

  await new Promise(resolve => {
    cancelDuringOnDataListener.resolved = resolve;
    cancelDuringOnDataListener.channel = chan;
    chan.asyncOpen(cancelDuringOnDataListener);
  });
});

var cancelAfterOnStopListener = {
  data: "",
  channel: null,
  onStartRequest: function test_onStartR(request, ctx) {
    Assert.equal(request.status, Cr.NS_OK);
  },

  onDataAvailable: function test_ODA(request, stream, offset, count) {
    let string = NetUtil.readInputStreamToString(stream, count);
    this.data += string;
  },

  onStopRequest: function test_onStopR(request, status) {
    info("onStopRequest");
    Assert.equal(request.status, Cr.NS_OK);
    this.resolved();
  },
};

add_task(async function test_cancel_after_onStop() {
  var chan = makeChan(
    "http://localhost:" + httpserv.identity.primaryPort + "/normal_response"
  );

  await new Promise(resolve => {
    cancelAfterOnStopListener.resolved = resolve;
    cancelAfterOnStopListener.channel = chan;
    chan.asyncOpen(cancelAfterOnStopListener);
  });
  Assert.equal(chan.status, Cr.NS_OK);

  // For now it's unclear if cancelling after onStop should throw,
  // silently fail, or overwrite the channel's status as we currently do.
  // See discussion in bug 1553083
  chan.cancel(Cr.NS_BINDING_ABORTED);
  Assert.equal(chan.status, Cr.NS_BINDING_ABORTED);
});

// PATHS

// /failtest
function failtest(metadata, response) {
  do_throw("This should not be reached");
}

function cancel_middle(metadata, response) {
  response.processAsync();
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  let str1 = "a".repeat(128 * 1024);
  response.write(str1, str1.length);
  response.bodyOutputStream.flush();

  let p = new Promise(resolve => {
    cancelDuringOnDataListener.receivedSomeData = resolve;
  });
  p.then(() => {
    let str2 = "b".repeat(128 * 1024);
    response.write(str2, str2.length);
    response.finish();
  });
}

function normal_response(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  let str1 = "Is this normal?";
  response.write(str1, str1.length);
}
