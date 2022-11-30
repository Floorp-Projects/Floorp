/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests that the server accepts requests to custom host names.
 * This is commonly used in tests that map custom host names to the server via
 * a proxy e.g. by XPCShellContentUtils.createHttpServer.
 */

var srv = createServer();
srv.start(-1);
registerCleanupFunction(() => new Promise(resolve => srv.stop(resolve)));
const PORT = srv.identity.primaryPort;
srv.registerPathHandler("/dump-request", dumpRequestLines);

function dumpRequestLines(request, response) {
  writeDetails(request, response);
  response.setStatusLine(request.httpVersion, 200, "TEST PASSED");
}

function makeRawRequest(requestLinePath, hostHeader) {
  return `GET ${requestLinePath} HTTP/1.1\r\nHost: ${hostHeader}\r\n\r\n`;
}

function verifyResponseHostPort(data, query, expectedHost, expectedPort) {
  var iter = LineIterator(data);

  // Status-Line
  Assert.equal(iter.next().value, "HTTP/1.1 200 TEST PASSED");

  skipHeaders(iter);

  // Okay, next line must be the data we expected to be written
  var body = [
    "Method:  GET",
    "Path:    /dump-request",
    "Query:   " + query,
    "Version: 1.1",
    "Scheme:  http",
    "Host:    " + expectedHost,
    "Port:    " + expectedPort,
  ];

  expectLines(iter, body);
}

function runIdentityTest(host, port) {
  srv.identity.add("http", host, port);

  function checkAbsoluteRequestURI(data) {
    verifyResponseHostPort(data, "absolute", host, port);
  }
  function checkHostHeader(data) {
    verifyResponseHostPort(data, "relative", host, port);
  }

  let tests = [];
  let test, data;
  let hostport = `${host}:${port}`;
  data = makeRawRequest(`http://${hostport}/dump-request?absolute`, hostport);
  test = new RawTest("localhost", PORT, data, checkAbsoluteRequestURI);
  tests.push(test);

  data = makeRawRequest("/dump-request?relative", hostport);
  test = new RawTest("localhost", PORT, data, checkHostHeader);
  tests.push(test);
  return new Promise(resolve => {
    runRawTests(tests, resolve);
  });
}

/** *************
 * BEGIN TESTS *
 ***************/

add_task(async function test_basic_example_com() {
  await runIdentityTest("example.com", 1234);
  await runIdentityTest("example.com", 5432);
});

add_task(async function test_fully_qualified_domain_name_aka_fqdn() {
  await runIdentityTest("fully-qualified-domain-name.", 1234);
});

add_task(async function test_ipv4() {
  await runIdentityTest("1.2.3.4", 1234);
});

add_task(async function test_ipv6() {
  Assert.throws(
    () => srv.identity.add("http", "[notipv6]", 1234),
    /NS_ERROR_ILLEGAL_VALUE/,
    "should reject invalid host, clearly not bracketed IPv6"
  );
  Assert.throws(
    () => srv.identity.add("http", "[::127.0.0.1]", 1234),
    /NS_ERROR_ILLEGAL_VALUE/,
    "should reject non-canonical IPv6"
  );
  await runIdentityTest("[::123]", 1234);
  await runIdentityTest("[1:2:3:a:b:c:d:abcd]", 1234);
});

add_task(async function test_internationalized_domain_name() {
  Assert.throws(
    () => srv.identity.add("http", "δοκιμή", 1234),
    /NS_ERROR_ILLEGAL_VALUE/,
    "should reject IDN not in punycode"
  );

  await runIdentityTest("xn--jxalpdlp", 1234);
});
