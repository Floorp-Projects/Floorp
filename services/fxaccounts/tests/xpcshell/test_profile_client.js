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
    Assert.equal(options.scope, "profile");
    return "token";
  }
};

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
        Assert.equal(client._Request._requestUri, "http://127.0.0.1:1111/v1/profile");
        Assert.equal(result.body.email, "someone@restmail.net");
        Assert.equal(result.body.uid, "0d5c1a89b8c54580b8e3e8adadae864a");
        Assert.equal(result.etag, "bogusETag");
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
        Assert.equal(client._Request._requestUri, "http://127.0.0.1:1111/v1/profile");
        Assert.equal(result.body.email, "someone@restmail.net");
        Assert.equal(result.body.uid, "0d5c1a89b8c54580b8e3e8adadae864a");
        Assert.ok(req.ifNoneMatchSet);
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
        Assert.equal(result, null);
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
        Assert.equal(e.name, "FxAccountsProfileClientError");
        Assert.equal(e.code, STATUS_SUCCESS);
        Assert.equal(e.errno, ERRNO_PARSE);
        Assert.equal(e.error, ERROR_PARSE);
        Assert.equal(e.message, "unexpected");
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
      Assert.equal(e.name, "FxAccountsProfileClientError");
      Assert.equal(e.code, 500);
      Assert.equal(e.errno, 100);
      Assert.equal(e.error, "Bad Request");
      Assert.equal(e.message, "Something went wrong");
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
      Assert.equal(options.scope, "profile");
      return "" + ++lastToken; // tokens are strings.
    },
    removeCachedOAuthToken(options) {
      // This test never has more than 1 token alive at once, so the token
      // being revoked must always be the last token we handed out.
      Assert.equal(parseInt(options.token), lastToken);
      ++numTokensRemoved;
    }
  };
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
          Assert.equal(value, "Bearer " + lastToken);
        }
      },
      get() {
        this.response = responses[numRequests];
        ++numRequests;
        this.onComplete();
      }
    };
  };

  client.fetchProfile()
    .then(result => {
      Assert.equal(result.body.avatar, "http://example.com/image.jpg");
      Assert.equal(result.body.id, "0d5c1a89b8c54580b8e3e8adadae864a");
      // should have been exactly 2 requests and exactly 2 auth headers.
      Assert.equal(numRequests, 2);
      Assert.equal(numAuthHeaders, 2);
      // and we should have seen one token revoked.
      Assert.equal(numTokensRemoved, 1);

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
      Assert.equal(options.scope, "profile");
      return "" + ++lastToken; // tokens are strings.
    },
    removeCachedOAuthToken(options) {
      // This test never has more than 1 token alive at once, so the token
      // being revoked must always be the last token we handed out.
      Assert.equal(parseInt(options.token), lastToken);
      ++numTokensRemoved;
    }
  };
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
          Assert.equal(value, "Bearer " + lastToken);
        }
      },
      get() {
        this.response = response;
        ++numRequests;
        this.onComplete();
      }
    };
  };

  client.fetchProfile().catch(function(e) {
      Assert.equal(e.name, "FxAccountsProfileClientError");
      Assert.equal(e.code, 401);
      Assert.equal(e.errno, 100);
      Assert.equal(e.error, "It's not your token, it's you!");
      // should have been exactly 2 requests and exactly 2 auth headers.
      Assert.equal(numRequests, 2);
      Assert.equal(numAuthHeaders, 2);
      // and we should have seen both tokens revoked.
      Assert.equal(numTokensRemoved, 2);
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
        Assert.equal(e.name, "FxAccountsProfileClientError");
        Assert.equal(e.code, null);
        Assert.equal(e.errno, ERRNO_NETWORK);
        Assert.equal(e.error, ERROR_NETWORK);
        run_next_test();
      }
    );
});

add_test(function unsupportedMethod() {
  let client = new FxAccountsProfileClient(PROFILE_OPTIONS);

  return client._createRequest("/profile", "PUT")
    .catch(function(e) {
        Assert.equal(e.name, "FxAccountsProfileClientError");
        Assert.equal(e.code, ERROR_CODE_METHOD_NOT_ALLOWED);
        Assert.equal(e.errno, ERRNO_NETWORK);
        Assert.equal(e.error, ERROR_NETWORK);
        Assert.equal(e.message, ERROR_MSG_METHOD_NOT_ALLOWED);
        run_next_test();
      }
    );
});

add_test(function onCompleteRequestError() {
  let client = new FxAccountsProfileClient(PROFILE_OPTIONS);
  client._Request = new mockResponseError(new Error("onComplete error"));
  client.fetchProfile()
    .catch(function(e) {
        Assert.equal(e.name, "FxAccountsProfileClientError");
        Assert.equal(e.code, null);
        Assert.equal(e.errno, ERRNO_NETWORK);
        Assert.equal(e.error, ERROR_NETWORK);
        Assert.equal(e.message, "Error: onComplete error");
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
  Assert.equal(error1.name, "FxAccountsProfileClientError");
  Assert.equal(error1.code, null);
  Assert.equal(error1.errno, ERRNO_UNKNOWN_ERROR);
  Assert.equal(error1.error, ERROR_UNKNOWN);
  Assert.equal(error1.message, null);

  let error2 = new FxAccountsProfileClientError({
    code: STATUS_SUCCESS,
    errno: 1,
    error: "Error",
    message: "Something",
  });
  let fields2 = error2._toStringFields();
  let statusCode = 1;

  Assert.equal(error2.name, "FxAccountsProfileClientError");
  Assert.equal(error2.code, STATUS_SUCCESS);
  Assert.equal(error2.errno, statusCode);
  Assert.equal(error2.error, "Error");
  Assert.equal(error2.message, "Something");

  Assert.equal(fields2.name, "FxAccountsProfileClientError");
  Assert.equal(fields2.code, STATUS_SUCCESS);
  Assert.equal(fields2.errno, statusCode);
  Assert.equal(fields2.error, "Error");
  Assert.equal(fields2.message, "Something");

  Assert.ok(error2.toString().indexOf("Something") >= 0);
  run_next_test();
});

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
    return Assert.equal(e.toString(), expected);
  }
  throw new Error("Validation helper error");
}
