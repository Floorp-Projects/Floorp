/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/async.js");
Cu.import("resource://services-common/tokenserverclient.js");

function run_test() {
  initTestLogging("Trace");

  run_next_test();
}

add_test(function test_working_bid_exchange() {
  _("Ensure that working BrowserID token exchange works as expected.");

  let service = "http://example.com/foo";

  let server = httpd_setup({
    "/1.0/foo/1.0": function(request, response) {
      do_check_true(request.hasHeader("accept"));
      do_check_false(request.hasHeader("x-conditions-accepted"));
      do_check_eq("application/json", request.getHeader("accept"));

      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "application/json");

      let body = JSON.stringify({
        id:           "id",
        key:          "key",
        api_endpoint: service,
        uid:          "uid",
      });
      response.bodyOutputStream.write(body, body.length);
    }
  });

  let client = new TokenServerClient();
  let cb = Async.makeSpinningCallback();
  let url = server.baseURI + "/1.0/foo/1.0";
  client.getTokenFromBrowserIDAssertion(url, "assertion", cb);
  let result = cb.wait();
  do_check_eq("object", typeof(result));
  do_check_attribute_count(result, 4);
  do_check_eq(service, result.endpoint);
  do_check_eq("id", result.id);
  do_check_eq("key", result.key);
  do_check_eq("uid", result.uid);

  server.stop(run_next_test);
});

add_test(function test_invalid_arguments() {
  _("Ensure invalid arguments to APIs are rejected.");

  let args = [
    [null, "assertion", function() {}],
    ["http://example.com/", null, function() {}],
    ["http://example.com/", "assertion", null]
  ];

  for each (let arg in args) {
    try {
      let client = new TokenServerClient();
      client.getTokenFromBrowserIDAssertion(arg[0], arg[1], arg[2]);
      do_throw("Should never get here.");
    } catch (ex) {
      do_check_true(ex instanceof TokenServerClientError);
    }
  }

  run_next_test();
});

add_test(function test_conditions_required_response_handling() {
  _("Ensure that a conditions required response is handled properly.");

  let description = "Need to accept conditions";
  let tosURL = "http://example.com/tos";

  let server = httpd_setup({
    "/1.0/foo/1.0": function(request, response) {
      do_check_false(request.hasHeader("x-conditions-accepted"));

      response.setStatusLine(request.httpVersion, 403, "Forbidden");
      response.setHeader("Content-Type", "application/json");

      let body = JSON.stringify({
        errors: [{description: description, location: "body", name: ""}],
        urls: {tos: tosURL}
      });
      response.bodyOutputStream.write(body, body.length);
    }
  });

  let client = new TokenServerClient();
  let url = server.baseURI + "/1.0/foo/1.0";

  function onResponse(error, token) {
    do_check_true(error instanceof TokenServerClientServerError);
    do_check_eq(error.cause, "conditions-required");
    do_check_null(token);

    do_check_eq(error.urls.tos, tosURL);

    server.stop(run_next_test);
  }

  client.getTokenFromBrowserIDAssertion(url, "assertion", onResponse);
});

add_test(function test_invalid_403_no_content_type() {
  _("Ensure that a 403 without content-type is handled properly.");

  let server = httpd_setup({
    "/1.0/foo/1.0": function(request, response) {
      response.setStatusLine(request.httpVersion, 403, "Forbidden");
      // No Content-Type header by design.

      let body = JSON.stringify({
        errors: [{description: "irrelevant", location: "body", name: ""}],
        urls: {foo: "http://bar"}
      });
      response.bodyOutputStream.write(body, body.length);
    }
  });

  let client = new TokenServerClient();
  let url = server.baseURI + "/1.0/foo/1.0";

  function onResponse(error, token) {
    do_check_true(error instanceof TokenServerClientServerError);
    do_check_eq(error.cause, "malformed-response");
    do_check_null(token);

    do_check_null(error.urls);

    server.stop(run_next_test);
  }

  client.getTokenFromBrowserIDAssertion(url, "assertion", onResponse);
});

add_test(function test_invalid_403_bad_json() {
  _("Ensure that a 403 with JSON that isn't proper is handled properly.");

  let server = httpd_setup({
    "/1.0/foo/1.0": function(request, response) {
      response.setStatusLine(request.httpVersion, 403, "Forbidden");
      response.setHeader("Content-Type", "application/json; charset=utf-8");

      let body = JSON.stringify({
        foo: "bar"
      });
      response.bodyOutputStream.write(body, body.length);
    }
  });

  let client = new TokenServerClient();
  let url = server.baseURI + "/1.0/foo/1.0";

  function onResponse(error, token) {
    do_check_true(error instanceof TokenServerClientServerError);
    do_check_eq(error.cause, "malformed-response");
    do_check_null(token);
    do_check_null(error.urls);

    server.stop(run_next_test);
  }

  client.getTokenFromBrowserIDAssertion(url, "assertion", onResponse);
});

add_test(function test_403_no_urls() {
  _("Ensure that a 403 without a urls field is handled properly.");

  let server = httpd_setup({
    "/1.0/foo/1.0": function(request, response) {
      response.setStatusLine(request.httpVersion, 403, "Forbidden");
      response.setHeader("Content-Type", "application/json; charset=utf-8");

      let body = "{}";
      response.bodyOutputStream.write(body, body.length);
    }
  });

  let client = new TokenServerClient();
  let url = server.baseURI + "/1.0/foo/1.0";

  client.getTokenFromBrowserIDAssertion(url, "assertion",
                                        function onResponse(error, result) {
    do_check_true(error instanceof TokenServerClientServerError);
    do_check_eq(error.cause, "malformed-response");
    do_check_null(result);

    server.stop(run_next_test);

  });
});

add_test(function test_send_conditions_accepted() {
  _("Ensures that the condition acceptance header is sent when asked.");

  let server = httpd_setup({
    "/1.0/foo/1.0": function(request, response) {
      do_check_true(request.hasHeader("x-conditions-accepted"));
      do_check_eq(request.getHeader("x-conditions-accepted"), "1");

      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "application/json");

      let body = JSON.stringify({
        id:           "id",
        key:          "key",
        api_endpoint: "http://example.com/",
        uid:          "uid",
      });
      response.bodyOutputStream.write(body, body.length);
    }
  });

  let client = new TokenServerClient();
  let url = server.baseURI + "/1.0/foo/1.0";

  function onResponse(error, token) {
    do_check_null(error);

    // Other tests validate other things.

    server.stop(run_next_test);
  }

  client.getTokenFromBrowserIDAssertion(url, "assertion", onResponse, true);
});

add_test(function test_error_404_empty() {
  _("Ensure that 404 responses without proper response are handled properly.");

  let server = httpd_setup();

  let client = new TokenServerClient();
  let url = server.baseURI + "/foo";
  client.getTokenFromBrowserIDAssertion(url, "assertion", function(error, r) {
    do_check_true(error instanceof TokenServerClientServerError);
    do_check_eq(error.cause, "malformed-response");

    do_check_neq(null, error.response);
    do_check_null(r);

    server.stop(run_next_test);
  });
});

add_test(function test_error_404_proper_response() {
  _("Ensure that a Cornice error report for 404 is handled properly.");

  let server = httpd_setup({
    "/1.0/foo/1.0": function(request, response) {
      response.setStatusLine(request.httpVersion, 404, "Not Found");
      response.setHeader("Content-Type", "application/json; charset=utf-8");

      let body = JSON.stringify({
        status: 404,
        errors: [{description: "No service", location: "body", name: ""}],
      });

      response.bodyOutputStream.write(body, body.length);
    }
  });

  function onResponse(error, token) {
    do_check_true(error instanceof TokenServerClientServerError);
    do_check_eq(error.cause, "unknown-service");
    do_check_null(token);

    server.stop(run_next_test);
  }

  let client = new TokenServerClient();
  let url = server.baseURI + "/1.0/foo/1.0";
  client.getTokenFromBrowserIDAssertion(url, "assertion", onResponse);
});

add_test(function test_bad_json() {
  _("Ensure that malformed JSON is handled properly.");

  let server = httpd_setup({
    "/1.0/foo/1.0": function(request, response) {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "application/json");

      let body = '{"id": "id", baz}'
      response.bodyOutputStream.write(body, body.length);
    }
  });

  let client = new TokenServerClient();
  let url = server.baseURI + "/1.0/foo/1.0";
  client.getTokenFromBrowserIDAssertion(url, "assertion", function(error, r) {
    do_check_neq(null, error);
    do_check_eq("TokenServerClientServerError", error.name);
    do_check_eq(error.cause, "malformed-response");
    do_check_neq(null, error.response);
    do_check_eq(null, r);

    server.stop(run_next_test);
  });
});

add_test(function test_400_response() {
  _("Ensure HTTP 400 is converted to malformed-request.");

  let server = httpd_setup({
    "/1.0/foo/1.0": function(request, response) {
      response.setStatusLine(request.httpVersion, 400, "Bad Request");
      response.setHeader("Content-Type", "application/json; charset=utf-8");

      let body = "{}"; // Actual content may not be used.
      response.bodyOutputStream.write(body, body.length);
    }
  });

  let client = new TokenServerClient();
  let url = server.baseURI + "/1.0/foo/1.0";
  client.getTokenFromBrowserIDAssertion(url, "assertion", function(error, r) {
    do_check_neq(null, error);
    do_check_eq("TokenServerClientServerError", error.name);
    do_check_neq(null, error.response);
    do_check_eq(error.cause, "malformed-request");

    server.stop(run_next_test);
  });
});

add_test(function test_unhandled_media_type() {
  _("Ensure that unhandled media types throw an error.");

  let server = httpd_setup({
    "/1.0/foo/1.0": function(request, response) {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "text/plain");

      let body = "hello, world";
      response.bodyOutputStream.write(body, body.length);
    }
  });

  let url = server.baseURI + "/1.0/foo/1.0";
  let client = new TokenServerClient();
  client.getTokenFromBrowserIDAssertion(url, "assertion", function(error, r) {
    do_check_neq(null, error);
    do_check_eq("TokenServerClientServerError", error.name);
    do_check_neq(null, error.response);
    do_check_eq(null, r);

    server.stop(run_next_test);
  });
});

add_test(function test_rich_media_types() {
  _("Ensure that extra tokens in the media type aren't rejected.");

  let server = httpd_setup({
    "/foo": function(request, response) {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "application/json; foo=bar; bar=foo");

      let body = JSON.stringify({
        id:           "id",
        key:          "key",
        api_endpoint: "foo",
        uid:          "uid",
      });
      response.bodyOutputStream.write(body, body.length);
    }
  });

  let url = server.baseURI + "/foo";
  let client = new TokenServerClient();
  client.getTokenFromBrowserIDAssertion(url, "assertion", function(error, r) {
    do_check_eq(null, error);

    server.stop(run_next_test);
  });
});

add_test(function test_exception_during_callback() {
  _("Ensure that exceptions thrown during callback handling are handled.");

  let server = httpd_setup({
    "/foo": function(request, response) {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "application/json");

      let body = JSON.stringify({
        id:           "id",
        key:          "key",
        api_endpoint: "foo",
        uid:          "uid",
      });
      response.bodyOutputStream.write(body, body.length);
    }
  });

  let url = server.baseURI + "/foo";
  let client = new TokenServerClient();
  let cb = Async.makeSpinningCallback();
  let callbackCount = 0;

  client.getTokenFromBrowserIDAssertion(url, "assertion", function(error, r) {
    do_check_eq(null, error);

    cb();

    callbackCount += 1;
    throw new Error("I am a bad function!");
  });

  cb.wait();
  // This relies on some heavy event loop magic. The error in the main
  // callback should already have been raised at this point.
  do_check_eq(callbackCount, 1);

  server.stop(run_next_test);
});
