/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FxAccountsClient } = ChromeUtils.import(
  "resource://gre/modules/FxAccountsClient.jsm"
);

const FAKE_SESSION_TOKEN =
  "a0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebf";

// https://wiki.mozilla.org/Identity/AttachedServices/KeyServerProtocol#.2Faccount.2Fkeys
var ACCOUNT_KEYS = {
  keyFetch: h(
    // eslint-disable-next-line no-useless-concat
    "8081828384858687 88898a8b8c8d8e8f" + "9091929394959697 98999a9b9c9d9e9f"
  ),

  response: h(
    "ee5c58845c7c9412 b11bbd20920c2fdd" +
      "d83c33c9cd2c2de2 d66b222613364636" +
      "c2c0f8cfbb7c6304 72c0bd88451342c6" +
      "c05b14ce342c5ad4 6ad89e84464c993c" +
      "3927d30230157d08 17a077eef4b20d97" +
      "6f7a97363faf3f06 4c003ada7d01aa70"
  ),

  kA: h(
    // eslint-disable-next-line no-useless-concat
    "2021222324252627 28292a2b2c2d2e2f" + "3031323334353637 38393a3b3c3d3e3f"
  ),

  wrapKB: h(
    // eslint-disable-next-line no-useless-concat
    "4041424344454647 48494a4b4c4d4e4f" + "5051525354555657 58595a5b5c5d5e5f"
  ),
};

add_task(async function test_authenticated_get_request() {
  let message = '{"msg": "Great Success!"}';
  let credentials = {
    id: "eyJleHBpcmVzIjogMTM2NTAxMDg5OC4x",
    key: "qTZf4ZFpAMpMoeSsX3zVRjiqmNs=",
    algorithm: "sha256",
  };
  let method = "GET";

  let server = httpd_setup({
    "/foo": function(request, response) {
      Assert.ok(request.hasHeader("Authorization"));

      response.setStatusLine(request.httpVersion, 200, "OK");
      response.bodyOutputStream.write(message, message.length);
    },
  });

  let client = new FxAccountsClient(server.baseURI);

  let result = await client._request("/foo", method, credentials);
  Assert.equal("Great Success!", result.msg);

  await promiseStopServer(server);
});

add_task(async function test_authenticated_post_request() {
  let credentials = {
    id: "eyJleHBpcmVzIjogMTM2NTAxMDg5OC4x",
    key: "qTZf4ZFpAMpMoeSsX3zVRjiqmNs=",
    algorithm: "sha256",
  };
  let method = "POST";

  let server = httpd_setup({
    "/foo": function(request, response) {
      Assert.ok(request.hasHeader("Authorization"));

      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "application/json");
      response.bodyOutputStream.writeFrom(
        request.bodyInputStream,
        request.bodyInputStream.available()
      );
    },
  });

  let client = new FxAccountsClient(server.baseURI);

  let result = await client._request("/foo", method, credentials, {
    foo: "bar",
  });
  Assert.equal("bar", result.foo);

  await promiseStopServer(server);
});

add_task(async function test_500_error() {
  let message = "<h1>Ooops!</h1>";
  let method = "GET";

  let server = httpd_setup({
    "/foo": function(request, response) {
      response.setStatusLine(request.httpVersion, 500, "Internal Server Error");
      response.bodyOutputStream.write(message, message.length);
    },
  });

  let client = new FxAccountsClient(server.baseURI);

  try {
    await client._request("/foo", method);
    do_throw("Expected to catch an exception");
  } catch (e) {
    Assert.equal(500, e.code);
    Assert.equal("Internal Server Error", e.message);
  }

  await promiseStopServer(server);
});

add_task(async function test_backoffError() {
  let method = "GET";
  let server = httpd_setup({
    "/retryDelay": function(request, response) {
      response.setHeader("Retry-After", "30");
      response.setStatusLine(
        request.httpVersion,
        429,
        "Client has sent too many requests"
      );
      let message = "<h1>Ooops!</h1>";
      response.bodyOutputStream.write(message, message.length);
    },
    "/duringDelayIShouldNotBeCalled": function(request, response) {
      response.setStatusLine(request.httpVersion, 200, "OK");
      let jsonMessage = '{"working": "yes"}';
      response.bodyOutputStream.write(jsonMessage, jsonMessage.length);
    },
  });

  let client = new FxAccountsClient(server.baseURI);

  // Retry-After header sets client.backoffError
  Assert.equal(client.backoffError, null);
  try {
    await client._request("/retryDelay", method);
  } catch (e) {
    Assert.equal(429, e.code);
    Assert.equal(30, e.retryAfter);
    Assert.notEqual(typeof client.fxaBackoffTimer, "undefined");
    Assert.notEqual(client.backoffError, null);
  }
  // While delay is in effect, client short-circuits any requests
  // and re-rejects with previous error.
  try {
    await client._request("/duringDelayIShouldNotBeCalled", method);
    throw new Error("I should not be reached");
  } catch (e) {
    Assert.equal(e.retryAfter, 30);
    Assert.equal(e.message, "Client has sent too many requests");
    Assert.notEqual(client.backoffError, null);
  }
  // Once timer fires, client nulls error out and HTTP calls work again.
  client._clearBackoff();
  let result = await client._request("/duringDelayIShouldNotBeCalled", method);
  Assert.equal(client.backoffError, null);
  Assert.equal(result.working, "yes");

  await promiseStopServer(server);
});

add_task(async function test_signUp() {
  let creationMessage_noKey = JSON.stringify({
    uid: "uid",
    sessionToken: "sessionToken",
  });
  let creationMessage_withKey = JSON.stringify({
    uid: "uid",
    sessionToken: "sessionToken",
    keyFetchToken: "keyFetchToken",
  });
  let errorMessage = JSON.stringify({
    code: 400,
    errno: 101,
    error: "account exists",
  });
  let created = false;

  // Note these strings must be unicode and not already utf-8 encoded.
  let unicodeUsername = "andr\xe9@example.org"; // 'andré@example.org'
  let unicodePassword = "p\xe4ssw\xf6rd"; // 'pässwörd'
  let server = httpd_setup({
    "/account/create": function(request, response) {
      let body = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
      body = CommonUtils.decodeUTF8(body);
      let jsonBody = JSON.parse(body);

      // https://github.com/mozilla/fxa-auth-server/wiki/onepw-protocol#wiki-test-vectors

      if (created) {
        // Error trying to create same account a second time
        response.setStatusLine(request.httpVersion, 400, "Bad request");
        response.bodyOutputStream.write(errorMessage, errorMessage.length);
        return;
      }

      if (jsonBody.email == unicodeUsername) {
        Assert.equal("", request._queryString);
        Assert.equal(
          jsonBody.authPW,
          "247b675ffb4c46310bc87e26d712153abe5e1c90ef00a4784594f97ef54f2375"
        );

        response.setStatusLine(request.httpVersion, 200, "OK");
        response.bodyOutputStream.write(
          creationMessage_noKey,
          creationMessage_noKey.length
        );
        return;
      }

      if (jsonBody.email == "you@example.org") {
        Assert.equal("keys=true", request._queryString);
        Assert.equal(
          jsonBody.authPW,
          "e5c1cdfdaa5fcee06142db865b212cc8ba8abee2a27d639d42c139f006cdb930"
        );
        created = true;

        response.setStatusLine(request.httpVersion, 200, "OK");
        response.bodyOutputStream.write(
          creationMessage_withKey,
          creationMessage_withKey.length
        );
        return;
      }
      // just throwing here doesn't make any log noise, so have an assertion
      // fail instead.
      Assert.ok(false, "unexpected email: " + jsonBody.email);
    },
  });

  // Try to create an account without retrieving optional keys.
  let client = new FxAccountsClient(server.baseURI);
  let result = await client.signUp(unicodeUsername, unicodePassword);
  Assert.equal("uid", result.uid);
  Assert.equal("sessionToken", result.sessionToken);
  Assert.equal(undefined, result.keyFetchToken);
  Assert.equal(
    result.unwrapBKey,
    "de6a2648b78284fcb9ffa81ba95803309cfba7af583c01a8a1a63e567234dd28"
  );

  // Try to create an account retrieving optional keys.
  result = await client.signUp("you@example.org", "pässwörd", true);
  Assert.equal("uid", result.uid);
  Assert.equal("sessionToken", result.sessionToken);
  Assert.equal("keyFetchToken", result.keyFetchToken);
  Assert.equal(
    result.unwrapBKey,
    "f589225b609e56075d76eb74f771ff9ab18a4dc0e901e131ba8f984c7fb0ca8c"
  );

  // Try to create an existing account.  Triggers error path.
  try {
    result = await client.signUp(unicodeUsername, unicodePassword);
    do_throw("Expected to catch an exception");
  } catch (expectedError) {
    Assert.equal(101, expectedError.errno);
  }

  await promiseStopServer(server);
});

add_task(async function test_signIn() {
  let sessionMessage_noKey = JSON.stringify({
    sessionToken: FAKE_SESSION_TOKEN,
  });
  let sessionMessage_withKey = JSON.stringify({
    sessionToken: FAKE_SESSION_TOKEN,
    keyFetchToken: "keyFetchToken",
  });
  let errorMessage_notExistent = JSON.stringify({
    code: 400,
    errno: 102,
    error: "doesn't exist",
  });
  let errorMessage_wrongCap = JSON.stringify({
    code: 400,
    errno: 120,
    error: "Incorrect email case",
    email: "you@example.com",
  });

  // Note this strings must be unicode and not already utf-8 encoded.
  let unicodeUsername = "m\xe9@example.com"; // 'mé@example.com'
  let server = httpd_setup({
    "/account/login": function(request, response) {
      let body = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
      body = CommonUtils.decodeUTF8(body);
      let jsonBody = JSON.parse(body);

      if (jsonBody.email == unicodeUsername) {
        Assert.equal("", request._queryString);
        Assert.equal(
          jsonBody.authPW,
          "08b9d111196b8408e8ed92439da49206c8ecfbf343df0ae1ecefcd1e0174a8b6"
        );
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.bodyOutputStream.write(
          sessionMessage_noKey,
          sessionMessage_noKey.length
        );
      } else if (jsonBody.email == "you@example.com") {
        Assert.equal("keys=true", request._queryString);
        Assert.equal(
          jsonBody.authPW,
          "93d20ec50304d496d0707ec20d7e8c89459b6396ec5dd5b9e92809c5e42856c7"
        );
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.bodyOutputStream.write(
          sessionMessage_withKey,
          sessionMessage_withKey.length
        );
      } else if (jsonBody.email == "You@example.com") {
        // Error trying to sign in with a wrong capitalization
        response.setStatusLine(request.httpVersion, 400, "Bad request");
        response.bodyOutputStream.write(
          errorMessage_wrongCap,
          errorMessage_wrongCap.length
        );
      } else {
        // Error trying to sign in to nonexistent account
        response.setStatusLine(request.httpVersion, 400, "Bad request");
        response.bodyOutputStream.write(
          errorMessage_notExistent,
          errorMessage_notExistent.length
        );
      }
    },
  });

  // Login without retrieving optional keys
  let client = new FxAccountsClient(server.baseURI);
  let result = await client.signIn(unicodeUsername, "bigsecret");
  Assert.equal(FAKE_SESSION_TOKEN, result.sessionToken);
  Assert.equal(
    result.unwrapBKey,
    "c076ec3f4af123a615157154c6e1d0d6293e514fd7b0221e32d50517ecf002b8"
  );
  Assert.equal(undefined, result.keyFetchToken);

  // Login with retrieving optional keys
  result = await client.signIn("you@example.com", "bigsecret", true);
  Assert.equal(FAKE_SESSION_TOKEN, result.sessionToken);
  Assert.equal(
    result.unwrapBKey,
    "65970516211062112e955d6420bebe020269d6b6a91ebd288319fc8d0cb49624"
  );
  Assert.equal("keyFetchToken", result.keyFetchToken);

  // Retry due to wrong email capitalization
  result = await client.signIn("You@example.com", "bigsecret", true);
  Assert.equal(FAKE_SESSION_TOKEN, result.sessionToken);
  Assert.equal(
    result.unwrapBKey,
    "65970516211062112e955d6420bebe020269d6b6a91ebd288319fc8d0cb49624"
  );
  Assert.equal("keyFetchToken", result.keyFetchToken);

  // Trigger error path
  try {
    result = await client.signIn("yøü@bad.example.org", "nofear");
    do_throw("Expected to catch an exception");
  } catch (expectedError) {
    Assert.equal(102, expectedError.errno);
  }

  await promiseStopServer(server);
});

add_task(async function test_signOut() {
  let signoutMessage = JSON.stringify({});
  let errorMessage = JSON.stringify({
    code: 400,
    errno: 102,
    error: "doesn't exist",
  });
  let signedOut = false;

  let server = httpd_setup({
    "/session/destroy": function(request, response) {
      if (!signedOut) {
        signedOut = true;
        Assert.ok(request.hasHeader("Authorization"));
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.bodyOutputStream.write(signoutMessage, signoutMessage.length);
        return;
      }

      // Error trying to sign out of nonexistent account
      response.setStatusLine(request.httpVersion, 400, "Bad request");
      response.bodyOutputStream.write(errorMessage, errorMessage.length);
    },
  });

  let client = new FxAccountsClient(server.baseURI);
  let result = await client.signOut("FakeSession");
  Assert.equal(typeof result, "object");

  // Trigger error path
  try {
    result = await client.signOut("FakeSession");
    do_throw("Expected to catch an exception");
  } catch (expectedError) {
    Assert.equal(102, expectedError.errno);
  }

  await promiseStopServer(server);
});

add_task(async function test_recoveryEmailStatus() {
  let emailStatus = JSON.stringify({ verified: true });
  let errorMessage = JSON.stringify({
    code: 400,
    errno: 102,
    error: "doesn't exist",
  });
  let tries = 0;

  let server = httpd_setup({
    "/recovery_email/status": function(request, response) {
      Assert.ok(request.hasHeader("Authorization"));
      Assert.equal("", request._queryString);

      if (tries === 0) {
        tries += 1;
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.bodyOutputStream.write(emailStatus, emailStatus.length);
        return;
      }

      // Second call gets an error trying to query a nonexistent account
      response.setStatusLine(request.httpVersion, 400, "Bad request");
      response.bodyOutputStream.write(errorMessage, errorMessage.length);
    },
  });

  let client = new FxAccountsClient(server.baseURI);
  let result = await client.recoveryEmailStatus(FAKE_SESSION_TOKEN);
  Assert.equal(result.verified, true);

  // Trigger error path
  try {
    result = await client.recoveryEmailStatus("some bogus session");
    do_throw("Expected to catch an exception");
  } catch (expectedError) {
    Assert.equal(102, expectedError.errno);
  }

  await promiseStopServer(server);
});

add_task(async function test_recoveryEmailStatusWithReason() {
  let emailStatus = JSON.stringify({ verified: true });
  let server = httpd_setup({
    "/recovery_email/status": function(request, response) {
      Assert.ok(request.hasHeader("Authorization"));
      // if there is a query string then it will have a reason
      Assert.equal("reason=push", request._queryString);

      response.setStatusLine(request.httpVersion, 200, "OK");
      response.bodyOutputStream.write(emailStatus, emailStatus.length);
    },
  });

  let client = new FxAccountsClient(server.baseURI);
  let result = await client.recoveryEmailStatus(FAKE_SESSION_TOKEN, {
    reason: "push",
  });
  Assert.equal(result.verified, true);
  await promiseStopServer(server);
});

add_task(async function test_resendVerificationEmail() {
  let emptyMessage = "{}";
  let errorMessage = JSON.stringify({
    code: 400,
    errno: 102,
    error: "doesn't exist",
  });
  let tries = 0;

  let server = httpd_setup({
    "/recovery_email/resend_code": function(request, response) {
      Assert.ok(request.hasHeader("Authorization"));
      if (tries === 0) {
        tries += 1;
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.bodyOutputStream.write(emptyMessage, emptyMessage.length);
        return;
      }

      // Second call gets an error trying to query a nonexistent account
      response.setStatusLine(request.httpVersion, 400, "Bad request");
      response.bodyOutputStream.write(errorMessage, errorMessage.length);
    },
  });

  let client = new FxAccountsClient(server.baseURI);
  let result = await client.resendVerificationEmail(FAKE_SESSION_TOKEN);
  Assert.equal(JSON.stringify(result), emptyMessage);

  // Trigger error path
  try {
    result = await client.resendVerificationEmail("some bogus session");
    do_throw("Expected to catch an exception");
  } catch (expectedError) {
    Assert.equal(102, expectedError.errno);
  }

  await promiseStopServer(server);
});

add_task(async function test_accountKeys() {
  // Four calls to accountKeys().  The first one should work correctly, and we
  // should get a valid bundle back, in exchange for our keyFetch token, from
  // which we correctly derive kA and wrapKB.  The subsequent three calls
  // should all trigger separate error paths.
  let responseMessage = JSON.stringify({ bundle: ACCOUNT_KEYS.response });
  let errorMessage = JSON.stringify({
    code: 400,
    errno: 102,
    error: "doesn't exist",
  });
  let emptyMessage = "{}";
  let attempt = 0;

  let server = httpd_setup({
    "/account/keys": function(request, response) {
      Assert.ok(request.hasHeader("Authorization"));
      attempt += 1;

      switch (attempt) {
        case 1:
          // First time succeeds
          response.setStatusLine(request.httpVersion, 200, "OK");
          response.bodyOutputStream.write(
            responseMessage,
            responseMessage.length
          );
          break;

        case 2:
          // Second time, return no bundle to trigger client error
          response.setStatusLine(request.httpVersion, 200, "OK");
          response.bodyOutputStream.write(emptyMessage, emptyMessage.length);
          break;

        case 3:
          // Return gibberish to trigger client MAC error
          // Tweak a byte
          let garbageResponse = JSON.stringify({
            bundle: ACCOUNT_KEYS.response.slice(0, -1) + "1",
          });
          response.setStatusLine(request.httpVersion, 200, "OK");
          response.bodyOutputStream.write(
            garbageResponse,
            garbageResponse.length
          );
          break;

        case 4:
          // Trigger error for nonexistent account
          response.setStatusLine(request.httpVersion, 400, "Bad request");
          response.bodyOutputStream.write(errorMessage, errorMessage.length);
          break;
      }
    },
  });

  let client = new FxAccountsClient(server.baseURI);

  // First try, all should be good
  let result = await client.accountKeys(ACCOUNT_KEYS.keyFetch);
  Assert.equal(CommonUtils.hexToBytes(ACCOUNT_KEYS.kA), result.kA);
  Assert.equal(CommonUtils.hexToBytes(ACCOUNT_KEYS.wrapKB), result.wrapKB);

  // Second try, empty bundle should trigger error
  try {
    result = await client.accountKeys(ACCOUNT_KEYS.keyFetch);
    do_throw("Expected to catch an exception");
  } catch (expectedError) {
    Assert.equal(expectedError.message, "failed to retrieve keys");
  }

  // Third try, bad bundle results in MAC error
  try {
    result = await client.accountKeys(ACCOUNT_KEYS.keyFetch);
    do_throw("Expected to catch an exception");
  } catch (expectedError) {
    Assert.equal(expectedError.message, "error unbundling encryption keys");
  }

  // Fourth try, pretend account doesn't exist
  try {
    result = await client.accountKeys(ACCOUNT_KEYS.keyFetch);
    do_throw("Expected to catch an exception");
  } catch (expectedError) {
    Assert.equal(102, expectedError.errno);
  }

  await promiseStopServer(server);
});

add_task(async function test_accessTokenWithSessionToken() {
  let server = httpd_setup({
    "/oauth/token": function(request, response) {
      const responseMessage = JSON.stringify({
        access_token:
          "43793fdfffec22eb39fc3c44ed09193a6fde4c24e5d6a73f73178597b268af69",
        token_type: "bearer",
        scope: "https://identity.mozilla.com/apps/oldsync",
        expires_in: 21600,
        auth_at: 1589579900,
      });

      response.setStatusLine(request.httpVersion, 200, "OK");
      response.bodyOutputStream.write(responseMessage, responseMessage.length);
    },
  });

  let client = new FxAccountsClient(server.baseURI);
  let sessionTokenHex =
    "0599c36ebb5cad6feb9285b9547b65342b5434d55c07b33bffd4307ab8f82dc4";
  let clientId = "5882386c6d801776";
  let scope = "https://identity.mozilla.com/apps/oldsync";
  let ttl = 100;
  let result = await client.accessTokenWithSessionToken(
    sessionTokenHex,
    clientId,
    scope,
    ttl
  );
  Assert.ok(result);

  await promiseStopServer(server);
});

add_task(async function test_accountExists() {
  let existsMessage = JSON.stringify({
    error: "wrong password",
    code: 400,
    errno: 103,
  });
  let doesntExistMessage = JSON.stringify({
    error: "no such account",
    code: 400,
    errno: 102,
  });
  let emptyMessage = "{}";

  let server = httpd_setup({
    "/account/login": function(request, response) {
      let body = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
      let jsonBody = JSON.parse(body);

      switch (jsonBody.email) {
        // We'll test that these users' accounts exist
        case "i.exist@example.com":
        case "i.also.exist@example.com":
          response.setStatusLine(request.httpVersion, 400, "Bad request");
          response.bodyOutputStream.write(existsMessage, existsMessage.length);
          break;

        // This user's account doesn't exist
        case "i.dont.exist@example.com":
          response.setStatusLine(request.httpVersion, 400, "Bad request");
          response.bodyOutputStream.write(
            doesntExistMessage,
            doesntExistMessage.length
          );
          break;

        // This user throws an unexpected response
        // This will reject the client signIn promise
        case "i.break.things@example.com":
          response.setStatusLine(request.httpVersion, 500, "Alas");
          response.bodyOutputStream.write(emptyMessage, emptyMessage.length);
          break;

        default:
          throw new Error("Unexpected login from " + jsonBody.email);
      }
    },
  });

  let client = new FxAccountsClient(server.baseURI);
  let result;

  result = await client.accountExists("i.exist@example.com");
  Assert.ok(result);

  result = await client.accountExists("i.also.exist@example.com");
  Assert.ok(result);

  result = await client.accountExists("i.dont.exist@example.com");
  Assert.ok(!result);

  try {
    result = await client.accountExists("i.break.things@example.com");
    do_throw("Expected to catch an exception");
  } catch (unexpectedError) {
    Assert.equal(unexpectedError.code, 500);
  }

  await promiseStopServer(server);
});

add_task(async function test_registerDevice() {
  const DEVICE_ID = "device id";
  const DEVICE_NAME = "device name";
  const DEVICE_TYPE = "device type";
  const ERROR_NAME = "test that the client promise rejects";

  const server = httpd_setup({
    "/account/device": function(request, response) {
      const body = JSON.parse(
        CommonUtils.readBytesFromInputStream(request.bodyInputStream)
      );

      if (
        body.id ||
        !body.name ||
        !body.type ||
        Object.keys(body).length !== 2
      ) {
        response.setStatusLine(request.httpVersion, 400, "Invalid request");
        response.bodyOutputStream.write("{}", 2);
        return;
      }

      if (body.name === ERROR_NAME) {
        response.setStatusLine(request.httpVersion, 500, "Alas");
        response.bodyOutputStream.write("{}", 2);
        return;
      }

      body.id = DEVICE_ID;
      body.createdAt = Date.now();

      const responseMessage = JSON.stringify(body);

      response.setStatusLine(request.httpVersion, 200, "OK");
      response.bodyOutputStream.write(responseMessage, responseMessage.length);
    },
  });

  const client = new FxAccountsClient(server.baseURI);
  const result = await client.registerDevice(
    FAKE_SESSION_TOKEN,
    DEVICE_NAME,
    DEVICE_TYPE
  );

  Assert.ok(result);
  Assert.equal(Object.keys(result).length, 4);
  Assert.equal(result.id, DEVICE_ID);
  Assert.equal(typeof result.createdAt, "number");
  Assert.ok(result.createdAt > 0);
  Assert.equal(result.name, DEVICE_NAME);
  Assert.equal(result.type, DEVICE_TYPE);

  try {
    await client.registerDevice(FAKE_SESSION_TOKEN, ERROR_NAME, DEVICE_TYPE);
    do_throw("Expected to catch an exception");
  } catch (unexpectedError) {
    Assert.equal(unexpectedError.code, 500);
  }

  await promiseStopServer(server);
});

add_task(async function test_updateDevice() {
  const DEVICE_ID = "some other id";
  const DEVICE_NAME = "some other name";
  const ERROR_ID = "test that the client promise rejects";

  const server = httpd_setup({
    "/account/device": function(request, response) {
      const body = JSON.parse(
        CommonUtils.readBytesFromInputStream(request.bodyInputStream)
      );

      if (
        !body.id ||
        !body.name ||
        body.type ||
        Object.keys(body).length !== 2
      ) {
        response.setStatusLine(request.httpVersion, 400, "Invalid request");
        response.bodyOutputStream.write("{}", 2);
        return;
      }

      if (body.id === ERROR_ID) {
        response.setStatusLine(request.httpVersion, 500, "Alas");
        response.bodyOutputStream.write("{}", 2);
        return;
      }

      const responseMessage = JSON.stringify(body);

      response.setStatusLine(request.httpVersion, 200, "OK");
      response.bodyOutputStream.write(responseMessage, responseMessage.length);
    },
  });

  const client = new FxAccountsClient(server.baseURI);
  const result = await client.updateDevice(
    FAKE_SESSION_TOKEN,
    DEVICE_ID,
    DEVICE_NAME
  );

  Assert.ok(result);
  Assert.equal(Object.keys(result).length, 2);
  Assert.equal(result.id, DEVICE_ID);
  Assert.equal(result.name, DEVICE_NAME);

  try {
    await client.updateDevice(FAKE_SESSION_TOKEN, ERROR_ID, DEVICE_NAME);
    do_throw("Expected to catch an exception");
  } catch (unexpectedError) {
    Assert.equal(unexpectedError.code, 500);
  }

  await promiseStopServer(server);
});

add_task(async function test_getDeviceList() {
  let canReturnDevices;

  const server = httpd_setup({
    "/account/devices": function(request, response) {
      if (canReturnDevices) {
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.bodyOutputStream.write("[]", 2);
      } else {
        response.setStatusLine(request.httpVersion, 500, "Alas");
        response.bodyOutputStream.write("{}", 2);
      }
    },
  });

  const client = new FxAccountsClient(server.baseURI);

  canReturnDevices = true;
  const result = await client.getDeviceList(FAKE_SESSION_TOKEN);
  Assert.ok(Array.isArray(result));
  Assert.equal(result.length, 0);

  try {
    canReturnDevices = false;
    await client.getDeviceList(FAKE_SESSION_TOKEN);
    do_throw("Expected to catch an exception");
  } catch (unexpectedError) {
    Assert.equal(unexpectedError.code, 500);
  }

  await promiseStopServer(server);
});

add_task(async function test_client_metrics() {
  function writeResp(response, msg) {
    if (typeof msg === "object") {
      msg = JSON.stringify(msg);
    }
    response.bodyOutputStream.write(msg, msg.length);
  }

  let server = httpd_setup({
    "/session/destroy": function(request, response) {
      response.setHeader("Content-Type", "application/json; charset=utf-8");
      response.setStatusLine(request.httpVersion, 401, "Unauthorized");
      writeResp(response, {
        error: "invalid authentication timestamp",
        code: 401,
        errno: 111,
      });
    },
  });

  let client = new FxAccountsClient(server.baseURI);

  await Assert.rejects(
    client.signOut(FAKE_SESSION_TOKEN, {
      service: "sync",
    }),
    function(err) {
      return err.errno == 111;
    }
  );

  await promiseStopServer(server);
});

add_task(async function test_email_case() {
  let canonicalEmail = "greta.garbo@gmail.com";
  let clientEmail = "Greta.Garbo@gmail.COM";
  let attempts = 0;

  function writeResp(response, msg) {
    if (typeof msg === "object") {
      msg = JSON.stringify(msg);
    }
    response.bodyOutputStream.write(msg, msg.length);
  }

  let server = httpd_setup({
    "/account/login": function(request, response) {
      response.setHeader("Content-Type", "application/json; charset=utf-8");
      attempts += 1;
      if (attempts > 2) {
        response.setStatusLine(
          request.httpVersion,
          429,
          "Sorry, you had your chance"
        );
        return writeResp(response, "");
      }

      let body = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
      let jsonBody = JSON.parse(body);
      let email = jsonBody.email;

      // If the client has the wrong case on the email, we return a 400, with
      // the capitalization of the email as saved in the accounts database.
      if (email == canonicalEmail) {
        response.setStatusLine(request.httpVersion, 200, "Yay");
        return writeResp(response, { areWeHappy: "yes" });
      }

      response.setStatusLine(request.httpVersion, 400, "Incorrect email case");
      return writeResp(response, {
        code: 400,
        errno: 120,
        error: "Incorrect email case",
        email: canonicalEmail,
      });
    },
  });

  let client = new FxAccountsClient(server.baseURI);

  let result = await client.signIn(clientEmail, "123456");
  Assert.equal(result.areWeHappy, "yes");
  Assert.equal(attempts, 2);

  await promiseStopServer(server);
});

// turn formatted test vectors into normal hex strings
function h(hexStr) {
  return hexStr.replace(/\s+/g, "");
}
