"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

var httpserver = new HttpServer();
var index = 0;
var tests = [
  { url: "/test/test", datalen: 16 },

  // Test that the http channel fails and the response body is suppressed
  // bug 255119
  {
    url: "/test/test",
    responseheader: ["Location: javascript:alert()"],
    flags: CL_EXPECT_FAILURE,
    datalen: 0,
  },
];

function setupChannel(url) {
  return NetUtil.newChannel({
    uri: "http://localhost:" + httpserver.identity.primaryPort + url,
    loadUsingSystemPrincipal: true,
  });
}

function startIter() {
  var channel = setupChannel(tests[index].url);
  channel.asyncOpen(
    new ChannelListener(completeIter, channel, tests[index].flags)
  );
}

function completeIter(request, data, ctx) {
  Assert.ok(data.length == tests[index].datalen);
  if (++index < tests.length) {
    startIter();
  } else {
    httpserver.stop(do_test_finished);
  }
}

function run_test() {
  httpserver.registerPathHandler("/test/test", handler);
  httpserver.start(-1);

  startIter();
  do_test_pending();
}

function handler(metadata, response) {
  var body = "thequickbrownfox";
  response.setHeader("Content-Type", "text/plain", false);

  var header = tests[index].responseheader;
  if (header != undefined) {
    for (var i = 0; i < header.length; i++) {
      var splitHdr = header[i].split(": ");
      response.setHeader(splitHdr[0], splitHdr[1], false);
    }
  }

  response.setStatusLine(metadata.httpVersion, 302, "Redirected");
  response.bodyOutputStream.write(body, body.length);
}
