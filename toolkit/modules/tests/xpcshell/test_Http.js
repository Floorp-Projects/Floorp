/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/Http.jsm");
Components.utils.import("resource://testing-common/httpd.js");

const BinaryInputStream = Components.Constructor("@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream", "setInputStream");

var server;

const kDefaultServerPort = 9000;
const kSuccessPath = "/success";
const kBaseUrl = "http://localhost:" + kDefaultServerPort;
const kSuccessUrl = kBaseUrl + kSuccessPath;

const kPostPath = "/post";
const kPostUrl = kBaseUrl + kPostPath;
const kPostDataSent = [["foo", "bar"], ["complex", "!*()@"]];
const kPostDataReceived = "foo=bar&complex=%21%2A%28%29%40";

const kPutPath = "/put";
const kPutUrl = kBaseUrl + kPutPath;
const kPutDataSent = [["P", "NP"]];
const kPutDataReceived = "P=NP";

const kGetPath = "/get";
const kGetUrl = kBaseUrl + kGetPath;

function successResult(aRequest, aResponse) {
  aResponse.setStatusLine(null, 200, "OK");
  aResponse.setHeader("Content-Type", "application/json");
  aResponse.write("Success!");
}

function getDataChecker(aExpectedMethod, aExpectedData) {
  return function(aRequest, aResponse) {
    let body = new BinaryInputStream(aRequest.bodyInputStream);
    let bytes = [];
    let avail;
    while ((avail = body.available()) > 0)
      Array.prototype.push.apply(bytes, body.readByteArray(avail));

    do_check_eq(aRequest.method, aExpectedMethod);

    var data = String.fromCharCode.apply(null, bytes);

    do_check_eq(data, aExpectedData);

    aResponse.setStatusLine(null, 200, "OK");
    aResponse.setHeader("Content-Type", "application/json");
    aResponse.write("Success!");
  }
}

add_test(function test_successCallback() {
  do_test_pending();
  let options = {
    onLoad: function(aResponse) {
      do_check_eq(aResponse, "Success!");
      do_test_finished();
      run_next_test();
    },
    onError: function(e) {
      do_check_true(false);
      do_test_finished();
      run_next_test();
    }
  }
  httpRequest(kSuccessUrl, options);
});

add_test(function test_errorCallback() {
  do_test_pending();
  let options = {
    onSuccess: function(aResponse) {
      do_check_true(false);
      do_test_finished();
      run_next_test();
    },
    onError: function(e, aResponse) {
      do_check_eq(e, "404 - Not Found");
      do_test_finished();
      run_next_test();
    }
  }
  httpRequest(kBaseUrl + "/failure", options);
});

add_test(function test_PostData() {
  do_test_pending();
  let options = {
    onLoad: function(aResponse) {
      do_check_eq(aResponse, "Success!");
      do_test_finished();
      run_next_test();
    },
    onError: function(e) {
      do_check_true(false);
      do_test_finished();
      run_next_test();
    },
    postData: kPostDataSent
  }
  httpRequest(kPostUrl, options);
});

add_test(function test_PutData() {
  do_test_pending();
  let options = {
    method: "PUT",
    onLoad: function(aResponse) {
      do_check_eq(aResponse, "Success!");
      do_test_finished();
      run_next_test();
    },
    onError: function(e) {
      do_check_true(false);
      do_test_finished();
      run_next_test();
    },
    postData: kPutDataSent
  }
  httpRequest(kPutUrl, options);
});

add_test(function test_GetData() {
  do_test_pending();
  let options = {
    onLoad: function(aResponse) {
      do_check_eq(aResponse, "Success!");
      do_test_finished();
      run_next_test();
    },
    onError: function(e) {
      do_check_true(false);
      do_test_finished();
      run_next_test();
    },
    postData: null
  }
  httpRequest(kGetUrl, options);
});

function run_test() {
  // Set up a mock HTTP server to serve a success page.
  server = new HttpServer();
  server.registerPathHandler(kSuccessPath, successResult);
  server.registerPathHandler(kPostPath,
                             getDataChecker("POST", kPostDataReceived));
  server.registerPathHandler(kPutPath,
                             getDataChecker("PUT", kPutDataReceived));
  server.registerPathHandler(kGetPath, getDataChecker("GET", ""));
  server.start(kDefaultServerPort);

  run_next_test();

  // Teardown.
  do_register_cleanup(function() {
    server.stop(function() { });
  });
}

