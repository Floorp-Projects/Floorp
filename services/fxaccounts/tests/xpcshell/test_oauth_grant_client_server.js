/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// A test of FxAccountsOAuthGrantClient but using a real server it can
// hit.
"use strict";

Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/FxAccountsOAuthGrantClient.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// handlers for our server.
var numTokenFetches;
var activeTokens;

function authorize(request, response) {
  response.setStatusLine("1.1", 200, "OK");
  let token = "token" + numTokenFetches;
  numTokenFetches += 1;
  activeTokens.add(token);
  response.write(JSON.stringify({access_token: token}));
}

function destroy(request, response) {
  // Getting the body seems harder than it should be!
  let sis = Cc["@mozilla.org/scriptableinputstream;1"]
            .createInstance(Ci.nsIScriptableInputStream);
  sis.init(request.bodyInputStream);
  let body = JSON.parse(sis.read(sis.available()));
  sis.close();
  let token = body.token;
  ok(activeTokens.delete(token));
  print("after destroy have", activeTokens.size, "tokens left.");
  response.setStatusLine("1.1", 200, "OK");
  response.write("{}");
}

function startServer() {
  numTokenFetches = 0;
  activeTokens = new Set();
  let srv = new HttpServer();
  srv.registerPathHandler("/v1/authorization", authorize);
  srv.registerPathHandler("/v1/destroy", destroy);
  srv.start(-1);
  return srv;
}

function promiseStopServer(server) {
  return new Promise(resolve => {
    server.stop(resolve);
  });
}

add_task(async function getAndRevokeToken() {
  Services.prefs.setBoolPref("identity.fxaccounts.allowHttp", true);
  let server = startServer();
  try {
    let clientOptions = {
      serverURL: "http://localhost:" + server.identity.primaryPort + "/v1",
      client_id: "abc123",
    };

    let client = new FxAccountsOAuthGrantClient(clientOptions);
    let result = await client.getTokenFromAssertion("assertion", "scope");
    equal(result.access_token, "token0");
    equal(numTokenFetches, 1, "we hit the server to fetch a token");
    await client.destroyToken("token0");
    equal(activeTokens.size, 0, "We hit the server to revoke it");
  } finally {
    await promiseStopServer(server);
    Services.prefs.clearUserPref("identity.fxaccounts.allowHttp");
  }
});

// XXX - TODO - we should probably add more tests for unexpected responses etc.
