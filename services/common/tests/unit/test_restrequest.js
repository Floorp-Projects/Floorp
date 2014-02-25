/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-common/rest.js");
Cu.import("resource://services-common/utils.js");

//DEBUG = true;

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

  do_check_true(request.uri instanceof Ci.nsIURI);
  do_check_eq(request.uri.spec, uri);
  do_check_eq(request.response, null);
  do_check_eq(request.status, request.NOT_SENT);
  let expectedLoadFlags = Ci.nsIRequest.LOAD_BYPASS_CACHE |
                          Ci.nsIRequest.INHIBIT_CACHING |
                          Ci.nsIRequest.LOAD_ANONYMOUS;
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
  PACSystemSettings.PACURI = server.baseURI + "/pac3";
  installFakePAC();

  let res = new RESTRequest(server.baseURI + "/original");
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
 * Ensure that failures that cause asyncOpen to throw
 * result in callbacks being invoked.
 * Bug 826086.
 */
add_test(function test_forbidden_port() {
  let request = new RESTRequest("http://localhost:6000/");
  request.get(function(error) {
    if (!error) {
      do_throw("Should have got an error.");
    }
    do_check_eq(error.result, Components.results.NS_ERROR_PORT_ACCESS_NOT_ALLOWED);
    run_next_test();
  });
});

/**
 * Demonstrate API short-hand: create a request and dispatch it immediately.
 */
add_test(function test_simple_get() {
  let handler = httpd_handler(200, "OK", "Huzzah!");
  let server = httpd_setup({"/resource": handler});

  let request = new RESTRequest(server.baseURI + "/resource").get(function (error) {
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

  let request = new RESTRequest(server.baseURI + "/resource");
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
 * Test HTTP GET with UTF-8 content, and custom Content-Type.
 */
add_test(function test_get_utf8() {
  let response = "Hello World or Καλημέρα κόσμε or こんにちは 世界";

  let contentType = "text/plain";
  let charset = true;
  let charsetSuffix = "; charset=UTF-8";

  let server = httpd_setup({"/resource": function(req, res) {
    res.setStatusLine(req.httpVersion, 200, "OK");
    res.setHeader("Content-Type", contentType + (charset ? charsetSuffix : ""));

    let converter = Cc["@mozilla.org/intl/converter-output-stream;1"]
                    .createInstance(Ci.nsIConverterOutputStream);
    converter.init(res.bodyOutputStream, "UTF-8", 0, 0x0000);
    converter.writeString(response);
    converter.close();
  }});

  // Check if charset in Content-Type is propertly interpreted.
  let request1 = new RESTRequest(server.baseURI + "/resource");
  request1.get(function(error) {
    do_check_null(error);

    do_check_eq(request1.response.status, 200);
    do_check_eq(request1.response.body, response);
    do_check_eq(request1.response.headers["content-type"],
                contentType + charsetSuffix);

    // Check that we default to UTF-8 if Content-Type doesn't have a charset.
    charset = false;
    let request2 = new RESTRequest(server.baseURI + "/resource");
    request2.get(function(error) {
      do_check_null(error);

      do_check_eq(request2.response.status, 200);
      do_check_eq(request2.response.body, response);
      do_check_eq(request2.response.headers["content-type"], contentType);
      do_check_eq(request2.response.charset, "utf-8");

      server.stop(run_next_test);
    });
  });
});

/**
 * Test more variations of charset handling.
 */
add_test(function test_charsets() {
  let response = "Hello World, I can't speak Russian";

  let contentType = "text/plain";
  let charset = true;
  let charsetSuffix = "; charset=us-ascii";

  let server = httpd_setup({"/resource": function(req, res) {
    res.setStatusLine(req.httpVersion, 200, "OK");
    res.setHeader("Content-Type", contentType + (charset ? charsetSuffix : ""));

    let converter = Cc["@mozilla.org/intl/converter-output-stream;1"]
                    .createInstance(Ci.nsIConverterOutputStream);
    converter.init(res.bodyOutputStream, "us-ascii", 0, 0x0000);
    converter.writeString(response);
    converter.close();
  }});

  // Check that provided charset overrides hint.
  let request1 = new RESTRequest(server.baseURI + "/resource");
  request1.charset = "not-a-charset";
  request1.get(function(error) {
    do_check_null(error);

    do_check_eq(request1.response.status, 200);
    do_check_eq(request1.response.body, response);
    do_check_eq(request1.response.headers["content-type"],
                contentType + charsetSuffix);
    do_check_eq(request1.response.charset, "us-ascii");

    // Check that hint is used if Content-Type doesn't have a charset.
    charset = false;
    let request2 = new RESTRequest(server.baseURI + "/resource");
    request2.charset = "us-ascii";
    request2.get(function(error) {
      do_check_null(error);

      do_check_eq(request2.response.status, 200);
      do_check_eq(request2.response.body, response);
      do_check_eq(request2.response.headers["content-type"], contentType);
      do_check_eq(request2.response.charset, "us-ascii");

      server.stop(run_next_test);
    });
  });
});

/**
 * Test HTTP PUT with a simple string argument and default Content-Type.
 */
add_test(function test_put() {
  let handler = httpd_handler(200, "OK", "Got it!");
  let server = httpd_setup({"/resource": handler});

  let request = new RESTRequest(server.baseURI + "/resource");
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

  let request = new RESTRequest(server.baseURI + "/resource");
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

  let request = new RESTRequest(server.baseURI + "/resource");
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

  let request = new RESTRequest(server.baseURI + "/resource");
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
  let request = new RESTRequest(server.baseURI + "/resource");
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
  let request = new RESTRequest(server.baseURI + "/resource");
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

  let request = new RESTRequest(server.baseURI + "/resource");
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

  let request = new RESTRequest(server.baseURI + "/resource");
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

  new RESTRequest(server.baseURI + "/resource").get(function (error) {
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

  let request = new RESTRequest("http://localhost:1234/the-wrong-resource");
  request.uri = CommonUtils.makeURI(server.baseURI + "/resource");
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

  let request = new RESTRequest(server.baseURI + "/resource");

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
  let request = new RESTRequest(server.baseURI + "/resource");

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
  let request = new RESTRequest("http://localhost:1234/resource");
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

  let request = new RESTRequest(server.baseURI + "/resource");

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
  let uri = identity.primaryScheme + "://" + identity.primaryHost + ":" +
            identity.primaryPort;

  let request = new RESTRequest(uri + "/resource");
  request.timeout = 0.1; // 100 milliseconds
  request.get(function (error) {
    do_check_eq(error.result, Cr.NS_ERROR_NET_TIMEOUT);
    do_check_eq(this.status, this.ABORTED);

    // server_connection is undefined on the Android emulator for reasons
    // unknown. Yet, we still get here. If this test is refactored, we should
    // investigate the reason why the above callback is behaving differently.
    if (server_connection) {
      _("Closing connection.");
      server_connection.close();
    }

    _("Shutting down server.");
    server.stop(run_next_test);
  });
});

/**
 * An exception thrown in 'onProgress' propagates to the 'onComplete' handler.
 */
add_test(function test_exception_in_onProgress() {
  let handler = httpd_handler(200, "OK", "Foobar");
  let server = httpd_setup({"/resource": handler});

  let request = new RESTRequest(server.baseURI + "/resource");
  request.onProgress = function onProgress() {
    it.does.not.exist();
  };
  request.get(function onComplete(error) {
    do_check_eq(error, "ReferenceError: it is not defined");
    do_check_eq(this.status, this.ABORTED);

    server.stop(run_next_test);
  });
});

add_test(function test_new_channel() {
  _("Ensure a redirect to a new channel is handled properly.");

  function checkUA(metadata) {
    let ua = metadata.getHeader("User-Agent");
    _("User-Agent is " + ua);
    do_check_eq("foo bar", ua);
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

  let server1 = httpd_setup({"/redirect": redirectHandler});
  let server2 = httpd_setup({"/resource": resourceHandler});
  redirectURL = server2.baseURI + "/resource";

  function advance() {
    server1.stop(function () {
      server2.stop(run_next_test);
    });
  }

  let request = new RESTRequest(server1.baseURI + "/redirect");
  request.setHeader("User-Agent", "foo bar");

  // Swizzle in our own fakery, because this redirect is neither
  // internal nor URI-preserving. RESTRequest's policy is to only
  // copy headers under certain circumstances.
  let protoMethod = request.shouldCopyOnRedirect;
  request.shouldCopyOnRedirect = function wrapped(o, n, f) {
    // Check the default policy.
    do_check_false(protoMethod.call(this, o, n, f));
    return true;
  };

  request.get(function onComplete(error) {
    let response = this.response;

    do_check_eq(200, response.status);
    do_check_eq("Test", response.body);
    do_check_true(redirectRequested);
    do_check_true(resourceRequested);

    advance();
  });
});

add_test(function test_not_sending_cookie() {
  function handler(metadata, response) {
    let body = "COOKIE!";
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.bodyOutputStream.write(body, body.length);
    do_check_false(metadata.hasHeader("Cookie"));
  }
  let server = httpd_setup({"/test": handler});

  let cookieSer = Cc["@mozilla.org/cookieService;1"]
                    .getService(Ci.nsICookieService);
  let uri = CommonUtils.makeURI(server.baseURI);
  cookieSer.setCookieString(uri, null, "test=test; path=/;", null);

  let res = new RESTRequest(server.baseURI + "/test");
  res.get(function (error) {
    do_check_null(error);
    do_check_true(this.response.success);
    do_check_eq("COOKIE!", this.response.body);
    server.stop(run_next_test);
  });
});

add_test(function test_hawk_authenticated_request() {
  do_test_pending();

  let onProgressCalled = false;
  let postData = {your: "data"};

  // An arbitrary date - Feb 2, 1971.  It ends in a bunch of zeroes to make our
  // computation with the hawk timestamp easier, since hawk throws away the
  // millisecond values.
  let then = 34329600000;

  let clockSkew = 120000;
  let timeOffset = -1 * clockSkew;
  let localTime = then + clockSkew;

  // Set the accept-languages pref to the Nepalese dialect of Zulu.
  let acceptLanguage = Cc['@mozilla.org/supports-string;1'].createInstance(Ci.nsISupportsString);
  acceptLanguage.data = 'zu-NP'; // omit trailing ';', which our HTTP libs snip
  Services.prefs.setComplexValue('intl.accept_languages', Ci.nsISupportsString, acceptLanguage);

  let credentials = {
    id: "eyJleHBpcmVzIjogMTM2NTAxMDg5OC4x",
    key: "qTZf4ZFpAMpMoeSsX3zVRjiqmNs=",
    algorithm: "sha256"
  };

  let server = httpd_setup({
    "/elysium": function(request, response) {
      do_check_true(request.hasHeader("Authorization"));

      // check that the header timestamp is our arbitrary system date, not
      // today's date.  Note that hawk header timestamps are in seconds, not
      // milliseconds.
      let authorization = request.getHeader("Authorization");
      let tsMS = parseInt(/ts="(\d+)"/.exec(authorization)[1], 10) * 1000;
      do_check_eq(tsMS, then);

      // This testing can be a little wonky. In an environment where
      //   pref("intl.accept_languages") === 'en-US, en'
      // the header is sent as:
      //   'en-US,en;q=0.5'
      // hence our fake value for acceptLanguage.
      let lang = request.getHeader('Accept-Language');
      do_check_eq(lang, acceptLanguage);

      let message = "yay";
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.bodyOutputStream.write(message, message.length);
    }
  });

  function onProgress() {
    onProgressCalled = true;
  }

  function onComplete(error) {
    do_check_eq(200, this.response.status);
    do_check_eq(this.response.body, "yay");
    do_check_true(onProgressCalled);
    do_test_finished();
    server.stop(run_next_test);
  }

  let url = server.baseURI + "/elysium";
  let extra = {
    now: localTime,
    localtimeOffsetMsec: timeOffset
  };
  let request = new HAWKAuthenticatedRESTRequest(url, credentials, extra);
  request.post(postData, onComplete, onProgress);

  Services.prefs.resetUserPrefs();
  let pref = Services.prefs.getComplexValue('intl.accept_languages',
                                            Ci.nsIPrefLocalizedString);
  do_check_neq(acceptLanguage.data, pref.data);
});
