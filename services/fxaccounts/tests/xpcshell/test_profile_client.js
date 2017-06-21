/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/FxAccountsProfileClient.jsm");

const STATUS_SUCCESS = 200;

/**
 * Mock request responder
 * @param {String} response
 *        Mocked raw response from the server
 * @returns {Function}
 */
let mockResponse = function(response) {
  let Request = function(requestUri) {
    // Store the request uri so tests can inspect it
    Request._requestUri = requestUri;
    Request.ifNoneMatchSet = false;
    return {
      setHeader(header, value) {
        if (header == "If-None-Match" && value == "bogusETag") {
          Request.ifNoneMatchSet = true;
        }
      },
      get() {
        this.response = response;
        this.onComplete();
      }
    };
  };

  return Request;
};

// A simple mock FxA that hands out tokens without checking them and doesn't
// expect tokens to be revoked. We have specific token tests further down that
// has more checks here.
let mockFxa = {
  getOAuthToken(options) {
    do_check_eq(options.scope, "profile");
    return "token";
  }
}

const PROFILE_OPTIONS = {
  serverURL: "http://127.0.0.1:1111/v1",
  fxa: mockFxa,
};

/**
 * Mock request error responder
 * @param {Error} error
 *        Error object
 * @returns {Function}
 */
let mockResponseError = function(error) {
  return function() {
    return {
      setHeader() {},
      get() {
        this.onComplete(error);
      }
    };
  };
};

add_test(function successfulResponse() {
  let client = new FxAccountsProfileClient(PROFILE_OPTIONS);
  let response = {
    success: true,
    status: STATUS_SUCCESS,
    headers: { etag: "bogusETag" },
    body: "{\"email\":\"someone@restmail.net\",\"uid\":\"0d5c1a89b8c54580b8e3e8adadae864a\"}",
  };

  client._Request = new mockResponse(response);
  client.fetchProfile()
    .then(
      function(result) {
        do_check_eq(client._Request._requestUri, "http://127.0.0.1:1111/v1/profile");
        do_check_eq(result.body.email, "someone@restmail.net");
        do_check_eq(result.body.uid, "0d5c1a89b8c54580b8e3e8adadae864a");
        do_check_eq(result.etag, "bogusETag");
        run_next_test();
      }
    );
});

add_test(function setsIfNoneMatchETagHeader() {
  let client = new FxAccountsProfileClient(PROFILE_OPTIONS);
  let response = {
    success: true,
    status: STATUS_SUCCESS,
    headers: {},
    body: "{\"email\":\"someone@restmail.net\",\"uid\":\"0d5c1a89b8c54580b8e3e8adadae864a\"}",
  };

  let req = new mockResponse(response);
  client._Request = req;
  client.fetchProfile("bogusETag")
    .then(
      function(result) {
        do_check_eq(client._Request._requestUri, "http://127.0.0.1:1111/v1/profile");
        do_check_eq(result.body.email, "someone@restmail.net");
        do_check_eq(result.body.uid, "0d5c1a89b8c54580b8e3e8adadae864a");
        do_check_true(req.ifNoneMatchSet);
        run_next_test();
      }
    );
});

add_test(function successful304Response() {
  let client = new FxAccountsProfileClient(PROFILE_OPTIONS);
  let response = {
    success: true,
    headers: { etag: "bogusETag" },
    status: 304,
  };

  client._Request = new mockResponse(response);
  client.fetchProfile()
    .then(
      function(result) {
        do_check_eq(result, null);
        run_next_test();
      }
    );
});

add_test(function parseErrorResponse() {
  let client = new FxAccountsProfileClient(PROFILE_OPTIONS);
  let response = {
    success: true,
    status: STATUS_SUCCESS,
    body: "unexpected",
  };

  client._Request = new mockResponse(response);
  client.fetchProfile()
    .catch(function(e) {
        do_check_eq(e.name, "FxAccountsProfileClientError");
        do_check_eq(e.code, STATUS_SUCCESS);
        do_check_eq(e.errno, ERRNO_PARSE);
        do_check_eq(e.error, ERROR_PARSE);
        do_check_eq(e.message, "unexpected");
        run_next_test();
      }
    );
});

add_test(function serverErrorResponse() {
  let client = new FxAccountsProfileClient(PROFILE_OPTIONS);
  let response = {
    status: 500,
    body: "{ \"code\": 500, \"errno\": 100, \"error\": \"Bad Request\", \"message\": \"Something went wrong\", \"reason\": \"Because the internet\" }",
  };

  client._Request = new mockResponse(response);
  client.fetchProfile()
    .catch(function(e) {
      do_check_eq(e.name, "FxAccountsProfileClientError");
      do_check_eq(e.code, 500);
      do_check_eq(e.errno, 100);
      do_check_eq(e.error, "Bad Request");
      do_check_eq(e.message, "Something went wrong");
      run_next_test();
    }
  );
});

// Test that we get a token, then if we get a 401 we revoke it, get a new one
// and retry.
add_test(function server401ResponseThenSuccess() {
  // The last token we handed out.
  let lastToken = -1;
  // The number of times our removeCachedOAuthToken function was called.
  let numTokensRemoved = 0;

  let mockFxaWithRemove = {
    getOAuthToken(options) {
      do_check_eq(options.scope, "profile");
      return "" + ++lastToken; // tokens are strings.
    },
    removeCachedOAuthToken(options) {
      // This test never has more than 1 token alive at once, so the token
      // being revoked must always be the last token we handed out.
      do_check_eq(parseInt(options.token), lastToken);
      ++numTokensRemoved;
    }
  }
  let profileOptions = {
    serverURL: "http://127.0.0.1:1111/v1",
    fxa: mockFxaWithRemove,
  };
  let client = new FxAccountsProfileClient(profileOptions);

  // 2 responses - first one implying the token has expired, second works.
  let responses = [
    {
      status: 401,
      body: "{ \"code\": 401, \"errno\": 100, \"error\": \"Token expired\", \"message\": \"That token is too old\", \"reason\": \"Because security\" }",
    },
    {
      success: true,
      status: STATUS_SUCCESS,
      headers: {},
      body: "{\"avatar\":\"http://example.com/image.jpg\",\"id\":\"0d5c1a89b8c54580b8e3e8adadae864a\"}",
    },
  ];

  let numRequests = 0;
  let numAuthHeaders = 0;
  // Like mockResponse but we want access to headers etc.
  client._Request = function(requestUri) {
    return {
      setHeader(name, value) {
        if (name == "Authorization") {
          numAuthHeaders++;
          do_check_eq(value, "Bearer " + lastToken);
        }
      },
      get() {
        this.response = responses[numRequests];
        ++numRequests;
        this.onComplete();
      }
    };
  }

  client.fetchProfile()
    .then(result => {
      do_check_eq(result.body.avatar, "http://example.com/image.jpg");
      do_check_eq(result.body.id, "0d5c1a89b8c54580b8e3e8adadae864a");
      // should have been exactly 2 requests and exactly 2 auth headers.
      do_check_eq(numRequests, 2);
      do_check_eq(numAuthHeaders, 2);
      // and we should have seen one token revoked.
      do_check_eq(numTokensRemoved, 1);

      run_next_test();
    }
  );
});

// Test that we get a token, then if we get a 401 we revoke it, get a new one
// and retry - but we *still* get a 401 on the retry, so the caller sees that.
add_test(function server401ResponsePersists() {
  // The last token we handed out.
  let lastToken = -1;
  // The number of times our removeCachedOAuthToken function was called.
  let numTokensRemoved = 0;

  let mockFxaWithRemove = {
    getOAuthToken(options) {
      do_check_eq(options.scope, "profile");
      return "" + ++lastToken; // tokens are strings.
    },
    removeCachedOAuthToken(options) {
      // This test never has more than 1 token alive at once, so the token
      // being revoked must always be the last token we handed out.
      do_check_eq(parseInt(options.token), lastToken);
      ++numTokensRemoved;
    }
  }
  let profileOptions = {
    serverURL: "http://127.0.0.1:1111/v1",
    fxa: mockFxaWithRemove,
  };
  let client = new FxAccountsProfileClient(profileOptions);

  let response = {
      status: 401,
      body: "{ \"code\": 401, \"errno\": 100, \"error\": \"It's not your token, it's you!\", \"message\": \"I don't like you\", \"reason\": \"Because security\" }",
  };

  let numRequests = 0;
  let numAuthHeaders = 0;
  client._Request = function(requestUri) {
    return {
      setHeader(name, value) {
        if (name == "Authorization") {
          numAuthHeaders++;
          do_check_eq(value, "Bearer " + lastToken);
        }
      },
      get() {
        this.response = response;
        ++numRequests;
        this.onComplete();
      }
    };
  }

  client.fetchProfile().catch(function(e) {
      do_check_eq(e.name, "FxAccountsProfileClientError");
      do_check_eq(e.code, 401);
      do_check_eq(e.errno, 100);
      do_check_eq(e.error, "It's not your token, it's you!");
      // should have been exactly 2 requests and exactly 2 auth headers.
      do_check_eq(numRequests, 2);
      do_check_eq(numAuthHeaders, 2);
      // and we should have seen both tokens revoked.
      do_check_eq(numTokensRemoved, 2);
      run_next_test();
    }
  );
});

add_test(function networkErrorResponse() {
  let client = new FxAccountsProfileClient({
    serverURL: "http://domain.dummy",
    fxa: mockFxa,
  });
  client.fetchProfile()
    .catch(function(e) {
        do_check_eq(e.name, "FxAccountsProfileClientError");
        do_check_eq(e.code, null);
        do_check_eq(e.errno, ERRNO_NETWORK);
        do_check_eq(e.error, ERROR_NETWORK);
        run_next_test();
      }
    );
});

add_test(function unsupportedMethod() {
  let client = new FxAccountsProfileClient(PROFILE_OPTIONS);

  return client._createRequest("/profile", "PUT")
    .catch(function(e) {
        do_check_eq(e.name, "FxAccountsProfileClientError");
        do_check_eq(e.code, ERROR_CODE_METHOD_NOT_ALLOWED);
        do_check_eq(e.errno, ERRNO_NETWORK);
        do_check_eq(e.error, ERROR_NETWORK);
        do_check_eq(e.message, ERROR_MSG_METHOD_NOT_ALLOWED);
        run_next_test();
      }
    );
});

add_test(function onCompleteRequestError() {
  let client = new FxAccountsProfileClient(PROFILE_OPTIONS);
  client._Request = new mockResponseError(new Error("onComplete error"));
  client.fetchProfile()
    .catch(function(e) {
        do_check_eq(e.name, "FxAccountsProfileClientError");
        do_check_eq(e.code, null);
        do_check_eq(e.errno, ERRNO_NETWORK);
        do_check_eq(e.error, ERROR_NETWORK);
        do_check_eq(e.message, "Error: onComplete error");
        run_next_test();
      }
  );
});

add_test(function constructorTests() {
  validationHelper(undefined,
    "Error: Missing 'serverURL' configuration option");

  validationHelper({},
    "Error: Missing 'serverURL' configuration option");

  validationHelper({ serverURL: "badUrl" },
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
  // add fxa to options - that missing isn't what we are testing here.
  if (options) {
    options.fxa = mockFxa;
  }
  try {
    new FxAccountsProfileClient(options);
  } catch (e) {
    return do_check_eq(e.toString(), expected);
  }
  throw new Error("Validation helper error");
}
