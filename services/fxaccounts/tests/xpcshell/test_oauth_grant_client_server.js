/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// A test of FxAccountsOAuthGrantClient but using a real server it can
// hit.
"use strict";

Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/FxAccountsOAuthGrantClient.jsm");

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
  print("after destroy have", activeTokens.size, "tokens left.")
  response.setStatusLine("1.1", 200, "OK");
  response.write('{}');
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

add_task(function* getAndRevokeToken () {
  let server = startServer();
  let clientOptions = {
    serverURL: "http://localhost:" + server.identity.primaryPort + "/v1",
    client_id: 'abc123',
  }

  let client = new FxAccountsOAuthGrantClient(clientOptions);
  let result = yield client.getTokenFromAssertion("assertion", "scope");
  equal(result.access_token, "token0");
  equal(numTokenFetches, 1, "we hit the server to fetch a token");
  yield client.destroyToken("token0");
  equal(activeTokens.size, 0, "We hit the server to revoke it");
  yield promiseStopServer(server);
});

// XXX - TODO - we should probably add more tests for unexpected responses etc.

function run_test() {
  run_next_test();
}
