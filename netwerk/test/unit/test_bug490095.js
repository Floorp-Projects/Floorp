//
// Verify that the VALIDATE_NEVER and LOAD_FROM_CACHE flags override
// heuristic query freshness as defined in RFC 2616 section 13.9
//

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

var httpserver = new HttpServer();
var index = 0;
var tests = [
  { url: "/freshness?a", server: "0", expected: "0" },
  { url: "/freshness?a", server: "1", expected: "1" },

  // Setting the VALIDATE_NEVER flag should grab entry from cache
  {
    url: "/freshness?a",
    server: "2",
    expected: "1",
    flags: Ci.nsIRequest.VALIDATE_NEVER,
  },

  // Finally, check that request is validated with no flags set
  { url: "/freshness?a", server: "99", expected: "99" },

  { url: "/freshness?b", server: "0", expected: "0" },
  { url: "/freshness?b", server: "1", expected: "1" },

  // Setting the LOAD_FROM_CACHE flag also grab the entry from cache
  {
    url: "/freshness?b",
    server: "2",
    expected: "1",
    flags: Ci.nsIRequest.LOAD_FROM_CACHE,
  },

  // Finally, check that request is validated with no flags set
  { url: "/freshness?b", server: "99", expected: "99" },
];

function logit(i, data) {
  dump(
    tests[i].url +
      "\t requested [" +
      tests[i].server +
      "]" +
      " got [" +
      data +
      "] expected [" +
      tests[i].expected +
      "]"
  );
  if (tests[i].responseheader) {
    dump("\t[" + tests[i].responseheader + "]");
  }
  dump("\n");
}

function setupChannel(suffix, value) {
  var chan = NetUtil.newChannel({
    uri: "http://localhost:" + httpserver.identity.primaryPort + suffix,
    loadUsingSystemPrincipal: true,
  });
  var httpChan = chan.QueryInterface(Ci.nsIHttpChannel);
  httpChan.requestMethod = "GET";
  httpChan.setRequestHeader("x-request", value, false);
  return httpChan;
}

function triggerNextTest() {
  var test = tests[index];
  var channel = setupChannel(test.url, test.server);
  if (test.flags) {
    channel.loadFlags = test.flags;
  }
  channel.asyncOpen(new ChannelListener(checkValueAndTrigger, null));
}

function checkValueAndTrigger(request, data, ctx) {
  logit(index, data);
  Assert.equal(tests[index].expected, data);

  if (index < tests.length - 1) {
    index++;
    // this call happens in onStopRequest from the channel, and opening a
    // new channel to the same url here is no good idea...  post it instead
    do_timeout(1, triggerNextTest);
  } else {
    httpserver.stop(do_test_finished);
  }
}

function run_test() {
  httpserver.registerPathHandler("/freshness", handler);
  httpserver.start(-1);

  // clear cache
  evict_cache_entries();

  triggerNextTest();

  do_test_pending();
}

function handler(metadata, response) {
  var body = metadata.getHeader("x-request");
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Date", getDateString(0), false);
  response.setHeader("Cache-Control", "max-age=0", false);

  var header = tests[index].responseheader;
  if (header == null) {
    response.setHeader("Last-Modified", getDateString(-1), false);
  } else {
    var splitHdr = header.split(": ");
    response.setHeader(splitHdr[0], splitHdr[1], false);
  }

  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.bodyOutputStream.write(body, body.length);
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
