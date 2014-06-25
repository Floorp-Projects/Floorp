/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that we can detect nested event loops in tabs with the same URL.

var gClient1, gClient2, gThreadClient1, gThreadClient2;

function run_test() {
  initTestDebuggerServer();
  addTestGlobal("test-nesting1");
  addTestGlobal("test-nesting1");
  // Conect the first client to the first debuggee.
  gClient1 = new DebuggerClient(DebuggerServer.connectPipe());
  gClient1.connect(function () {
    attachTestThread(gClient1, "test-nesting1", function (aResponse, aTabClient, aThreadClient) {
      gThreadClient1 = aThreadClient;
      start_second_connection();
    });
  });
  do_test_pending();
}

function start_second_connection() {
  gClient2 = new DebuggerClient(DebuggerServer.connectPipe());
  gClient2.connect(function () {
    attachTestThread(gClient2, "test-nesting1", function (aResponse, aTabClient, aThreadClient) {
      gThreadClient2 = aThreadClient;
      test_nesting();
    });
  });
}

function test_nesting() {
  const { resolve, reject, promise: p } = promise.defer();

  gThreadClient1.resume(aResponse => {
    do_check_eq(aResponse.error, "wrongOrder");
    gThreadClient2.resume(aResponse => {
      do_check_true(!aResponse.error);
      do_check_eq(aResponse.from, gThreadClient2.actor);

      gThreadClient1.resume(aResponse => {
        do_check_true(!aResponse.error);
        do_check_eq(aResponse.from, gThreadClient1.actor);

        gClient1.close(() => finishClient(gClient2));
      });
    });
  });
}
