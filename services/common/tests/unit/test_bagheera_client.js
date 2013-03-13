/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://services-common/bagheeraclient.js");
Cu.import("resource://services-common/rest.js");
Cu.import("resource://testing-common/services-common/bagheeraserver.js");


const PORT = 8080;

function getClientAndServer(port=PORT) {
  let uri = "http://localhost";
  let server = new BagheeraServer(uri);

  server.start(port);

  let client = new BagheeraClient(uri + ":" + port);

  return [client, server];
}

function run_test() {
  initTestLogging("Trace");
  run_next_test();
}

add_test(function test_constructor() {
  let client = new BagheeraClient("http://localhost:8080/");

  run_next_test();
});

add_test(function test_post_json_transport_failure() {
  let client = new BagheeraClient("http://localhost:8080/");

  client.uploadJSON("foo", "bar", {}).then(function onResult(result) {
    do_check_false(result.transportSuccess);

    run_next_test();
  });
});

add_test(function test_post_json_simple() {
  let [client, server] = getClientAndServer();

  server.createNamespace("foo");
  let promise = client.uploadJSON("foo", "bar", {foo: "bar", biz: "baz"});

  promise.then(function onSuccess(result) {
    do_check_true(result instanceof BagheeraClientRequestResult);
    do_check_true(result.request instanceof RESTRequest);
    do_check_true(result.transportSuccess);
    do_check_true(result.serverSuccess);

    server.stop(run_next_test);
  }, do_check_null);
});

add_test(function test_post_json_bad_data() {
  let [client, server] = getClientAndServer();

  server.createNamespace("foo");

  client.uploadJSON("foo", "bar", "{this is invalid json}").then(
    function onResult(result) {
    do_check_true(result.transportSuccess);
    do_check_false(result.serverSuccess);

    server.stop(run_next_test);
  });
});

add_test(function test_post_json_delete_obsolete() {
  let [client, server] = getClientAndServer();
  server.createNamespace("foo");
  server.setDocument("foo", "obsolete", "Old payload");

  let promise = client.uploadJSON("foo", "new", {foo: "bar"}, {deleteID: "obsolete"});
  promise.then(function onSuccess(result) {
    do_check_true(result.transportSuccess);
    do_check_true(result.serverSuccess);
    do_check_true(server.hasDocument("foo", "new"));
    do_check_false(server.hasDocument("foo", "obsolete"));

    server.stop(run_next_test);
  });
});

add_test(function test_delete_document() {
  let [client, server] = getClientAndServer();

  server.createNamespace("foo");
  server.setDocument("foo", "bar", "{}");

  client.deleteDocument("foo", "bar").then(function onResult(result) {
    do_check_true(result.transportSuccess);
    do_check_true(result.serverSuccess);

    do_check_null(server.getDocument("foo", "bar"));

    server.stop(run_next_test);
  });
});
