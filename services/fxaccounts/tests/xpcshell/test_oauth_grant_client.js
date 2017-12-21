/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/FxAccountsOAuthGrantClient.jsm");

const CLIENT_OPTIONS = {
  serverURL: "https://127.0.0.1:9010/v1",
  client_id: "abc123"
};

const STATUS_SUCCESS = 200;

/**
 * Mock request responder
 * @param {String} response
 *        Mocked raw response from the server
 * @returns {Function}
 */
var mockResponse = function(response) {
  return function() {
    return {
      setHeader() {},
      post() {
        this.response = response;
        this.onComplete();
      }
    };
  };
};

/**
 * Mock request error responder
 * @param {Error} error
 *        Error object
 * @returns {Function}
 */
var mockResponseError = function(error) {
  return function() {
    return {
      setHeader() {},
      post() {
        this.onComplete(error);
      }
    };
  };
};

add_test(function missingParams() {
  let client = new FxAccountsOAuthGrantClient(CLIENT_OPTIONS);
  try {
    client.getTokenFromAssertion();
  } catch (e) {
    Assert.equal(e.message, "Missing 'assertion' parameter");
  }

  try {
    client.getTokenFromAssertion("assertion");
  } catch (e) {
    Assert.equal(e.message, "Missing 'scope' parameter");
  }

  run_next_test();
});

add_test(function successfulResponse() {
  let client = new FxAccountsOAuthGrantClient(CLIENT_OPTIONS);
  let response = {
    success: true,
    status: STATUS_SUCCESS,
    body: "{\"access_token\":\"http://example.com/image.jpeg\",\"id\":\"0d5c1a89b8c54580b8e3e8adadae864a\"}",
  };

  client._Request = new mockResponse(response);
  client.getTokenFromAssertion("assertion", "scope")
    .then(
      function(result) {
        Assert.equal(result.access_token, "http://example.com/image.jpeg");
        run_next_test();
      }
    );
});

add_test(function successfulDestroy() {
  let client = new FxAccountsOAuthGrantClient(CLIENT_OPTIONS);
  let response = {
    success: true,
    status: STATUS_SUCCESS,
    body: "{}",
  };

  client._Request = new mockResponse(response);
  client.destroyToken("deadbeef").then(run_next_test);
});

add_test(function parseErrorResponse() {
  let client = new FxAccountsOAuthGrantClient(CLIENT_OPTIONS);
  let response = {
    success: true,
    status: STATUS_SUCCESS,
    body: "unexpected",
  };

  client._Request = new mockResponse(response);
  client.getTokenFromAssertion("assertion", "scope")
    .catch(function(e) {
      Assert.equal(e.name, "FxAccountsOAuthGrantClientError");
      Assert.equal(e.code, STATUS_SUCCESS);
      Assert.equal(e.errno, ERRNO_PARSE);
      Assert.equal(e.error, ERROR_PARSE);
      Assert.equal(e.message, "unexpected");
      run_next_test();
    });
});

add_test(function serverErrorResponse() {
  let client = new FxAccountsOAuthGrantClient(CLIENT_OPTIONS);
  let response = {
    status: 400,
    body: "{ \"code\": 400, \"errno\": 104, \"error\": \"Bad Request\", \"message\": \"Unauthorized\", \"reason\": \"Invalid fxa assertion\" }",
  };

  client._Request = new mockResponse(response);
  client.getTokenFromAssertion("blah", "scope")
    .catch(function(e) {
      Assert.equal(e.name, "FxAccountsOAuthGrantClientError");
      Assert.equal(e.code, 400);
      Assert.equal(e.errno, ERRNO_INVALID_FXA_ASSERTION);
      Assert.equal(e.error, "Bad Request");
      Assert.equal(e.message, "Unauthorized");
      run_next_test();
    });
});

add_test(function networkErrorResponse() {
  let client = new FxAccountsOAuthGrantClient({
    serverURL: "https://domain.dummy",
    client_id: "abc123"
  });
  Services.prefs.setBoolPref("identity.fxaccounts.skipDeviceRegistration", true);
  client.getTokenFromAssertion("assertion", "scope")
    .catch(function(e) {
      Assert.equal(e.name, "FxAccountsOAuthGrantClientError");
      Assert.equal(e.code, null);
      Assert.equal(e.errno, ERRNO_NETWORK);
      Assert.equal(e.error, ERROR_NETWORK);
      run_next_test();
    }).catch(() => {}).then(() =>
      Services.prefs.clearUserPref("identity.fxaccounts.skipDeviceRegistration"));
});

add_test(function unsupportedMethod() {
  let client = new FxAccountsOAuthGrantClient(CLIENT_OPTIONS);

  return client._createRequest("/", "PUT")
    .catch(function(e) {
      Assert.equal(e.name, "FxAccountsOAuthGrantClientError");
      Assert.equal(e.code, ERROR_CODE_METHOD_NOT_ALLOWED);
      Assert.equal(e.errno, ERRNO_NETWORK);
      Assert.equal(e.error, ERROR_NETWORK);
      Assert.equal(e.message, ERROR_MSG_METHOD_NOT_ALLOWED);
      run_next_test();
    });
});

add_test(function onCompleteRequestError() {
  let client = new FxAccountsOAuthGrantClient(CLIENT_OPTIONS);
  client._Request = new mockResponseError(new Error("onComplete error"));
  client.getTokenFromAssertion("assertion", "scope")
    .catch(function(e) {
      Assert.equal(e.name, "FxAccountsOAuthGrantClientError");
      Assert.equal(e.code, null);
      Assert.equal(e.errno, ERRNO_NETWORK);
      Assert.equal(e.error, ERROR_NETWORK);
      Assert.equal(e.message, "Error: onComplete error");
      run_next_test();
    });
});

add_test(function incorrectErrno() {
  let client = new FxAccountsOAuthGrantClient(CLIENT_OPTIONS);
  let response = {
    status: 400,
    body: "{ \"code\": 400, \"errno\": \"bad errno\", \"error\": \"Bad Request\", \"message\": \"Unauthorized\", \"reason\": \"Invalid fxa assertion\" }",
  };

  client._Request = new mockResponse(response);
  client.getTokenFromAssertion("blah", "scope")
    .catch(function(e) {
      Assert.equal(e.name, "FxAccountsOAuthGrantClientError");
      Assert.equal(e.code, 400);
      Assert.equal(e.errno, ERRNO_UNKNOWN_ERROR);
      Assert.equal(e.error, "Bad Request");
      Assert.equal(e.message, "Unauthorized");
      run_next_test();
    });
});

add_test(function constructorTests() {
  validationHelper(undefined,
    "Error: Missing configuration options");

  validationHelper({},
    "Error: Missing 'serverURL' parameter");

  validationHelper({ serverURL: "https://example.com" },
    "Error: Missing 'client_id' parameter");

  validationHelper({ serverURL: "https://example.com" },
    "Error: Missing 'client_id' parameter");

  validationHelper({ client_id: "123ABC" },
    "Error: Missing 'serverURL' parameter");

  validationHelper({ client_id: "123ABC", serverURL: "http://example.com" },
    "Error: 'serverURL' must be HTTPS");

  try {
    Services.prefs.setBoolPref("identity.fxaccounts.allowHttp", true);
    validationHelper({ client_id: "123ABC", serverURL: "http://example.com" }, null);
  } finally {
    Services.prefs.clearUserPref("identity.fxaccounts.allowHttp");
  }



  run_next_test();
});

add_test(function errorTests() {
  let error1 = new FxAccountsOAuthGrantClientError();
  Assert.equal(error1.name, "FxAccountsOAuthGrantClientError");
  Assert.equal(error1.code, null);
  Assert.equal(error1.errno, ERRNO_UNKNOWN_ERROR);
  Assert.equal(error1.error, ERROR_UNKNOWN);
  Assert.equal(error1.message, null);

  let error2 = new FxAccountsOAuthGrantClientError({
    code: STATUS_SUCCESS,
    errno: 1,
    error: "Error",
    message: "Something",
  });
  let fields2 = error2._toStringFields();
  let statusCode = 1;

  Assert.equal(error2.name, "FxAccountsOAuthGrantClientError");
  Assert.equal(error2.code, STATUS_SUCCESS);
  Assert.equal(error2.errno, statusCode);
  Assert.equal(error2.error, "Error");
  Assert.equal(error2.message, "Something");

  Assert.equal(fields2.name, "FxAccountsOAuthGrantClientError");
  Assert.equal(fields2.code, STATUS_SUCCESS);
  Assert.equal(fields2.errno, statusCode);
  Assert.equal(fields2.error, "Error");
  Assert.equal(fields2.message, "Something");

  Assert.ok(error2.toString().indexOf("Something") >= 0);
  run_next_test();
});


add_test(function networkErrorResponse() {
  let client = new FxAccountsOAuthGrantClient({
    serverURL: "https://domain.dummy",
    client_id: "abc123"
  });
  Services.prefs.setBoolPref("identity.fxaccounts.skipDeviceRegistration", true);
  client.getTokenFromAssertion("assertion", "scope")
    .catch(function(e) {
      Assert.equal(e.name, "FxAccountsOAuthGrantClientError");
      Assert.equal(e.code, null);
      Assert.equal(e.errno, ERRNO_NETWORK);
      Assert.equal(e.error, ERROR_NETWORK);
      run_next_test();
    }).catch(() => {}).then(() =>
      Services.prefs.clearUserPref("identity.fxaccounts.skipDeviceRegistration"));
});


/**
 * Quick way to test the "FxAccountsOAuthGrantClient" constructor.
 *
 * @param {Object} options
 *        FxAccountsOAuthGrantClient constructor options
 * @param {String} expected
 *        Expected error message, or null if it's expected to pass.
 * @returns {*}
 */
function validationHelper(options, expected) {
  try {
    new FxAccountsOAuthGrantClient(options);
  } catch (e) {
    return Assert.equal(e.toString(), expected);
  }
  return Assert.equal(expected, null);
}
