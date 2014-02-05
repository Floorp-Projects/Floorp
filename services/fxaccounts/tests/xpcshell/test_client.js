/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/FxAccountsClient.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-crypto/utils.js");

const FAKE_SESSION_TOKEN = "a0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebf";

// Test vectors for account keys:
// https://wiki.mozilla.org/Identity/AttachedServices/KeyServerProtocol#.2Faccount.2Fkeys
const ACCOUNT_KEYS = {
  keyFetch: h("8081828384858687 88898a8b8c8d8e8f"+
              "9091929394959697 98999a9b9c9d9e9f"),

  response: h("ee5c58845c7c9412 b11bbd20920c2fdd"+
              "d83c33c9cd2c2de2 d66b222613364636"+
              "c2c0f8cfbb7c6304 72c0bd88451342c6"+
              "c05b14ce342c5ad4 6ad89e84464c993c"+
              "3927d30230157d08 17a077eef4b20d97"+
              "6f7a97363faf3f06 4c003ada7d01aa70"),

  kA:       h("2021222324252627 28292a2b2c2d2e2f"+
              "3031323334353637 38393a3b3c3d3e3f"),

  wrapKB:   h("4041424344454647 48494a4b4c4d4e4f"+
              "5051525354555657 58595a5b5c5d5e5f"),
};

function run_test() {
  run_next_test();
}

function deferredStop(server) {
  let deferred = Promise.defer();
  server.stop(deferred.resolve);
  return deferred.promise;
}

add_test(function test_hawk_credentials() {
  let client = new FxAccountsClient();

  let sessionToken = "a0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebf";
  let result = client._deriveHawkCredentials(sessionToken, "session");

  do_check_eq(result.id, "639503a218ffbb62983e9628be5cd64a0438d0ae81b2b9dadeb900a83470bc6b");
  do_check_eq(CommonUtils.bytesAsHex(result.key), "3a0188943837ab228fe74e759566d0e4837cbcc7494157aac4da82025b2811b2");

  run_next_test();
});

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
    do_throw("Expected an error");
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
    do_throw("Expected an error");
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
    do_throw("Expected an error");
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
    do_throw("Expected an error");
  } catch(expectedError) {
    do_check_eq(101, expectedError.errno);
  }

  yield deferredStop(server);
  run_next_test();
});

add_task(function test_signIn() {
  let sessionMessage = JSON.stringify({sessionToken: FAKE_SESSION_TOKEN});
  let errorMessage = JSON.stringify({code: 400, errno: 102, error: "doesn't exist"});
  let server = httpd_setup({
    "/account/login": function(request, response) {
      let body = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
      let jsonBody = JSON.parse(body);

      if (jsonBody.email == "mé@example.com") {
        do_check_eq(jsonBody.authPW, "08b9d111196b8408e8ed92439da49206c8ecfbf343df0ae1ecefcd1e0174a8b6");
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.bodyOutputStream.write(sessionMessage, sessionMessage.length);
        return;
      }

      // Error trying to sign in to nonexistent account
      response.setStatusLine(request.httpVersion, 400, "Bad request");
      response.bodyOutputStream.write(errorMessage, errorMessage.length);
      return;
    },
  });

  let client = new FxAccountsClient(server.baseURI);
  let result = yield client.signIn('mé@example.com', 'bigsecret');
  do_check_eq(FAKE_SESSION_TOKEN, result.sessionToken);

  // Trigger error path
  try {
    result = yield client.signIn("yøü@bad.example.org", "nofear");
    do_throw("Expected an error");
  } catch(expectedError) {
    do_check_eq(102, expectedError.errno);
  }

  yield deferredStop(server);
  run_next_test();
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
    do_throw("Expected an error");
  } catch(expectedError) {
    do_check_eq(102, expectedError.errno);
  }

  yield deferredStop(server);
  run_next_test();
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
    do_throw("Expected an error");
  } catch(expectedError) {
    do_check_eq(102, expectedError.errno);
  }

  yield deferredStop(server);
  run_next_test();
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
    do_throw("Expected an error");
  } catch(expectedError) {
    do_check_eq(102, expectedError.errno);
  }

  yield deferredStop(server);
  run_next_test();
});

add_task(function test_accountKeys_success() {
  let responseMessage = JSON.stringify({bundle: ACCOUNT_KEYS.response});

  let server = httpd_setup({
    "/account/keys": function(request, response) {
      do_check_true(request.hasHeader("Authorization"));
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.bodyOutputStream.write(responseMessage, responseMessage.length);
    },
  });

  let client = new FxAccountsClient(server.baseURI);
  let result = yield client.accountKeys(ACCOUNT_KEYS.keyFetch);

  // Confirm that accountKeys() did the math right
  do_check_eq(CommonUtils.hexToBytes(ACCOUNT_KEYS.kA), result.kA);
  do_check_eq(CommonUtils.hexToBytes(ACCOUNT_KEYS.wrapKB), result.wrapKB);

  yield deferredStop(server);
  run_next_test();
});

add_task(function test_accountKeys_emptyResponse() {
  let emptyMessage = "{}";
  let failedAsExpected = false;

  let server = httpd_setup({
    "/account/keys": function(request, response) {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.bodyOutputStream.write(emptyMessage, emptyMessage.length);
    },
  });

  let client = new FxAccountsClient(server.baseURI);

  // Empty bundle should trigger error
  try {
    result = yield client.accountKeys(ACCOUNT_KEYS.keyFetch);
    do_throw("Expected an error");
  } catch(expectedError) {
    do_check_eq(expectedError.message, "failed to retrieve keys");
    failedAsExpected = true;
  }

  do_check_true(failedAsExpected);

  yield deferredStop(server);
  run_next_test();
});

add_task(function test_accountKeys_garbageResponse() {
  let failedAsExpected = false;

  let server = httpd_setup({
    "/account/keys": function(request, response) {
      // tweak a byte
      let garbage = ACCOUNT_KEYS.response.slice(0, -1) + "1";
      let responseMessage = JSON.stringify({bundle: garbage});
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.bodyOutputStream.write(responseMessage, responseMessage.length);
    },
  });

  let client = new FxAccountsClient(server.baseURI);

  // Bad bundle results in MAC error
  try {
    result = yield client.accountKeys(ACCOUNT_KEYS.keyFetch);
    do_throw("Expected an error");
  } catch(expectedError) {
    do_check_eq(expectedError.message, "error unbundling encryption keys");
    failedAsExpected = true;
  }

  do_check_true(failedAsExpected);

  yield deferredStop(server);
  run_next_test();
});

add_task(function test_accountKeys_doesNotExist() {
  let errorMessage = JSON.stringify({code: 400, errno: 102, error: "doesn't exist"});
  let failedAsExpected = false;

  let server = httpd_setup({
    "/account/keys": function(request, response) {
      response.setStatusLine(request.httpVersion, 400, "Bad request");
      response.bodyOutputStream.write(errorMessage, errorMessage.length);
    },
  });

  let client = new FxAccountsClient(server.baseURI);

  // Error: Account does not exist
  try {
    result = yield client.accountKeys(ACCOUNT_KEYS.keyFetch);
    do_throw("Expected an error");
  } catch(expectedError) {
    do_check_eq(102, expectedError.errno);
    failedAsExpected = true;
  }

  do_check_true(failedAsExpected);

  yield deferredStop(server);
  run_next_test();
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
    do_throw("Expected an error");
  } catch(expectedError) {
    do_check_eq(102, expectedError.errno);
  }

  yield deferredStop(server);
  run_next_test();
});

add_test(function test_accountExists_yes() {
  let existsMessage = JSON.stringify({error: "wrong password", code: 400, errno: 103});
  let expectedFailure = false;

  let server = httpd_setup({
    "/account/login": function(request, response) {
      response.setStatusLine(request.httpVersion, 400, "Bad request");
      response.bodyOutputStream.write(existsMessage, existsMessage.length);
    },
  });

  let client = new FxAccountsClient(server.baseURI);

  try {
    yield client.accountExists("i.exist@example.com");
    do_throw("Expected an error");
  } catch(expectedError) {
    do_check_eq(expectedError.code, 400);
    do_check_eq(expectedError.errno, 103);
    expectedFailure = true;
  }
  do_check_true(expectedFailure);

  yield deferredStop(server);
  run_next_test();
});

add_test(function test_accountExists_no() {
  let doesntExistMessage = JSON.stringify({error: "no such account", code: 400, errno: 102});
  let expectedFailure = false;

  let server = httpd_setup({
    "/account/login": function(request, response) {
      response.setStatusLine(request.httpVersion, 400, "Bad request");
      response.bodyOutputStream.write(doesntExistMessage, doesntExistMessage.length);
    },
  });

  let client = new FxAccountsClient(server.baseURI);

  try {
    yield client.accountExists("i.dont.exist@example.com");
    do_throw("Expected an error");
  } catch(expectedError) {
    do_check_eq(expectedError.code, 400);
    do_check_eq(expectedError.errno, 102);
    expectedFailure = true;
  }
  do_check_true(expectedFailure);

  yield deferredStop(server);
  run_next_test();
});

add_test(function test_accountExists_error() {
  let errorMessage = JSON.stringify({error: "ouch!", code: 500});
  let expectedFailure = false;

  let server = httpd_setup({
    "/account/login": function(request, response) {
      response.setStatusLine(request.httpVersion, 500, "Alas");
      response.bodyOutputStream.write(errorMessage, errorMessage.length);
    },
  });

  let client = new FxAccountsClient(server.baseURI);

  try {
    yield client.accountExists("i.break.stuff@example.com");
    do_throw("Expected an error");
  } catch(expectedError) {
    do_check_eq(expectedError.code, 500);
    expectedFailure = true;
  }
  do_check_true(expectedFailure);

  yield deferredStop(server);
  run_next_test();
});

// turn formatted test vectors into normal hex strings
function h(hexStr) {
  return hexStr.replace(/\s+/g, "");
}
