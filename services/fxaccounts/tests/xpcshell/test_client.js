/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/FxAccountsClient.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-crypto/utils.js");

const FAKE_SESSION_TOKEN = "a0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebf";

function run_test() {
  run_next_test();
}

// https://wiki.mozilla.org/Identity/AttachedServices/KeyServerProtocol#.2Faccount.2Fkeys
let ACCOUNT_KEYS = {
  keyFetch:     h("8081828384858687 88898a8b8c8d8e8f"+
                  "9091929394959697 98999a9b9c9d9e9f"),

  response:     h("ee5c58845c7c9412 b11bbd20920c2fdd"+
                  "d83c33c9cd2c2de2 d66b222613364636"+
                  "c2c0f8cfbb7c6304 72c0bd88451342c6"+
                  "c05b14ce342c5ad4 6ad89e84464c993c"+
                  "3927d30230157d08 17a077eef4b20d97"+
                  "6f7a97363faf3f06 4c003ada7d01aa70"),

  kA:           h("2021222324252627 28292a2b2c2d2e2f"+
                  "3031323334353637 38393a3b3c3d3e3f"),

  wrapKB:       h("4041424344454647 48494a4b4c4d4e4f"+
                  "5051525354555657 58595a5b5c5d5e5f"),
};

// https://github.com/mozilla/fxa-auth-server/wiki/onepw-protocol#wiki-use-session-certificatesign-etc
let SESSION_KEYS = {
  sessionToken: h("a0a1a2a3a4a5a6a7 a8a9aaabacadaeaf"+
                  "b0b1b2b3b4b5b6b7 b8b9babbbcbdbebf"),

  tokenID:      h("c0a29dcf46174973 da1378696e4c82ae"+
                  "10f723cf4f4d9f75 e39f4ae3851595ab"),

  reqHMACkey:   h("9d8f22998ee7f579 8b887042466b72d5"+
                  "3e56ab0c094388bf 65831f702d2febc0"),
};

function deferredStop(server) {
  let deferred = Promise.defer();
  server.stop(deferred.resolve);
  return deferred.promise;
}

add_task(function test_authenticated_get_request() {
  let message = "{\"msg\": \"Great Success!\"}";
  let credentials = {
    id: "eyJleHBpcmVzIjogMTM2NTAxMDg5OC4x",
    key: "qTZf4ZFpAMpMoeSsX3zVRjiqmNs=",
    algorithm: "sha256"
  };
  let method = "GET";

  let server = httpd_setup({"/foo": function(request, response) {
      do_check_true(request.hasHeader("Authorization"));

      response.setStatusLine(request.httpVersion, 200, "OK");
      response.bodyOutputStream.write(message, message.length);
    }
  });

  let client = new FxAccountsClient(server.baseURI);

  let result = yield client._request("/foo", method, credentials);
  do_check_eq("Great Success!", result.msg);

  yield deferredStop(server);
});

add_task(function test_authenticated_post_request() {
  let credentials = {
    id: "eyJleHBpcmVzIjogMTM2NTAxMDg5OC4x",
    key: "qTZf4ZFpAMpMoeSsX3zVRjiqmNs=",
    algorithm: "sha256"
  };
  let method = "POST";

  let server = httpd_setup({"/foo": function(request, response) {
      do_check_true(request.hasHeader("Authorization"));

      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "application/json");
      response.bodyOutputStream.writeFrom(request.bodyInputStream, request.bodyInputStream.available());
    }
  });

  let client = new FxAccountsClient(server.baseURI);

  let result = yield client._request("/foo", method, credentials, {foo: "bar"});
  do_check_eq("bar", result.foo);

  yield deferredStop(server);
});

add_task(function test_500_error() {
  let message = "<h1>Ooops!</h1>";
  let method = "GET";

  let server = httpd_setup({"/foo": function(request, response) {
      response.setStatusLine(request.httpVersion, 500, "Internal Server Error");
      response.bodyOutputStream.write(message, message.length);
    }
  });

  let client = new FxAccountsClient(server.baseURI);

  try {
    yield client._request("/foo", method);
    do_throw("Expected to catch an exception");
  } catch (e) {
    do_check_eq(500, e.code);
    do_check_eq("Internal Server Error", e.message);
  }

  yield deferredStop(server);
});

add_task(function test_backoffError() {
  let method = "GET";
  let server = httpd_setup({
    "/retryDelay": function(request, response) {
      response.setHeader("Retry-After", "30");
      response.setStatusLine(request.httpVersion, 429, "Client has sent too many requests");
      let message = "<h1>Ooops!</h1>";
      response.bodyOutputStream.write(message, message.length);
    },
    "/duringDelayIShouldNotBeCalled": function(request, response) {
      response.setStatusLine(request.httpVersion, 200, "OK");
      let jsonMessage = "{\"working\": \"yes\"}";
      response.bodyOutputStream.write(jsonMessage, jsonMessage.length);
    },
  });

  let client = new FxAccountsClient(server.baseURI);

  // Retry-After header sets client.backoffError
  do_check_eq(client.backoffError, null);
  try {
    yield client._request("/retryDelay", method);
  } catch (e) {
    do_check_eq(429, e.code);
    do_check_eq(30, e.retryAfter);
    do_check_neq(typeof(client.fxaBackoffTimer), "undefined");
    do_check_neq(client.backoffError, null);
  }
  // While delay is in effect, client short-circuits any requests
  // and re-rejects with previous error.
  try {
    yield client._request("/duringDelayIShouldNotBeCalled", method);
    throw new Error("I should not be reached");
  } catch (e) {
    do_check_eq(e.retryAfter, 30);
    do_check_eq(e.message, "Client has sent too many requests");
    do_check_neq(client.backoffError, null);
  }
  // Once timer fires, client nulls error out and HTTP calls work again.
  client._clearBackoff();
  let result = yield client._request("/duringDelayIShouldNotBeCalled", method);
  do_check_eq(client.backoffError, null);
  do_check_eq(result.working, "yes");

  yield deferredStop(server);
});

add_task(function test_signUp() {
  let creationMessage = JSON.stringify({
    uid: "uid",
    sessionToken: "sessionToken",
    keyFetchToken: "keyFetchToken"
  });
  let errorMessage = JSON.stringify({code: 400, errno: 101, error: "account exists"});
  let created = false;

  let server = httpd_setup({
    "/account/create": function(request, response) {
      let body = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
      let jsonBody = JSON.parse(body);

      // https://github.com/mozilla/fxa-auth-server/wiki/onepw-protocol#wiki-test-vectors
      do_check_eq(jsonBody.email, "andré@example.org");

      if (!created) {
        do_check_eq(jsonBody.authPW, "247b675ffb4c46310bc87e26d712153abe5e1c90ef00a4784594f97ef54f2375");
        created = true;

        response.setStatusLine(request.httpVersion, 200, "OK");
        response.bodyOutputStream.write(creationMessage, creationMessage.length);
        return;
      }

      // Error trying to create same account a second time
      response.setStatusLine(request.httpVersion, 400, "Bad request");
      response.bodyOutputStream.write(errorMessage, errorMessage.length);
      return;
    },
  });

  let client = new FxAccountsClient(server.baseURI);
  let result = yield client.signUp('andré@example.org', 'pässwörd');
  do_check_eq("uid", result.uid);
  do_check_eq("sessionToken", result.sessionToken);
  do_check_eq("keyFetchToken", result.keyFetchToken);

  // Try to create account again.  Triggers error path.
  try {
    result = yield client.signUp('andré@example.org', 'pässwörd');
    do_throw("Expected to catch an exception");
  } catch(expectedError) {
    do_check_eq(101, expectedError.errno);
  }

  yield deferredStop(server);
});

add_task(function test_signIn() {
  let sessionMessage_noKey = JSON.stringify({
    sessionToken: FAKE_SESSION_TOKEN
  });
  let sessionMessage_withKey = JSON.stringify({
    sessionToken: FAKE_SESSION_TOKEN,
    keyFetchToken: "keyFetchToken"
  });
  let errorMessage_notExistent = JSON.stringify({
    code: 400,
    errno: 102,
    error: "doesn't exist"
  });
  let errorMessage_wrongCap = JSON.stringify({
    code: 400,
    errno: 120,
    error: "Incorrect email case",
    email: "you@example.com"
  });

  let server = httpd_setup({
    "/account/login": function(request, response) {
      let body = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
      let jsonBody = JSON.parse(body);

      if (jsonBody.email == "mé@example.com") {
        do_check_eq("", request._queryString);
        do_check_eq(jsonBody.authPW, "08b9d111196b8408e8ed92439da49206c8ecfbf343df0ae1ecefcd1e0174a8b6");
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.bodyOutputStream.write(sessionMessage_noKey,
                                        sessionMessage_noKey.length);
        return;
      }
      else if (jsonBody.email == "you@example.com") {
        do_check_eq("keys=true", request._queryString);
        do_check_eq(jsonBody.authPW, "93d20ec50304d496d0707ec20d7e8c89459b6396ec5dd5b9e92809c5e42856c7");
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.bodyOutputStream.write(sessionMessage_withKey,
                                        sessionMessage_withKey.length);
        return;
      }
      else if (jsonBody.email == "You@example.com") {
        // Error trying to sign in with a wrong capitalization
        response.setStatusLine(request.httpVersion, 400, "Bad request");
        response.bodyOutputStream.write(errorMessage_wrongCap,
                                        errorMessage_wrongCap.length);
        return;
      }
      else {
        // Error trying to sign in to nonexistent account
        response.setStatusLine(request.httpVersion, 400, "Bad request");
        response.bodyOutputStream.write(errorMessage_notExistent,
                                        errorMessage_notExistent.length);
        return;
      }
    },
  });

  // Login without retrieving optional keys
  let client = new FxAccountsClient(server.baseURI);
  let result = yield client.signIn('mé@example.com', 'bigsecret');
  do_check_eq(FAKE_SESSION_TOKEN, result.sessionToken);
  do_check_eq(result.unwrapBKey,
              "c076ec3f4af123a615157154c6e1d0d6293e514fd7b0221e32d50517ecf002b8");
  do_check_eq(undefined, result.keyFetchToken);

  // Login with retrieving optional keys
  let result = yield client.signIn('you@example.com', 'bigsecret', true);
  do_check_eq(FAKE_SESSION_TOKEN, result.sessionToken);
  do_check_eq(result.unwrapBKey,
              "65970516211062112e955d6420bebe020269d6b6a91ebd288319fc8d0cb49624");
  do_check_eq("keyFetchToken", result.keyFetchToken);

  // Retry due to wrong email capitalization
  let result = yield client.signIn('You@example.com', 'bigsecret', true);
  do_check_eq(FAKE_SESSION_TOKEN, result.sessionToken);
  do_check_eq(result.unwrapBKey,
              "65970516211062112e955d6420bebe020269d6b6a91ebd288319fc8d0cb49624");
  do_check_eq("keyFetchToken", result.keyFetchToken);

  // Don't retry due to wrong email capitalization
  try {
    let result = yield client.signIn('You@example.com', 'bigsecret', true, false);
    do_throw("Expected to catch an exception");
  } catch (expectedError) {
    do_check_eq(120, expectedError.errno);
    do_check_eq("you@example.com", expectedError.email);
  }

  // Trigger error path
  try {
    result = yield client.signIn("yøü@bad.example.org", "nofear");
    do_throw("Expected to catch an exception");
  } catch (expectedError) {
    do_check_eq(102, expectedError.errno);
  }

  yield deferredStop(server);
});

add_task(function test_signOut() {
  let signoutMessage = JSON.stringify({});
  let errorMessage = JSON.stringify({code: 400, errno: 102, error: "doesn't exist"});
  let signedOut = false;

  let server = httpd_setup({
    "/session/destroy": function(request, response) {
      if (!signedOut) {
        signedOut = true;
        do_check_true(request.hasHeader("Authorization"));
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.bodyOutputStream.write(signoutMessage, signoutMessage.length);
        return;
      }

      // Error trying to sign out of nonexistent account
      response.setStatusLine(request.httpVersion, 400, "Bad request");
      response.bodyOutputStream.write(errorMessage, errorMessage.length);
      return;
    },
  });

  let client = new FxAccountsClient(server.baseURI);
  let result = yield client.signOut("FakeSession");
  do_check_eq(typeof result, "object");

  // Trigger error path
  try {
    result = yield client.signOut("FakeSession");
    do_throw("Expected to catch an exception");
  } catch(expectedError) {
    do_check_eq(102, expectedError.errno);
  }

  yield deferredStop(server);
});

add_task(function test_recoveryEmailStatus() {
  let emailStatus = JSON.stringify({verified: true});
  let errorMessage = JSON.stringify({code: 400, errno: 102, error: "doesn't exist"});
  let tries = 0;

  let server = httpd_setup({
    "/recovery_email/status": function(request, response) {
      do_check_true(request.hasHeader("Authorization"));

      if (tries === 0) {
        tries += 1;
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.bodyOutputStream.write(emailStatus, emailStatus.length);
        return;
      }

      // Second call gets an error trying to query a nonexistent account
      response.setStatusLine(request.httpVersion, 400, "Bad request");
      response.bodyOutputStream.write(errorMessage, errorMessage.length);
      return;
    },
  });

  let client = new FxAccountsClient(server.baseURI);
  let result = yield client.recoveryEmailStatus(FAKE_SESSION_TOKEN);
  do_check_eq(result.verified, true);

  // Trigger error path
  try {
    result = yield client.recoveryEmailStatus("some bogus session");
    do_throw("Expected to catch an exception");
  } catch(expectedError) {
    do_check_eq(102, expectedError.errno);
  }

  yield deferredStop(server);
});

add_task(function test_resendVerificationEmail() {
  let emptyMessage = "{}";
  let errorMessage = JSON.stringify({code: 400, errno: 102, error: "doesn't exist"});
  let tries = 0;

  let server = httpd_setup({
    "/recovery_email/resend_code": function(request, response) {
      do_check_true(request.hasHeader("Authorization"));
      if (tries === 0) {
        tries += 1;
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.bodyOutputStream.write(emptyMessage, emptyMessage.length);
        return;
      }

      // Second call gets an error trying to query a nonexistent account
      response.setStatusLine(request.httpVersion, 400, "Bad request");
      response.bodyOutputStream.write(errorMessage, errorMessage.length);
      return;
    },
  });

  let client = new FxAccountsClient(server.baseURI);
  let result = yield client.resendVerificationEmail(FAKE_SESSION_TOKEN);
  do_check_eq(JSON.stringify(result), emptyMessage);

  // Trigger error path
  try {
    result = yield client.resendVerificationEmail("some bogus session");
    do_throw("Expected to catch an exception");
  } catch(expectedError) {
    do_check_eq(102, expectedError.errno);
  }

  yield deferredStop(server);
});

add_task(function test_accountKeys() {
  // Four calls to accountKeys().  The first one should work correctly, and we
  // should get a valid bundle back, in exchange for our keyFetch token, from
  // which we correctly derive kA and wrapKB.  The subsequent three calls
  // should all trigger separate error paths.
  let responseMessage = JSON.stringify({bundle: ACCOUNT_KEYS.response});
  let errorMessage = JSON.stringify({code: 400, errno: 102, error: "doesn't exist"});
  let emptyMessage = "{}";
  let attempt = 0;

  let server = httpd_setup({
    "/account/keys": function(request, response) {
      do_check_true(request.hasHeader("Authorization"));
      attempt += 1;

      switch(attempt) {
        case 1:
          // First time succeeds
          response.setStatusLine(request.httpVersion, 200, "OK");
          response.bodyOutputStream.write(responseMessage, responseMessage.length);
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
            bundle: ACCOUNT_KEYS.response.slice(0, -1) + "1"
          });
          response.setStatusLine(request.httpVersion, 200, "OK");
          response.bodyOutputStream.write(garbageResponse, garbageResponse.length);
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
  let result = yield client.accountKeys(ACCOUNT_KEYS.keyFetch);
  do_check_eq(CommonUtils.hexToBytes(ACCOUNT_KEYS.kA), result.kA);
  do_check_eq(CommonUtils.hexToBytes(ACCOUNT_KEYS.wrapKB), result.wrapKB);

  // Second try, empty bundle should trigger error
  try {
    result = yield client.accountKeys(ACCOUNT_KEYS.keyFetch);
    do_throw("Expected to catch an exception");
  } catch(expectedError) {
    do_check_eq(expectedError.message, "failed to retrieve keys");
  }

  // Third try, bad bundle results in MAC error
  try {
    result = yield client.accountKeys(ACCOUNT_KEYS.keyFetch);
    do_throw("Expected to catch an exception");
  } catch(expectedError) {
    do_check_eq(expectedError.message, "error unbundling encryption keys");
  }

  // Fourth try, pretend account doesn't exist
  try {
    result = yield client.accountKeys(ACCOUNT_KEYS.keyFetch);
    do_throw("Expected to catch an exception");
  } catch(expectedError) {
    do_check_eq(102, expectedError.errno);
  }

  yield deferredStop(server);
});

add_task(function test_signCertificate() {
  let certSignMessage = JSON.stringify({cert: {bar: "baz"}});
  let errorMessage = JSON.stringify({code: 400, errno: 102, error: "doesn't exist"});
  let tries = 0;

  let server = httpd_setup({
    "/certificate/sign": function(request, response) {
      do_check_true(request.hasHeader("Authorization"));

      if (tries === 0) {
        tries += 1;
        let body = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
        let jsonBody = JSON.parse(body);
        do_check_eq(JSON.parse(jsonBody.publicKey).foo, "bar");
        do_check_eq(jsonBody.duration, 600);
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.bodyOutputStream.write(certSignMessage, certSignMessage.length);
        return;
      }

      // Second attempt, trigger error
      response.setStatusLine(request.httpVersion, 400, "Bad request");
      response.bodyOutputStream.write(errorMessage, errorMessage.length);
      return;
    },
  });

  let client = new FxAccountsClient(server.baseURI);
  let result = yield client.signCertificate(FAKE_SESSION_TOKEN, JSON.stringify({foo: "bar"}), 600);
  do_check_eq("baz", result.bar);

  // Account doesn't exist
  try {
    result = yield client.signCertificate("bogus", JSON.stringify({foo: "bar"}), 600);
    do_throw("Expected to catch an exception");
  } catch(expectedError) {
    do_check_eq(102, expectedError.errno);
  }

  yield deferredStop(server);
});

add_task(function test_accountExists() {
  let sessionMessage = JSON.stringify({sessionToken: FAKE_SESSION_TOKEN});
  let existsMessage = JSON.stringify({error: "wrong password", code: 400, errno: 103});
  let doesntExistMessage = JSON.stringify({error: "no such account", code: 400, errno: 102});
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
          response.bodyOutputStream.write(doesntExistMessage, doesntExistMessage.length);
          break;

        // This user throws an unexpected response
        // This will reject the client signIn promise
        case "i.break.things@example.com":
          response.setStatusLine(request.httpVersion, 500, "Alas");
          response.bodyOutputStream.write(emptyMessage, emptyMessage.length);
          break;

        default:
          throw new Error("Unexpected login from " + jsonBody.email);
          break;
      }
    },
  });

  let client = new FxAccountsClient(server.baseURI);
  let result;

  result = yield client.accountExists("i.exist@example.com");
  do_check_true(result);

  result = yield client.accountExists("i.also.exist@example.com");
  do_check_true(result);

  result = yield client.accountExists("i.dont.exist@example.com");
  do_check_false(result);

  try {
    result = yield client.accountExists("i.break.things@example.com");
    do_throw("Expected to catch an exception");
  } catch(unexpectedError) {
    do_check_eq(unexpectedError.code, 500);
  }

  yield deferredStop(server);
});

add_task(function test_email_case() {
  let canonicalEmail = "greta.garbo@gmail.com";
  let clientEmail = "Greta.Garbo@gmail.COM";
  let attempts = 0;

  function writeResp(response, msg) {
    if (typeof msg === "object") {
      msg = JSON.stringify(msg);
    }
    response.bodyOutputStream.write(msg, msg.length);
  }

  let server = httpd_setup(
    {
      "/account/login": function(request, response) {
        response.setHeader("Content-Type", "application/json; charset=utf-8");
        attempts += 1;
        if (attempts > 2) {
          response.setStatusLine(request.httpVersion, 429, "Sorry, you had your chance");
          return writeResp(response, "");
        }

        let body = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
        let jsonBody = JSON.parse(body);
        let email = jsonBody.email;

        // If the client has the wrong case on the email, we return a 400, with
        // the capitalization of the email as saved in the accounts database.
        if (email == canonicalEmail) {
          response.setStatusLine(request.httpVersion, 200, "Yay");
          return writeResp(response, {areWeHappy: "yes"});
        }

        response.setStatusLine(request.httpVersion, 400, "Incorrect email case");
        return writeResp(response, {
          code: 400,
          errno: 120,
          error: "Incorrect email case",
          email: canonicalEmail
        });
      },
    }
  );

  let client = new FxAccountsClient(server.baseURI);

  let result = yield client.signIn(clientEmail, "123456");
  do_check_eq(result.areWeHappy, "yes");
  do_check_eq(attempts, 2);

  yield deferredStop(server);
});

add_task(function test__deriveHawkCredentials() {
  let client = new FxAccountsClient("https://example.org");

  let credentials = client._deriveHawkCredentials(
    SESSION_KEYS.sessionToken, "sessionToken");

  do_check_eq(credentials.algorithm, "sha256");
  do_check_eq(credentials.id, SESSION_KEYS.tokenID);
  do_check_eq(CommonUtils.bytesAsHex(credentials.key), SESSION_KEYS.reqHMACkey);
});

// turn formatted test vectors into normal hex strings
function h(hexStr) {
  return hexStr.replace(/\s+/g, "");
}
