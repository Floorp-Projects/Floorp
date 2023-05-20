/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {
  TokenServerClient,
  TokenServerClientError,
  TokenServerClientServerError,
} = ChromeUtils.importESModule(
  "resource://services-common/tokenserverclient.sys.mjs"
);

initTestLogging("Trace");

add_task(async function test_working_token_exchange() {
  _("Ensure that working OAuth token exchange works as expected.");

  let service = "http://example.com/foo";
  let duration = 300;

  let server = httpd_setup({
    "/1.0/foo/1.0": function (request, response) {
      Assert.ok(request.hasHeader("accept"));
      Assert.equal("application/json", request.getHeader("accept"));

      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "application/json");

      let body = JSON.stringify({
        id: "id",
        key: "key",
        api_endpoint: service,
        uid: "uid",
        duration,
      });
      response.bodyOutputStream.write(body, body.length);
    },
  });

  let client = new TokenServerClient();
  let url = server.baseURI + "/1.0/foo/1.0";
  let result = await client.getTokenUsingOAuth(url, "access_token");
  Assert.equal("object", typeof result);
  do_check_attribute_count(result, 7);
  Assert.equal(service, result.endpoint);
  Assert.equal("id", result.id);
  Assert.equal("key", result.key);
  Assert.equal("uid", result.uid);
  Assert.equal(duration, result.duration);
  Assert.deepEqual(undefined, result.node_type);
  await promiseStopServer(server);
});

add_task(async function test_working_token_exchange_with_nodetype() {
  _("Ensure that a token response with a node type as expected.");

  let service = "http://example.com/foo";
  let duration = 300;
  let nodeType = "the-node-type";

  let server = httpd_setup({
    "/1.0/foo/1.0": function (request, response) {
      Assert.ok(request.hasHeader("accept"));
      Assert.equal("application/json", request.getHeader("accept"));

      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "application/json");

      let body = JSON.stringify({
        id: "id",
        key: "key",
        api_endpoint: service,
        uid: "uid",
        duration,
        node_type: nodeType,
      });
      response.bodyOutputStream.write(body, body.length);
    },
  });

  let client = new TokenServerClient();
  let url = server.baseURI + "/1.0/foo/1.0";
  let result = await client.getTokenUsingOAuth(url, "access_token");
  Assert.equal("object", typeof result);
  do_check_attribute_count(result, 7);
  Assert.equal(service, result.endpoint);
  Assert.equal("id", result.id);
  Assert.equal("key", result.key);
  Assert.equal("uid", result.uid);
  Assert.equal(duration, result.duration);
  Assert.equal(nodeType, result.node_type);
  await promiseStopServer(server);
});

add_task(async function test_invalid_arguments() {
  _("Ensure invalid arguments to APIs are rejected.");

  let args = [
    [null, "access_token"],
    ["http://example.com/", null],
  ];

  for (let arg of args) {
    let client = new TokenServerClient();
    await Assert.rejects(client.getTokenUsingOAuth(arg[0], arg[1]), ex => {
      Assert.ok(ex instanceof TokenServerClientError);
      return true;
    });
  }
});

add_task(async function test_invalid_403_no_content_type() {
  _("Ensure that a 403 without content-type is handled properly.");

  let server = httpd_setup({
    "/1.0/foo/1.0": function (request, response) {
      response.setStatusLine(request.httpVersion, 403, "Forbidden");
      // No Content-Type header by design.

      let body = JSON.stringify({
        errors: [{ description: "irrelevant", location: "body", name: "" }],
        urls: { foo: "http://bar" },
      });
      response.bodyOutputStream.write(body, body.length);
    },
  });

  let client = new TokenServerClient();
  let url = server.baseURI + "/1.0/foo/1.0";

  await Assert.rejects(
    client.getTokenUsingOAuth(url, "access_token"),
    error => {
      Assert.ok(error instanceof TokenServerClientServerError);
      Assert.equal(error.cause, "malformed-response");

      Assert.equal(null, error.urls);
      return true;
    }
  );

  await promiseStopServer(server);
});

add_task(async function test_send_extra_headers() {
  _("Ensures that the condition acceptance header is sent when asked.");

  let duration = 300;
  let server = httpd_setup({
    "/1.0/foo/1.0": function (request, response) {
      Assert.ok(request.hasHeader("x-foo"));
      Assert.equal(request.getHeader("x-foo"), "42");

      Assert.ok(request.hasHeader("x-bar"));
      Assert.equal(request.getHeader("x-bar"), "17");

      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "application/json");

      let body = JSON.stringify({
        id: "id",
        key: "key",
        api_endpoint: "http://example.com/",
        uid: "uid",
        duration,
      });
      response.bodyOutputStream.write(body, body.length);
    },
  });

  let client = new TokenServerClient();
  let url = server.baseURI + "/1.0/foo/1.0";

  let extra = {
    "X-Foo": 42,
    "X-Bar": 17,
  };

  await client.getTokenUsingOAuth(url, "access_token", extra);
  // Other tests validate other things.

  await promiseStopServer(server);
});

add_task(async function test_error_404_empty() {
  _("Ensure that 404 responses without proper response are handled properly.");

  let server = httpd_setup();

  let client = new TokenServerClient();
  let url = server.baseURI + "/foo";

  await Assert.rejects(
    client.getTokenUsingOAuth(url, "access_token"),
    error => {
      Assert.ok(error instanceof TokenServerClientServerError);
      Assert.equal(error.cause, "malformed-response");

      Assert.notEqual(null, error.response);
      return true;
    }
  );

  await promiseStopServer(server);
});

add_task(async function test_error_404_proper_response() {
  _("Ensure that a Cornice error report for 404 is handled properly.");

  let server = httpd_setup({
    "/1.0/foo/1.0": function (request, response) {
      response.setStatusLine(request.httpVersion, 404, "Not Found");
      response.setHeader("Content-Type", "application/json; charset=utf-8");

      let body = JSON.stringify({
        status: 404,
        errors: [{ description: "No service", location: "body", name: "" }],
      });

      response.bodyOutputStream.write(body, body.length);
    },
  });

  let client = new TokenServerClient();
  let url = server.baseURI + "/1.0/foo/1.0";

  await Assert.rejects(
    client.getTokenUsingOAuth(url, "access_token"),
    error => {
      Assert.ok(error instanceof TokenServerClientServerError);
      Assert.equal(error.cause, "unknown-service");
      return true;
    }
  );

  await promiseStopServer(server);
});

add_task(async function test_bad_json() {
  _("Ensure that malformed JSON is handled properly.");

  let server = httpd_setup({
    "/1.0/foo/1.0": function (request, response) {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "application/json");

      let body = '{"id": "id", baz}';
      response.bodyOutputStream.write(body, body.length);
    },
  });

  let client = new TokenServerClient();
  let url = server.baseURI + "/1.0/foo/1.0";

  await Assert.rejects(
    client.getTokenUsingOAuth(url, "access_token"),
    error => {
      Assert.notEqual(null, error);
      Assert.equal("TokenServerClientServerError", error.name);
      Assert.equal(error.cause, "malformed-response");
      Assert.notEqual(null, error.response);
      return true;
    }
  );

  await promiseStopServer(server);
});

add_task(async function test_400_response() {
  _("Ensure HTTP 400 is converted to malformed-request.");

  let server = httpd_setup({
    "/1.0/foo/1.0": function (request, response) {
      response.setStatusLine(request.httpVersion, 400, "Bad Request");
      response.setHeader("Content-Type", "application/json; charset=utf-8");

      let body = "{}"; // Actual content may not be used.
      response.bodyOutputStream.write(body, body.length);
    },
  });

  let client = new TokenServerClient();
  let url = server.baseURI + "/1.0/foo/1.0";

  await Assert.rejects(
    client.getTokenUsingOAuth(url, "access_token"),
    error => {
      Assert.notEqual(null, error);
      Assert.equal("TokenServerClientServerError", error.name);
      Assert.notEqual(null, error.response);
      Assert.equal(error.cause, "malformed-request");
      return true;
    }
  );

  await promiseStopServer(server);
});

add_task(async function test_401_with_error_cause() {
  _("Ensure 401 cause is specified in body.status");

  let server = httpd_setup({
    "/1.0/foo/1.0": function (request, response) {
      response.setStatusLine(request.httpVersion, 401, "Unauthorized");
      response.setHeader("Content-Type", "application/json; charset=utf-8");

      let body = JSON.stringify({ status: "no-soup-for-you" });
      response.bodyOutputStream.write(body, body.length);
    },
  });

  let client = new TokenServerClient();
  let url = server.baseURI + "/1.0/foo/1.0";

  await Assert.rejects(
    client.getTokenUsingOAuth(url, "access_token"),
    error => {
      Assert.notEqual(null, error);
      Assert.equal("TokenServerClientServerError", error.name);
      Assert.notEqual(null, error.response);
      Assert.equal(error.cause, "no-soup-for-you");
      return true;
    }
  );

  await promiseStopServer(server);
});

add_task(async function test_unhandled_media_type() {
  _("Ensure that unhandled media types throw an error.");

  let server = httpd_setup({
    "/1.0/foo/1.0": function (request, response) {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "text/plain");

      let body = "hello, world";
      response.bodyOutputStream.write(body, body.length);
    },
  });

  let url = server.baseURI + "/1.0/foo/1.0";
  let client = new TokenServerClient();

  await Assert.rejects(
    client.getTokenUsingOAuth(url, "access_token"),
    error => {
      Assert.notEqual(null, error);
      Assert.equal("TokenServerClientServerError", error.name);
      Assert.notEqual(null, error.response);
      return true;
    }
  );

  await promiseStopServer(server);
});

add_task(async function test_rich_media_types() {
  _("Ensure that extra tokens in the media type aren't rejected.");

  let duration = 300;
  let server = httpd_setup({
    "/foo": function (request, response) {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "application/json; foo=bar; bar=foo");

      let body = JSON.stringify({
        id: "id",
        key: "key",
        api_endpoint: "foo",
        uid: "uid",
        duration,
      });
      response.bodyOutputStream.write(body, body.length);
    },
  });

  let url = server.baseURI + "/foo";
  let client = new TokenServerClient();

  await client.getTokenUsingOAuth(url, "access_token");
  await promiseStopServer(server);
});
