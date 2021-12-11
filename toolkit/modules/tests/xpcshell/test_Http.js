/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { httpRequest } = ChromeUtils.import("resource://gre/modules/Http.jsm");
const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

const BinaryInputStream = Components.Constructor(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

var server;

const kDefaultServerPort = 9000;
const kSuccessPath = "/success";
const kBaseUrl = "http://localhost:" + kDefaultServerPort;
const kSuccessUrl = kBaseUrl + kSuccessPath;

const kPostPath = "/post";
const kPostUrl = kBaseUrl + kPostPath;
const kPostDataSent = [
  ["foo", "bar"],
  ["complex", "!*()@"],
];
const kPostDataReceived = "foo=bar&complex=%21%2A%28%29%40";
const kPostMimeTypeReceived =
  "application/x-www-form-urlencoded; charset=utf-8";

const kJsonPostPath = "/json_post";
const kJsonPostUrl = kBaseUrl + kJsonPostPath;
const kJsonPostData = JSON.stringify(kPostDataSent);
const kJsonPostMimeType = "application/json";

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

function getDataChecker(
  aExpectedMethod,
  aExpectedData,
  aExpectedMimeType = null
) {
  return function(aRequest, aResponse) {
    let body = new BinaryInputStream(aRequest.bodyInputStream);
    let bytes = [];
    let avail;
    while ((avail = body.available()) > 0) {
      Array.prototype.push.apply(bytes, body.readByteArray(avail));
    }

    Assert.equal(aRequest.method, aExpectedMethod);

    // Checking if the Content-Type is as expected.
    if (aExpectedMimeType) {
      let contentType = aRequest.getHeader("Content-Type");
      Assert.equal(contentType, aExpectedMimeType);
    }

    var data = String.fromCharCode.apply(null, bytes);

    Assert.equal(data, aExpectedData);

    aResponse.setStatusLine(null, 200, "OK");
    aResponse.setHeader("Content-Type", "application/json");
    aResponse.write("Success!");
  };
}

add_test(function test_successCallback() {
  do_test_pending();
  let options = {
    onLoad(aResponse) {
      Assert.equal(aResponse, "Success!");
      do_test_finished();
      run_next_test();
    },
    onError(e) {
      Assert.ok(false);
      do_test_finished();
      run_next_test();
    },
  };
  httpRequest(kSuccessUrl, options);
});

add_test(function test_errorCallback() {
  do_test_pending();
  let options = {
    onSuccess(aResponse) {
      Assert.ok(false);
      do_test_finished();
      run_next_test();
    },
    onError(e, aResponse) {
      Assert.equal(e.message, "404 - Not Found");
      do_test_finished();
      run_next_test();
    },
  };
  httpRequest(kBaseUrl + "/failure", options);
});

add_test(function test_PostData() {
  do_test_pending();
  let options = {
    onLoad(aResponse) {
      Assert.equal(aResponse, "Success!");
      do_test_finished();
      run_next_test();
    },
    onError(e) {
      Assert.ok(false);
      do_test_finished();
      run_next_test();
    },
    postData: kPostDataSent,
  };
  httpRequest(kPostUrl, options);
});

add_test(function test_PutData() {
  do_test_pending();
  let options = {
    method: "PUT",
    onLoad(aResponse) {
      Assert.equal(aResponse, "Success!");
      do_test_finished();
      run_next_test();
    },
    onError(e) {
      Assert.ok(false);
      do_test_finished();
      run_next_test();
    },
    postData: kPutDataSent,
  };
  httpRequest(kPutUrl, options);
});

add_test(function test_GetData() {
  do_test_pending();
  let options = {
    onLoad(aResponse) {
      Assert.equal(aResponse, "Success!");
      do_test_finished();
      run_next_test();
    },
    onError(e) {
      Assert.ok(false);
      do_test_finished();
      run_next_test();
    },
    postData: null,
  };
  httpRequest(kGetUrl, options);
});

add_test(function test_OptionalParameters() {
  let options = {
    onLoad: null,
    onError: null,
    logger: null,
  };
  // Just make sure that nothing throws when doing this (i.e. httpRequest
  // doesn't try to access null options).
  httpRequest(kGetUrl, options);
  run_next_test();
});

/**
 * Makes sure that httpRequest API allows setting a custom Content-Type header
 * for POST requests when data is a string.
 */
add_test(function test_CustomContentTypeOnPost() {
  do_test_pending();

  // Preparing the request parameters.
  let options = {
    onLoad(aResponse) {
      Assert.equal(aResponse, "Success!");
      do_test_finished();
      run_next_test();
    },
    onError(e) {
      Assert.ok(false);
      do_test_finished();
      run_next_test();
    },
    postData: kJsonPostData,
    // Setting a custom Content-Type header.
    headers: [["Content-Type", "application/json"]],
  };

  // Firing the request.
  httpRequest(kJsonPostUrl, options);
});

/**
 * Ensures that the httpRequest API provides a way to override the response
 * MIME type.
 */
add_test(function test_OverrideMimeType() {
  do_test_pending();

  // Preparing the request parameters.
  const kMimeType = "text/xml; charset=UTF-8";
  let options = {
    onLoad(aResponse, xhr) {
      Assert.equal(aResponse, "Success!");

      // Set the expected MIME-type.
      let reportedMimeType = xhr.getResponseHeader("Content-Type");
      Assert.notEqual(reportedMimeType, kMimeType);

      // responseXML should not be not null if overriding mime type succeeded.
      Assert.ok(xhr.responseXML != null);

      do_test_finished();
      run_next_test();
    },
    onError(e) {
      Assert.ok(false);
      do_test_finished();
      run_next_test();
    },
  };

  // Firing the request.
  let xhr = httpRequest(kGetUrl, options);

  // Override the response MIME type.
  xhr.overrideMimeType(kMimeType);
});

function run_test() {
  const PREF = "dom.xhr.standard_content_type_normalization";
  Services.prefs.setBoolPref(PREF, true);

  // Set up a mock HTTP server to serve a success page.
  server = new HttpServer();
  server.registerPathHandler(kSuccessPath, successResult);
  server.registerPathHandler(
    kPostPath,
    getDataChecker("POST", kPostDataReceived, kPostMimeTypeReceived)
  );
  server.registerPathHandler(kPutPath, getDataChecker("PUT", kPutDataReceived));
  server.registerPathHandler(kGetPath, getDataChecker("GET", ""));
  server.registerPathHandler(
    kJsonPostPath,
    getDataChecker("POST", kJsonPostData, kJsonPostMimeType)
  );

  server.start(kDefaultServerPort);

  run_next_test();

  // Teardown.
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(PREF);
    server.stop(function() {});
  });
}
