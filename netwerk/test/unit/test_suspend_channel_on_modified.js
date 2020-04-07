// This file tests async handling of a channel suspended in http-on-modify-request.
"use strict";

var CC = Components.Constructor;

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

var obs = Cc["@mozilla.org/observer-service;1"].getService(
  Ci.nsIObserverService
);

var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);

// baseUrl is always the initial connection attempt and is handled by
// failResponseHandler since every test expects that request will either be
// redirected or cancelled.
var baseUrl;

function failResponseHandler(metadata, response) {
  var text = "failure response";
  response.setHeader("Content-Type", "text/plain", false);
  response.bodyOutputStream.write(text, text.length);
  Assert.ok(false, "Received request when we shouldn't.");
}

function successResponseHandler(metadata, response) {
  var text = "success response";
  response.setHeader("Content-Type", "text/plain", false);
  response.bodyOutputStream.write(text, text.length);
  Assert.ok(true, "Received expected request.");
}

function onModifyListener(callback) {
  obs.addObserver(
    {
      observe(subject, topic, data) {
        var obs = Cc["@mozilla.org/observer-service;1"].getService();
        obs = obs.QueryInterface(Ci.nsIObserverService);
        obs.removeObserver(this, "http-on-modify-request");
        callback(subject.QueryInterface(Ci.nsIHttpChannel));
      },
    },
    "http-on-modify-request"
  );
}

function startChannelRequest(baseUrl, flags, expectedResponse = null) {
  var chan = NetUtil.newChannel({
    uri: baseUrl,
    loadUsingSystemPrincipal: true,
  });
  chan.asyncOpen(
    new ChannelListener(
      (request, data, context) => {
        if (expectedResponse) {
          Assert.equal(data, expectedResponse);
        } else {
          Assert.ok(!data, "no response");
        }
        executeSoon(run_next_test);
      },
      null,
      flags
    )
  );
}

add_test(function testSimpleRedirect() {
  onModifyListener(chan => {
    chan.redirectTo(ios.newURI(`${baseUrl}/success`));
  });
  startChannelRequest(baseUrl, undefined, "success response");
});

add_test(function testSimpleCancel() {
  onModifyListener(chan => {
    chan.cancel(Cr.NS_BINDING_ABORTED);
  });
  startChannelRequest(baseUrl, CL_EXPECT_FAILURE);
});

add_test(function testSimpleCancelRedirect() {
  onModifyListener(chan => {
    chan.redirectTo(ios.newURI(`${baseUrl}/fail`));
    chan.cancel(Cr.NS_BINDING_ABORTED);
  });
  startChannelRequest(baseUrl, CL_EXPECT_FAILURE);
});

// Test a request that will get redirected asynchronously.  baseUrl should
// not be requested, we should receive the request for the redirectedUrl.
add_test(function testAsyncRedirect() {
  onModifyListener(chan => {
    // Suspend the channel then yield to make this async.
    chan.suspend();
    Promise.resolve().then(() => {
      chan.redirectTo(ios.newURI(`${baseUrl}/success`));
      chan.resume();
    });
  });
  startChannelRequest(baseUrl, undefined, "success response");
});

add_test(function testSyncRedirect() {
  onModifyListener(chan => {
    chan.suspend();
    chan.redirectTo(ios.newURI(`${baseUrl}/success`));
    Promise.resolve().then(() => {
      chan.resume();
    });
  });
  startChannelRequest(baseUrl, undefined, "success response");
});

add_test(function testAsyncCancel() {
  onModifyListener(chan => {
    // Suspend the channel then yield to make this async.
    chan.suspend();
    Promise.resolve().then(() => {
      chan.cancel(Cr.NS_BINDING_ABORTED);
      chan.resume();
    });
  });
  startChannelRequest(baseUrl, CL_EXPECT_FAILURE);
});

add_test(function testSyncCancel() {
  onModifyListener(chan => {
    chan.suspend();
    chan.cancel(Cr.NS_BINDING_ABORTED);
    Promise.resolve().then(() => {
      chan.resume();
    });
  });
  startChannelRequest(baseUrl, CL_EXPECT_FAILURE);
});

// Test request that will get redirected and cancelled asynchronously,
// ensure no connection is made.
add_test(function testAsyncCancelRedirect() {
  onModifyListener(chan => {
    // Suspend the channel then yield to make this async.
    chan.suspend();
    Promise.resolve().then(() => {
      chan.cancel(Cr.NS_BINDING_ABORTED);
      chan.redirectTo(ios.newURI(`${baseUrl}/fail`));
      chan.resume();
    });
  });
  startChannelRequest(baseUrl, CL_EXPECT_FAILURE);
});

// Test a request that will get cancelled synchronously, ensure async redirect
// is not made.
add_test(function testSyncCancelRedirect() {
  onModifyListener(chan => {
    chan.suspend();
    chan.cancel(Cr.NS_BINDING_ABORTED);
    Promise.resolve().then(() => {
      chan.redirectTo(ios.newURI(`${baseUrl}/fail`));
      chan.resume();
    });
  });
  startChannelRequest(baseUrl, CL_EXPECT_FAILURE);
});

function run_test() {
  var httpServer = new HttpServer();
  httpServer.registerPathHandler("/", failResponseHandler);
  httpServer.registerPathHandler("/fail", failResponseHandler);
  httpServer.registerPathHandler("/success", successResponseHandler);
  httpServer.start(-1);

  baseUrl = `http://localhost:${httpServer.identity.primaryPort}`;

  run_next_test();

  registerCleanupFunction(function() {
    httpServer.stop(() => {});
  });
}
