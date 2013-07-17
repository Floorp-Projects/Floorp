/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://services-common/bagheeraclient.js");
Cu.import("resource://services-common/rest.js");
Cu.import("resource://testing-common/services-common/bagheeraserver.js");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Task.jsm");

function getClientAndServer() {
  let server = new BagheeraServer();
  server.start();

  let client = new BagheeraClient(server.serverURI);

  return [client, server];
}

function run_test() {
  initTestLogging("Trace");
  run_next_test();
}

add_test(function test_constructor() {
  let client = new BagheeraClient("http://localhost:1234/");

  run_next_test();
});

add_test(function test_post_json_transport_failure() {
  let client = new BagheeraClient("http://localhost:1234/");

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

add_task(function test_post_delete_multiple_obsolete_documents () {
  let [client, server] = getClientAndServer();
  let namespace = "foo";
  let documents = [
    [namespace, "one", "{v:1}"],
    [namespace, "two", "{v:2}"],
    [namespace, "three", "{v:3}"],
    [namespace, "four", "{v:4}"],
  ];

  try {
    // create initial documents
    server.createNamespace(namespace);
    for (let [ns, id, payload] of documents) {
      server.setDocument(ns, id, payload);
      do_check_true(server.hasDocument(ns, id));
    }

    // Test uploading with deleting some documents.
    let deleteIDs = [0, 1].map((no) => { return documents[no][1]; });
    let result = yield client.uploadJSON(namespace, "new-1", {foo: "bar"}, {deleteIDs: deleteIDs});
    do_check_true(result.transportSuccess);
    do_check_true(result.serverSuccess);
    do_check_true(server.hasDocument(namespace, "new-1"));
    for (let id of deleteIDs) {
      do_check_false(server.hasDocument(namespace, id));
    }
    // Check if the documents that were not staged for deletion are still there.
    for (let [,id,] of documents) {
      if (deleteIDs.indexOf(id) == -1) {
        do_check_true(server.hasDocument(namespace, id));
      }
    }

    // Test upload without deleting documents.
    let ids = Object.keys(server.namespaces[namespace]);
    result = yield client.uploadJSON(namespace, "new-2", {foo: "bar"});
    do_check_true(result.transportSuccess);
    do_check_true(result.serverSuccess);
    do_check_true(server.hasDocument(namespace, "new-2"));
    // Check to see if all the original documents are still there.
    for (let id of ids) {
      do_check_true(deleteIDs.indexOf(id) !== -1 || server.hasDocument(namespace, id));
    }
  } finally {
    let deferred = Promise.defer();
    server.stop(deferred.resolve.bind(deferred));
    yield deferred.promise;
  }
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
