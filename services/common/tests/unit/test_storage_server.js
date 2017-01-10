/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/async.js");
Cu.import("resource://services-common/rest.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://testing-common/services/common/storageserver.js");

const DEFAULT_USER = "123";
const DEFAULT_PASSWORD = "password";

/**
 * Helper function to prepare a RESTRequest against the server.
 */
function localRequest(server, path, user = DEFAULT_USER, password = DEFAULT_PASSWORD) {
  _("localRequest: " + path);
  let identity = server.server.identity;
  let url = identity.primaryScheme + "://" + identity.primaryHost + ":" +
            identity.primaryPort + path;
  _("url: " + url);
  let req = new RESTRequest(url);

  let header = basic_auth_header(user, password);
  req.setHeader("Authorization", header);
  req.setHeader("Accept", "application/json");

  return req;
}

/**
 * Helper function to validate an HTTP response from the server.
 */
function validateResponse(response) {
  do_check_true("x-timestamp" in response.headers);

  if ("content-length" in response.headers) {
    let cl = parseInt(response.headers["content-length"]);

    if (cl != 0) {
      do_check_true("content-type" in response.headers);
      do_check_eq("application/json", response.headers["content-type"]);
    }
  }

  if (response.status == 204 || response.status == 304) {
    do_check_false("content-type" in response.headers);

    if ("content-length" in response.headers) {
      do_check_eq(response.headers["content-length"], "0");
    }
  }

  if (response.status == 405) {
    do_check_true("allow" in response.headers);
  }
}

/**
 * Helper function to synchronously wait for a response and validate it.
 */
function waitAndValidateResponse(cb, request) {
  let error = cb.wait();

  if (!error) {
    validateResponse(request.response);
  }

  return error;
}

/**
 * Helper function to synchronously perform a GET request.
 *
 * @return Error instance or null if no error.
 */
function doGetRequest(request) {
  let cb = Async.makeSpinningCallback();
  request.get(cb);

  return waitAndValidateResponse(cb, request);
}

/**
 * Helper function to synchronously perform a PUT request.
 *
 * @return Error instance or null if no error.
 */
function doPutRequest(request, data) {
  let cb = Async.makeSpinningCallback();
  request.put(data, cb);

  return waitAndValidateResponse(cb, request);
}

/**
 * Helper function to synchronously perform a DELETE request.
 *
 * @return Error or null if no error was encountered.
 */
function doDeleteRequest(request) {
  let cb = Async.makeSpinningCallback();
  request.delete(cb);

  return waitAndValidateResponse(cb, request);
}

function run_test() {
  Log.repository.getLogger("Services.Common.Test.StorageServer").level =
    Log.Level.Trace;
  initTestLogging();

  run_next_test();
}

add_test(function test_creation() {
  _("Ensure a simple server can be created.");

  // Explicit callback for this one.
  let server = new StorageServer({
    __proto__: StorageServerCallback,
  });
  do_check_true(!!server);

  server.start(-1, function() {
    _("Started on " + server.port);
    server.stop(run_next_test);
  });
});

add_test(function test_synchronous_start() {
  _("Ensure starting using startSynchronous works.");

  let server = new StorageServer();
  server.startSynchronous();
  server.stop(run_next_test);
});

add_test(function test_url_parsing() {
  _("Ensure server parses URLs properly.");

  let server = new StorageServer();

  // Check that we can parse a BSO URI.
  let parts = server.pathRE.exec("/2.0/12345/storage/crypto/keys");
  let [all, version, user, first, rest] = parts;
  do_check_eq(all, "/2.0/12345/storage/crypto/keys");
  do_check_eq(version, "2.0");
  do_check_eq(user, "12345");
  do_check_eq(first, "storage");
  do_check_eq(rest, "crypto/keys");
  do_check_eq(null, server.pathRE.exec("/nothing/else"));

  // Check that we can parse a collection URI.
  parts = server.pathRE.exec("/2.0/123/storage/crypto");
  [all, version, user, first, rest] = parts;
  do_check_eq(all, "/2.0/123/storage/crypto");
  do_check_eq(version, "2.0");
  do_check_eq(user, "123");
  do_check_eq(first, "storage");
  do_check_eq(rest, "crypto");

  // We don't allow trailing slash on storage URI.
  parts = server.pathRE.exec("/2.0/1234/storage/");
  do_check_eq(parts, undefined);

  // storage alone is a valid request.
  parts = server.pathRE.exec("/2.0/123456/storage");
  [all, version, user, first, rest] = parts;
  do_check_eq(all, "/2.0/123456/storage");
  do_check_eq(version, "2.0");
  do_check_eq(user, "123456");
  do_check_eq(first, "storage");
  do_check_eq(rest, undefined);

  parts = server.storageRE.exec("storage");
  let storage, collection, id;
  [all, storage, collection, id] = parts;
  do_check_eq(all, "storage");
  do_check_eq(collection, undefined);

  run_next_test();
});

add_test(function test_basic_http() {
  let server = new StorageServer();
  server.registerUser("345", "password");
  do_check_true(server.userExists("345"));
  server.startSynchronous();

  _("Started on " + server.port);
  do_check_eq(server.requestCount, 0);
  let req = localRequest(server, "/2.0/storage/crypto/keys");
  _("req is " + req);
  req.get(function(err) {
    do_check_eq(null, err);
    do_check_eq(server.requestCount, 1);
    server.stop(run_next_test);
  });
});

add_test(function test_info_collections() {
  let server = new StorageServer();
  server.registerUser("123", "password");
  server.startSynchronous();

  let path = "/2.0/123/info/collections";

  _("info/collections on empty server should be empty object.");
  let request = localRequest(server, path, "123", "password");
  let error = doGetRequest(request);
  do_check_eq(error, null);
  do_check_eq(request.response.status, 200);
  do_check_eq(request.response.body, "{}");

  _("Creating an empty collection should result in collection appearing.");
  let coll = server.createCollection("123", "col1");
  request = localRequest(server, path, "123", "password");
  error = doGetRequest(request);
  do_check_eq(error, null);
  do_check_eq(request.response.status, 200);
  let info = JSON.parse(request.response.body);
  do_check_attribute_count(info, 1);
  do_check_true("col1" in info);
  do_check_eq(info.col1, coll.timestamp);

  server.stop(run_next_test);
});

add_test(function test_bso_get_existing() {
  _("Ensure that BSO retrieval works.");

  let server = new StorageServer();
  server.registerUser("123", "password");
  server.createContents("123", {
    test: {"bso": {"foo": "bar"}}
  });
  server.startSynchronous();

  let coll = server.user("123").collection("test");

  let request = localRequest(server, "/2.0/123/storage/test/bso", "123",
                             "password");
  let error = doGetRequest(request);
  do_check_eq(error, null);
  do_check_eq(request.response.status, 200);
  do_check_eq(request.response.headers["content-type"], "application/json");
  let bso = JSON.parse(request.response.body);
  do_check_attribute_count(bso, 3);
  do_check_eq(bso.id, "bso");
  do_check_eq(bso.modified, coll.bso("bso").modified);
  let payload = JSON.parse(bso.payload);
  do_check_attribute_count(payload, 1);
  do_check_eq(payload.foo, "bar");

  server.stop(run_next_test);
});

add_test(function test_percent_decoding() {
  _("Ensure query string arguments with percent encoded are handled.");

  let server = new StorageServer();
  server.registerUser("123", "password");
  server.startSynchronous();

  let coll = server.user("123").createCollection("test");
  coll.insert("001", {foo: "bar"});
  coll.insert("002", {bar: "foo"});

  let request = localRequest(server, "/2.0/123/storage/test?ids=001%2C002",
                             "123", "password");
  let error = doGetRequest(request);
  do_check_null(error);
  do_check_eq(request.response.status, 200);
  let items = JSON.parse(request.response.body).items;
  do_check_attribute_count(items, 2);

  server.stop(run_next_test);
});

add_test(function test_bso_404() {
  _("Ensure the server responds with a 404 if a BSO does not exist.");

  let server = new StorageServer();
  server.registerUser("123", "password");
  server.createContents("123", {
    test: {}
  });
  server.startSynchronous();

  let request = localRequest(server, "/2.0/123/storage/test/foo");
  let error = doGetRequest(request);
  do_check_eq(error, null);

  do_check_eq(request.response.status, 404);
  do_check_false("content-type" in request.response.headers);

  server.stop(run_next_test);
});

add_test(function test_bso_if_modified_since_304() {
  _("Ensure the server responds properly to X-If-Modified-Since for BSOs.");

  let server = new StorageServer();
  server.registerUser("123", "password");
  server.createContents("123", {
    test: {bso: {foo: "bar"}}
  });
  server.startSynchronous();

  let coll = server.user("123").collection("test");
  do_check_neq(coll, null);

  // Rewind clock just in case.
  coll.timestamp -= 10000;
  coll.bso("bso").modified -= 10000;

  let request = localRequest(server, "/2.0/123/storage/test/bso",
                             "123", "password");
  request.setHeader("X-If-Modified-Since", "" + server.serverTime());
  let error = doGetRequest(request);
  do_check_eq(null, error);

  do_check_eq(request.response.status, 304);
  do_check_false("content-type" in request.response.headers);

  request = localRequest(server, "/2.0/123/storage/test/bso",
                             "123", "password");
  request.setHeader("X-If-Modified-Since", "" + (server.serverTime() - 20000));
  error = doGetRequest(request);
  do_check_eq(null, error);
  do_check_eq(request.response.status, 200);
  do_check_eq(request.response.headers["content-type"], "application/json");

  server.stop(run_next_test);
});

add_test(function test_bso_if_unmodified_since() {
  _("Ensure X-If-Unmodified-Since works properly on BSOs.");

  let server = new StorageServer();
  server.registerUser("123", "password");
  server.createContents("123", {
    test: {bso: {foo: "bar"}}
  });
  server.startSynchronous();

  let coll = server.user("123").collection("test");
  do_check_neq(coll, null);

  let time = coll.bso("bso").modified;

  _("Ensure we get a 412 for specified times older than server time.");
  let request = localRequest(server, "/2.0/123/storage/test/bso",
                             "123", "password");
  request.setHeader("X-If-Unmodified-Since", time - 5000);
  request.setHeader("Content-Type", "application/json");
  let payload = JSON.stringify({"payload": "foobar"});
  let error = doPutRequest(request, payload);
  do_check_eq(null, error);
  do_check_eq(request.response.status, 412);

  _("Ensure we get a 204 if update goes through.");
  request = localRequest(server, "/2.0/123/storage/test/bso",
                         "123", "password");
  request.setHeader("Content-Type", "application/json");
  request.setHeader("X-If-Unmodified-Since", time + 1);
  error = doPutRequest(request, payload);
  do_check_eq(null, error);
  do_check_eq(request.response.status, 204);
  do_check_true(coll.timestamp > time);

  // Not sure why a client would send X-If-Unmodified-Since if a BSO doesn't
  // exist. But, why not test it?
  _("Ensure we get a 201 if creation goes through.");
  request = localRequest(server, "/2.0/123/storage/test/none",
                         "123", "password");
  request.setHeader("Content-Type", "application/json");
  request.setHeader("X-If-Unmodified-Since", time);
  error = doPutRequest(request, payload);
  do_check_eq(null, error);
  do_check_eq(request.response.status, 201);

  server.stop(run_next_test);
});

add_test(function test_bso_delete_not_exist() {
  _("Ensure server behaves properly when deleting a BSO that does not exist.");

  let server = new StorageServer();
  server.registerUser("123", "password");
  server.user("123").createCollection("empty");
  server.startSynchronous();

  server.callback.onItemDeleted = function onItemDeleted(username, collection,
                                                         id) {
    do_throw("onItemDeleted should not have been called.");
  };

  let request = localRequest(server, "/2.0/123/storage/empty/nada",
                             "123", "password");
  let error = doDeleteRequest(request);
  do_check_eq(error, null);
  do_check_eq(request.response.status, 404);
  do_check_false("content-type" in request.response.headers);

  server.stop(run_next_test);
});

add_test(function test_bso_delete_exists() {
  _("Ensure proper semantics when deleting a BSO that exists.");

  let server = new StorageServer();
  server.registerUser("123", "password");
  server.startSynchronous();

  let coll = server.user("123").createCollection("test");
  let bso = coll.insert("myid", {foo: "bar"});
  let timestamp = coll.timestamp;

  server.callback.onItemDeleted = function onDeleted(username, collection, id) {
    delete server.callback.onItemDeleted;
    do_check_eq(username, "123");
    do_check_eq(collection, "test");
    do_check_eq(id, "myid");
  };

  let request = localRequest(server, "/2.0/123/storage/test/myid",
                             "123", "password");
  let error = doDeleteRequest(request);
  do_check_eq(error, null);
  do_check_eq(request.response.status, 204);
  do_check_eq(coll.bsos().length, 0);
  do_check_true(coll.timestamp > timestamp);

  _("On next request the BSO should not exist.");
  request = localRequest(server, "/2.0/123/storage/test/myid",
                         "123", "password");
  error = doGetRequest(request);
  do_check_eq(error, null);
  do_check_eq(request.response.status, 404);

  server.stop(run_next_test);
});

add_test(function test_bso_delete_unmodified() {
  _("Ensure X-If-Unmodified-Since works when deleting BSOs.");

  let server = new StorageServer();
  server.startSynchronous();
  server.registerUser("123", "password");
  let coll = server.user("123").createCollection("test");
  let bso = coll.insert("myid", {foo: "bar"});

  let modified = bso.modified;

  _("Issuing a DELETE with an older time should fail.");
  let path = "/2.0/123/storage/test/myid";
  let request = localRequest(server, path, "123", "password");
  request.setHeader("X-If-Unmodified-Since", modified - 1000);
  let error = doDeleteRequest(request);
  do_check_eq(error, null);
  do_check_eq(request.response.status, 412);
  do_check_false("content-type" in request.response.headers);
  do_check_neq(coll.bso("myid"), null);

  _("Issuing a DELETE with a newer time should work.");
  request = localRequest(server, path, "123", "password");
  request.setHeader("X-If-Unmodified-Since", modified + 1000);
  error = doDeleteRequest(request);
  do_check_eq(error, null);
  do_check_eq(request.response.status, 204);
  do_check_true(coll.bso("myid").deleted);

  server.stop(run_next_test);
});

add_test(function test_collection_get_unmodified_since() {
  _("Ensure conditional unmodified get on collection works when it should.");

  let server = new StorageServer();
  server.registerUser("123", "password");
  server.startSynchronous();
  let collection = server.user("123").createCollection("testcoll");
  collection.insert("bso0", {foo: "bar"});

  let serverModified = collection.timestamp;

  let request1 = localRequest(server, "/2.0/123/storage/testcoll",
                              "123", "password");
  request1.setHeader("X-If-Unmodified-Since", serverModified);
  let error = doGetRequest(request1);
  do_check_null(error);
  do_check_eq(request1.response.status, 200);

  let request2 = localRequest(server, "/2.0/123/storage/testcoll",
                              "123", "password");
  request2.setHeader("X-If-Unmodified-Since", serverModified - 1);
  error = doGetRequest(request2);
  do_check_null(error);
  do_check_eq(request2.response.status, 412);

  server.stop(run_next_test);
});

add_test(function test_bso_get_unmodified_since() {
  _("Ensure conditional unmodified get on BSO works appropriately.");

  let server = new StorageServer();
  server.registerUser("123", "password");
  server.startSynchronous();
  let collection = server.user("123").createCollection("testcoll");
  let bso = collection.insert("bso0", {foo: "bar"});

  let serverModified = bso.modified;

  let request1 = localRequest(server, "/2.0/123/storage/testcoll/bso0",
                              "123", "password");
  request1.setHeader("X-If-Unmodified-Since", serverModified);
  let error = doGetRequest(request1);
  do_check_null(error);
  do_check_eq(request1.response.status, 200);

  let request2 = localRequest(server, "/2.0/123/storage/testcoll/bso0",
                              "123", "password");
  request2.setHeader("X-If-Unmodified-Since", serverModified - 1);
  error = doGetRequest(request2);
  do_check_null(error);
  do_check_eq(request2.response.status, 412);

  server.stop(run_next_test);
});

add_test(function test_missing_collection_404() {
  _("Ensure a missing collection returns a 404.");

  let server = new StorageServer();
  server.registerUser("123", "password");
  server.startSynchronous();

  let request = localRequest(server, "/2.0/123/storage/none", "123", "password");
  let error = doGetRequest(request);
  do_check_eq(error, null);
  do_check_eq(request.response.status, 404);
  do_check_false("content-type" in request.response.headers);

  server.stop(run_next_test);
});

add_test(function test_get_storage_405() {
  _("Ensure that a GET on /storage results in a 405.");

  let server = new StorageServer();
  server.registerUser("123", "password");
  server.startSynchronous();

  let request = localRequest(server, "/2.0/123/storage", "123", "password");
  let error = doGetRequest(request);
  do_check_eq(error, null);
  do_check_eq(request.response.status, 405);
  do_check_eq(request.response.headers["allow"], "DELETE");

  server.stop(run_next_test);
});

add_test(function test_delete_storage() {
  _("Ensure that deleting all of storage works.");

  let server = new StorageServer();
  server.registerUser("123", "password");
  server.createContents("123", {
    foo: {a: {foo: "bar"}, b: {bar: "foo"}},
    baz: {c: {bob: "law"}, blah: {law: "blog"}}
  });

  server.startSynchronous();

  let request = localRequest(server, "/2.0/123/storage", "123", "password");
  let error = doDeleteRequest(request);
  do_check_eq(error, null);
  do_check_eq(request.response.status, 204);
  do_check_attribute_count(server.users["123"].collections, 0);

  server.stop(run_next_test);
});

add_test(function test_x_num_records() {
  let server = new StorageServer();
  server.registerUser("123", "password");

  server.createContents("123", {
    crypto: {foos: {foo: "bar"},
             bars: {foo: "baz"}}
  });
  server.startSynchronous();
  let bso = localRequest(server, "/2.0/123/storage/crypto/foos");
  bso.get(function(err) {
    // BSO fetches don't have one.
    do_check_false("x-num-records" in this.response.headers);
    let col = localRequest(server, "/2.0/123/storage/crypto");
    col.get(function(err) {
      // Collection fetches do.
      do_check_eq(this.response.headers["x-num-records"], "2");
      server.stop(run_next_test);
    });
  });
});

add_test(function test_put_delete_put() {
  _("Bug 790397: Ensure BSO deleted flag is reset on PUT.");

  let server = new StorageServer();
  server.registerUser("123", "password");
  server.createContents("123", {
    test: {bso: {foo: "bar"}}
  });
  server.startSynchronous();

  _("Ensure we can PUT an existing record.");
  let request1 = localRequest(server, "/2.0/123/storage/test/bso", "123", "password");
  request1.setHeader("Content-Type", "application/json");
  let payload1 = JSON.stringify({"payload": "foobar"});
  let error1 = doPutRequest(request1, payload1);
  do_check_eq(null, error1);
  do_check_eq(request1.response.status, 204);

  _("Ensure we can DELETE it.");
  let request2 = localRequest(server, "/2.0/123/storage/test/bso", "123", "password");
  let error2 = doDeleteRequest(request2);
  do_check_eq(error2, null);
  do_check_eq(request2.response.status, 204);
  do_check_false("content-type" in request2.response.headers);

  _("Ensure we can PUT a previously deleted record.");
  let request3 = localRequest(server, "/2.0/123/storage/test/bso", "123", "password");
  request3.setHeader("Content-Type", "application/json");
  let payload3 = JSON.stringify({"payload": "foobar"});
  let error3 = doPutRequest(request3, payload3);
  do_check_eq(null, error3);
  do_check_eq(request3.response.status, 201);

  _("Ensure we can GET the re-uploaded record.");
  let request4 = localRequest(server, "/2.0/123/storage/test/bso", "123", "password");
  let error4 = doGetRequest(request4);
  do_check_eq(error4, null);
  do_check_eq(request4.response.status, 200);
  do_check_eq(request4.response.headers["content-type"], "application/json");

  server.stop(run_next_test);
});

add_test(function test_collection_get_newer() {
  _("Ensure get with newer argument on collection works.");

  let server = new StorageServer();
  server.registerUser("123", "password");
  server.startSynchronous();

  let coll = server.user("123").createCollection("test");
  let bso1 = coll.insert("001", {foo: "bar"});
  let bso2 = coll.insert("002", {bar: "foo"});

  // Don't want both records to have the same timestamp.
  bso2.modified = bso1.modified + 1000;

  function newerRequest(newer) {
    return localRequest(server, "/2.0/123/storage/test?newer=" + newer,
                        "123", "password");
  }

  let request1 = newerRequest(0);
  let error1 = doGetRequest(request1);
  do_check_null(error1);
  do_check_eq(request1.response.status, 200);
  let items1 = JSON.parse(request1.response.body).items;
  do_check_attribute_count(items1, 2);

  let request2 = newerRequest(bso1.modified + 1);
  let error2 = doGetRequest(request2);
  do_check_null(error2);
  do_check_eq(request2.response.status, 200);
  let items2 = JSON.parse(request2.response.body).items;
  do_check_attribute_count(items2, 1);

  let request3 = newerRequest(bso2.modified + 1);
  let error3 = doGetRequest(request3);
  do_check_null(error3);
  do_check_eq(request3.response.status, 200);
  let items3 = JSON.parse(request3.response.body).items;
  do_check_attribute_count(items3, 0);

  server.stop(run_next_test);
});
