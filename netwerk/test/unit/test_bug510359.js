"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

var httpserver = new HttpServer();
var index = 0;
var tests = [
  { url: "/bug510359", server: "0", expected: "0" },
  { url: "/bug510359", server: "1", expected: "1" },
];

function setupChannel(suffix, value) {
  var chan = NetUtil.newChannel({
    uri: "http://localhost:" + httpserver.identity.primaryPort + suffix,
    loadUsingSystemPrincipal: true,
  });
  var httpChan = chan.QueryInterface(Ci.nsIHttpChannel);
  httpChan.requestMethod = "GET";
  httpChan.setRequestHeader("x-request", value, false);
  httpChan.setRequestHeader("Cookie", "c=" + value, false);
  return httpChan;
}

function triggerNextTest() {
  var channel = setupChannel(tests[index].url, tests[index].server);
  channel.asyncOpen(new ChannelListener(checkValueAndTrigger, null));
}

function checkValueAndTrigger(request, data, ctx) {
  Assert.equal(tests[index].expected, data);

  if (index < tests.length - 1) {
    index++;
    triggerNextTest();
  } else {
    httpserver.stop(do_test_finished);
  }
}

function run_test() {
  httpserver.registerPathHandler("/bug510359", handler);
  httpserver.start(-1);

  // clear cache
  evict_cache_entries();

  triggerNextTest();

  do_test_pending();
}

function handler(metadata, response) {
  try {
    var IMS = metadata.getHeader("If-Modified-Since");
    response.setStatusLine(metadata.httpVersion, 500, "Failed");
    var msg = "Client should not set If-Modified-Since header";
    response.bodyOutputStream.write(msg, msg.length);
  } catch (ex) {
    response.setStatusLine(metadata.httpVersion, 200, "Ok");
    response.setHeader("Content-Type", "text/plain", false);
    response.setHeader("Last-Modified", getDateString(-1), false);
    response.setHeader("Vary", "Cookie", false);
    var body = metadata.getHeader("x-request");
    response.bodyOutputStream.write(body, body.length);
  }
}

function getDateString(yearDelta) {
  var months = [
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec",
  ];
  var days = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];

  var d = new Date();
  return (
    days[d.getUTCDay()] +
    ", " +
    d.getUTCDate() +
    " " +
    months[d.getUTCMonth()] +
    " " +
    (d.getUTCFullYear() + yearDelta) +
    " " +
    d.getUTCHours() +
    ":" +
    d.getUTCMinutes() +
    ":" +
    d.getUTCSeconds() +
    " UTC"
  );
}
