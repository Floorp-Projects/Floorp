/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/storageservice.js");
Cu.import("resource://testing-common/services-common/storageserver.js");

const BASE_URI = "http://localhost:8080/2.0";

function run_test() {
  initTestLogging("Trace");

  run_next_test();
}

function getRandomUser() {
  return "" + (Math.floor(Math.random() * 100000) + 1);
}

function getEmptyServer(user=getRandomUser(), password="password") {
  let users = {};
  users[user] = password;

  return storageServerForUsers(users, {
    meta:    {},
    clients: {},
    crypto:  {},
  });
}

function getClient(user=getRandomUser(), password="password") {
  let client = new StorageServiceClient(BASE_URI + "/" + user);
  client.addListener({
    onDispatch: function onDispatch(request) {
      let up = user + ":" + password;
      request.request.setHeader("authorization", "Basic " + btoa(up));
    }
  });

  return client;
}

function getServerAndClient(user=getRandomUser(), password="password") {
  let server = getEmptyServer(user, password);
  let client = getClient(user, password);

  return [server, client, user, password];
}

add_test(function test_auth_failure_listener() {
  _("Ensure the onAuthFailure listener is invoked.");

  let server = getEmptyServer();
  let client = getClient("324", "INVALID");
  client.addListener({
    onAuthFailure: function onAuthFailure(client, request) {
      _("onAuthFailure");
      server.stop(run_next_test);
    }
  });

  let request = client.getCollectionInfo();
  request.dispatch();
});

add_test(function test_duplicate_listeners() {
  _("Ensure that duplicate listeners aren't installed multiple times.");

  let server = getEmptyServer();
  let client = getClient("1234567", "BAD_PASSWORD");

  let invokeCount = 0;
  let listener = {
    onAuthFailure: function onAuthFailure() {
      invokeCount++;
    }
  };

  client.addListener(listener);
  // No error expected.
  client.addListener(listener);

  let request = client.getCollectionInfo();
  request.dispatch(function onComplete() {
    do_check_eq(invokeCount, 1);

    server.stop(run_next_test);
  });
});

add_test(function test_handler_object() {
  _("Ensure that installed handlers get their callbacks invoked.");

  let [server, client] = getServerAndClient();

  let onCompleteCount = 0;
  let onDispatchCount = 0;

  let handler = {
    onComplete: function onComplete() {
      onCompleteCount++;

      do_check_eq(onDispatchCount, 1);
      do_check_eq(onCompleteCount, 1);

      server.stop(run_next_test);
    },

    onDispatch: function onDispatch() {
      onDispatchCount++;
    },
  };
  let request = client.getCollectionInfo();
  request.handler = handler;
  request.dispatch();
});

add_test(function test_info_collections() {
  _("Ensure requests to /info/collections work as expected.");

  let [server, client] = getServerAndClient();

  let request = client.getCollectionInfo();
  request.dispatch(function onComplete(error, req) {
    do_check_null(error);
    do_check_eq("object", typeof req.resultObj);
    do_check_attribute_count(req.resultObj, 3);
    do_check_true("meta" in req.resultObj);

    server.stop(run_next_test);
  });
});

add_test(function test_info_collections_conditional_not_modified() {
  _("Ensure conditional getCollectionInfo requests work.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  let now = Date.now();

  user.createCollection("testcoll", {
    foo: new ServerBSO("foo", "payload", now)
  });

  let request = client.getCollectionInfo();
  request.locallyModifiedVersion = now + 10;
  request.dispatch(function onComplete(error, req) {
    do_check_null(error);
    do_check_true(req.notModified);

    server.stop(run_next_test);
  });
});

add_test(function test_info_collections_conditional_modified() {
  _("Ensure conditional getCollectionInfo requests work.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  let now = Date.now();

  user.createCollection("testcoll", {
    foo: new ServerBSO("foo", "payload", now)
  });

  let request = client.getCollectionInfo();
  request.locallyModifiedVersion = now - 10;
  request.dispatch(function onComplete(error, req) {
    do_check_null(error);
    do_check_false(req.notModified);

    server.stop(run_next_test);
  });
});

add_test(function test_get_quota() {
  _("Ensure quota requests work.");

  let [server, client] = getServerAndClient();

  let request = client.getQuota();
  request.dispatch(function onComplete(error, req) {
    do_check_null(error);

    do_check_eq(req.resultObj.quota, 1048576);

    server.stop(run_next_test);
  });
});

add_test(function test_get_quota_conditional_not_modified() {
  _("Ensure conditional getQuota requests work.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  let now = Date.now();

  user.createCollection("testcoll", {
    foo: new ServerBSO("foo", "payload", now)
  });

  let request = client.getQuota();
  request.locallyModifiedVersion = now + 10;
  request.dispatch(function onComplete(error, req) {
    do_check_null(error);
    do_check_true(req.notModified);

    server.stop(run_next_test);
  });
});

add_test(function test_get_quota_conditional_modified() {
  _("Ensure conditional getQuota requests work.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  let now = Date.now();

  user.createCollection("testcoll", {
    foo: new ServerBSO("foo", "payload", now)
  });

  let request = client.getQuota();
  request.locallyModifiedVersion = now - 10;
  request.dispatch(function onComplete(error, req) {
    do_check_null(error);
    do_check_false(req.notModified);

    server.stop(run_next_test);
  });
});

add_test(function test_get_collection_usage() {
  _("Ensure info/collection_usage requests work.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  user.createCollection("testcoll", {
    abc123: new ServerBSO("abc123", "payload", Date.now())
  });

  let request = client.getCollectionUsage();
  request.dispatch(function onComplete(error, req) {
    do_check_null(error);

    let usage = req.resultObj;
    do_check_true("testcoll" in usage);
    do_check_eq(usage.testcoll, "payload".length);

    server.stop(run_next_test);
  });
});

add_test(function test_get_usage_conditional_not_modified() {
  _("Ensure conditional getCollectionUsage requests work.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  let now = Date.now();

  user.createCollection("testcoll", {
    foo: new ServerBSO("foo", "payload", now)
  });

  let request = client.getCollectionUsage();
  request.locallyModifiedVersion = now + 10;
  request.dispatch(function onComplete(error, req) {
    do_check_null(error);
    do_check_true(req.notModified);

    server.stop(run_next_test);
  });
});

add_test(function test_get_usage_conditional_modified() {
  _("Ensure conditional getCollectionUsage requests work.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  let now = Date.now();

  user.createCollection("testcoll", {
    foo: new ServerBSO("foo", "payload", now)
  });

  let request = client.getCollectionUsage();
  request.locallyModifiedVersion = now - 10;
  request.dispatch(function onComplete(error, req) {
    do_check_null(error);
    do_check_false(req.notModified);

    server.stop(run_next_test);
  });
});

add_test(function test_get_collection_counts() {
  _("Ensure info/collection_counts requests work.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  user.createCollection("testcoll", {
    foo: new ServerBSO("foo", "payload0", Date.now()),
    bar: new ServerBSO("bar", "payload1", Date.now())
  });

  let request = client.getCollectionCounts();
  request.dispatch(function onComplete(error, req) {
    do_check_null(error);

    let counts = req.resultObj;
    do_check_true("testcoll" in counts);
    do_check_eq(counts.testcoll, 2);

    server.stop(run_next_test);
  });
});

add_test(function test_get_counts_conditional_not_modified() {
  _("Ensure conditional getCollectionCounts requests work.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  let now = Date.now();

  user.createCollection("testcoll", {
    foo: new ServerBSO("foo", "payload", now)
  });

  let request = client.getCollectionCounts();
  request.locallyModifiedVersion = now + 10;
  request.dispatch(function onComplete(error, req) {
    do_check_null(error);
    do_check_true(req.notModified);

    server.stop(run_next_test);
  });
});

add_test(function test_get_counts_conditional_modified() {
  _("Ensure conditional getCollectionCounts requests work.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  let now = Date.now();

  user.createCollection("testcoll", {
    foo: new ServerBSO("foo", "payload", now)
  });

  let request = client.getCollectionCounts();
  request.locallyModifiedVersion = now - 10;
  request.dispatch(function onComplete(error, req) {
    do_check_null(error);
    do_check_false(req.notModified);

    server.stop(run_next_test);
  });
});

add_test(function test_get_collection_simple() {
  _("Ensure basic collection retrieval works.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  user.createCollection("testcoll", {
    foo: new ServerBSO("foo", "payload0", Date.now()),
    bar: new ServerBSO("bar", "payload1", Date.now())
  });

  let request = client.getCollection("testcoll");
  let bsos = [];
  request.handler = {
    onBSORecord: function onBSORecord(request, bso) {
      bsos.push(bso);
    },

    onComplete: function onComplete(error, request) {
      do_check_null(error);

      do_check_eq(bsos.length, 2);
      do_check_eq(bsos[0], "foo");
      do_check_eq(bsos[1], "bar");

      server.stop(run_next_test);
    }
  };
  request.dispatch();
});

add_test(function test_get_collection_conditional_not_modified() {
  _("Ensure conditional requests with no new data to getCollection work.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  let now = Date.now();

  user.createCollection("testcoll", {
    foo: new ServerBSO("foo", "payload0", now)
  });

  let request = client.getCollection("testcoll");
  request.locallyModifiedVersion = now + 1;

  request.dispatch(function onComplete(error, req) {
    do_check_null(error);

    do_check_true(req.notModified);

    server.stop(run_next_test);
  });
});

add_test(function test_get_collection_conditional_modified() {
  _("Ensure conditional requests with new data to getCollection work.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  let now = Date.now();

  user.createCollection("testcoll", {
    foo: new ServerBSO("foo", "payload0", now)
  });

  let request = client.getCollection("testcoll");
  request.locallyModifiedVersion = now - 1;

  let bsoCount = 0;
  request.handler = {
    onBSORecord: function onBSORecord() {
      bsoCount++;
    },

    onComplete: function onComplete(error, req) {
      do_check_null(error);

      do_check_false(req.notModified);
      do_check_eq(bsoCount, 1);

      server.stop(run_next_test);
    }
  };
  request.dispatch();
});

// This is effectively a sanity test for query string generation.
add_test(function test_get_collection_newer() {
  _("Ensure query string for newer and full work together.");

  let [server, client, username] = getServerAndClient();

  let date0 = Date.now();
  let date1 = date0 + 500;

  let user = server.user(username);
  user.createCollection("testcoll", {
    foo: new ServerBSO("foo", "payload0", date0),
    bar: new ServerBSO("bar", "payload1", date1)
  });

  let request = client.getCollection("testcoll");
  request.full = true;
  request.newer = date0;

  let bsos = [];
  request.handler = {
    onBSORecord: function onBSORecord(request, bso) {
      bsos.push(bso);
    },

    onComplete: function onComplete(error, req) {
      do_check_null(error);

      do_check_eq(bsos.length, 1);
      let bso = bsos[0];

      do_check_eq(bso.id, "bar");
      do_check_eq(bso.payload, "payload1");

      server.stop(run_next_test);
    }
  };
  request.dispatch();
});

add_test(function test_get_bso() {
  _("Ensure that simple BSO fetches work.");

  let [server, client, username] = getServerAndClient();

  server.createCollection(username, "testcoll", {
    abc123: new ServerBSO("abc123", "payload", Date.now())
  });

  let request = client.getBSO("testcoll", "abc123");
  request.dispatch(function(error, req) {
    do_check_null(error);
    do_check_true(req.resultObj instanceof BasicStorageObject);

    let bso = req.resultObj;
    do_check_eq(bso.id, "abc123");
    do_check_eq(bso.payload, "payload");

    server.stop(run_next_test);
  });
});

add_test(function test_bso_conditional() {
  _("Ensure conditional requests for an individual BSO work.");

  let [server, client, username] = getServerAndClient();

  let user = server.user(username);
  let now = Date.now();
  user.createCollection("testcoll", {
    foo: new ServerBSO("foo", "payload", now)
  });

  let request = client.getBSO("testcoll", "foo");
  request.locallyModifiedVersion = now;

  request.dispatch(function onComplete(error, req) {
    do_check_null(error);
    do_check_true(req.notModified);

    server.stop(run_next_test);
  });
});

add_test(function test_set_bso() {
  _("Ensure simple BSO PUT works.");

  let [server, client] = getServerAndClient();

  let id = "mnas08h3f3r2351";

  let bso = new BasicStorageObject(id, "testcoll");
  bso.payload = "my test payload";

  let request = client.setBSO(bso);
  request.dispatch(function(error, req) {
    do_check_eq(error, null);
    do_check_eq(req.resultObj, null);

    server.stop(run_next_test);
  });
});


add_test(function test_set_bso_conditional() {
  _("Ensure conditional setting a BSO is properly rejected.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  let now = Date.now();
  user.createCollection("testcoll", {
    foo: new ServerBSO("foo", "payload0", now + 1000)
  });

  // Should get an mtime newer than server's.
  let bso = new BasicStorageObject("foo", "testcoll");
  bso.payload = "payload1";

  let request = client.setBSO(bso);
  request.locallyModifiedVersion = now;
  request.dispatch(function onComplete(error, req) {
    do_check_true(error instanceof StorageServiceRequestError);
    do_check_true(error.serverModified);

    server.stop(run_next_test);
  });
});

add_test(function test_set_bso_argument_errors() {
  _("Ensure BSO set detects invalid arguments.");

  let bso = new BasicStorageObject();
  let client = getClient();

  let threw = false;
  try {
    client.setBSO(bso);
  } catch (ex) {
    threw = true;
    do_check_eq(ex.name, "Error");
    do_check_neq(ex.message.indexOf("does not have collection defined"), -1);
  } finally {
    do_check_true(threw);
    threw = false;
  }

  bso = new BasicStorageObject("id");
  try {
    client.setBSO(bso);
  } catch (ex) {
    threw = true;
    do_check_eq(ex.name, "Error");
    do_check_neq(ex.message.indexOf("does not have collection defined"), -1);
  } finally {
    do_check_true(threw);
    threw = false;
  }

  bso = new BasicStorageObject(null, "coll");
  try {
    client.setBSO(bso);
  } catch (ex) {
    threw = true;
    do_check_eq(ex.name, "Error");
    do_check_neq(ex.message.indexOf("does not have ID defined"), -1);
  } finally {
    do_check_true(threw);
    threw = false;
  }

  run_next_test();
});

add_test(function test_set_bsos_simple() {
  _("Ensure setBSOs with basic options works.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  let bso0 = new BasicStorageObject("foo");
  bso0.payload = "payload0";

  let bso1 = new BasicStorageObject("bar");
  bso1.payload = "payload1";

  let request = client.setBSOs("testcollection");
  request.addBSO(bso0);
  request.addBSO(bso1);

  request.dispatch(function onComplete(error, req) {
    do_check_null(error);

    let successful = req.successfulIDs;
    do_check_eq(successful.length, 2);
    do_check_eq(successful.indexOf(bso0.id), 0);
    do_check_true(successful.indexOf(bso1.id), 1);

    server.stop(run_next_test);
  });
});

add_test(function test_set_bsos_invalid_bso() {
  _("Ensure that adding an invalid BSO throws.");

  let client = getClient();
  let request = client.setBSOs("testcoll");

  let threw = false;

  // Empty argument is invalid.
  try {
    request.addBSO(null);
  } catch (ex) {
    threw = true;
  } finally {
    do_check_true(threw);
    threw = false;
  }

  try {
    let bso = new BasicStorageObject();
    request.addBSO(bso);
  } catch (ex) {
    threw = true;
  } finally {
    do_check_true(threw);
    threw = false;
  }

  run_next_test();
});

add_test(function test_set_bsos_newline() {
  _("Ensure that newlines in BSO payloads are formatted properly.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  let request = client.setBSOs("testcoll");

  let bso0 = new BasicStorageObject("bso0");
  bso0.payload = "hello\nworld";
  request.addBSO(bso0);

  let bso1 = new BasicStorageObject("bso1");
  bso1.payload = "foobar";
  request.addBSO(bso1);

  request.dispatch(function onComplete(error, request) {
    do_check_null(error);
    do_check_eq(request.successfulIDs.length, 2);

    let coll = user.collection("testcoll");
    do_check_eq(coll.bso("bso0").payload, bso0.payload);
    do_check_eq(coll.bso("bso1").payload, bso1.payload);

    server.stop(run_next_test);
  });
});

add_test(function test_delete_bso_simple() {
  _("Ensure deletion of individual BSOs works.");

  let [server, client, username] = getServerAndClient();

  let user = server.user(username);
  let coll = user.createCollection("testcoll", {
    foo: new ServerBSO("foo", "payload0", Date.now()),
    bar: new ServerBSO("bar", "payload1", Date.now())
  });

  let request = client.deleteBSO("testcoll", "foo");
  request.dispatch(function onComplete(error, req) {
    do_check_null(error);
    do_check_eq(req.statusCode, 204);

    do_check_eq(coll.count(), 1);

    server.stop(run_next_test);
  });
});

add_test(function test_delete_bso_conditional_failed() {
  _("Ensure deletion of an individual BSO with older modification fails.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  let now = Date.now();
  user.createCollection("testcoll", {
    foo: new ServerBSO("foo", "payload0", now)
  });

  let request = client.deleteBSO("testcoll", "foo");
  request.locallyModifiedVersion = now - 10;

  request.dispatch(function onComplete(error, req) {
    do_check_true(error instanceof StorageServiceRequestError);
    do_check_true(error.serverModified);

    server.stop(run_next_test);
  });
});

add_test(function test_delete_bso_conditional_success() {
  _("Ensure deletion of an individual BSO with newer modification works.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  let now = Date.now();
  user.createCollection("testcoll", {
    foo: new ServerBSO("foo", "payload0", now)
  });

  let request = client.deleteBSO("testcoll", "foo");
  request.locallyModifiedVersion = now;

  request.dispatch(function onComplete(error, req) {
    do_check_null(error);
    do_check_eq(req.statusCode, 204);

    server.stop(run_next_test);
  });
});

add_test(function test_delete_bsos_simple() {
  _("Ensure deletion of multiple BSOs works.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  let coll = user.createCollection("testcoll", {
    foo: new ServerBSO("foo", "payload0", Date.now()),
    bar: new ServerBSO("bar", "payload1", Date.now()),
    baz: new ServerBSO("baz", "payload2", Date.now())
  });

  let request = client.deleteBSOs("testcoll", ["foo", "baz"]);
  request.dispatch(function onComplete(error, req) {
    do_check_null(error);
    do_check_eq(req.statusCode, 204);

    do_check_eq(coll.count(), 1);

    server.stop(run_next_test);
  });
});

add_test(function test_delete_bsos_conditional_failed() {
  _("Ensure deletion of BSOs with server modifications fails.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  let now = Date.now();
  let coll = user.createCollection("testcoll", {
    foo: new ServerBSO("foo", "payload0", now)
  });

  let request = client.deleteBSOs("testcoll", ["foo"]);
  request.locallyModifiedVersion = coll.timestamp - 1;

  request.dispatch(function onComplete(error, req) {
    do_check_true(error instanceof StorageServiceRequestError);
    do_check_true(error.serverModified);

    server.stop(run_next_test);
  });
});

add_test(function test_delete_bsos_conditional_success() {
  _("Ensure conditional deletion of BSOs without server modifications works.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  let now = Date.now();
  let coll = user.createCollection("testcoll", {
    foo: new ServerBSO("foo", "payload0", now),
    bar: new ServerBSO("bar", "payload1", now - 10)
  });

  let request = client.deleteBSOs("testcoll", ["bar"]);
  request.locallyModifiedVersion = coll.timestamp;

  request.dispatch(function onComplete(error, req) {
    do_check_null(error);

    do_check_eq(req.statusCode, 204);

    server.stop(run_next_test);
  });
});

add_test(function test_delete_collection() {
  _("Ensure deleteCollection() works.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  user.createCollection("testcoll", {
    foo: new ServerBSO("foo", "payload0", Date.now())
  });

  let request = client.deleteCollection("testcoll");
  request.dispatch(function onComplete(error, req) {
    do_check_null(error);

    do_check_eq(user.collection("testcoll", undefined));

    server.stop(run_next_test);
  });
});

add_test(function test_delete_collection_conditional_failed() {
  _("Ensure conditional deletes with server modifications fail.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  let now = Date.now();

  let coll = user.createCollection("testcoll", {
    foo: new ServerBSO("foo", "payload0", now)
  });

  let request = client.deleteCollection("testcoll");
  request.locallyModifiedVersion = coll.timestamp - 1;

  request.dispatch(function onComplete(error, req) {
    do_check_true(error instanceof StorageServiceRequestError);
    do_check_true(error.serverModified);

    server.stop(run_next_test);
  });
});

add_test(function test_delete_collection_conditional_success() {
  _("Ensure conditional delete of collection works when it's supposed to.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  let now = Date.now();

  let coll = user.createCollection("testcoll", {
    foo: new ServerBSO("foo", "payload0", now)
  });

  let request = client.deleteCollection("testcoll");
  request.locallyModifiedVersion = coll.timestamp;

  request.dispatch(function onComplete(error, req) {
    do_check_null(error);

    do_check_eq(user.collection("testcoll"), undefined);

    server.stop(run_next_test);
  });
});

add_test(function test_delete_collections() {
  _("Ensure deleteCollections() works.");

  let [server, client, username] = getServerAndClient();
  let user = server.user(username);

  user.createCollection("testColl", {
    foo: new ServerBSO("foo", "payload0", Date.now())
  });

  let request = client.deleteCollections();
  request.dispatch(function onComplete(error, req) {
    do_check_null(error);

    do_check_eq(user.collection("testcoll"), undefined);

    server.stop(run_next_test);
  });
});

add_test(function test_network_error_captured() {
  _("Ensure network errors are captured.");

  // Network errors should result in .networkError being set on request.
  let client = new StorageServiceClient("http://rnewman-is-splendid.badtld/");

  let request = client.getCollectionInfo();
  request.dispatch(function(error, req) {
    do_check_neq(error, null);
    do_check_neq(error.network, null);

    run_next_test();
  });
});

add_test(function test_network_error_listener() {
  _("Ensure the onNetworkError listener is invoked on network errors.");

  let listenerCalled = false;

  let client = new StorageServiceClient("http://philikon-is-too.badtld/");
  client.addListener({
    onNetworkError: function(client, request) {
      listenerCalled = true;
    }
  });
  let request = client.getCollectionInfo();
  request.dispatch(function() {
    do_check_true(listenerCalled);
    run_next_test();
  });
});

add_test(function test_batching_set_too_large() {
  _("Ensure we throw when attempting to add a BSO that is too large to fit.");

  let [server, client, username] = getServerAndClient();

  let request = client.setBSOsBatching("testcoll");
  let payload = "";

  // The actual length of the payload is a little less. But, this ensures we
  // exceed it.
  for (let i = 0; i < client.REQUEST_SIZE_LIMIT; i++) {
    payload += i;
  }

  let bso = new BasicStorageObject("bso");
  bso.payload = payload;
  do_check_throws(function add() { request.addBSO(bso); });

  server.stop(run_next_test);
});

add_test(function test_batching_set_basic() {
  _("Ensure batching set works with single requests.");

  let [server, client, username] = getServerAndClient();

  let request = client.setBSOsBatching("testcoll");
  for (let i = 0; i < 10; i++) {
    let bso = new BasicStorageObject("bso" + i);
    bso.payload = "payload" + i;
    request.addBSO(bso);
  }

  request.finish(function onFinish(request) {
    do_check_eq(request.successfulIDs.length, 10);

    let collection = server.user(username).collection("testcoll");
    do_check_eq(collection.timestamp, request.serverModifiedVersion);

    server.stop(run_next_test);
  });
});

add_test(function test_batching_set_batch_count() {
  _("Ensure multiple outgoing request batching works when count is exceeded.");

  let [server, client, username] = getServerAndClient();
  let requestCount = 0;
  server.callback.onRequest = function onRequest() {
    requestCount++;
  }

  let request = client.setBSOsBatching("testcoll");
  for (let i = 1; i <= 300; i++) {
    let bso = new BasicStorageObject("bso" + i);
    bso.payload = "XXXXXXX";
    request.addBSO(bso);
  }

  request.finish(function onFinish(request) {
    do_check_eq(request.successfulIDs.length, 300);
    do_check_eq(requestCount, 3);

    let collection = server.user(username).collection("testcoll");
    do_check_eq(collection.timestamp, request.serverModifiedVersion);

    server.stop(run_next_test);
  });
});

add_test(function test_batching_set_batch_size() {
  _("Ensure outgoing requests batch when size is exceeded.");

  let [server, client, username] = getServerAndClient();
  let requestCount = 0;
  server.callback.onRequest = function onRequest() {
    requestCount++;
  };

  let limit = client.REQUEST_SIZE_LIMIT;

  let request = client.setBSOsBatching("testcoll");

  // JavaScript: Y U NO EASY REPETITION FUNCTIONALITY?
  let data = [];
  for (let i = (limit / 2) - 100; i; i -= 1) {
    data.push("X");
  }

  let payload = data.join("");

  for (let i = 0; i < 4; i++) {
    let bso = new BasicStorageObject("bso" + i);
    bso.payload = payload;
    request.addBSO(bso);
  }

  request.finish(function onFinish(request) {
    do_check_eq(request.successfulIDs.length, 4);
    do_check_eq(requestCount, 2);

    let collection = server.user(username).collection("testcoll");
    do_check_eq(collection.timestamp, request.serverModifiedVersion);

    server.stop(run_next_test);
  });
});

add_test(function test_batching_set_flush() {
  _("Ensure flushing batch sets works.");

  let [server, client, username] = getServerAndClient();

  let requestCount = 0;
  server.callback.onRequest = function onRequest() {
    requestCount++;
  }

  let request = client.setBSOsBatching("testcoll");
  for (let i = 1; i < 101; i++) {
    let bso = new BasicStorageObject("bso" + i);
    bso.payload = "foo";
    request.addBSO(bso);

    if (i % 10 == 0) {
      request.flush();
    }
  }

  request.finish(function onFinish(request) {
    do_check_eq(request.successfulIDs.length, 100);
    do_check_eq(requestCount, 10);

    let collection = server.user(username).collection("testcoll");
    do_check_eq(collection.timestamp, request.serverModifiedVersion);

    server.stop(run_next_test);
  });
});

add_test(function test_batching_set_conditional_success() {
  _("Ensure conditional requests for batched sets work properly.");

  let [server, client, username] = getServerAndClient();

  let collection = server.user(username).createCollection("testcoll");

  let lastServerVersion = Date.now();
  collection.insertBSO(new ServerBSO("foo", "bar", lastServerVersion));
  collection.timestamp = lastServerVersion;
  do_check_eq(collection.timestamp, lastServerVersion);

  let requestCount = 0;
  server.callback.onRequest = function onRequest() {
    requestCount++;
  }

  let request = client.setBSOsBatching("testcoll");
  request.locallyModifiedVersion = collection.timestamp;

  for (let i = 1; i < 251; i++) {
    let bso = new BasicStorageObject("bso" + i);
    bso.payload = "foo" + i;
    request.addBSO(bso);
  }

  request.finish(function onFinish(request) {
    do_check_eq(requestCount, 3);

    do_check_eq(collection.timestamp, request.serverModifiedVersion);
    do_check_eq(collection.timestamp, request.locallyModifiedVersion);

    server.stop(run_next_test);
  });
});

add_test(function test_batching_set_initial_failure() {
  _("Ensure that an initial request failure setting BSOs is handled properly.");

  let [server, client, username] = getServerAndClient();

  let collection = server.user(username).createCollection("testcoll");
  collection.timestamp = Date.now();

  let requestCount = 0;
  server.callback.onRequest = function onRequest() {
    requestCount++;
  }

  let request = client.setBSOsBatching("testcoll");
  request.locallyModifiedVersion = collection.timestamp - 1;

  for (let i = 1; i < 250; i++) {
    let bso = new BasicStorageObject("bso" + i);
    bso.payload = "foo" + i;
    request.addBSO(bso);
  }

  request.finish(function onFinish(request) {
    do_check_eq(requestCount, 1);

    do_check_eq(request.successfulIDs.length, 0);
    do_check_eq(Object.keys(request.failures).length, 0);

    server.stop(run_next_test);
  });
});

add_test(function test_batching_set_subsequent_failure() {
  _("Ensure a non-initial failure during batching set is handled properly.");

  let [server, client, username] = getServerAndClient();
  let collection = server.user(username).createCollection("testcoll");
  collection.timestamp = Date.now();

  let requestCount = 0;
  server.callback.onRequest = function onRequest() {
    requestCount++;

    if (requestCount == 1) {
      return;
    }

    collection.timestamp++;
  }

  let request = client.setBSOsBatching("testcoll");
  request.locallyModifiedVersion = collection.timestamp;

  for (let i = 0; i < 250; i++) {
    let bso = new BasicStorageObject("bso" + i);
    bso.payload = "foo" + i;
    request.addBSO(bso);
  }

  request.finish(function onFinish(request) {
    do_check_eq(requestCount, 2);
    do_check_eq(request.successfulIDs.length, 100);
    do_check_eq(Object.keys(request.failures).length, 0);

    server.stop(run_next_test);
  });
});

function getBatchedDeleteData(collection="testcoll") {
  let [server, client, username] = getServerAndClient();

  let serverBSOs = {};
  for (let i = 1000; i; i -= 1) {
    serverBSOs["bso" + i] = new ServerBSO("bso" + i, "payload" + i);
  }

  let user = server.user(username);
  user.createCollection(collection, serverBSOs);

  return [server, client, username, collection];
}

add_test(function test_batched_delete_single() {
  _("Ensure batched delete with single request works.");

  let [server, client, username, collection] = getBatchedDeleteData();

  let requestCount = 0;
  server.callback.onRequest = function onRequest() {
    requestCount += 1;
  }

  let request = client.deleteBSOsBatching(collection);
  for (let i = 1; i < 51; i += 1) {
    request.addID("bso" + i);
  }

  request.finish(function onFinish(request) {
    do_check_eq(requestCount, 1);
    do_check_eq(request.errors.length, 0);

    let coll = server.user(username).collection(collection);
    do_check_eq(coll.count(), 950);

    do_check_eq(request.serverModifiedVersion, coll.timestamp);

    server.stop(run_next_test);
  });
});

add_test(function test_batched_delete_multiple() {
  _("Ensure batched delete splits requests properly.");

  let [server, client, username, collection] = getBatchedDeleteData();

  let requestCount = 0;
  server.callback.onRequest = function onRequest() {
    requestCount += 1;
  }

  let request = client.deleteBSOsBatching(collection);
  for (let i = 1; i < 251; i += 1) {
    request.addID("bso" + i);
  }

  request.finish(function onFinish(request) {
    do_check_eq(requestCount, 3);
    do_check_eq(request.errors.length, 0);

    let coll = server.user(username).collection(collection);
    do_check_eq(coll.count(), 750);

    do_check_eq(request.serverModifiedVersion, coll.timestamp);

    server.stop(run_next_test);
  });
});

add_test(function test_batched_delete_conditional_success() {
  _("Ensure conditional batched delete all work.");

  let [server, client, username, collection] = getBatchedDeleteData();

  let requestCount = 0;
  server.callback.onRequest = function onRequest() {
    requestCount++;
  }

  let serverCollection = server.user(username).collection(collection);
  let initialTimestamp = serverCollection.timestamp;

  let request = client.deleteBSOsBatching(collection);
  request.locallyModifiedVersion = initialTimestamp;

  for (let i = 1; i < 251; i += 1) {
    request.addID("bso" + 1);
  }

  request.finish(function onFinish(request) {
    do_check_eq(requestCount, 3);
    do_check_eq(request.errors.length, 0);

    do_check_true(request.locallyModifiedVersion > initialTimestamp);

    server.stop(run_next_test);
  });
});

add_test(function test_batched_delete_conditional_initial_failure() {
  _("Ensure conditional batched delete failure on initial request works.");

  // The client needs to issue multiple requests but the first one was
  // rejected. The client should only issue that initial request.
  let [server, client, username, collection] = getBatchedDeleteData();

  let requestCount = 0;
  server.callback.onRequest = function onRequest() {
    requestCount++;
  }

  let serverCollection = server.user(username).collection(collection);
  let request = client.deleteBSOsBatching(collection);
  request.locallyModifiedVersion = serverCollection.timestamp - 1;

  for (let i = 1; i < 251; i += 1) {
    request.addID("bso" + i);
  }

  request.finish(function onFinish(request) {
    do_check_eq(requestCount, 1);
    do_check_eq(request.errors.length, 1);

    server.stop(run_next_test);
  });
});

add_test(function test_batched_delete_conditional_subsequent_failure() {
  _("Ensure conditional batched delete failure on non-initial request.");

  let [server, client, username, collection] = getBatchedDeleteData();

  let serverCollection = server.user(username).collection(collection);

  let requestCount = 0;
  server.callback.onRequest = function onRequest() {
    requestCount++;

    if (requestCount <= 1) {
      return;
    }

    // Advance collection's timestamp on subsequent requests so request is
    // rejected.
    serverCollection.timestamp++;
  }

  let request = client.deleteBSOsBatching(collection);
  request.locallyModifiedVersion = serverCollection.timestamp;

  for (let i = 1; i < 251; i += 1) {
    request.addID("bso" + i);
  }

  request.finish(function onFinish(request) {
    do_check_eq(requestCount, 2);
    do_check_eq(request.errors.length, 1);

    server.stop(run_next_test);
  });
});
