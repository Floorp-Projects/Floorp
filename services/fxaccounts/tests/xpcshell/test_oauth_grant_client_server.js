/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// A test of FxAccountsOAuthGrantClient but using a real server it can
// hit.
"use strict";

const { FxAccountsOAuthGrantClient } = ChromeUtils.import(
  "resource://gre/modules/FxAccountsOAuthGrantClient.jsm"
);

// handlers for our server.
var numTokenFetches;
var authorizeError;

function authorize(request, response) {
  if (authorizeError) {
    response.setStatusLine(
      "1.1",
      authorizeError.status,
      authorizeError.description
    );
    response.write(JSON.stringify(authorizeError.body));
  } else {
    response.setStatusLine("1.1", 200, "OK");
    let token = "token" + numTokenFetches;
    numTokenFetches += 1;
    response.write(JSON.stringify({ access_token: token }));
  }
}

function startServer() {
  numTokenFetches = 0;
  authorizeError = null;
  let srv = new HttpServer();
  srv.registerPathHandler("/v1/authorization", authorize);
  srv.start(-1);
  return srv;
}

async function runWithServer(cb) {
  Services.prefs.setBoolPref("identity.fxaccounts.allowHttp", true);
  let server = startServer();
  try {
    return await cb(server);
  } finally {
    await promiseStopServer(server);
    Services.prefs.clearUserPref("identity.fxaccounts.allowHttp");
  }
}

add_task(async function test_getToken() {
  await runWithServer(async server => {
    let clientOptions = {
      serverURL: "http://localhost:" + server.identity.primaryPort + "/v1",
      client_id: "abc123",
    };

    let client = new FxAccountsOAuthGrantClient(clientOptions);
    let result = await client.getTokenFromAssertion("assertion", "scope");
    equal(result.access_token, "token0");
    equal(numTokenFetches, 1, "we hit the server to fetch a token");
  });
});

add_task(async function test_authError() {
  await runWithServer(async server => {
    let clientOptions = {
      serverURL: "http://localhost:" + server.identity.primaryPort + "/v1",
      client_id: "abc123",
    };

    authorizeError = {
      status: 401,
      description: "Unauthorized",
      body: {
        code: 401,
        errno: 101,
        message: "fake error",
      },
    };

    let client = new FxAccountsOAuthGrantClient(clientOptions);
    try {
      await client.getTokenFromAssertion("assertion", "scope");
      do_throw("Expected to catch an exception");
    } catch (e) {
      equal(e.name, "FxAccountsOAuthGrantClientError");
      equal(e.code, 401);
      // The errno is offset by 1000 to distinguish from auth-server errnos.
      equal(e.errno, 1101);
      equal(e.error, "UNKNOWN_ERROR");
      equal(e.message, "fake error");
    }
  });
});
