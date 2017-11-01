/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://services-sync/resource.js");

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
  log.warn = function(message) {
    let regEx = /The response body\'s length of: \d+ doesn\'t match the header\'s content-length of: \d+/i;
    if (message.match(regEx)) {
      warnMessages.push(message);
    }
    warn.call(log, message);
  };
  return warnMessages;
}

add_task(async function test_resource_logs_content_length_mismatch() {
  _("Issuing request.");
  let httpServer = httpd_setup({"/content": contentHandler});
  let resource = new Resource(httpServer.baseURI + "/content");

  let warnMessages = getWarningMessages(resource._log);
  let result = await resource.get();

  notEqual(warnMessages.length, 0, "test that a warning was logged");
  notEqual(result.length, contentLength);
  equal(result, BODY);

  await promiseStopServer(httpServer);
});

add_task(async function test_async_resource_logs_content_length_mismatch() {
  _("Issuing request.");
  let httpServer = httpd_setup({"/content": contentHandler});
  let asyncResource = new Resource(httpServer.baseURI + "/content");

  let warnMessages = getWarningMessages(asyncResource._log);

  let content = await asyncResource.get();
  equal(content, BODY);
  notEqual(warnMessages.length, 0, "test that warning was logged");
  notEqual(content.length, contentLength);
  await promiseStopServer(httpServer);
});
