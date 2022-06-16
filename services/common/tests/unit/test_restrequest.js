/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RESTRequest } = ChromeUtils.import(
  "resource://services-common/rest.js"
);

function run_test() {
  Log.repository.getLogger("Services.Common.RESTRequest").level =
    Log.Level.Trace;
  initTestLogging("Trace");

  run_next_test();
}

/**
 * Initializing a RESTRequest with an invalid URI throws
 * NS_ERROR_MALFORMED_URI.
 */
add_test(function test_invalid_uri() {
  do_check_throws(function() {
    new RESTRequest("an invalid URI");
  }, Cr.NS_ERROR_MALFORMED_URI);
  run_next_test();
});

/**
 * Verify initial values for attributes.
 */
add_test(function test_attributes() {
  let uri = "http://foo.com/bar/baz";
  let request = new RESTRequest(uri);

  Assert.ok(request.uri instanceof Ci.nsIURI);
  Assert.equal(request.uri.spec, uri);
  Assert.equal(request.response, null);
  Assert.equal(request.status, request.NOT_SENT);
  let expectedLoadFlags =
    Ci.nsIRequest.LOAD_BYPASS_CACHE |
    Ci.nsIRequest.INHIBIT_CACHING |
    Ci.nsIRequest.LOAD_ANONYMOUS;
  Assert.equal(request.loadFlags, expectedLoadFlags);

  run_next_test();
});

/**
 * Verify that a proxy auth redirect doesn't break us. This has to be the first
 * request made in the file!
 */
add_task(async function test_proxy_auth_redirect() {
  let pacFetched = false;
  function pacHandler(metadata, response) {
    pacFetched = true;
    let body = 'function FindProxyForURL(url, host) { return "DIRECT"; }';
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.setHeader(
      "Content-Type",
      "application/x-ns-proxy-autoconfig",
      false
    );
    response.bodyOutputStream.write(body, body.length);
  }

  let fetched = false;
  function original(metadata, response) {
    fetched = true;
    let body = "TADA!";
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.bodyOutputStream.write(body, body.length);
  }

  let server = httpd_setup({
    "/original": original,
    "/pac3": pacHandler,
  });
  PACSystemSettings.PACURI = server.baseURI + "/pac3";
  installFakePAC();

  let req = new RESTRequest(server.baseURI + "/original");
  await req.get();

  Assert.ok(pacFetched);
  Assert.ok(fetched);

  Assert.ok(req.response.success);
  Assert.equal("TADA!", req.response.body);
  uninstallFakePAC();
  await promiseStopServer(server);
});

/**
 * Ensure that failures that cause asyncOpen to throw
 * result in callbacks being invoked.
 * Bug 826086.
 */
add_task(async function test_forbidden_port() {
  let request = new RESTRequest("http://localhost:6000/");

  await Assert.rejects(
    request.get(),
    error => error.result == Cr.NS_ERROR_PORT_ACCESS_NOT_ALLOWED
  );
});

/**
 * Demonstrate API short-hand: create a request and dispatch it immediately.
 */
add_task(async function test_simple_get() {
  let handler = httpd_handler(200, "OK", "Huzzah!");
  let server = httpd_setup({ "/resource": handler });
  let request = new RESTRequest(server.baseURI + "/resource");
  let promiseResponse = request.get();

  Assert.equal(request.status, request.SENT);
  Assert.equal(request.method, "GET");

  let response = await promiseResponse;
  Assert.equal(response, request.response);

  Assert.equal(request.status, request.COMPLETED);
  Assert.ok(response.success);
  Assert.equal(response.status, 200);
  Assert.equal(response.body, "Huzzah!");
  await promiseStopServer(server);
});

/**
 * Test HTTP GET with all bells and whistles.
 */
add_task(async function test_get() {
  let handler = httpd_handler(200, "OK", "Huzzah!");
  let server = httpd_setup({ "/resource": handler });

  let request = new RESTRequest(server.baseURI + "/resource");
  Assert.equal(request.status, request.NOT_SENT);

  let promiseResponse = request.get();

  Assert.equal(request.status, request.SENT);
  Assert.equal(request.method, "GET");

  Assert.ok(!!(request.channel.loadFlags & Ci.nsIRequest.LOAD_BYPASS_CACHE));
  Assert.ok(!!(request.channel.loadFlags & Ci.nsIRequest.INHIBIT_CACHING));

  let response = await promiseResponse;

  Assert.equal(response, request.response);
  Assert.equal(request.status, request.COMPLETED);
  Assert.ok(request.response.success);
  Assert.equal(request.response.status, 200);
  Assert.equal(request.response.body, "Huzzah!");
  Assert.equal(handler.request.method, "GET");

  await Assert.rejects(request.get(), /Request has already been sent/);

  await promiseStopServer(server);
});

/**
 * Test HTTP GET with UTF-8 content, and custom Content-Type.
 */
add_task(async function test_get_utf8() {
  let response = "Hello World or Καλημέρα κόσμε or こんにちは 世界 😺";

  let contentType = "text/plain";
  let charset = true;
  let charsetSuffix = "; charset=UTF-8";

  let server = httpd_setup({
    "/resource": function(req, res) {
      res.setStatusLine(req.httpVersion, 200, "OK");
      res.setHeader(
        "Content-Type",
        contentType + (charset ? charsetSuffix : "")
      );

      let converter = Cc[
        "@mozilla.org/intl/converter-output-stream;1"
      ].createInstance(Ci.nsIConverterOutputStream);
      converter.init(res.bodyOutputStream, "UTF-8");
      converter.writeString(response);
      converter.close();
    },
  });

  // Check if charset in Content-Type is propertly interpreted.
  let request1 = new RESTRequest(server.baseURI + "/resource");
  await request1.get();

  Assert.equal(request1.response.status, 200);
  Assert.equal(request1.response.body, response);
  Assert.equal(
    request1.response.headers["content-type"],
    contentType + charsetSuffix
  );

  // Check that we default to UTF-8 if Content-Type doesn't have a charset
  charset = false;
  let request2 = new RESTRequest(server.baseURI + "/resource");
  await request2.get();
  Assert.equal(request2.response.status, 200);
  Assert.equal(request2.response.body, response);
  Assert.equal(request2.response.headers["content-type"], contentType);
  Assert.equal(request2.response.charset, "utf-8");

  let request3 = new RESTRequest(server.baseURI + "/resource");

  // With the test server we tend to get onDataAvailable in chunks of 8192 (in
  // real network requests there doesn't appear to be any pattern to the size of
  // the data `onDataAvailable` is called with), the smiling cat emoji encodes as
  // 4 bytes, and so when utf8 encoded, the `"a" + "😺".repeat(2048)` will not be
  // aligned onto a codepoint.
  //
  // Since 8192 isn't guaranteed and could easily change, the following string is
  // a) very long, and b) misaligned on roughly 3/4 of the bytes, as a safety
  // measure.
  response = ("a" + "😺".repeat(2048)).repeat(10);

  await request3.get();

  Assert.equal(request3.response.status, 200);

  // Make sure it came through ok, despite the misalignment.
  Assert.equal(request3.response.body, response);

  await promiseStopServer(server);
});

/**
 * Test HTTP POST data is encoded as UTF-8 by default.
 */
add_task(async function test_post_utf8() {
  // We setup a handler that responds with exactly what it received.
  // Given we've already tested above that responses are correctly utf-8
  // decoded we can surmise that the correct response coming back means the
  // input must also have been encoded.
  let server = httpd_setup({
    "/echo": function(req, res) {
      res.setStatusLine(req.httpVersion, 200, "OK");
      res.setHeader("Content-Type", req.getHeader("content-type"));
      // Get the body as bytes and write them back without touching them
      let sis = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
        Ci.nsIScriptableInputStream
      );
      sis.init(req.bodyInputStream);
      let body = sis.read(sis.available());
      sis.close();
      res.write(body);
    },
  });

  let data = {
    copyright: "©",
    // See the comment in test_get_utf8 about this string.
    long: ("a" + "😺".repeat(2048)).repeat(10),
  };
  let request1 = new RESTRequest(server.baseURI + "/echo");
  await request1.post(data);

  Assert.equal(request1.response.status, 200);
  deepEqual(JSON.parse(request1.response.body), data);
  Assert.equal(
    request1.response.headers["content-type"],
    "application/json; charset=utf-8"
  );

  await promiseStopServer(server);
});

/**
 * Test more variations of charset handling.
 */
add_task(async function test_charsets() {
  let response = "Hello World, I can't speak Russian";

  let contentType = "text/plain";
  let charset = true;
  let charsetSuffix = "; charset=us-ascii";

  let server = httpd_setup({
    "/resource": function(req, res) {
      res.setStatusLine(req.httpVersion, 200, "OK");
      res.setHeader(
        "Content-Type",
        contentType + (charset ? charsetSuffix : "")
      );

      let converter = Cc[
        "@mozilla.org/intl/converter-output-stream;1"
      ].createInstance(Ci.nsIConverterOutputStream);
      converter.init(res.bodyOutputStream, "us-ascii");
      converter.writeString(response);
      converter.close();
    },
  });

  // Check that provided charset overrides hint.
  let request1 = new RESTRequest(server.baseURI + "/resource");
  request1.charset = "not-a-charset";
  await request1.get();
  Assert.equal(request1.response.status, 200);
  Assert.equal(request1.response.body, response);
  Assert.equal(
    request1.response.headers["content-type"],
    contentType + charsetSuffix
  );
  Assert.equal(request1.response.charset, "us-ascii");

  // Check that hint is used if Content-Type doesn't have a charset.
  charset = false;
  let request2 = new RESTRequest(server.baseURI + "/resource");
  request2.charset = "us-ascii";
  await request2.get();

  Assert.equal(request2.response.status, 200);
  Assert.equal(request2.response.body, response);
  Assert.equal(request2.response.headers["content-type"], contentType);
  Assert.equal(request2.response.charset, "us-ascii");

  await promiseStopServer(server);
});

/**
 * Used for testing PATCH/PUT/POST methods.
 */
async function check_posting_data(method) {
  let funcName = method.toLowerCase();
  let handler = httpd_handler(200, "OK", "Got it!");
  let server = httpd_setup({ "/resource": handler });

  let request = new RESTRequest(server.baseURI + "/resource");
  Assert.equal(request.status, request.NOT_SENT);
  let responsePromise = request[funcName]("Hullo?");
  Assert.equal(request.status, request.SENT);
  Assert.equal(request.method, method);

  let response = await responsePromise;

  Assert.equal(response, request.response);

  Assert.equal(request.status, request.COMPLETED);
  Assert.ok(request.response.success);
  Assert.equal(request.response.status, 200);
  Assert.equal(request.response.body, "Got it!");

  Assert.equal(handler.request.method, method);
  Assert.equal(handler.request.body, "Hullo?");
  Assert.equal(handler.request.getHeader("Content-Type"), "text/plain");

  await Assert.rejects(
    request[funcName]("Hai!"),
    /Request has already been sent/
  );

  await promiseStopServer(server);
}

/**
 * Test HTTP PATCH with a simple string argument and default Content-Type.
 */
add_task(async function test_patch() {
  await check_posting_data("PATCH");
});

/**
 * Test HTTP PUT with a simple string argument and default Content-Type.
 */
add_task(async function test_put() {
  await check_posting_data("PUT");
});

/**
 * Test HTTP POST with a simple string argument and default Content-Type.
 */
add_task(async function test_post() {
  await check_posting_data("POST");
});

/**
 * Test HTTP DELETE.
 */
add_task(async function test_delete() {
  let handler = httpd_handler(200, "OK", "Got it!");
  let server = httpd_setup({ "/resource": handler });

  let request = new RESTRequest(server.baseURI + "/resource");
  Assert.equal(request.status, request.NOT_SENT);
  let responsePromise = request.delete();
  Assert.equal(request.status, request.SENT);
  Assert.equal(request.method, "DELETE");

  let response = await responsePromise;
  Assert.equal(response, request.response);

  Assert.equal(request.status, request.COMPLETED);
  Assert.ok(request.response.success);
  Assert.equal(request.response.status, 200);
  Assert.equal(request.response.body, "Got it!");
  Assert.equal(handler.request.method, "DELETE");

  await Assert.rejects(request.delete(), /Request has already been sent/);

  await promiseStopServer(server);
});

/**
 * Test an HTTP response with a non-200 status code.
 */
add_task(async function test_get_404() {
  let handler = httpd_handler(404, "Not Found", "Cannae find it!");
  let server = httpd_setup({ "/resource": handler });

  let request = new RESTRequest(server.baseURI + "/resource");
  await request.get();

  Assert.equal(request.status, request.COMPLETED);
  Assert.ok(!request.response.success);
  Assert.equal(request.response.status, 404);
  Assert.equal(request.response.body, "Cannae find it!");

  await promiseStopServer(server);
});

/**
 * The 'data' argument to PUT, if not a string already, is automatically
 * stringified as JSON.
 */
add_task(async function test_put_json() {
  let handler = httpd_handler(200, "OK");
  let server = httpd_setup({ "/resource": handler });

  let sample_data = {
    some: "sample_data",
    injson: "format",
    number: 42,
  };
  let request = new RESTRequest(server.baseURI + "/resource");
  await request.put(sample_data);

  Assert.equal(request.status, request.COMPLETED);
  Assert.ok(request.response.success);
  Assert.equal(request.response.status, 200);
  Assert.equal(request.response.body, "");

  Assert.equal(handler.request.method, "PUT");
  Assert.equal(handler.request.body, JSON.stringify(sample_data));
  Assert.equal(
    handler.request.getHeader("Content-Type"),
    "application/json; charset=utf-8"
  );

  await promiseStopServer(server);
});

/**
 * The 'data' argument to POST, if not a string already, is automatically
 * stringified as JSON.
 */
add_task(async function test_post_json() {
  let handler = httpd_handler(200, "OK");
  let server = httpd_setup({ "/resource": handler });

  let sample_data = {
    some: "sample_data",
    injson: "format",
    number: 42,
  };
  let request = new RESTRequest(server.baseURI + "/resource");
  await request.post(sample_data);

  Assert.equal(request.status, request.COMPLETED);
  Assert.ok(request.response.success);
  Assert.equal(request.response.status, 200);
  Assert.equal(request.response.body, "");

  Assert.equal(handler.request.method, "POST");
  Assert.equal(handler.request.body, JSON.stringify(sample_data));
  Assert.equal(
    handler.request.getHeader("Content-Type"),
    "application/json; charset=utf-8"
  );

  await promiseStopServer(server);
});

/**
 * The content-type will be text/plain without a charset if the 'data' argument
 * to POST is already a string.
 */
add_task(async function test_post_json() {
  let handler = httpd_handler(200, "OK");
  let server = httpd_setup({ "/resource": handler });

  let sample_data = "hello";
  let request = new RESTRequest(server.baseURI + "/resource");
  await request.post(sample_data);
  Assert.equal(request.status, request.COMPLETED);
  Assert.ok(request.response.success);
  Assert.equal(request.response.status, 200);
  Assert.equal(request.response.body, "");

  Assert.equal(handler.request.method, "POST");
  Assert.equal(handler.request.body, sample_data);
  Assert.equal(handler.request.getHeader("Content-Type"), "text/plain");

  await promiseStopServer(server);
});

/**
 * HTTP PUT with a custom Content-Type header.
 */
add_task(async function test_put_override_content_type() {
  let handler = httpd_handler(200, "OK");
  let server = httpd_setup({ "/resource": handler });

  let request = new RESTRequest(server.baseURI + "/resource");
  request.setHeader("Content-Type", "application/lolcat");
  await request.put("O HAI!!1!");

  Assert.equal(request.status, request.COMPLETED);
  Assert.ok(request.response.success);
  Assert.equal(request.response.status, 200);
  Assert.equal(request.response.body, "");

  Assert.equal(handler.request.method, "PUT");
  Assert.equal(handler.request.body, "O HAI!!1!");
  Assert.equal(handler.request.getHeader("Content-Type"), "application/lolcat");

  await promiseStopServer(server);
});

/**
 * HTTP POST with a custom Content-Type header.
 */
add_task(async function test_post_override_content_type() {
  let handler = httpd_handler(200, "OK");
  let server = httpd_setup({ "/resource": handler });

  let request = new RESTRequest(server.baseURI + "/resource");
  request.setHeader("Content-Type", "application/lolcat");
  await request.post("O HAI!!1!");

  Assert.equal(request.status, request.COMPLETED);
  Assert.ok(request.response.success);
  Assert.equal(request.response.status, 200);
  Assert.equal(request.response.body, "");

  Assert.equal(handler.request.method, "POST");
  Assert.equal(handler.request.body, "O HAI!!1!");
  Assert.equal(handler.request.getHeader("Content-Type"), "application/lolcat");

  await promiseStopServer(server);
});

/**
 * No special headers are sent by default on a GET request.
 */
add_task(async function test_get_no_headers() {
  let handler = httpd_handler(200, "OK");
  let server = httpd_setup({ "/resource": handler });

  let ignore_headers = [
    "host",
    "user-agent",
    "accept",
    "accept-language",
    "accept-encoding",
    "accept-charset",
    "keep-alive",
    "connection",
    "pragma",
    "cache-control",
    "content-length",
    "sec-fetch-dest",
    "sec-fetch-mode",
    "sec-fetch-site",
    "sec-fetch-user",
  ];
  let request = new RESTRequest(server.baseURI + "/resource");
  await request.get();

  Assert.equal(request.response.status, 200);
  Assert.equal(request.response.body, "");

  let server_headers = handler.request.headers;
  while (server_headers.hasMoreElements()) {
    let header = server_headers.getNext().toString();
    if (!ignore_headers.includes(header)) {
      do_throw("Got unexpected header!");
    }
  }

  await promiseStopServer(server);
});

/**
 * Client includes default Accept header in API requests
 */
add_task(async function test_default_accept_headers() {
  let handler = httpd_handler(200, "OK");
  let server = httpd_setup({ "/resource": handler });

  let request = new RESTRequest(server.baseURI + "/resource");
  await request.get();

  Assert.equal(request.response.status, 200);
  Assert.equal(request.response.body, "");

  let accept_header = handler.request.getHeader("accept");

  Assert.ok(!accept_header.includes("text/html"));
  Assert.ok(!accept_header.includes("application/xhtml+xml"));
  Assert.ok(!accept_header.includes("applcation/xml"));

  Assert.ok(
    accept_header.includes("application/json") ||
      accept_header.includes("application/newlines")
  );

  await promiseStopServer(server);
});

/**
 * Test changing the URI after having created the request.
 */
add_task(async function test_changing_uri() {
  let handler = httpd_handler(200, "OK");
  let server = httpd_setup({ "/resource": handler });

  let request = new RESTRequest("http://localhost:1234/the-wrong-resource");
  request.uri = CommonUtils.makeURI(server.baseURI + "/resource");
  let response = await request.get();
  Assert.equal(response.status, 200);
  await promiseStopServer(server);
});

/**
 * Test setting HTTP request headers.
 */
add_task(async function test_request_setHeader() {
  let handler = httpd_handler(200, "OK");
  let server = httpd_setup({ "/resource": handler });

  let request = new RESTRequest(server.baseURI + "/resource");

  request.setHeader("X-What-Is-Weave", "awesome");
  request.setHeader("X-WHAT-is-Weave", "more awesomer");
  request.setHeader("Another-Header", "Hello World");
  await request.get();

  Assert.equal(request.response.status, 200);
  Assert.equal(request.response.body, "");

  Assert.equal(handler.request.getHeader("X-What-Is-Weave"), "more awesomer");
  Assert.equal(handler.request.getHeader("another-header"), "Hello World");

  await promiseStopServer(server);
});

/**
 * Test receiving HTTP response headers.
 */
add_task(async function test_response_headers() {
  function handler(request, response) {
    response.setHeader("X-What-Is-Weave", "awesome");
    response.setHeader("Another-Header", "Hello World");
    response.setStatusLine(request.httpVersion, 200, "OK");
  }
  let server = httpd_setup({ "/resource": handler });
  let request = new RESTRequest(server.baseURI + "/resource");
  await request.get();

  Assert.equal(request.response.status, 200);
  Assert.equal(request.response.body, "");

  Assert.equal(request.response.headers["x-what-is-weave"], "awesome");
  Assert.equal(request.response.headers["another-header"], "Hello World");

  await promiseStopServer(server);
});

/**
 * The onComplete() handler gets called in case of any network errors
 * (e.g. NS_ERROR_CONNECTION_REFUSED).
 */
add_task(async function test_connection_refused() {
  let request = new RESTRequest("http://localhost:1234/resource");

  // Fail the test if we resolve, return the error if we reject
  await Assert.rejects(
    request.get(),
    error =>
      error.result == Cr.NS_ERROR_CONNECTION_REFUSED &&
      error.message == "NS_ERROR_CONNECTION_REFUSED"
  );

  Assert.equal(request.status, request.COMPLETED);
});

/**
 * Abort a request that just sent off.
 */
add_task(async function test_abort() {
  function handler() {
    do_throw("Shouldn't have gotten here!");
  }
  let server = httpd_setup({ "/resource": handler });

  let request = new RESTRequest(server.baseURI + "/resource");

  // Aborting a request that hasn't been sent yet is pointless and will throw.
  do_check_throws(function() {
    request.abort();
  });

  let responsePromise = request.get();
  request.abort();

  // Aborting an already aborted request is pointless and will throw.
  do_check_throws(function() {
    request.abort();
  });

  Assert.equal(request.status, request.ABORTED);

  await Assert.rejects(responsePromise, /NS_BINDING_ABORTED/);

  await promiseStopServer(server);
});

/**
 * A non-zero 'timeout' property specifies the amount of seconds to wait after
 * channel activity until the request is automatically canceled.
 */
add_task(async function test_timeout() {
  let server = new HttpServer();
  let server_connection;
  server._handler.handleResponse = function(connection) {
    // This is a handler that doesn't do anything, just keeps the connection
    // open, thereby mimicking a timing out connection. We keep a reference to
    // the open connection for later so it can be properly disposed of. That's
    // why you really only want to make one HTTP request to this server ever.
    server_connection = connection;
  };
  server.start();
  let identity = server.identity;
  let uri =
    identity.primaryScheme +
    "://" +
    identity.primaryHost +
    ":" +
    identity.primaryPort;

  let request = new RESTRequest(uri + "/resource");
  request.timeout = 0.1; // 100 milliseconds

  await Assert.rejects(
    request.get(),
    error => error.result == Cr.NS_ERROR_NET_TIMEOUT
  );

  Assert.equal(request.status, request.ABORTED);

  // server_connection is undefined on the Android emulator for reasons
  // unknown. Yet, we still get here. If this test is refactored, we should
  // investigate the reason why the above callback is behaving differently.
  if (server_connection) {
    _("Closing connection.");
    server_connection.close();
  }
  await promiseStopServer(server);
});

add_task(async function test_new_channel() {
  _("Ensure a redirect to a new channel is handled properly.");

  function checkUA(metadata) {
    let ua = metadata.getHeader("User-Agent");
    _("User-Agent is " + ua);
    Assert.equal("foo bar", ua);
  }

  let redirectRequested = false;
  let redirectURL;
  function redirectHandler(metadata, response) {
    checkUA(metadata);
    redirectRequested = true;

    let body = "Redirecting";
    response.setStatusLine(metadata.httpVersion, 307, "TEMPORARY REDIRECT");
    response.setHeader("Location", redirectURL);
    response.bodyOutputStream.write(body, body.length);
  }

  let resourceRequested = false;
  function resourceHandler(metadata, response) {
    checkUA(metadata);
    resourceRequested = true;

    let body = "Test";
    response.setHeader("Content-Type", "text/plain");
    response.bodyOutputStream.write(body, body.length);
  }

  let server1 = httpd_setup({ "/redirect": redirectHandler });
  let server2 = httpd_setup({ "/resource": resourceHandler });
  redirectURL = server2.baseURI + "/resource";

  let request = new RESTRequest(server1.baseURI + "/redirect");
  request.setHeader("User-Agent", "foo bar");

  // Swizzle in our own fakery, because this redirect is neither
  // internal nor URI-preserving. RESTRequest's policy is to only
  // copy headers under certain circumstances.
  let protoMethod = request.shouldCopyOnRedirect;
  request.shouldCopyOnRedirect = function wrapped(o, n, f) {
    // Check the default policy.
    Assert.ok(!protoMethod.call(this, o, n, f));
    return true;
  };

  let response = await request.get();

  Assert.equal(200, response.status);
  Assert.equal("Test", response.body);
  Assert.ok(redirectRequested);
  Assert.ok(resourceRequested);

  await promiseStopServer(server1);
  await promiseStopServer(server2);
});

add_task(async function test_not_sending_cookie() {
  function handler(metadata, response) {
    let body = "COOKIE!";
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.bodyOutputStream.write(body, body.length);
    Assert.ok(!metadata.hasHeader("Cookie"));
  }
  let server = httpd_setup({ "/test": handler });

  let uri = CommonUtils.makeURI(server.baseURI);
  let channel = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
  });
  Services.cookies.setCookieStringFromHttp(uri, "test=test; path=/;", channel);

  let res = new RESTRequest(server.baseURI + "/test");
  let response = await res.get();

  Assert.ok(response.success);
  Assert.equal("COOKIE!", response.body);

  await promiseStopServer(server);
});
