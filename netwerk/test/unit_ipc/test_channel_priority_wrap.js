/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

let httpserver;
let port;

function startHttpServer() {
  httpserver = new HttpServer();

  httpserver.registerPathHandler("/resource", (metadata, response) => {
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/plain", false);
    response.setHeader("Cache-Control", "no-cache", false);
    response.bodyOutputStream.write("data", 4);
  });

  httpserver.registerPathHandler("/redirect", (metadata, response) => {
    response.setStatusLine(metadata.httpVersion, 302, "Redirect");
    response.setHeader("Location", "/resource", false);
    response.setHeader("Cache-Control", "no-cache", false);
  });

  httpserver.start(-1);
  port = httpserver.identity.primaryPort;
}

function stopHttpServer() {
  httpserver.stop(() => {});
}

function run_test() {
  // jshint ignore:line
  registerCleanupFunction(stopHttpServer);

  run_test_in_child("../unit/test_channel_priority.js", () => {
    startHttpServer();
    sendCommand(`configPort(${port});`);
    do_await_remote_message("finished").then(() => {
      do_test_finished();
    });
  });
}
