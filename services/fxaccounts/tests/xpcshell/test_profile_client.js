/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/FxAccountsProfileClient.jsm");

const PROFILE_OPTIONS = {
  token: "123ABC",
  serverURL: "http://127.0.0.1:1111/v1",
};

const STATUS_SUCCESS = 200;

/**
 * Mock request responder
 * @param {String} response
 *        Mocked raw response from the server
 * @returns {Function}
 */
let mockResponse = function (response) {
  let Request = function (requestUri) {
    // Store the request uri so tests can inspect it
    Request._requestUri = requestUri;
    return {
      setHeader: function () {},
      get: function () {
        this.response = response;
        this.onComplete();
      }
    };
  };

  return Request;
};

/**
 * Mock request error responder
 * @param {Error} error
 *        Error object
 * @returns {Function}
 */
let mockResponseError = function (error) {
  return function () {
    return {
      setHeader: function () {},
      get: function () {
        this.onComplete(error);
      }
    };
  };
};

add_test(function successfulResponse () {
  let client = new FxAccountsProfileClient(PROFILE_OPTIONS);
  let response = {
    success: true,
    status: STATUS_SUCCESS,
    body: "{\"email\":\"someone@restmail.net\",\"uid\":\"0d5c1a89b8c54580b8e3e8adadae864a\"}",
  };

  client._Request = new mockResponse(response);
  client.fetchProfile()
    .then(
      function (result) {
        do_check_eq(client._Request._requestUri, "http://127.0.0.1:1111/v1/profile");
        do_check_eq(result.email, "someone@restmail.net");
        do_check_eq(result.uid, "0d5c1a89b8c54580b8e3e8adadae864a");
        run_next_test();
      }
    );
});

add_test(function parseErrorResponse () {
  let client = new FxAccountsProfileClient(PROFILE_OPTIONS);
  let response = {
    success: true,
    status: STATUS_SUCCESS,
    body: "unexpected",
  };

  client._Request = new mockResponse(response);
  client.fetchProfile()
    .then(
      null,
      function (e) {
        do_check_eq(e.name, "FxAccountsProfileClientError");
        do_check_eq(e.code, STATUS_SUCCESS);
        do_check_eq(e.errno, ERRNO_PARSE);
        do_check_eq(e.error, ERROR_PARSE);
        do_check_eq(e.message, "unexpected");
        run_next_test();
      }
    );
});

add_test(function serverErrorResponse () {
  let client = new FxAccountsProfileClient(PROFILE_OPTIONS);
  let response = {
    status: 401,
    body: "{ \"code\": 401, \"errno\": 100, \"error\": \"Bad Request\", \"message\": \"Unauthorized\", \"reason\": \"Bearer token not provided\" }",
  };

  client._Request = new mockResponse(response);
  client.fetchProfile()
    .then(
    null,
    function (e) {
      do_check_eq(e.name, "FxAccountsProfileClientError");
      do_check_eq(e.code, 401);
      do_check_eq(e.errno, 100);
      do_check_eq(e.error, "Bad Request");
      do_check_eq(e.message, "Unauthorized");
      run_next_test();
    }
  );
});

add_test(function networkErrorResponse () {
  let client = new FxAccountsProfileClient({
    token: "123ABC",
    serverURL: "http://"
  });
  client.fetchProfile()
    .then(
      null,
      function (e) {
        do_check_eq(e.name, "FxAccountsProfileClientError");
        do_check_eq(e.code, null);
        do_check_eq(e.errno, ERRNO_NETWORK);
        do_check_eq(e.error, ERROR_NETWORK);
        run_next_test();
      }
    );
});

add_test(function unsupportedMethod () {
  let client = new FxAccountsProfileClient(PROFILE_OPTIONS);

  return client._createRequest("/profile", "PUT")
    .then(
      null,
      function (e) {
        do_check_eq(e.name, "FxAccountsProfileClientError");
        do_check_eq(e.code, ERROR_CODE_METHOD_NOT_ALLOWED);
        do_check_eq(e.errno, ERRNO_NETWORK);
        do_check_eq(e.error, ERROR_NETWORK);
        do_check_eq(e.message, ERROR_MSG_METHOD_NOT_ALLOWED);
        run_next_test();
      }
    );
});

add_test(function onCompleteRequestError () {
  let client = new FxAccountsProfileClient(PROFILE_OPTIONS);
  client._Request = new mockResponseError(new Error("onComplete error"));
  client.fetchProfile()
    .then(
      null,
      function (e) {
        do_check_eq(e.name, "FxAccountsProfileClientError");
        do_check_eq(e.code, null);
        do_check_eq(e.errno, ERRNO_NETWORK);
        do_check_eq(e.error, ERROR_NETWORK);
        do_check_eq(e.message, "Error: onComplete error");
        run_next_test();
      }
  );
});

add_test(function fetchProfileImage_successfulResponse () {
  let client = new FxAccountsProfileClient(PROFILE_OPTIONS);
  let response = {
    success: true,
    status: STATUS_SUCCESS,
    body: "{\"avatar\":\"http://example.com/image.jpg\",\"id\":\"0d5c1a89b8c54580b8e3e8adadae864a\"}",
  };

  client._Request = new mockResponse(response);
  client.fetchProfileImage()
    .then(
      function (result) {
        do_check_eq(client._Request._requestUri, "http://127.0.0.1:1111/v1/avatar");
        do_check_eq(result.avatar, "http://example.com/image.jpg");
        do_check_eq(result.id, "0d5c1a89b8c54580b8e3e8adadae864a");
        run_next_test();
      }
    );
});

add_test(function constructorTests() {
  validationHelper(undefined,
    "Error: Missing 'serverURL' or 'token' configuration option");

  validationHelper({},
    "Error: Missing 'serverURL' or 'token' configuration option");

  validationHelper({ serverURL: "http://example.com" },
    "Error: Missing 'serverURL' or 'token' configuration option");

  validationHelper({ token: "123ABC" },
    "Error: Missing 'serverURL' or 'token' configuration option");

  validationHelper({ token: "123ABC", serverURL: "badUrl" },
    "Error: Invalid 'serverURL'");

  run_next_test();
});

add_test(function errorTests() {
  let error1 = new FxAccountsProfileClientError();
  do_check_eq(error1.name, "FxAccountsProfileClientError");
  do_check_eq(error1.code, null);
  do_check_eq(error1.errno, ERRNO_UNKNOWN_ERROR);
  do_check_eq(error1.error, ERROR_UNKNOWN);
  do_check_eq(error1.message, null);

  let error2 = new FxAccountsProfileClientError({
    code: STATUS_SUCCESS,
    errno: 1,
    error: "Error",
    message: "Something",
  });
  let fields2 = error2._toStringFields();
  let statusCode = 1;

  do_check_eq(error2.name, "FxAccountsProfileClientError");
  do_check_eq(error2.code, STATUS_SUCCESS);
  do_check_eq(error2.errno, statusCode);
  do_check_eq(error2.error, "Error");
  do_check_eq(error2.message, "Something");

  do_check_eq(fields2.name, "FxAccountsProfileClientError");
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
 * Quick way to test the "FxAccountsProfileClient" constructor.
 *
 * @param {Object} options
 *        FxAccountsProfileClient constructor options
 * @param {String} expected
 *        Expected error message
 * @returns {*}
 */
function validationHelper(options, expected) {
  try {
    new FxAccountsProfileClient(options);
  } catch (e) {
    return do_check_eq(e.toString(), expected);
  }
  throw new Error("Validation helper error");
}
