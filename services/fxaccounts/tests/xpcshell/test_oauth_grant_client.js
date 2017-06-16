/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/FxAccountsOAuthGrantClient.jsm");

const CLIENT_OPTIONS = {
  serverURL: "http://127.0.0.1:9010/v1",
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
    client.getTokenFromAssertion()
  } catch (e) {
    do_check_eq(e.message, "Missing 'assertion' parameter");
  }

  try {
    client.getTokenFromAssertion("assertion")
  } catch (e) {
    do_check_eq(e.message, "Missing 'scope' parameter");
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
        do_check_eq(result.access_token, "http://example.com/image.jpeg");
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
      do_check_eq(e.name, "FxAccountsOAuthGrantClientError");
      do_check_eq(e.code, STATUS_SUCCESS);
      do_check_eq(e.errno, ERRNO_PARSE);
      do_check_eq(e.error, ERROR_PARSE);
      do_check_eq(e.message, "unexpected");
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
      do_check_eq(e.name, "FxAccountsOAuthGrantClientError");
      do_check_eq(e.code, 400);
      do_check_eq(e.errno, ERRNO_INVALID_FXA_ASSERTION);
      do_check_eq(e.error, "Bad Request");
      do_check_eq(e.message, "Unauthorized");
      run_next_test();
    });
});

add_test(function networkErrorResponse() {
  let client = new FxAccountsOAuthGrantClient({
    serverURL: "http://domain.dummy",
    client_id: "abc123"
  });
  Services.prefs.setBoolPref("identity.fxaccounts.skipDeviceRegistration", true);
  client.getTokenFromAssertion("assertion", "scope")
    .catch(function(e) {
      do_check_eq(e.name, "FxAccountsOAuthGrantClientError");
      do_check_eq(e.code, null);
      do_check_eq(e.errno, ERRNO_NETWORK);
      do_check_eq(e.error, ERROR_NETWORK);
      run_next_test();
    }).catch(() => {}).then(() =>
      Services.prefs.clearUserPref("identity.fxaccounts.skipDeviceRegistration"));
});

add_test(function unsupportedMethod() {
  let client = new FxAccountsOAuthGrantClient(CLIENT_OPTIONS);

  return client._createRequest("/", "PUT")
    .catch(function(e) {
      do_check_eq(e.name, "FxAccountsOAuthGrantClientError");
      do_check_eq(e.code, ERROR_CODE_METHOD_NOT_ALLOWED);
      do_check_eq(e.errno, ERRNO_NETWORK);
      do_check_eq(e.error, ERROR_NETWORK);
      do_check_eq(e.message, ERROR_MSG_METHOD_NOT_ALLOWED);
      run_next_test();
    });
});

add_test(function onCompleteRequestError() {
  let client = new FxAccountsOAuthGrantClient(CLIENT_OPTIONS);
  client._Request = new mockResponseError(new Error("onComplete error"));
  client.getTokenFromAssertion("assertion", "scope")
    .catch(function(e) {
      do_check_eq(e.name, "FxAccountsOAuthGrantClientError");
      do_check_eq(e.code, null);
      do_check_eq(e.errno, ERRNO_NETWORK);
      do_check_eq(e.error, ERROR_NETWORK);
      do_check_eq(e.message, "Error: onComplete error");
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
      do_check_eq(e.name, "FxAccountsOAuthGrantClientError");
      do_check_eq(e.code, 400);
      do_check_eq(e.errno, ERRNO_UNKNOWN_ERROR);
      do_check_eq(e.error, "Bad Request");
      do_check_eq(e.message, "Unauthorized");
      run_next_test();
    });
});

add_test(function constructorTests() {
  validationHelper(undefined,
    "Error: Missing configuration options");

  validationHelper({},
    "Error: Missing 'serverURL' parameter");

  validationHelper({ serverURL: "http://example.com" },
    "Error: Missing 'client_id' parameter");

  validationHelper({ client_id: "123ABC" },
    "Error: Missing 'serverURL' parameter");

  validationHelper({ client_id: "123ABC", serverURL: "badUrl" },
    "Error: Invalid 'serverURL'");

  run_next_test();
});

add_test(function errorTests() {
  let error1 = new FxAccountsOAuthGrantClientError();
  do_check_eq(error1.name, "FxAccountsOAuthGrantClientError");
  do_check_eq(error1.code, null);
  do_check_eq(error1.errno, ERRNO_UNKNOWN_ERROR);
  do_check_eq(error1.error, ERROR_UNKNOWN);
  do_check_eq(error1.message, null);

  let error2 = new FxAccountsOAuthGrantClientError({
    code: STATUS_SUCCESS,
    errno: 1,
    error: "Error",
    message: "Something",
  });
  let fields2 = error2._toStringFields();
  let statusCode = 1;

  do_check_eq(error2.name, "FxAccountsOAuthGrantClientError");
  do_check_eq(error2.code, STATUS_SUCCESS);
  do_check_eq(error2.errno, statusCode);
  do_check_eq(error2.error, "Error");
  do_check_eq(error2.message, "Something");

  do_check_eq(fields2.name, "FxAccountsOAuthGrantClientError");
  do_check_eq(fields2.code, STATUS_SUCCESS);
  do_check_eq(fields2.errno, statusCode);
  do_check_eq(fields2.error, "Error");
  do_check_eq(fields2.message, "Something");

  do_check_true(error2.toString().indexOf("Something") >= 0);
  run_next_test();
});

function run_test() {
  run_next_test();
}

/**
 * Quick way to test the "FxAccountsOAuthGrantClient" constructor.
 *
 * @param {Object} options
 *        FxAccountsOAuthGrantClient constructor options
 * @param {String} expected
 *        Expected error message
 * @returns {*}
 */
function validationHelper(options, expected) {
  try {
    new FxAccountsOAuthGrantClient(options);
  } catch (e) {
    return do_check_eq(e.toString(), expected);
  }
  throw new Error("Validation helper error");
}
