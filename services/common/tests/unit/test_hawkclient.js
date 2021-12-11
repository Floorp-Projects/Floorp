/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { HawkClient } = ChromeUtils.import(
  "resource://services-common/hawkclient.js"
);

const SECOND_MS = 1000;
const MINUTE_MS = SECOND_MS * 60;
const HOUR_MS = MINUTE_MS * 60;

const TEST_CREDS = {
  id: "eyJleHBpcmVzIjogMTM2NTAxMDg5OC4x",
  key: "qTZf4ZFpAMpMoeSsX3zVRjiqmNs=",
  algorithm: "sha256",
};

initTestLogging("Trace");

add_task(function test_now() {
  let client = new HawkClient("https://example.com");

  Assert.ok(client.now() - Date.now() < SECOND_MS);
});

add_task(function test_updateClockOffset() {
  let client = new HawkClient("https://example.com");

  let now = new Date();
  let serverDate = now.toUTCString();

  // Client's clock is off
  client.now = () => {
    return now.valueOf() + HOUR_MS;
  };

  client._updateClockOffset(serverDate);

  // Check that they're close; there will likely be a one-second rounding
  // error, so checking strict equality will likely fail.
  //
  // localtimeOffsetMsec is how many milliseconds to add to the local clock so
  // that it agrees with the server.  We are one hour ahead of the server, so
  // our offset should be -1 hour.
  Assert.ok(Math.abs(client.localtimeOffsetMsec + HOUR_MS) <= SECOND_MS);
});

add_task(async function test_authenticated_get_request() {
  let message = '{"msg": "Great Success!"}';
  let method = "GET";

  let server = httpd_setup({
    "/foo": (request, response) => {
      Assert.ok(request.hasHeader("Authorization"));

      response.setStatusLine(request.httpVersion, 200, "OK");
      response.bodyOutputStream.write(message, message.length);
    },
  });

  let client = new HawkClient(server.baseURI);

  let response = await client.request("/foo", method, TEST_CREDS);
  let result = JSON.parse(response.body);

  Assert.equal("Great Success!", result.msg);

  await promiseStopServer(server);
});

async function check_authenticated_request(method) {
  let server = httpd_setup({
    "/foo": (request, response) => {
      Assert.ok(request.hasHeader("Authorization"));

      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "application/json");
      response.bodyOutputStream.writeFrom(
        request.bodyInputStream,
        request.bodyInputStream.available()
      );
    },
  });

  let client = new HawkClient(server.baseURI);

  let response = await client.request("/foo", method, TEST_CREDS, {
    foo: "bar",
  });
  let result = JSON.parse(response.body);

  Assert.equal("bar", result.foo);

  await promiseStopServer(server);
}

add_task(async function test_authenticated_post_request() {
  await check_authenticated_request("POST");
});

add_task(async function test_authenticated_put_request() {
  await check_authenticated_request("PUT");
});

add_task(async function test_authenticated_patch_request() {
  await check_authenticated_request("PATCH");
});

add_task(async function test_extra_headers() {
  let server = httpd_setup({
    "/foo": (request, response) => {
      Assert.ok(request.hasHeader("Authorization"));
      Assert.ok(request.hasHeader("myHeader"));
      Assert.equal(request.getHeader("myHeader"), "fake");

      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "application/json");
      response.bodyOutputStream.writeFrom(
        request.bodyInputStream,
        request.bodyInputStream.available()
      );
    },
  });

  let client = new HawkClient(server.baseURI);

  let response = await client.request(
    "/foo",
    "POST",
    TEST_CREDS,
    { foo: "bar" },
    { myHeader: "fake" }
  );
  let result = JSON.parse(response.body);

  Assert.equal("bar", result.foo);

  await promiseStopServer(server);
});

add_task(async function test_credentials_optional() {
  let method = "GET";
  let server = httpd_setup({
    "/foo": (request, response) => {
      Assert.ok(!request.hasHeader("Authorization"));

      let message = JSON.stringify({ msg: "you're in the friend zone" });
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "application/json");
      response.bodyOutputStream.write(message, message.length);
    },
  });

  let client = new HawkClient(server.baseURI);
  let result = await client.request("/foo", method); // credentials undefined
  Assert.equal(JSON.parse(result.body).msg, "you're in the friend zone");

  await promiseStopServer(server);
});

add_task(async function test_server_error() {
  let message = "Ohai!";
  let method = "GET";

  let server = httpd_setup({
    "/foo": (request, response) => {
      response.setStatusLine(request.httpVersion, 418, "I am a Teapot");
      response.bodyOutputStream.write(message, message.length);
    },
  });

  let client = new HawkClient(server.baseURI);

  try {
    await client.request("/foo", method, TEST_CREDS);
    do_throw("Expected an error");
  } catch (err) {
    Assert.equal(418, err.code);
    Assert.equal("I am a Teapot", err.message);
  }

  await promiseStopServer(server);
});

add_task(async function test_server_error_json() {
  let message = JSON.stringify({ error: "Cannot get ye flask." });
  let method = "GET";

  let server = httpd_setup({
    "/foo": (request, response) => {
      response.setStatusLine(
        request.httpVersion,
        400,
        "What wouldst thou deau?"
      );
      response.bodyOutputStream.write(message, message.length);
    },
  });

  let client = new HawkClient(server.baseURI);

  try {
    await client.request("/foo", method, TEST_CREDS);
    do_throw("Expected an error");
  } catch (err) {
    Assert.equal("Cannot get ye flask.", err.error);
  }

  await promiseStopServer(server);
});

add_task(async function test_offset_after_request() {
  let message = "Ohai!";
  let method = "GET";

  let server = httpd_setup({
    "/foo": (request, response) => {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.bodyOutputStream.write(message, message.length);
    },
  });

  let client = new HawkClient(server.baseURI);
  let now = Date.now();
  client.now = () => {
    return now + HOUR_MS;
  };

  Assert.equal(client.localtimeOffsetMsec, 0);

  await client.request("/foo", method, TEST_CREDS);
  // Should be about an hour off
  Assert.ok(Math.abs(client.localtimeOffsetMsec + HOUR_MS) < SECOND_MS);

  await promiseStopServer(server);
});

add_task(async function test_offset_in_hawk_header() {
  let message = "Ohai!";
  let method = "GET";

  let server = httpd_setup({
    "/first": function(request, response) {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.bodyOutputStream.write(message, message.length);
    },

    "/second": function(request, response) {
      // We see a better date now in the ts component of the header
      let delta = getTimestampDelta(request.getHeader("Authorization"));

      // We're now within HAWK's one-minute window.
      // I hope this isn't a recipe for intermittent oranges ...
      if (delta < MINUTE_MS) {
        response.setStatusLine(request.httpVersion, 200, "OK");
      } else {
        response.setStatusLine(request.httpVersion, 400, "Delta: " + delta);
      }
      response.bodyOutputStream.write(message, message.length);
    },
  });

  let client = new HawkClient(server.baseURI);

  client.now = () => {
    return Date.now() + 12 * HOUR_MS;
  };

  // We begin with no offset
  Assert.equal(client.localtimeOffsetMsec, 0);
  await client.request("/first", method, TEST_CREDS);

  // After the first server response, our offset is updated to -12 hours.
  // We should be safely in the window, now.
  Assert.ok(Math.abs(client.localtimeOffsetMsec + 12 * HOUR_MS) < MINUTE_MS);
  await client.request("/second", method, TEST_CREDS);

  await promiseStopServer(server);
});

add_task(async function test_2xx_success() {
  // Just to ensure that we're not biased toward 200 OK for success
  let credentials = {
    id: "eyJleHBpcmVzIjogMTM2NTAxMDg5OC4x",
    key: "qTZf4ZFpAMpMoeSsX3zVRjiqmNs=",
    algorithm: "sha256",
  };
  let method = "GET";

  let server = httpd_setup({
    "/foo": (request, response) => {
      response.setStatusLine(request.httpVersion, 202, "Accepted");
    },
  });

  let client = new HawkClient(server.baseURI);

  let response = await client.request("/foo", method, credentials);

  // Shouldn't be any content in a 202
  Assert.equal(response.body, "");

  await promiseStopServer(server);
});

add_task(async function test_retry_request_on_fail() {
  let attempts = 0;
  let credentials = {
    id: "eyJleHBpcmVzIjogMTM2NTAxMDg5OC4x",
    key: "qTZf4ZFpAMpMoeSsX3zVRjiqmNs=",
    algorithm: "sha256",
  };
  let method = "GET";

  let server = httpd_setup({
    "/maybe": function(request, response) {
      // This path should be hit exactly twice; once with a bad timestamp, and
      // again when the client retries the request with a corrected timestamp.
      attempts += 1;
      Assert.ok(attempts <= 2);

      let delta = getTimestampDelta(request.getHeader("Authorization"));

      // First time through, we should have a bad timestamp
      if (attempts === 1) {
        Assert.ok(delta > MINUTE_MS);
        let message = "never!!!";
        response.setStatusLine(request.httpVersion, 401, "Unauthorized");
        response.bodyOutputStream.write(message, message.length);
        return;
      }

      // Second time through, timestamp should be corrected by client
      Assert.ok(delta < MINUTE_MS);
      let message = "i love you!!!";
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.bodyOutputStream.write(message, message.length);
    },
  });

  let client = new HawkClient(server.baseURI);

  client.now = () => {
    return Date.now() + 12 * HOUR_MS;
  };

  // We begin with no offset
  Assert.equal(client.localtimeOffsetMsec, 0);

  // Request will have bad timestamp; client will retry once
  let response = await client.request("/maybe", method, credentials);
  Assert.equal(response.body, "i love you!!!");

  await promiseStopServer(server);
});

add_task(async function test_multiple_401_retry_once() {
  // Like test_retry_request_on_fail, but always return a 401
  // and ensure that the client only retries once.
  let attempts = 0;
  let credentials = {
    id: "eyJleHBpcmVzIjogMTM2NTAxMDg5OC4x",
    key: "qTZf4ZFpAMpMoeSsX3zVRjiqmNs=",
    algorithm: "sha256",
  };
  let method = "GET";

  let server = httpd_setup({
    "/maybe": function(request, response) {
      // This path should be hit exactly twice; once with a bad timestamp, and
      // again when the client retries the request with a corrected timestamp.
      attempts += 1;

      Assert.ok(attempts <= 2);

      let message = "never!!!";
      response.setStatusLine(request.httpVersion, 401, "Unauthorized");
      response.bodyOutputStream.write(message, message.length);
    },
  });

  let client = new HawkClient(server.baseURI);

  client.now = () => {
    return Date.now() - 12 * HOUR_MS;
  };

  // We begin with no offset
  Assert.equal(client.localtimeOffsetMsec, 0);

  // Request will have bad timestamp; client will retry once
  try {
    await client.request("/maybe", method, credentials);
    do_throw("Expected an error");
  } catch (err) {
    Assert.equal(err.code, 401);
  }
  Assert.equal(attempts, 2);

  await promiseStopServer(server);
});

add_task(async function test_500_no_retry() {
  // If we get a 500 error, the client should not retry (as it would with a
  // 401)
  let credentials = {
    id: "eyJleHBpcmVzIjogMTM2NTAxMDg5OC4x",
    key: "qTZf4ZFpAMpMoeSsX3zVRjiqmNs=",
    algorithm: "sha256",
  };
  let method = "GET";

  let server = httpd_setup({
    "/no-shutup": function(request, response) {
      let message = "Cannot get ye flask.";
      response.setStatusLine(request.httpVersion, 500, "Internal server error");
      response.bodyOutputStream.write(message, message.length);
    },
  });

  let client = new HawkClient(server.baseURI);

  // Throw off the clock so the HawkClient would want to retry the request if
  // it could
  client.now = () => {
    return Date.now() - 12 * HOUR_MS;
  };

  // Request will 500; no retries
  try {
    await client.request("/no-shutup", method, credentials);
    do_throw("Expected an error");
  } catch (err) {
    Assert.equal(err.code, 500);
  }

  await promiseStopServer(server);
});

add_task(async function test_401_then_500() {
  // Like test_multiple_401_retry_once, but return a 500 to the
  // second request, ensuring that the promise is properly rejected
  // in client.request.
  let attempts = 0;
  let credentials = {
    id: "eyJleHBpcmVzIjogMTM2NTAxMDg5OC4x",
    key: "qTZf4ZFpAMpMoeSsX3zVRjiqmNs=",
    algorithm: "sha256",
  };
  let method = "GET";

  let server = httpd_setup({
    "/maybe": function(request, response) {
      // This path should be hit exactly twice; once with a bad timestamp, and
      // again when the client retries the request with a corrected timestamp.
      attempts += 1;
      Assert.ok(attempts <= 2);

      let delta = getTimestampDelta(request.getHeader("Authorization"));

      // First time through, we should have a bad timestamp
      // Client will retry
      if (attempts === 1) {
        Assert.ok(delta > MINUTE_MS);
        let message = "never!!!";
        response.setStatusLine(request.httpVersion, 401, "Unauthorized");
        response.bodyOutputStream.write(message, message.length);
        return;
      }

      // Second time through, timestamp should be corrected by client
      // And fail on the client
      Assert.ok(delta < MINUTE_MS);
      let message = "Cannot get ye flask.";
      response.setStatusLine(request.httpVersion, 500, "Internal server error");
      response.bodyOutputStream.write(message, message.length);
    },
  });

  let client = new HawkClient(server.baseURI);

  client.now = () => {
    return Date.now() - 12 * HOUR_MS;
  };

  // We begin with no offset
  Assert.equal(client.localtimeOffsetMsec, 0);

  // Request will have bad timestamp; client will retry once
  try {
    await client.request("/maybe", method, credentials);
  } catch (err) {
    Assert.equal(err.code, 500);
  }
  Assert.equal(attempts, 2);

  await promiseStopServer(server);
});

// End of tests.
// Utility functions follow

function getTimestampDelta(authHeader, now = Date.now()) {
  let tsMS = new Date(
    parseInt(/ts="(\d+)"/.exec(authHeader)[1], 10) * SECOND_MS
  );
  return Math.abs(tsMS - now);
}

function run_test() {
  initTestLogging("Trace");
  run_next_test();
}
