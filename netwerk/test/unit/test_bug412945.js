"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

var httpserv;

function TestListener() {}

TestListener.prototype.onStartRequest = function (request) {};

TestListener.prototype.onStopRequest = function (request, status) {
  httpserv.stop(do_test_finished);
};

function run_test() {
  httpserv = new HttpServer();

  httpserv.registerPathHandler("/bug412945", bug412945);

  httpserv.start(-1);

  // make request
  var channel = NetUtil.newChannel({
    uri: "http://localhost:" + httpserv.identity.primaryPort + "/bug412945",
    loadUsingSystemPrincipal: true,
  });

  channel.QueryInterface(Ci.nsIHttpChannel);
  channel.requestMethod = "POST";
  channel.asyncOpen(new TestListener(), null);

  do_test_pending();
}

function bug412945(metadata, response) {
  if (
    !metadata.hasHeader("Content-Length") ||
    metadata.getHeader("Content-Length") != "0"
  ) {
    do_throw("Content-Length header not found!");
  }
}
