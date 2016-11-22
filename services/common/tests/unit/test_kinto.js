/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/kinto-offline-client.js");
Cu.import("resource://services-common/kinto-storage-adapter.js");
Cu.import("resource://testing-common/httpd.js");

const BinaryInputStream = Components.Constructor("@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream", "setInputStream");

var server;

// set up what we need to make storage adapters
const kintoFilename = "kinto.sqlite";

let kintoClient;

function do_get_kinto_collection() {
  if (!kintoClient) {
    let config = {
      remote:`http://localhost:${server.identity.primaryPort}/v1/`,
      headers: {Authorization: "Basic " + btoa("user:pass")},
      adapter: FirefoxAdapter
    };
    kintoClient = new Kinto(config);
  }
  return kintoClient.collection("test_collection");
}

function* clear_collection() {
  const collection = do_get_kinto_collection();
  try {
    yield collection.db.open();
    yield collection.clear();
  } finally {
    yield collection.db.close();
  }
}

// test some operations on a local collection
add_task(function* test_kinto_add_get() {
  const collection = do_get_kinto_collection();
  try {
    yield collection.db.open();

    let newRecord = { foo: "bar" };
    // check a record is created
    let createResult = yield collection.create(newRecord);
    do_check_eq(createResult.data.foo, newRecord.foo);
    // check getting the record gets the same info
    let getResult = yield collection.get(createResult.data.id);
    deepEqual(createResult.data, getResult.data);
    // check what happens if we create the same item again (it should throw
    // since you can't create with id)
    try {
      yield collection.create(createResult.data);
      do_throw("Creation of a record with an id should fail");
    } catch (err) { }
    // try a few creates without waiting for the first few to resolve
    let promises = [];
    promises.push(collection.create(newRecord));
    promises.push(collection.create(newRecord));
    promises.push(collection.create(newRecord));
    yield collection.create(newRecord);
    yield Promise.all(promises);
  } finally {
    yield collection.db.close();
  }
});

add_task(clear_collection);

// test some operations on multiple connections
add_task(function* test_kinto_add_get() {
  const collection1 = do_get_kinto_collection();
  const collection2 = kintoClient.collection("test_collection_2");

  try {
    yield collection1.db.open();
    yield collection2.db.open();

    let newRecord = { foo: "bar" };

    // perform several write operations alternately without waiting for promises
    // to resolve
    let promises = [];
    for (let i = 0; i < 10; i++) {
      promises.push(collection1.create(newRecord));
      promises.push(collection2.create(newRecord));
    }

    // ensure subsequent operations still work
    yield Promise.all([collection1.create(newRecord),
                       collection2.create(newRecord)]);
    yield Promise.all(promises);
  } finally {
    yield collection1.db.close();
    yield collection2.db.close();
  }
});

add_task(clear_collection);

add_task(function* test_kinto_update() {
  const collection = do_get_kinto_collection();
  try {
    yield collection.db.open();
    const newRecord = { foo: "bar" };
    // check a record is created
    let createResult = yield collection.create(newRecord);
    do_check_eq(createResult.data.foo, newRecord.foo);
    do_check_eq(createResult.data._status, "created");
    // check we can update this OK
    let copiedRecord = Object.assign(createResult.data, {});
    deepEqual(createResult.data, copiedRecord);
    copiedRecord.foo = "wibble";
    let updateResult = yield collection.update(copiedRecord);
    // check the field was updated
    do_check_eq(updateResult.data.foo, copiedRecord.foo);
    // check the status is still "created", since we haven't synced
    // the record
    do_check_eq(updateResult.data._status, "created");
  } finally {
    yield collection.db.close();
  }
});

add_task(clear_collection);

add_task(function* test_kinto_clear() {
  const collection = do_get_kinto_collection();
  try {
    yield collection.db.open();

    // create an expected number of records
    const expected = 10;
    const newRecord = { foo: "bar" };
    for (let i = 0; i < expected; i++) {
      yield collection.create(newRecord);
    }
    // check the collection contains the correct number
    let list = yield collection.list();
    do_check_eq(list.data.length, expected);
    // clear the collection and check again - should be 0
    yield collection.clear();
    list = yield collection.list();
    do_check_eq(list.data.length, 0);
  } finally {
    yield collection.db.close();
  }
});

add_task(clear_collection);

add_task(function* test_kinto_delete(){
  const collection = do_get_kinto_collection();
  try {
    yield collection.db.open();
    const newRecord = { foo: "bar" };
    // check a record is created
    let createResult = yield collection.create(newRecord);
    do_check_eq(createResult.data.foo, newRecord.foo);
    // check getting the record gets the same info
    let getResult = yield collection.get(createResult.data.id);
    deepEqual(createResult.data, getResult.data);
    // delete that record
    let deleteResult = yield collection.delete(createResult.data.id);
    // check the ID is set on the result
    do_check_eq(getResult.data.id, deleteResult.data.id);
    // and check that get no longer returns the record
    try {
      getResult = yield collection.get(createResult.data.id);
      do_throw("there should not be a result");
    } catch (e) { }
  } finally {
    yield collection.db.close();
  }
});

add_task(function* test_kinto_list(){
  const collection = do_get_kinto_collection();
  try {
    yield collection.db.open();
    const expected = 10;
    const created = [];
    for (let i = 0; i < expected; i++) {
      let newRecord = { foo: "test " + i };
      let createResult = yield collection.create(newRecord);
      created.push(createResult.data);
    }
    // check the collection contains the correct number
    let list = yield collection.list();
    do_check_eq(list.data.length, expected);

    // check that all created records exist in the retrieved list
    for (let createdRecord of created) {
      let found = false;
      for (let retrievedRecord of list.data) {
        if (createdRecord.id == retrievedRecord.id) {
          deepEqual(createdRecord, retrievedRecord);
          found = true;
        }
      }
      do_check_true(found);
    }
  } finally {
    yield collection.db.close();
  }
});

add_task(clear_collection);

add_task(function* test_loadDump_ignores_already_imported_records(){
  const collection = do_get_kinto_collection();
  try {
    yield collection.db.open();
    const record = {id: "41b71c13-17e9-4ee3-9268-6a41abf9730f", title: "foo", last_modified: 1457896541};
    yield collection.loadDump([record]);
    let impactedRecords = yield collection.loadDump([record]);
    do_check_eq(impactedRecords.length, 0);
  } finally {
    yield collection.db.close();
  }
});

add_task(clear_collection);

add_task(function* test_loadDump_should_overwrite_old_records(){
  const collection = do_get_kinto_collection();
  try {
    yield collection.db.open();
    const record = {id: "41b71c13-17e9-4ee3-9268-6a41abf9730f", title: "foo", last_modified: 1457896541};
    yield collection.loadDump([record]);
    const updated = Object.assign({}, record, {last_modified: 1457896543});
    let impactedRecords = yield collection.loadDump([updated]);
    do_check_eq(impactedRecords.length, 1);
  } finally {
    yield collection.db.close();
  }
});

add_task(clear_collection);

add_task(function* test_loadDump_should_not_overwrite_unsynced_records(){
  const collection = do_get_kinto_collection();
  try {
    yield collection.db.open();
    const recordId = "41b71c13-17e9-4ee3-9268-6a41abf9730f";
    yield collection.create({id: recordId, title: "foo"}, {useRecordId: true});
    const record = {id: recordId, title: "bar", last_modified: 1457896541};
    let impactedRecords = yield collection.loadDump([record]);
    do_check_eq(impactedRecords.length, 0);
  } finally {
    yield collection.db.close();
  }
});

add_task(clear_collection);

add_task(function* test_loadDump_should_not_overwrite_records_without_last_modified(){
  const collection = do_get_kinto_collection();
  try {
    yield collection.db.open();
    const recordId = "41b71c13-17e9-4ee3-9268-6a41abf9730f";
    yield collection.create({id: recordId, title: "foo"}, {synced: true});
    const record = {id: recordId, title: "bar", last_modified: 1457896541};
    let impactedRecords = yield collection.loadDump([record]);
    do_check_eq(impactedRecords.length, 0);
  } finally {
    yield collection.db.close();
  }
});

add_task(clear_collection);

// Now do some sanity checks against a server - we're not looking to test
// core kinto.js functionality here (there is excellent test coverage in
// kinto.js), more making sure things are basically working as expected.
add_task(function* test_kinto_sync(){
  const configPath = "/v1/";
  const recordsPath = "/v1/buckets/default/collections/test_collection/records";
  // register a handler
  function handleResponse (request, response) {
    try {
      const sampled = getSampleResponse(request, server.identity.primaryPort);
      if (!sampled) {
        do_throw(`unexpected ${request.method} request for ${request.path}?${request.queryString}`);
      }

      response.setStatusLine(null, sampled.status.status,
                             sampled.status.statusText);
      // send the headers
      for (let headerLine of sampled.sampleHeaders) {
        let headerElements = headerLine.split(':');
        response.setHeader(headerElements[0], headerElements[1].trimLeft());
      }
      response.setHeader("Date", (new Date()).toUTCString());

      response.write(sampled.responseBody);
    } catch (e) {
      dump(`${e}\n`);
    }
  }
  server.registerPathHandler(configPath, handleResponse);
  server.registerPathHandler(recordsPath, handleResponse);

  // create an empty collection, sync to populate
  const collection = do_get_kinto_collection();
  try {
    let result;

    yield collection.db.open();
    result = yield collection.sync();
    do_check_true(result.ok);

    // our test data has a single record; it should be in the local collection
    let list = yield collection.list();
    do_check_eq(list.data.length, 1);

    // now sync again; we should now have 2 records
    result = yield collection.sync();
    do_check_true(result.ok);
    list = yield collection.list();
    do_check_eq(list.data.length, 2);

    // sync again; the second records should have been modified
    const before = list.data[0].title;
    result = yield collection.sync();
    do_check_true(result.ok);
    list = yield collection.list();
    const after = list.data[0].title;
    do_check_neq(before, after);
  } finally {
    yield collection.db.close();
  }
});

function run_test() {
  // Set up an HTTP Server
  server = new HttpServer();
  server.start(-1);

  run_next_test();

  do_register_cleanup(function() {
    server.stop(function() { });
  });
}

// get a response for a given request from sample data
function getSampleResponse(req, port) {
  const responses = {
    "OPTIONS": {
      "sampleHeaders": [
        "Access-Control-Allow-Headers: Content-Length,Expires,Backoff,Retry-After,Last-Modified,Total-Records,ETag,Pragma,Cache-Control,authorization,content-type,if-none-match,Alert,Next-Page",
        "Access-Control-Allow-Methods: GET,HEAD,OPTIONS,POST,DELETE,OPTIONS",
        "Access-Control-Allow-Origin: *",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress"
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": "null"
    },
    "GET:/v1/?": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress"
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"settings":{"batch_max_requests":25}, "url":`http://localhost:${port}/v1/`, "documentation":"https://kinto.readthedocs.org/", "version":"1.5.1", "commit":"cbc6f58", "hello":"kinto"})
    },
    "GET:/v1/buckets/default/collections/test_collection/records?_sort=-last_modified": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"1445606341071\""
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data":[{"last_modified":1445606341071, "done":false, "id":"68db8313-686e-4fff-835e-07d78ad6f2af", "title":"New test"}]})
    },
    "GET:/v1/buckets/default/collections/test_collection/records?_sort=-last_modified&_since=1445606341071": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"1445607941223\""
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data":[{"last_modified":1445607941223, "done":false, "id":"901967b0-f729-4b30-8d8d-499cba7f4b1d", "title":"Another new test"}]})
    },
    "GET:/v1/buckets/default/collections/test_collection/records?_sort=-last_modified&_since=1445607941223": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"1445607541265\""
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data":[{"last_modified":1445607541265, "done":false, "id":"901967b0-f729-4b30-8d8d-499cba7f4b1d", "title":"Modified title"}]})
    }
  };
  return responses[`${req.method}:${req.path}?${req.queryString}`] ||
         responses[req.method];

}
