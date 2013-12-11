/* -*- Mode: javascript; js-indent-level: 2; -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that we can detect nested event loops in tabs with the same URL.

const { defer } = devtools.require("sdk/core/promise");
var gClient1, gClient2;

function run_test() {
  initTestDebuggerServer();
  addTestGlobal("test-nesting1");
  addTestGlobal("test-nesting1");
  // Conect the first client to the first debuggee.
  gClient1 = new DebuggerClient(DebuggerServer.connectPipe());
  gClient1.connect(function () {
    attachTestThread(gClient1, "test-nesting1", function (aResponse, aTabClient, aThreadClient) {
      start_second_connection();
    });
  });
  do_test_pending();
}

function start_second_connection() {
  gClient2 = new DebuggerClient(DebuggerServer.connectPipe());
  gClient2.connect(function () {
    attachTestThread(gClient2, "test-nesting1", function (aResponse, aTabClient, aThreadClient) {
      test_nesting();
    });
  });
}

function test_nesting() {
  const { resolve, reject, promise } = defer();

  gClient1.activeThread.resume(aResponse => {
    do_check_eq(aResponse.error, "wrongOrder");
    gClient2.activeThread.resume(aResponse => {
      do_check_true(!aResponse.error);
      do_check_eq(aResponse.from, gClient2.activeThread.actor);

      gClient1.activeThread.resume(aResponse => {
        do_check_true(!aResponse.error);
        do_check_eq(aResponse.from, gClient1.activeThread.actor);

        gClient1.close(() => finishClient(gClient2));
      });
    });
  });
}
