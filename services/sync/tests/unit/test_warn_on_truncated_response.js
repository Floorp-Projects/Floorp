/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/rest.js");

function run_test() {
  initTestLogging("Trace");
  run_next_test();
}

var BODY = "response body";
// contentLength needs to be longer than the response body
// length in order to get a mismatch between what is sent in
// the response and the content-length header value.
var contentLength = BODY.length + 1;

function contentHandler(request, response) {
  _("Handling request.");
  response.setHeader("Content-Type", "text/plain");
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.bodyOutputStream.write(BODY, contentLength);
}

function getWarningMessages(log) {
  let warnMessages = [];
  let warn = log.warn;
  log.warn = function (message) {
    let regEx = /The response body\'s length of: \d+ doesn\'t match the header\'s content-length of: \d+/i
    if (message.match(regEx)) {
      warnMessages.push(message);
    }
    warn.call(log, message);
  }
  return warnMessages;
}

add_test(function test_resource_logs_content_length_mismatch() {
  _("Issuing request.");
  let httpServer = httpd_setup({"/content": contentHandler});
  let resource = new Resource(httpServer.baseURI + "/content");

  let warnMessages = getWarningMessages(resource._log);
  let result = resource.get();

  notEqual(warnMessages.length, 0, "test that a warning was logged");
  notEqual(result.length, contentLength);
  equal(result, BODY);

  httpServer.stop(run_next_test);
});

add_test(function test_async_resource_logs_content_length_mismatch() {
  _("Issuing request.");
  let httpServer = httpd_setup({"/content": contentHandler});
  let asyncResource = new AsyncResource(httpServer.baseURI + "/content");

  let warnMessages = getWarningMessages(asyncResource._log);

  asyncResource.get(function (error, content) {
    equal(error, null);
    equal(content, BODY);
    notEqual(warnMessages.length, 0, "test that warning was logged");
    notEqual(content.length, contentLength);
    httpServer.stop(run_next_test);
  });
});

add_test(function test_sync_storage_request_logs_content_length_mismatch() {
  _("Issuing request.");
  let httpServer = httpd_setup({"/content": contentHandler});
  let request = new SyncStorageRequest(httpServer.baseURI + "/content");
  let warnMessages = getWarningMessages(request._log);

  // Setting this affects how received data is read from the underlying
  // nsIHttpChannel in rest.js.  If it's left as UTF-8 (the default) an
  // nsIConverterInputStream is used and the data read from channel's stream
  // isn't truncated at the null byte mark (\u0000). Therefore the
  // content-length mismatch being tested for doesn't occur.  Setting it to
  // a falsy value results in an nsIScriptableInputStream being used to read
  // the stream, which stops reading at the null byte mark resulting in a
  // content-length mismatch.
  request.charset = "";

  request.get(function (error) {
    equal(error, null);
    equal(this.response.body, BODY);
    notEqual(warnMessages.length, 0, "test that a warning was logged");
    notEqual(BODY.length, contentLength);
    httpServer.stop(run_next_test);
  });
});
