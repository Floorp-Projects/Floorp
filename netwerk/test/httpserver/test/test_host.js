/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests that the scheme, host, and port of the server are correctly recorded
 * and used in HTTP requests and responses.
 */

const PORT = 4444;
const FAKE_PORT_ONE = 8888;
const FAKE_PORT_TWO = 8889;

var srv, id;

function run_test()
{
  dumpn("*** run_test");

  srv = createServer();

  srv.registerPathHandler("/http/1.0-request", http10Request);
  srv.registerPathHandler("/http/1.1-good-host", http11goodHost);
  srv.registerPathHandler("/http/1.1-good-host-wacky-port",
                          http11goodHostWackyPort);
  srv.registerPathHandler("/http/1.1-ip-host", http11ipHost);

  srv.start(FAKE_PORT_ONE);

  id = srv.identity;

  // The default location is http://localhost:PORT, where PORT is whatever you
  // provided when you started the server.  http://127.0.0.1:PORT is also part
  // of the default set of locations.
  Assert.equal(id.primaryScheme, "http");
  Assert.equal(id.primaryHost, "localhost");
  Assert.equal(id.primaryPort, FAKE_PORT_ONE);
  Assert.ok(id.has("http", "localhost", FAKE_PORT_ONE));
  Assert.ok(id.has("http", "127.0.0.1", FAKE_PORT_ONE));

  // This should be a nop.
  id.add("http", "localhost", FAKE_PORT_ONE);
  Assert.equal(id.primaryScheme, "http");
  Assert.equal(id.primaryHost, "localhost");
  Assert.equal(id.primaryPort, FAKE_PORT_ONE);
  Assert.ok(id.has("http", "localhost", FAKE_PORT_ONE));
  Assert.ok(id.has("http", "127.0.0.1", FAKE_PORT_ONE));

  // Change the primary location and make sure all the getters work correctly.
  id.setPrimary("http", "127.0.0.1", FAKE_PORT_ONE);
  Assert.equal(id.primaryScheme, "http");
  Assert.equal(id.primaryHost, "127.0.0.1");
  Assert.equal(id.primaryPort, FAKE_PORT_ONE);
  Assert.ok(id.has("http", "localhost", FAKE_PORT_ONE));
  Assert.ok(id.has("http", "127.0.0.1", FAKE_PORT_ONE));

  // Okay, now remove the primary location -- we fall back to the original
  // location.
  id.remove("http", "127.0.0.1", FAKE_PORT_ONE);
  Assert.equal(id.primaryScheme, "http");
  Assert.equal(id.primaryHost, "localhost");
  Assert.equal(id.primaryPort, FAKE_PORT_ONE);
  Assert.ok(id.has("http", "localhost", FAKE_PORT_ONE));
  Assert.ok(!id.has("http", "127.0.0.1", FAKE_PORT_ONE));

  // You can't remove every location -- try this and the original default
  // location will be silently readded.
  id.remove("http", "localhost", FAKE_PORT_ONE);
  Assert.equal(id.primaryScheme, "http");
  Assert.equal(id.primaryHost, "localhost");
  Assert.equal(id.primaryPort, FAKE_PORT_ONE);
  Assert.ok(id.has("http", "localhost", FAKE_PORT_ONE));
  Assert.ok(!id.has("http", "127.0.0.1", FAKE_PORT_ONE));

  // Okay, now that we've exercised that behavior, shut down the server and
  // restart it on the correct port, to exercise port-changing behaviors at
  // server start and stop.
  do_test_pending();
  srv.stop(function()
  {
    try
    {
      do_test_pending();
      run_test_2();
    }
    finally
    {
      do_test_finished();
    }
  });
}

function run_test_2()
{
  dumpn("*** run_test_2");

  do_test_finished();

  // Our primary location is gone because it was dependent on the port on which
  // the server was running.
  checkPrimariesThrow(id);
  Assert.ok(!id.has("http", "127.0.0.1", FAKE_PORT_ONE));
  Assert.ok(!id.has("http", "localhost", FAKE_PORT_ONE));

  srv.start(FAKE_PORT_TWO);

  // We should have picked up http://localhost:8889 as our primary location now
  // that we've restarted.
  Assert.equal(id.primaryScheme, "http");
  Assert.equal(id.primaryHost, "localhost");
  Assert.equal(id.primaryPort, FAKE_PORT_TWO);
  Assert.ok(!id.has("http", "localhost", FAKE_PORT_ONE));
  Assert.ok(!id.has("http", "127.0.0.1", FAKE_PORT_ONE));
  Assert.ok(id.has("http", "localhost", FAKE_PORT_TWO));
  Assert.ok(id.has("http", "127.0.0.1", FAKE_PORT_TWO));

  // Now we're going to see what happens when we shut down with a primary
  // location that wasn't a default.  That location should persist, and the
  // default we remove should still not be present.
  id.setPrimary("http", "example.com", FAKE_PORT_TWO);
  Assert.ok(id.has("http", "example.com", FAKE_PORT_TWO));
  Assert.ok(id.has("http", "127.0.0.1", FAKE_PORT_TWO));
  Assert.ok(id.has("http", "localhost", FAKE_PORT_TWO));
  Assert.ok(!id.has("http", "127.0.0.1", FAKE_PORT_ONE));
  Assert.ok(!id.has("http", "localhost", FAKE_PORT_ONE));

  id.remove("http", "localhost", FAKE_PORT_TWO);
  Assert.ok(id.has("http", "example.com", FAKE_PORT_TWO));
  Assert.ok(!id.has("http", "localhost", FAKE_PORT_TWO));
  Assert.ok(id.has("http", "127.0.0.1", FAKE_PORT_TWO));
  Assert.ok(!id.has("http", "localhost", FAKE_PORT_ONE));
  Assert.ok(!id.has("http", "127.0.0.1", FAKE_PORT_ONE));

  id.remove("http", "127.0.0.1", FAKE_PORT_TWO);
  Assert.ok(id.has("http", "example.com", FAKE_PORT_TWO));
  Assert.ok(!id.has("http", "localhost", FAKE_PORT_TWO));
  Assert.ok(!id.has("http", "127.0.0.1", FAKE_PORT_TWO));
  Assert.ok(!id.has("http", "localhost", FAKE_PORT_ONE));
  Assert.ok(!id.has("http", "127.0.0.1", FAKE_PORT_ONE));

  do_test_pending();
  srv.stop(function()
  {
    try
    {
      do_test_pending();
      run_test_3();
    }
    finally
    {
      do_test_finished();
    }
  });
}

function run_test_3()
{
  dumpn("*** run_test_3");

  do_test_finished();

  // Only the default added location disappears; any others stay around,
  // possibly as the primary location.  We may have removed the default primary
  // location, but the one we set manually should persist here.
  Assert.equal(id.primaryScheme, "http");
  Assert.equal(id.primaryHost, "example.com");
  Assert.equal(id.primaryPort, FAKE_PORT_TWO);
  Assert.ok(id.has("http", "example.com", FAKE_PORT_TWO));
  Assert.ok(!id.has("http", "localhost", FAKE_PORT_TWO));
  Assert.ok(!id.has("http", "127.0.0.1", FAKE_PORT_TWO));
  Assert.ok(!id.has("http", "localhost", FAKE_PORT_ONE));
  Assert.ok(!id.has("http", "127.0.0.1", FAKE_PORT_ONE));

  srv.start(PORT);

  // Starting always adds HTTP entries for 127.0.0.1:port and localhost:port.
  Assert.ok(id.has("http", "example.com", FAKE_PORT_TWO));
  Assert.ok(!id.has("http", "localhost", FAKE_PORT_TWO));
  Assert.ok(!id.has("http", "127.0.0.1", FAKE_PORT_TWO));
  Assert.ok(!id.has("http", "localhost", FAKE_PORT_ONE));
  Assert.ok(!id.has("http", "127.0.0.1", FAKE_PORT_ONE));
  Assert.ok(id.has("http", "localhost", PORT));
  Assert.ok(id.has("http", "127.0.0.1", PORT));

  // Remove the primary location we'd left set from last time.
  id.remove("http", "example.com", FAKE_PORT_TWO);

  // Default-port behavior testing requires the server responds to requests
  // claiming to be on one such port.
  id.add("http", "localhost", 80);

  // Make sure we don't have anything lying around from running on either the
  // first or the second port -- all we should have is our generated default,
  // plus the additional port to test "portless" hostport variants.
  Assert.ok(id.has("http", "localhost", 80));
  Assert.equal(id.primaryScheme, "http");
  Assert.equal(id.primaryHost, "localhost");
  Assert.equal(id.primaryPort, PORT);
  Assert.ok(id.has("http", "localhost", PORT));
  Assert.ok(id.has("http", "127.0.0.1", PORT));
  Assert.ok(!id.has("http", "localhost", FAKE_PORT_ONE));
  Assert.ok(!id.has("http", "127.0.0.1", FAKE_PORT_ONE));
  Assert.ok(!id.has("http", "example.com", FAKE_PORT_TWO));
  Assert.ok(!id.has("http", "localhost", FAKE_PORT_TWO));
  Assert.ok(!id.has("http", "127.0.0.1", FAKE_PORT_TWO));

  // Okay, finally done with identity testing.  Our primary location is the one
  // we want it to be, so we're off!
  runRawTests(tests, testComplete(srv));
}


/*********************
 * UTILITY FUNCTIONS *
 *********************/

/**
 * Verifies that all .primary* getters on a server identity correctly throw
 * NS_ERROR_NOT_INITIALIZED.
 *
 * @param id : nsIHttpServerIdentity
 *   the server identity to test
 */
function checkPrimariesThrow(id)
{
  var threw = false;
  try
  {
    id.primaryScheme;
  }
  catch (e)
  {
    threw = e.result === Cr.NS_ERROR_NOT_INITIALIZED;
  }
  Assert.ok(threw);

  threw = false;
  try
  {
    id.primaryHost;
  }
  catch (e)
  {
    threw = e.result === Cr.NS_ERROR_NOT_INITIALIZED;
  }
  Assert.ok(threw);

  threw = false;
  try
  {
    id.primaryPort;
  }
  catch (e)
  {
    threw = e.result === Cr.NS_ERROR_NOT_INITIALIZED;
  }
  Assert.ok(threw);
}

/**
 * Utility function to check for a 400 response.
 */
function check400(data)
{
  var iter = LineIterator(data);

  // Status-Line
  var { value: firstLine } = iter.next();
  Assert.equal(firstLine.substring(0, HTTP_400_LEADER_LENGTH), HTTP_400_LEADER);
}


/***************
 * BEGIN TESTS *
 ***************/

const HTTP_400_LEADER = "HTTP/1.1 400 ";
const HTTP_400_LEADER_LENGTH = HTTP_400_LEADER.length;

var test, data;
var tests = [];

// HTTP/1.0 request, to ensure we see our default scheme/host/port

function http10Request(request, response)
{
  writeDetails(request, response);
  response.setStatusLine("1.0", 200, "TEST PASSED");
}
data = "GET /http/1.0-request HTTP/1.0\r\n" +
       "\r\n";
function check10(data)
{
  var iter = LineIterator(data);

  // Status-Line
  Assert.equal(iter.next().value, "HTTP/1.0 200 TEST PASSED");

  skipHeaders(iter);

  // Okay, next line must be the data we expected to be written
  var body =
    [
     "Method:  GET",
     "Path:    /http/1.0-request",
     "Query:   ",
     "Version: 1.0",
     "Scheme:  http",
     "Host:    localhost",
     "Port:    4444",
    ];

  expectLines(iter, body);
}
test = new RawTest("localhost", PORT, data, check10),
tests.push(test);


// HTTP/1.1 request, no Host header, expect a 400 response

data = "GET /http/1.1-request HTTP/1.1\r\n" +
       "\r\n";
test = new RawTest("localhost", PORT, data, check400),
tests.push(test);


// HTTP/1.1 request, wrong host, expect a 400 response

data = "GET /http/1.1-request HTTP/1.1\r\n" +
       "Host: not-localhost\r\n" +
       "\r\n";
test = new RawTest("localhost", PORT, data, check400),
tests.push(test);


// HTTP/1.1 request, wrong host/right port, expect a 400 response

data = "GET /http/1.1-request HTTP/1.1\r\n" +
       "Host: not-localhost:4444\r\n" +
       "\r\n";
test = new RawTest("localhost", PORT, data, check400),
tests.push(test);


// HTTP/1.1 request, Host header has host but no port, expect a 400 response

data = "GET /http/1.1-request HTTP/1.1\r\n" +
       "Host: 127.0.0.1\r\n" +
       "\r\n";
test = new RawTest("localhost", PORT, data, check400),
tests.push(test);


// HTTP/1.1 request, Request-URI has wrong port, expect a 400 response

data = "GET http://127.0.0.1/http/1.1-request HTTP/1.1\r\n" +
       "Host: 127.0.0.1\r\n" +
       "\r\n";
test = new RawTest("localhost", PORT, data, check400),
tests.push(test);


// HTTP/1.1 request, Request-URI has wrong port, expect a 400 response

data = "GET http://localhost:31337/http/1.1-request HTTP/1.1\r\n" +
       "Host: localhost:31337\r\n" +
       "\r\n";
test = new RawTest("localhost", PORT, data, check400),
tests.push(test);


// HTTP/1.1 request, Request-URI has wrong scheme, expect a 400 response

data = "GET https://localhost:4444/http/1.1-request HTTP/1.1\r\n" +
       "Host: localhost:4444\r\n" +
       "\r\n";
test = new RawTest("localhost", PORT, data, check400),
tests.push(test);


// HTTP/1.1 request, correct Host header, expect handler's response

function http11goodHost(request, response)
{
  writeDetails(request, response);
  response.setStatusLine("1.1", 200, "TEST PASSED");
}
data = "GET /http/1.1-good-host HTTP/1.1\r\n" +
       "Host: localhost:4444\r\n" +
       "\r\n";
function check11goodHost(data)
{
  var iter = LineIterator(data);

  // Status-Line
  Assert.equal(iter.next().value, "HTTP/1.1 200 TEST PASSED");

  skipHeaders(iter);

  // Okay, next line must be the data we expected to be written
  var body =
    [
     "Method:  GET",
     "Path:    /http/1.1-good-host",
     "Query:   ",
     "Version: 1.1",
     "Scheme:  http",
     "Host:    localhost",
     "Port:    4444",
    ];

  expectLines(iter, body);
}
test = new RawTest("localhost", PORT, data, check11goodHost),
tests.push(test);


// HTTP/1.1 request, Host header is secondary identity

function http11ipHost(request, response)
{
  writeDetails(request, response);
  response.setStatusLine("1.1", 200, "TEST PASSED");
}
data = "GET /http/1.1-ip-host HTTP/1.1\r\n" +
       "Host: 127.0.0.1:4444\r\n" +
       "\r\n";
function check11ipHost(data)
{
  var iter = LineIterator(data);

  // Status-Line
  Assert.equal(iter.next().value, "HTTP/1.1 200 TEST PASSED");

  skipHeaders(iter);

  // Okay, next line must be the data we expected to be written
  var body =
    [
     "Method:  GET",
     "Path:    /http/1.1-ip-host",
     "Query:   ",
     "Version: 1.1",
     "Scheme:  http",
     "Host:    127.0.0.1",
     "Port:    4444",
    ];

  expectLines(iter, body);
}
test = new RawTest("localhost", PORT, data, check11ipHost),
tests.push(test);


// HTTP/1.1 request, absolute path, accurate Host header

// reusing previous request handler so not defining a new one

data = "GET http://localhost:4444/http/1.1-good-host HTTP/1.1\r\n" +
       "Host: localhost:4444\r\n" +
       "\r\n";
test = new RawTest("localhost", PORT, data, check11goodHost),
tests.push(test);


// HTTP/1.1 request, absolute path, inaccurate Host header

// reusing previous request handler so not defining a new one

data = "GET http://localhost:4444/http/1.1-good-host HTTP/1.1\r\n" +
       "Host: localhost:1234\r\n" +
       "\r\n";
test = new RawTest("localhost", PORT, data, check11goodHost),
tests.push(test);


// HTTP/1.1 request, absolute path, different inaccurate Host header

// reusing previous request handler so not defining a new one

data = "GET http://localhost:4444/http/1.1-good-host HTTP/1.1\r\n" +
       "Host: not-localhost:4444\r\n" +
       "\r\n";
test = new RawTest("localhost", PORT, data, check11goodHost),
tests.push(test);


// HTTP/1.1 request, absolute path, yet another inaccurate Host header

// reusing previous request handler so not defining a new one

data = "GET http://localhost:4444/http/1.1-good-host HTTP/1.1\r\n" +
       "Host: yippity-skippity\r\n" +
       "\r\n";
function checkInaccurate(data)
{
  check11goodHost(data);

  // dynamism setup
  srv.identity.setPrimary("http", "127.0.0.1", 4444);
}
test = new RawTest("localhost", PORT, data, checkInaccurate),
tests.push(test);


// HTTP/1.0 request, absolute path, different inaccurate Host header

// reusing previous request handler so not defining a new one

data = "GET /http/1.0-request HTTP/1.0\r\n" +
       "Host: not-localhost:4444\r\n" +
       "\r\n";
function check10ip(data)
{
  var iter = LineIterator(data);

  // Status-Line
  Assert.equal(iter.next().value, "HTTP/1.0 200 TEST PASSED");

  skipHeaders(iter);

  // Okay, next line must be the data we expected to be written
  var body =
    [
     "Method:  GET",
     "Path:    /http/1.0-request",
     "Query:   ",
     "Version: 1.0",
     "Scheme:  http",
     "Host:    127.0.0.1",
     "Port:    4444",
    ];

  expectLines(iter, body);
}
test = new RawTest("localhost", PORT, data, check10ip),
tests.push(test);


// HTTP/1.1 request, Host header with implied port

function http11goodHostWackyPort(request, response)
{
  writeDetails(request, response);
  response.setStatusLine("1.1", 200, "TEST PASSED");
}
data = "GET /http/1.1-good-host-wacky-port HTTP/1.1\r\n" +
       "Host: localhost\r\n" +
       "\r\n";
function check11goodHostWackyPort(data)
{
  var iter = LineIterator(data);

  // Status-Line
  Assert.equal(iter.next().value, "HTTP/1.1 200 TEST PASSED");

  skipHeaders(iter);

  // Okay, next line must be the data we expected to be written
  var body =
    [
     "Method:  GET",
     "Path:    /http/1.1-good-host-wacky-port",
     "Query:   ",
     "Version: 1.1",
     "Scheme:  http",
     "Host:    localhost",
     "Port:    80",
    ];

  expectLines(iter, body);
}
test = new RawTest("localhost", PORT, data, check11goodHostWackyPort),
tests.push(test);


// HTTP/1.1 request, Host header with wacky implied port

data = "GET /http/1.1-good-host-wacky-port HTTP/1.1\r\n" +
       "Host: localhost:\r\n" +
       "\r\n";
test = new RawTest("localhost", PORT, data, check11goodHostWackyPort),
tests.push(test);


// HTTP/1.1 request, absolute URI with implied port

data = "GET http://localhost/http/1.1-good-host-wacky-port HTTP/1.1\r\n" +
       "Host: localhost\r\n" +
       "\r\n";
test = new RawTest("localhost", PORT, data, check11goodHostWackyPort),
tests.push(test);


// HTTP/1.1 request, absolute URI with wacky implied port

data = "GET http://localhost:/http/1.1-good-host-wacky-port HTTP/1.1\r\n" +
       "Host: localhost\r\n" +
       "\r\n";
test = new RawTest("localhost", PORT, data, check11goodHostWackyPort),
tests.push(test);


// HTTP/1.1 request, absolute URI with explicit implied port, ignored Host

data = "GET http://localhost:80/http/1.1-good-host-wacky-port HTTP/1.1\r\n" +
       "Host: who-cares\r\n" +
       "\r\n";
test = new RawTest("localhost", PORT, data, check11goodHostWackyPort),
tests.push(test);


// HTTP/1.1 request, a malformed Request-URI

data = "GET is-this-the-real-life-is-this-just-fantasy HTTP/1.1\r\n" +
       "Host: localhost:4444\r\n" +
       "\r\n";
test = new RawTest("localhost", PORT, data, check400),
tests.push(test);


// HTTP/1.1 request, a malformed Host header

data = "GET /http/1.1-request HTTP/1.1\r\n" +
       "Host: la la la\r\n" +
       "\r\n";
test = new RawTest("localhost", PORT, data, check400),
tests.push(test);


// HTTP/1.1 request, a malformed Host header but absolute URI, 5.2 sez fine

data = "GET http://localhost:4444/http/1.1-good-host HTTP/1.1\r\n" +
       "Host: la la la\r\n" +
       "\r\n";
test = new RawTest("localhost", PORT, data, check11goodHost),
tests.push(test);


// HTTP/1.0 request, absolute URI, but those aren't valid in HTTP/1.0

data = "GET http://localhost:4444/http/1.1-request HTTP/1.0\r\n" +
       "Host: localhost:4444\r\n" +
       "\r\n";
test = new RawTest("localhost", PORT, data, check400),
tests.push(test);


// HTTP/1.1 request, absolute URI with unrecognized host

data = "GET http://not-localhost:4444/http/1.1-request HTTP/1.1\r\n" +
       "Host: not-localhost:4444\r\n" +
       "\r\n";
test = new RawTest("localhost", PORT, data, check400),
tests.push(test);


// HTTP/1.1 request, absolute URI with unrecognized host (but not in Host)

data = "GET http://not-localhost:4444/http/1.1-request HTTP/1.1\r\n" +
       "Host: localhost:4444\r\n" +
       "\r\n";
test = new RawTest("localhost", PORT, data, check400),
tests.push(test);
