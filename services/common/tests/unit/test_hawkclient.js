/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://services-common/hawkclient.js");

const SECOND_MS = 1000;
const MINUTE_MS = SECOND_MS * 60;
const HOUR_MS = MINUTE_MS * 60;

const TEST_CREDS = {
  id: "eyJleHBpcmVzIjogMTM2NTAxMDg5OC4x",
  key: "qTZf4ZFpAMpMoeSsX3zVRjiqmNs=",
  algorithm: "sha256"
};

initTestLogging("Trace");

add_task(function test_now() {
  let client = new HawkClient("https://example.com");

  do_check_true(client.now() - Date.now() < SECOND_MS);
});

add_task(function test_updateClockOffset() {
  let client = new HawkClient("https://example.com");

  let now = new Date();
  let serverDate = now.toUTCString();

  // Client's clock is off
  client.now = () => { return now.valueOf() + HOUR_MS; }

  client._updateClockOffset(serverDate);

  // Check that they're close; there will likely be a one-second rounding
  // error, so checking strict equality will likely fail.
  //
  // localtimeOffsetMsec is how many milliseconds to add to the local clock so
  // that it agrees with the server.  We are one hour ahead of the server, so
  // our offset should be -1 hour.
  do_check_true(Math.abs(client.localtimeOffsetMsec + HOUR_MS) <= SECOND_MS);
});

add_task(function test_authenticated_get_request() {
  let message = "{\"msg\": \"Great Success!\"}";
  let method = "GET";

  let server = httpd_setup({"/foo": (request, response) => {
      do_check_true(request.hasHeader("Authorization"));

      response.setStatusLine(request.httpVersion, 200, "OK");
      response.bodyOutputStream.write(message, message.length);
    }
  });

  let client = new HawkClient(server.baseURI);

  let response = yield client.request("/foo", method, TEST_CREDS);
  let result = JSON.parse(response);

  do_check_eq("Great Success!", result.msg);

  yield deferredStop(server);
});

add_task(function test_authenticated_post_request() {
  let method = "POST";

  let server = httpd_setup({"/foo": (request, response) => {
      do_check_true(request.hasHeader("Authorization"));

      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "application/json");
      response.bodyOutputStream.writeFrom(request.bodyInputStream, request.bodyInputStream.available());
    }
  });

  let client = new HawkClient(server.baseURI);

  let response = yield client.request("/foo", method, TEST_CREDS, {foo: "bar"});
  let result = JSON.parse(response);

  do_check_eq("bar", result.foo);

  yield deferredStop(server);
});

add_task(function test_credentials_optional() {
  let method = "GET";
  let server = httpd_setup({
    "/foo": (request, response) => {
      do_check_false(request.hasHeader("Authorization"));

      let message = JSON.stringify({msg: "you're in the friend zone"});
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "application/json");
      response.bodyOutputStream.write(message, message.length);
    }
  });

  let client = new HawkClient(server.baseURI);
  let result = yield client.request("/foo", method); // credentials undefined
  do_check_eq(JSON.parse(result).msg, "you're in the friend zone");

  yield deferredStop(server);
});

add_task(function test_server_error() {
  let message = "Ohai!";
  let method = "GET";

  let server = httpd_setup({"/foo": (request, response) => {
      response.setStatusLine(request.httpVersion, 418, "I am a Teapot");
      response.bodyOutputStream.write(message, message.length);
    }
  });

  let client = new HawkClient(server.baseURI);

  try {
    yield client.request("/foo", method, TEST_CREDS);
    do_throw("Expected an error");
  } catch(err) {
    do_check_eq(418, err.code);
    do_check_eq("I am a Teapot", err.message);
  }

  yield deferredStop(server);
});

add_task(function test_server_error_json() {
  let message = JSON.stringify({error: "Cannot get ye flask."});
  let method = "GET";

  let server = httpd_setup({"/foo": (request, response) => {
      response.setStatusLine(request.httpVersion, 400, "What wouldst thou deau?");
      response.bodyOutputStream.write(message, message.length);
    }
  });

  let client = new HawkClient(server.baseURI);

  try {
    yield client.request("/foo", method, TEST_CREDS);
    do_throw("Expected an error");
  } catch(err) {
    do_check_eq("Cannot get ye flask.", err.error);
  }

  yield deferredStop(server);
});

add_task(function test_offset_after_request() {
  let message = "Ohai!";
  let method = "GET";

  let server = httpd_setup({"/foo": (request, response) => {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.bodyOutputStream.write(message, message.length);
    }
  });

  let client = new HawkClient(server.baseURI);
  let now = Date.now();
  client.now = () => { return now + HOUR_MS; };

  do_check_eq(client.localtimeOffsetMsec, 0);

  let response = yield client.request("/foo", method, TEST_CREDS);
  // Should be about an hour off
  do_check_true(Math.abs(client.localtimeOffsetMsec + HOUR_MS) < SECOND_MS);

  yield deferredStop(server);
});

add_task(function test_offset_in_hawk_header() {
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
      let message = "Delta: " + delta;

      // We're now within HAWK's one-minute window.
      // I hope this isn't a recipe for intermittent oranges ...
      if (delta < MINUTE_MS) {
        response.setStatusLine(request.httpVersion, 200, "OK");
      } else {
        response.setStatusLine(request.httpVersion, 400, "Delta: " + delta);
      }
      response.bodyOutputStream.write(message, message.length);
    }
  });

  let client = new HawkClient(server.baseURI);
  function getOffset() {
    return client.localtimeOffsetMsec;
  }

  client.now = () => {
    return Date.now() + 12 * HOUR_MS;
  };

  // We begin with no offset
  do_check_eq(client.localtimeOffsetMsec, 0);
  yield client.request("/first", method, TEST_CREDS);

  // After the first server response, our offset is updated to -12 hours.
  // We should be safely in the window, now.
  do_check_true(Math.abs(client.localtimeOffsetMsec + 12 * HOUR_MS) < MINUTE_MS);
  yield client.request("/second", method, TEST_CREDS);

  yield deferredStop(server);
});

add_task(function test_2xx_success() {
  // Just to ensure that we're not biased toward 200 OK for success
  let credentials = {
    id: "eyJleHBpcmVzIjogMTM2NTAxMDg5OC4x",
    key: "qTZf4ZFpAMpMoeSsX3zVRjiqmNs=",
    algorithm: "sha256"
  };
  let method = "GET";

  let server = httpd_setup({"/foo": (request, response) => {
      response.setStatusLine(request.httpVersion, 202, "Accepted");
    }
  });

  let client = new HawkClient(server.baseURI);

  let response = yield client.request("/foo", method, credentials);

  // Shouldn't be any content in a 202
  do_check_eq(response, "");

  yield deferredStop(server);
});

add_task(function test_retry_request_on_fail() {
  let attempts = 0;
  let credentials = {
    id: "eyJleHBpcmVzIjogMTM2NTAxMDg5OC4x",
    key: "qTZf4ZFpAMpMoeSsX3zVRjiqmNs=",
    algorithm: "sha256"
  };
  let method = "GET";

  let server = httpd_setup({
    "/maybe": function(request, response) {
      // This path should be hit exactly twice; once with a bad timestamp, and
      // again when the client retries the request with a corrected timestamp.
      attempts += 1;
      do_check_true(attempts <= 2);

      let delta = getTimestampDelta(request.getHeader("Authorization"));

      // First time through, we should have a bad timestamp
      if (attempts === 1) {
        do_check_true(delta > MINUTE_MS);
        let message = "never!!!";
        response.setStatusLine(request.httpVersion, 401, "Unauthorized");
        response.bodyOutputStream.write(message, message.length);
        return;
      }

      // Second time through, timestamp should be corrected by client
      do_check_true(delta < MINUTE_MS);
      let message = "i love you!!!";
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.bodyOutputStream.write(message, message.length);
      return;
    }
  });

  let client = new HawkClient(server.baseURI);
  function getOffset() {
    return client.localtimeOffsetMsec;
  }

  client.now = () => {
    return Date.now() + 12 * HOUR_MS;
  };

  // We begin with no offset
  do_check_eq(client.localtimeOffsetMsec, 0);

  // Request will have bad timestamp; client will retry once
  let response = yield client.request("/maybe", method, credentials);
  do_check_eq(response, "i love you!!!");

  yield deferredStop(server);
});

add_task(function test_multiple_401_retry_once() {
  // Like test_retry_request_on_fail, but always return a 401
  // and ensure that the client only retries once.
  let attempts = 0;
  let credentials = {
    id: "eyJleHBpcmVzIjogMTM2NTAxMDg5OC4x",
    key: "qTZf4ZFpAMpMoeSsX3zVRjiqmNs=",
    algorithm: "sha256"
  };
  let method = "GET";

  let server = httpd_setup({
    "/maybe": function(request, response) {
      // This path should be hit exactly twice; once with a bad timestamp, and
      // again when the client retries the request with a corrected timestamp.
      attempts += 1;

      do_check_true(attempts <= 2);

      let message = "never!!!";
      response.setStatusLine(request.httpVersion, 401, "Unauthorized");
      response.bodyOutputStream.write(message, message.length);
    }
  });

  let client = new HawkClient(server.baseURI);
  function getOffset() {
    return client.localtimeOffsetMsec;
  }

  client.now = () => {
    return Date.now() - 12 * HOUR_MS;
  };

  // We begin with no offset
  do_check_eq(client.localtimeOffsetMsec, 0);

  // Request will have bad timestamp; client will retry once
  try {
    yield client.request("/maybe", method, credentials);
    do_throw("Expected an error");
  } catch (err) {
    do_check_eq(err.code, 401);
  }
  do_check_eq(attempts, 2);

  yield deferredStop(server);
});

add_task(function test_500_no_retry() {
  // If we get a 500 error, the client should not retry (as it would with a
  // 401)
  let credentials = {
    id: "eyJleHBpcmVzIjogMTM2NTAxMDg5OC4x",
    key: "qTZf4ZFpAMpMoeSsX3zVRjiqmNs=",
    algorithm: "sha256"
  };
  let method = "GET";

  let server = httpd_setup({
    "/no-shutup": function() {
      let message = "Cannot get ye flask.";
      response.setStatusLine(request.httpVersion, 500, "Internal server error");
      response.bodyOutputStream.write(message, message.length);
    }
  });

  let client = new HawkClient(server.baseURI);
  function getOffset() {
    return client.localtimeOffsetMsec;
  }

  // Throw off the clock so the HawkClient would want to retry the request if
  // it could
  client.now = () => {
    return Date.now() - 12 * HOUR_MS;
  };

  // Request will 500; no retries
  try {
    yield client.request("/no-shutup", method, credentials);
    do_throw("Expected an error");
  } catch(err) {
    do_check_eq(err.code, 500);
  }

  yield deferredStop(server);
});

add_task(function test_401_then_500() {
  // Like test_multiple_401_retry_once, but return a 500 to the
  // second request, ensuring that the promise is properly rejected
  // in client.request.
  let attempts = 0;
  let credentials = {
    id: "eyJleHBpcmVzIjogMTM2NTAxMDg5OC4x",
    key: "qTZf4ZFpAMpMoeSsX3zVRjiqmNs=",
    algorithm: "sha256"
  };
  let method = "GET";

  let server = httpd_setup({
    "/maybe": function(request, response) {
      // This path should be hit exactly twice; once with a bad timestamp, and
      // again when the client retries the request with a corrected timestamp.
      attempts += 1;
      do_check_true(attempts <= 2);

      let delta = getTimestampDelta(request.getHeader("Authorization"));

      // First time through, we should have a bad timestamp
      // Client will retry
      if (attempts === 1) {
        do_check_true(delta > MINUTE_MS);
        let message = "never!!!";
        response.setStatusLine(request.httpVersion, 401, "Unauthorized");
        response.bodyOutputStream.write(message, message.length);
        return;
      }

      // Second time through, timestamp should be corrected by client
      // And fail on the client
      do_check_true(delta < MINUTE_MS);
      let message = "Cannot get ye flask.";
      response.setStatusLine(request.httpVersion, 500, "Internal server error");
      response.bodyOutputStream.write(message, message.length);
      return;
    }
  });

  let client = new HawkClient(server.baseURI);
  function getOffset() {
    return client.localtimeOffsetMsec;
  }

  client.now = () => {
    return Date.now() - 12 * HOUR_MS;
  };

  // We begin with no offset
  do_check_eq(client.localtimeOffsetMsec, 0);

  // Request will have bad timestamp; client will retry once
  try {
    yield client.request("/maybe", method, credentials);
  } catch(err) {
    do_check_eq(err.code, 500);
  }
  do_check_eq(attempts, 2);

  yield deferredStop(server);
});

add_task(function throw_if_not_json_body() {
  let client = new HawkClient("https://example.com");
  try {
    yield client.request("/bogus", "GET", {}, "I am not json");
    do_throw("Expected an error");
  } catch(err) {
    do_check_true(!!err.message);
  }
});

// End of tests.
// Utility functions follow

function getTimestampDelta(authHeader, now=Date.now()) {
  let tsMS = new Date(
      parseInt(/ts="(\d+)"/.exec(authHeader)[1], 10) * SECOND_MS);
  return Math.abs(tsMS - now);
}

function deferredStop(server) {
  let deferred = Promise.defer();
  server.stop(deferred.resolve);
  return deferred.promise;
}

function run_test() {
  initTestLogging("Trace");
  run_next_test();
}

