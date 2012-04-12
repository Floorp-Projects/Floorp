/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-common/rest.js");
Cu.import("resource://services-common/utils.js");

const TEST_RESOURCE_URL = TEST_SERVER_URL + "resource";

//DEBUG = true;

function run_test() {
  Log4Moz.repository.getLogger("Services.Common.RESTRequest").level =
    Log4Moz.Level.Trace;
  initTestLogging();

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

  do_check_true(request.uri instanceof Ci.nsIURI);
  do_check_eq(request.uri.spec, uri);
  do_check_eq(request.response, null);
  do_check_eq(request.status, request.NOT_SENT);
  let expectedLoadFlags = Ci.nsIRequest.LOAD_BYPASS_CACHE |
                          Ci.nsIRequest.INHIBIT_CACHING;
  do_check_eq(request.loadFlags, expectedLoadFlags);

  run_next_test();
});

/**
 * Verify that a proxy auth redirect doesn't break us. This has to be the first
 * request made in the file!
 */
add_test(function test_proxy_auth_redirect() {
  let pacFetched = false;
  function pacHandler(metadata, response) {
    pacFetched = true;
    let body = 'function FindProxyForURL(url, host) { return "DIRECT"; }';
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "application/x-ns-proxy-autoconfig", false);
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
    "/pac3":     pacHandler
  });
  PACSystemSettings.PACURI = "http://localhost:8080/pac3";
  installFakePAC();

  let res = new RESTRequest("http://localhost:8080/original");
  res.get(function (error) {
    do_check_true(pacFetched);
    do_check_true(fetched);
    do_check_true(!error);
    do_check_true(this.response.success);
    do_check_eq("TADA!", this.response.body);
    uninstallFakePAC();
    server.stop(run_next_test);
  });
});

/**
 * Demonstrate API short-hand: create a request and dispatch it immediately.
 */
add_test(function test_simple_get() {
  let handler = httpd_handler(200, "OK", "Huzzah!");
  let server = httpd_setup({"/resource": handler});

  let uri = TEST_RESOURCE_URL;
  let request = new RESTRequest(uri).get(function (error) {
    do_check_eq(error, null);

    do_check_eq(this.status, this.COMPLETED);
    do_check_true(this.response.success);
    do_check_eq(this.response.status, 200);
    do_check_eq(this.response.body, "Huzzah!");

    server.stop(run_next_test);
  });
  do_check_eq(request.status, request.SENT);
  do_check_eq(request.method, "GET");
});

/**
 * Test HTTP GET with all bells and whistles.
 */
add_test(function test_get() {
  let handler = httpd_handler(200, "OK", "Huzzah!");
  let server = httpd_setup({"/resource": handler});

  let request = new RESTRequest(TEST_RESOURCE_URL);
  do_check_eq(request.status, request.NOT_SENT);

  request.onProgress = request.onComplete = function () {
    do_throw("This function should have been overwritten!");
  };

  let onProgress_called = false;
  function onProgress() {
    onProgress_called = true;
    do_check_eq(this.status, request.IN_PROGRESS);
    do_check_true(this.response.body.length > 0);

    do_check_true(!!(this.channel.loadFlags & Ci.nsIRequest.LOAD_BYPASS_CACHE));
    do_check_true(!!(this.channel.loadFlags & Ci.nsIRequest.INHIBIT_CACHING));
  };

  function onComplete(error) {
    do_check_eq(error, null);

    do_check_eq(this.status, this.COMPLETED);
    do_check_true(this.response.success);
    do_check_eq(this.response.status, 200);
    do_check_eq(this.response.body, "Huzzah!");
    do_check_eq(handler.request.method, "GET");

    do_check_true(onProgress_called);
    CommonUtils.nextTick(function () {
      do_check_eq(request.onComplete, null);
      do_check_eq(request.onProgress, null);
      server.stop(run_next_test);
    });
  };

  do_check_eq(request.get(onComplete, onProgress), request);
  do_check_eq(request.status, request.SENT);
  do_check_eq(request.method, "GET");
  do_check_throws(function () {
    request.get();
  });
});

/**
 * Test HTTP PUT with a simple string argument and default Content-Type.
 */
add_test(function test_put() {
  let handler = httpd_handler(200, "OK", "Got it!");
  let server = httpd_setup({"/resource": handler});

  let request = new RESTRequest(TEST_RESOURCE_URL);
  do_check_eq(request.status, request.NOT_SENT);

  request.onProgress = request.onComplete = function () {
    do_throw("This function should have been overwritten!");
  };

  let onProgress_called = false;
  function onProgress() {
    onProgress_called = true;
    do_check_eq(this.status, request.IN_PROGRESS);
    do_check_true(this.response.body.length > 0);
  };

  function onComplete(error) {
    do_check_eq(error, null);

    do_check_eq(this.status, this.COMPLETED);
    do_check_true(this.response.success);
    do_check_eq(this.response.status, 200);
    do_check_eq(this.response.body, "Got it!");

    do_check_eq(handler.request.method, "PUT");
    do_check_eq(handler.request.body, "Hullo?");
    do_check_eq(handler.request.getHeader("Content-Type"), "text/plain");

    do_check_true(onProgress_called);
    CommonUtils.nextTick(function () {
      do_check_eq(request.onComplete, null);
      do_check_eq(request.onProgress, null);
      server.stop(run_next_test);
    });
  };

  do_check_eq(request.put("Hullo?", onComplete, onProgress), request);
  do_check_eq(request.status, request.SENT);
  do_check_eq(request.method, "PUT");
  do_check_throws(function () {
    request.put("Hai!");
  });
});

/**
 * Test HTTP POST with a simple string argument and default Content-Type.
 */
add_test(function test_post() {
  let handler = httpd_handler(200, "OK", "Got it!");
  let server = httpd_setup({"/resource": handler});

  let request = new RESTRequest(TEST_RESOURCE_URL);
  do_check_eq(request.status, request.NOT_SENT);

  request.onProgress = request.onComplete = function () {
    do_throw("This function should have been overwritten!");
  };

  let onProgress_called = false;
  function onProgress() {
    onProgress_called = true;
    do_check_eq(this.status, request.IN_PROGRESS);
    do_check_true(this.response.body.length > 0);
  };

  function onComplete(error) {
    do_check_eq(error, null);

    do_check_eq(this.status, this.COMPLETED);
    do_check_true(this.response.success);
    do_check_eq(this.response.status, 200);
    do_check_eq(this.response.body, "Got it!");

    do_check_eq(handler.request.method, "POST");
    do_check_eq(handler.request.body, "Hullo?");
    do_check_eq(handler.request.getHeader("Content-Type"), "text/plain");

    do_check_true(onProgress_called);
    CommonUtils.nextTick(function () {
      do_check_eq(request.onComplete, null);
      do_check_eq(request.onProgress, null);
      server.stop(run_next_test);
    });
  };

  do_check_eq(request.post("Hullo?", onComplete, onProgress), request);
  do_check_eq(request.status, request.SENT);
  do_check_eq(request.method, "POST");
  do_check_throws(function () {
    request.post("Hai!");
  });
});

/**
 * Test HTTP DELETE.
 */
add_test(function test_delete() {
  let handler = httpd_handler(200, "OK", "Got it!");
  let server = httpd_setup({"/resource": handler});

  let request = new RESTRequest(TEST_RESOURCE_URL);
  do_check_eq(request.status, request.NOT_SENT);

  request.onProgress = request.onComplete = function () {
    do_throw("This function should have been overwritten!");
  };

  let onProgress_called = false;
  function onProgress() {
    onProgress_called = true;
    do_check_eq(this.status, request.IN_PROGRESS);
    do_check_true(this.response.body.length > 0);
  };

  function onComplete(error) {
    do_check_eq(error, null);

    do_check_eq(this.status, this.COMPLETED);
    do_check_true(this.response.success);
    do_check_eq(this.response.status, 200);
    do_check_eq(this.response.body, "Got it!");
    do_check_eq(handler.request.method, "DELETE");

    do_check_true(onProgress_called);
    CommonUtils.nextTick(function () {
      do_check_eq(request.onComplete, null);
      do_check_eq(request.onProgress, null);
      server.stop(run_next_test);
    });
  };

  do_check_eq(request.delete(onComplete, onProgress), request);
  do_check_eq(request.status, request.SENT);
  do_check_eq(request.method, "DELETE");
  do_check_throws(function () {
    request.delete();
  });
});

/**
 * Test an HTTP response with a non-200 status code.
 */
add_test(function test_get_404() {
  let handler = httpd_handler(404, "Not Found", "Cannae find it!");
  let server = httpd_setup({"/resource": handler});

  let request = new RESTRequest(TEST_RESOURCE_URL);
  request.get(function (error) {
    do_check_eq(error, null);

    do_check_eq(this.status, this.COMPLETED);
    do_check_false(this.response.success);
    do_check_eq(this.response.status, 404);
    do_check_eq(this.response.body, "Cannae find it!");

    server.stop(run_next_test);
  });
});

/**
 * The 'data' argument to PUT, if not a string already, is automatically
 * stringified as JSON.
 */
add_test(function test_put_json() {
  let handler = httpd_handler(200, "OK");
  let server = httpd_setup({"/resource": handler});

  let sample_data = {
    some: "sample_data",
    injson: "format",
    number: 42
  };
  let request = new RESTRequest(TEST_RESOURCE_URL);
  request.put(sample_data, function (error) {
    do_check_eq(error, null);

    do_check_eq(this.status, this.COMPLETED);
    do_check_true(this.response.success);
    do_check_eq(this.response.status, 200);
    do_check_eq(this.response.body, "");

    do_check_eq(handler.request.method, "PUT");
    do_check_eq(handler.request.body, JSON.stringify(sample_data));
    do_check_eq(handler.request.getHeader("Content-Type"), "text/plain");

    server.stop(run_next_test);
  });
});

/**
 * The 'data' argument to POST, if not a string already, is automatically
 * stringified as JSON.
 */
add_test(function test_post_json() {
  let handler = httpd_handler(200, "OK");
  let server = httpd_setup({"/resource": handler});

  let sample_data = {
    some: "sample_data",
    injson: "format",
    number: 42
  };
  let request = new RESTRequest(TEST_RESOURCE_URL);
  request.post(sample_data, function (error) {
    do_check_eq(error, null);

    do_check_eq(this.status, this.COMPLETED);
    do_check_true(this.response.success);
    do_check_eq(this.response.status, 200);
    do_check_eq(this.response.body, "");

    do_check_eq(handler.request.method, "POST");
    do_check_eq(handler.request.body, JSON.stringify(sample_data));
    do_check_eq(handler.request.getHeader("Content-Type"), "text/plain");

    server.stop(run_next_test);
  });
});

/**
 * HTTP PUT with a custom Content-Type header.
 */
add_test(function test_put_override_content_type() {
  let handler = httpd_handler(200, "OK");
  let server = httpd_setup({"/resource": handler});

  let request = new RESTRequest(TEST_RESOURCE_URL);
  request.setHeader("Content-Type", "application/lolcat");
  request.put("O HAI!!1!", function (error) {
    do_check_eq(error, null);

    do_check_eq(this.status, this.COMPLETED);
    do_check_true(this.response.success);
    do_check_eq(this.response.status, 200);
    do_check_eq(this.response.body, "");

    do_check_eq(handler.request.method, "PUT");
    do_check_eq(handler.request.body, "O HAI!!1!");
    do_check_eq(handler.request.getHeader("Content-Type"), "application/lolcat");

    server.stop(run_next_test);
  });
});

/**
 * HTTP POST with a custom Content-Type header.
 */
add_test(function test_post_override_content_type() {
  let handler = httpd_handler(200, "OK");
  let server = httpd_setup({"/resource": handler});

  let request = new RESTRequest(TEST_RESOURCE_URL);
  request.setHeader("Content-Type", "application/lolcat");
  request.post("O HAI!!1!", function (error) {
    do_check_eq(error, null);

    do_check_eq(this.status, this.COMPLETED);
    do_check_true(this.response.success);
    do_check_eq(this.response.status, 200);
    do_check_eq(this.response.body, "");

    do_check_eq(handler.request.method, "POST");
    do_check_eq(handler.request.body, "O HAI!!1!");
    do_check_eq(handler.request.getHeader("Content-Type"), "application/lolcat");

    server.stop(run_next_test);
  });
});

/**
 * No special headers are sent by default on a GET request.
 */
add_test(function test_get_no_headers() {
  let handler = httpd_handler(200, "OK");
  let server = httpd_setup({"/resource": handler});

  let ignore_headers = ["host", "user-agent", "accept", "accept-language",
                        "accept-encoding", "accept-charset", "keep-alive",
                        "connection", "pragma", "cache-control",
                        "content-length"];

  new RESTRequest(TEST_RESOURCE_URL).get(function (error) {
    do_check_eq(error, null);

    do_check_eq(this.response.status, 200);
    do_check_eq(this.response.body, "");

    let server_headers = handler.request.headers;
    while (server_headers.hasMoreElements()) {
      let header = server_headers.getNext().toString();
      if (ignore_headers.indexOf(header) == -1) {
        do_throw("Got unexpected header!");
      }
    }

    server.stop(run_next_test);
  });
});

/**
 * Test changing the URI after having created the request.
 */
add_test(function test_changing_uri() {
  let handler = httpd_handler(200, "OK");
  let server = httpd_setup({"/resource": handler});

  let request = new RESTRequest("http://localhost:8080/the-wrong-resource");
  request.uri = CommonUtils.makeURI(TEST_RESOURCE_URL);
  request.get(function (error) {
    do_check_eq(error, null);
    do_check_eq(this.response.status, 200);
    server.stop(run_next_test);
  });
});

/**
 * Test setting HTTP request headers.
 */
add_test(function test_request_setHeader() {
  let handler = httpd_handler(200, "OK");
  let server = httpd_setup({"/resource": handler});

  let request = new RESTRequest(TEST_RESOURCE_URL);

  request.setHeader("X-What-Is-Weave", "awesome");
  request.setHeader("X-WHAT-is-Weave", "more awesomer");
  request.setHeader("Another-Header", "Hello World");

  request.get(function (error) {
    do_check_eq(error, null);

    do_check_eq(this.response.status, 200);
    do_check_eq(this.response.body, "");

    do_check_eq(handler.request.getHeader("X-What-Is-Weave"), "more awesomer");
    do_check_eq(handler.request.getHeader("another-header"), "Hello World");

    server.stop(run_next_test);
  });
});

/**
 * Test receiving HTTP response headers.
 */
add_test(function test_response_headers() {
  function handler(request, response) {
    response.setHeader("X-What-Is-Weave", "awesome");
    response.setHeader("Another-Header", "Hello World");
    response.setStatusLine(request.httpVersion, 200, "OK");
  }
  let server = httpd_setup({"/resource": handler});
  let request = new RESTRequest(TEST_RESOURCE_URL);

  request.get(function (error) {
    do_check_eq(error, null);

    do_check_eq(this.response.status, 200);
    do_check_eq(this.response.body, "");

    do_check_eq(this.response.headers["x-what-is-weave"], "awesome");
    do_check_eq(this.response.headers["another-header"], "Hello World");

    server.stop(run_next_test);
  });
});

/**
 * The onComplete() handler gets called in case of any network errors
 * (e.g. NS_ERROR_CONNECTION_REFUSED).
 */
add_test(function test_connection_refused() {
  let request = new RESTRequest(TEST_RESOURCE_URL);
  request.onProgress = function onProgress() {
    do_throw("Shouldn't have called request.onProgress()!");
  };
  request.get(function (error) {
    do_check_eq(error.result, Cr.NS_ERROR_CONNECTION_REFUSED);
    do_check_eq(error.message, "NS_ERROR_CONNECTION_REFUSED");
    do_check_eq(this.status, this.COMPLETED);
    run_next_test();
  });
  do_check_eq(request.status, request.SENT);
});

/**
 * Abort a request that just sent off.
 */
add_test(function test_abort() {
  function handler() {
    do_throw("Shouldn't have gotten here!");
  }
  let server = httpd_setup({"/resource": handler});

  let request = new RESTRequest(TEST_RESOURCE_URL);

  // Aborting a request that hasn't been sent yet is pointless and will throw.
  do_check_throws(function () {
    request.abort();
  });

  request.onProgress = request.onComplete = function () {
    do_throw("Shouldn't have gotten here!");
  };
  request.get();
  request.abort();

  // Aborting an already aborted request is pointless and will throw.
  do_check_throws(function () {
    request.abort();
  });

  do_check_eq(request.status, request.ABORTED);
  CommonUtils.nextTick(function () {
    server.stop(run_next_test);
  });
});

/**
 * A non-zero 'timeout' property specifies the amount of seconds to wait after
 * channel activity until the request is automatically canceled.
 */
add_test(function test_timeout() {
  let server = new nsHttpServer();
  let server_connection;
  server._handler.handleResponse = function(connection) {
    // This is a handler that doesn't do anything, just keeps the connection
    // open, thereby mimicking a timing out connection. We keep a reference to
    // the open connection for later so it can be properly disposed of. That's
    // why you really only want to make one HTTP request to this server ever.
    server_connection = connection;
  };
  server.start(8080);

  let request = new RESTRequest(TEST_RESOURCE_URL);
  request.timeout = 0.1; // 100 milliseconds
  request.get(function (error) {
    do_check_eq(error.result, Cr.NS_ERROR_NET_TIMEOUT);
    do_check_eq(this.status, this.ABORTED);

    server_connection.close();
    server.stop(run_next_test);
  });
});

/**
 * An exception thrown in 'onProgress' propagates to the 'onComplete' handler.
 */
add_test(function test_exception_in_onProgress() {
  let handler = httpd_handler(200, "OK", "Foobar");
  let server = httpd_setup({"/resource": handler});

  let request = new RESTRequest(TEST_RESOURCE_URL);
  request.onProgress = function onProgress() {
    it.does.not.exist();
  };
  request.get(function (error) {
    do_check_eq(error, "ReferenceError: it is not defined");
    do_check_eq(this.status, this.ABORTED);

    server.stop(run_next_test);
  });
});

add_test(function test_new_channel() {
  _("Ensure a redirect to a new channel is handled properly.");

  let redirectRequested = false;
  function redirectHandler(metadata, response) {
    redirectRequested = true;

    let body = "Redirecting";
    response.setStatusLine(metadata.httpVersion, 307, "TEMPORARY REDIRECT");
    response.setHeader("Location", "http://localhost:8081/resource");
    response.bodyOutputStream.write(body, body.length);
  }

  let resourceRequested = false;
  function resourceHandler(metadata, response) {
    resourceRequested = true;

    let body = "Test";
    response.setHeader("Content-Type", "text/plain");
    response.bodyOutputStream.write(body, body.length);
  }
  let server1 = httpd_setup({"/redirect": redirectHandler}, 8080);
  let server2 = httpd_setup({"/resource": resourceHandler}, 8081);

  function advance() {
    server1.stop(function () {
      server2.stop(run_next_test);
    });
  }

  let request = new RESTRequest("http://localhost:8080/redirect");
  request.get(function onComplete(error) {
    let response = this.response;

    do_check_eq(200, response.status);
    do_check_eq("Test", response.body);

    advance();
  });
});
