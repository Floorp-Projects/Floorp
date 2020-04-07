/* global NetUtil, ChannelListener */

"use strict";

function inChildProcess() {
  return (
    Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime)
      .processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT
  );
}

let URL = null;
function makeChan() {
  return NetUtil.newChannel({
    uri: URL,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
}

function OpenChannelPromise(aChannel, aClosure) {
  return new Promise(resolve => {
    function processResponse(request, buffer, context) {
      aClosure(request.QueryInterface(Ci.nsIHttpChannel), buffer, context);
      resolve();
    }
    aChannel.asyncOpen(new ChannelListener(processResponse, null));
  });
}

// This test doesn't do much, except to communicate with the parent, and get
// URL we need to connect to.
add_task(async function setup() {
  ok(inChildProcess(), "Sanity check. This should run in the child process");
  // Initialize the URL. Parent runs the server
  do_send_remote_message("start-test");
  URL = await do_await_remote_message("start-test-done");
});

// This test performs a request, and checks that no cookie header are visible
// to the child process
add_task(async function test1() {
  let chan = makeChan();

  await OpenChannelPromise(chan, (request, buffer) => {
    equal(buffer, "response");
    Assert.throws(
      () => request.getRequestHeader("Cookie"),
      /NS_ERROR_NOT_AVAILABLE/,
      "Cookie header should not be visible on request in the child"
    );
    Assert.throws(
      () => request.getResponseHeader("Set-Cookie"),
      /NS_ERROR_NOT_AVAILABLE/,
      "Cookie header should not be visible on response in the child"
    );
  });

  // We also check that a cookie was saved by the Set-Cookie header
  // in the parent.
  do_send_remote_message("check-cookie-count");
  let count = await do_await_remote_message("check-cookie-count-done");
  equal(count, 1);
});

// This test communicates with the parent, to locally save a new cookie.
// Then it performs another request, makes sure no cookie headers are visible,
// after which it checks that both cookies are visible to the parent.
add_task(async function test2() {
  do_send_remote_message("set-cookie");
  await do_await_remote_message("set-cookie-done");

  let chan = makeChan();
  await OpenChannelPromise(chan, (request, buffer) => {
    equal(buffer, "response");
    Assert.throws(
      () => request.getRequestHeader("Cookie"),
      /NS_ERROR_NOT_AVAILABLE/,
      "Cookie header should not be visible on request in the child"
    );
    Assert.throws(
      () => request.getResponseHeader("Set-Cookie"),
      /NS_ERROR_NOT_AVAILABLE/,
      "Cookie header should not be visible on response in the child"
    );
  });

  // We should have two cookies. One set by the Set-Cookie header sent by the
  // server, and one that was manually set in the parent.
  do_send_remote_message("second-check-cookie-count");
  let count = await do_await_remote_message("second-check-cookie-count-done");
  equal(count, 2);
});
