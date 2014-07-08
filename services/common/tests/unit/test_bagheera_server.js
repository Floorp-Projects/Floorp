/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://testing-common/services/common/bagheeraserver.js");

function run_test() {
  run_next_test();
}

add_test(function test_server_empty() {
  let server = new BagheeraServer();

  do_check_false(server.hasNamespace("foo"));
  do_check_false(server.hasDocument("foo", "bar"));
  do_check_null(server.getDocument("foo", "bar"));

  server.createNamespace("foo");
  do_check_true(server.hasNamespace("foo"));

  run_next_test();
});

add_test(function test_server_start() {
  let server = new BagheeraServer();
  server.start();
  server.stop(run_next_test);
});

