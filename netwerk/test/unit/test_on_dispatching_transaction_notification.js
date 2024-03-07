// This file tests async handling of a channel suspended in http-on-dispatching-transaction
"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

let requestCount = 0;
function successResponseHandler(metadata, response) {
  const text = "success response";
  response.setHeader("Content-Type", "text/plain", false);
  response.bodyOutputStream.write(text, text.length);
  Assert.ok(true, "Received expected request.");
  requestCount++;
}

let baseUrl;
add_setup(async function setup() {
  let httpServer = new HttpServer();
  httpServer.registerPathHandler("/", successResponseHandler);
  httpServer.start(-1);

  baseUrl = `http://localhost:${httpServer.identity.primaryPort}/`;

  registerCleanupFunction(async () => {
    await httpServer.stop();
  });
});

function onDispatchingTransaction(callback, channel) {
  Services.obs.addObserver(
    {
      observe(subject) {
        if (!channel || channel == subject) {
          Services.obs.removeObserver(this, "http-on-dispatching-transaction");
          callback(subject.QueryInterface(Ci.nsIHttpChannel));
        }
      },
    },
    "http-on-dispatching-transaction"
  );
}

add_task(async function test_notification() {
  let initReqCount = requestCount;
  let chan = NetUtil.newChannel({
    uri: baseUrl,
    loadUsingSystemPrincipal: true,
  });
  let { promise: chanPromise, resolve: chanResolve } = Promise.withResolvers();
  await new Promise(resolve => {
    onDispatchingTransaction(resolve, chan);
    chan.asyncOpen(new SimpleChannelListener(chanResolve));
  });
  equal(requestCount, initReqCount);
  await chanPromise;
  equal(requestCount, initReqCount + 1);
});

add_task(async function test_cancel() {
  let initReqCount = requestCount;
  let chan = NetUtil.newChannel({
    uri: baseUrl,
    loadUsingSystemPrincipal: true,
  });
  onDispatchingTransaction(ch => {
    ch.cancel(Cr.NS_ERROR_ABORT);
  }, chan);
  await new Promise(resolve => {
    chan.asyncOpen(new SimpleChannelListener(resolve));
  });
  equal(requestCount, initReqCount);
  equal(chan.status, Cr.NS_ERROR_ABORT);
});

add_task(async function test_suspend_resume() {
  let initReqCount = requestCount;
  let chan = NetUtil.newChannel({
    uri: baseUrl,
    loadUsingSystemPrincipal: true,
  });
  let { promise: notificationPromise, resolve: notificationResolve } =
    Promise.withResolvers();

  onDispatchingTransaction(ch => {
    ch.suspend();
    notificationResolve(ch);
  }, chan);
  let chanPromise = new Promise(resolve => {
    chan.asyncOpen(new SimpleChannelListener(resolve));
  });

  await notificationPromise;
  await new Promise(resolve => do_timeout(10, resolve));
  equal(requestCount, initReqCount);

  chan.resume();
  await chanPromise;
  equal(requestCount, initReqCount + 1);
});
