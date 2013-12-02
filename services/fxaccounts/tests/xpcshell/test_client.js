/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/FxAccountsClient.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://services-common/utils.js");

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
  } catch (e) {
    do_check_eq(500, e.code);
    do_check_eq("Internal Server Error", e.message);
  }

  yield deferredStop(server);
});

add_task(function test_api_endpoints() {
  let sessionMessage = JSON.stringify({sessionToken: "NotARealToken"});
  let creationMessage = JSON.stringify({uid: "NotARealUid"});
  let signoutMessage = JSON.stringify({});
  let certSignMessage = JSON.stringify({cert: {bar: "baz"}});
  let emailStatus = JSON.stringify({verified: true});

  let authStarts = 0;

  function writeResp(response, msg) {
    response.bodyOutputStream.write(msg, msg.length);
  }

  let server = httpd_setup(
    {
      "/raw_password/account/create": function(request, response) {
        let body = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
        let jsonBody = JSON.parse(body);
        do_check_eq(jsonBody.email, "796f75406578616d706c652e636f6d");
        do_check_eq(jsonBody.password, "biggersecret");

        response.setStatusLine(request.httpVersion, 200, "OK");
        response.bodyOutputStream.write(creationMessage, creationMessage.length);
      },
      "/raw_password/session/create": function(request, response) {
        let body = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
        let jsonBody = JSON.parse(body);
        if (jsonBody.password === "bigsecret") {
          do_check_eq(jsonBody.email, "6dc3a9406578616d706c652e636f6d");
        } else if (jsonBody.password === "biggersecret") {
          do_check_eq(jsonBody.email, "796f75406578616d706c652e636f6d");
        }

        response.setStatusLine(request.httpVersion, 200, "OK");
        response.bodyOutputStream.write(sessionMessage, sessionMessage.length);
      },
      "/recovery_email/status": function(request, response) {
        do_check_true(request.hasHeader("Authorization"));
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.bodyOutputStream.write(emailStatus, emailStatus.length);
      },
      "/session/destroy": function(request, response) {
        do_check_true(request.hasHeader("Authorization"));
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.bodyOutputStream.write(signoutMessage, signoutMessage.length);
      },
      "/certificate/sign": function(request, response) {
        do_check_true(request.hasHeader("Authorization"));
        let body = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
        let jsonBody = JSON.parse(body);
        do_check_eq(JSON.parse(jsonBody.publicKey).foo, "bar");
        do_check_eq(jsonBody.duration, 600);
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.bodyOutputStream.write(certSignMessage, certSignMessage.length);
      },
      "/auth/start": function(request, response) {
        if (authStarts === 0) {
          response.setStatusLine(request.httpVersion, 200, "OK");
          writeResp(response, JSON.stringify({}));
        } else if (authStarts === 1) {
          response.setStatusLine(request.httpVersion, 400, "NOT OK");
          writeResp(response, JSON.stringify({errno: 102, error: "no such account"}));
        } else if (authStarts === 2) {
          response.setStatusLine(request.httpVersion, 400, "NOT OK");
          writeResp(response, JSON.stringify({errno: 107, error: "boom"}));
        }
        authStarts++;
      },
    }
  );

  let client = new FxAccountsClient(server.baseURI);
  let result = undefined;

  result = yield client.signUp('you@example.com', 'biggersecret');
  do_check_eq("NotARealUid", result.uid);

  result = yield client.signIn('mé@example.com', 'bigsecret');
  do_check_eq("NotARealToken", result.sessionToken);

  result = yield client.signOut('NotARealToken');
  do_check_eq(typeof result, "object");

  result = yield client.recoveryEmailStatus('NotARealToken');
  do_check_eq(result.verified, true);

  result = yield client.signCertificate('NotARealToken', JSON.stringify({foo: "bar"}), 600);
  do_check_eq("baz", result.bar);

  result = yield client.accountExists('hey@example.com');
  do_check_eq(result, true);
  result = yield client.accountExists('hey2@example.com');
  do_check_eq(result, false);
  try {
    result = yield client.accountExists('hey3@example.com');
  } catch(e) {
    do_check_eq(e.errno, 107);
  }

  yield deferredStop(server);
});

add_task(function test_error_response() {
  let errorMessage = JSON.stringify({error: "Oops", code: 400, errno: 99});

  let server = httpd_setup(
    {
      "/raw_password/session/create": function(request, response) {
        let body = CommonUtils.readBytesFromInputStream(request.bodyInputStream);

        response.setStatusLine(request.httpVersion, 400, "NOT OK");
        response.bodyOutputStream.write(errorMessage, errorMessage.length);
      },
    }
  );

  let client = new FxAccountsClient(server.baseURI);

  try {
    let result = yield client.signIn('mé@example.com', 'bigsecret');
  } catch(result) {
    do_check_eq("Oops", result.error);
    do_check_eq(400, result.code);
    do_check_eq(99, result.errno);
  }

  yield deferredStop(server);
});
