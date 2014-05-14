/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let { DebuggerServer } =
  Cu.import("resource://gre/modules/devtools/dbg-server.jsm", {});
let { DebuggerClient } =
  Cu.import("resource://gre/modules/devtools/dbg-client.jsm", {});
let { FileUtils } = Cu.import("resource://gre/modules/FileUtils.jsm", {});
let { NetUtil } = Cu.import("resource://gre/modules/NetUtil.jsm", {});
let Pipe = CC("@mozilla.org/pipe;1", "nsIPipe", "init");
let { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});

function run_test() {
  initTestDebuggerServer();
  add_test_bulk_actor();

  add_task(function() {
    yield test_bulk_request_cs(socket_transport, "jsonReply", "json");
    yield test_bulk_request_cs(local_transport, "jsonReply", "json");
    yield test_bulk_request_cs(socket_transport, "bulkEcho", "bulk");
    yield test_bulk_request_cs(local_transport, "bulkEcho", "bulk");
    yield test_json_request_cs(socket_transport, "bulkReply", "bulk");
    yield test_json_request_cs(local_transport, "bulkReply", "bulk");
    DebuggerServer.destroy();
  });

  run_next_test();
}

/*** Sample Bulk Actor ***/

function TestBulkActor(conn) {
  this.conn = conn;
}

TestBulkActor.prototype = {

  actorPrefix: "testBulk",

  bulkEcho: function({actor, type, length, copyTo}) {
    do_check_eq(length, really_long().length);
    this.conn.startBulkSend({
      actor: actor,
      type: type,
      length: length
    }).then(({copyFrom}) => {
      // We'll just echo back the same thing
      let pipe = new Pipe(true, true, 0, 0, null);
      copyTo(pipe.outputStream).then(() => {
        pipe.outputStream.close();
      });
      copyFrom(pipe.inputStream).then(() => {
        pipe.inputStream.close();
      });
    });
  },

  bulkReply: function({to, type}) {
    this.conn.startBulkSend({
      actor: to,
      type: type,
      length: really_long().length
    }).then(({copyFrom}) => {
      NetUtil.asyncFetch(getTestTempFile("bulk-input"), input => {
        copyFrom(input).then(() => {
          input.close();
        });
      });
    });
  },

  jsonReply: function({length, copyTo}) {
    do_check_eq(length, really_long().length);

    let outputFile = getTestTempFile("bulk-output", true);
    outputFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));

    let output = FileUtils.openSafeFileOutputStream(outputFile);

    return copyTo(output).then(() => {
      FileUtils.closeSafeFileOutputStream(output);
      return verify_files();
    }).then(() => {
      return { allDone: true };
    }, do_throw);
  }

};

TestBulkActor.prototype.requestTypes = {
  "bulkEcho": TestBulkActor.prototype.bulkEcho,
  "bulkReply": TestBulkActor.prototype.bulkReply,
  "jsonReply": TestBulkActor.prototype.jsonReply
};

function add_test_bulk_actor() {
  DebuggerServer.addGlobalActor(TestBulkActor);
}

/*** Reply Handlers ***/

let replyHandlers = {

  json: function(request) {
    // Receive JSON reply from server
    let replyDeferred = promise.defer();
    request.on("json-reply", (reply) => {
      do_check_true(reply.allDone);
      replyDeferred.resolve();
    });
    return replyDeferred.promise;
  },

  bulk: function(request) {
    // Receive bulk data reply from server
    let replyDeferred = promise.defer();
    request.on("bulk-reply", ({length, copyTo}) => {
      do_check_eq(length, really_long().length);

      let outputFile = getTestTempFile("bulk-output", true);
      outputFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));

      let output = FileUtils.openSafeFileOutputStream(outputFile);

      copyTo(output).then(() => {
        FileUtils.closeSafeFileOutputStream(output);
        replyDeferred.resolve(verify_files());
      });
    });
    return replyDeferred.promise;
  }

};

/*** Tests ***/

function test_bulk_request_cs(transportFactory, actorType, replyType) {
  // Ensure test files are not present from a failed run
  cleanup_files();
  writeTestTempFile("bulk-input", really_long());

  let clientDeferred = promise.defer();
  let serverDeferred = promise.defer();
  let bulkCopyDeferred = promise.defer();

  let transport = transportFactory();

  let client = new DebuggerClient(transport);
  client.connect((app, traits) => {
    do_check_eq(traits.bulk, true);
    client.listTabs(clientDeferred.resolve);
  });

  clientDeferred.promise.then(response => {
    let request = client.startBulkRequest({
      actor: response.testBulk,
      type: actorType,
      length: really_long().length
    });

    // Send bulk data to server
    request.on("bulk-send-ready", ({copyFrom}) => {
      NetUtil.asyncFetch(getTestTempFile("bulk-input"), input => {
        copyFrom(input).then(() => {
          input.close();
          bulkCopyDeferred.resolve();
        });
      });
    });

    // Set up reply handling for this type
    replyHandlers[replyType](request).then(() => {
      client.close();
      transport.close();
    });
  }).then(null, do_throw);

  DebuggerServer.on("connectionchange", (event, type) => {
    if (type === "closed") {
      serverDeferred.resolve();
    }
  });

  return promise.all([
    clientDeferred.promise,
    bulkCopyDeferred.promise,
    serverDeferred.promise
  ]);
}

function test_json_request_cs(transportFactory, actorType, replyType) {
  // Ensure test files are not present from a failed run
  cleanup_files();
  writeTestTempFile("bulk-input", really_long());

  let clientDeferred = promise.defer();
  let serverDeferred = promise.defer();

  let transport = transportFactory();

  let client = new DebuggerClient(transport);
  client.connect((app, traits) => {
    do_check_eq(traits.bulk, true);
    client.listTabs(clientDeferred.resolve);
  });

  clientDeferred.promise.then(response => {
    let request = client.request({
      to: response.testBulk,
      type: actorType
    });

    // Set up reply handling for this type
    replyHandlers[replyType](request).then(() => {
      client.close();
      transport.close();
    });
  }).then(null, do_throw);

  DebuggerServer.on("connectionchange", (event, type) => {
    if (type === "closed") {
      serverDeferred.resolve();
    }
  });

  return promise.all([
    clientDeferred.promise,
    serverDeferred.promise
  ]);
}

/*** Test Utils ***/

function verify_files() {
  let reallyLong = really_long();

  let inputFile = getTestTempFile("bulk-input");
  let outputFile = getTestTempFile("bulk-output");

  do_check_eq(inputFile.fileSize, reallyLong.length);
  do_check_eq(outputFile.fileSize, reallyLong.length);

  // Ensure output file contents actually match
  let compareDeferred = promise.defer();
  NetUtil.asyncFetch(getTestTempFile("bulk-output"), input => {
    let outputData = NetUtil.readInputStreamToString(input, reallyLong.length);
    // Avoid do_check_eq here so we don't log the contents
    do_check_true(outputData === reallyLong);
    input.close();
    compareDeferred.resolve();
  });

  return compareDeferred.promise.then(cleanup_files);
}

function cleanup_files() {
  let inputFile = getTestTempFile("bulk-input", true);
  if (inputFile.exists()) {
    inputFile.remove(false);
  }

  let outputFile = getTestTempFile("bulk-output", true);
  if (outputFile.exists()) {
    outputFile.remove(false);
  }
}
