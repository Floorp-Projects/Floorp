/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/FxAccountsOAuthClient.jsm");

function run_test() {
  validationHelper(undefined,
  "Error: Missing 'parameters' configuration option");

  validationHelper({},
  "Error: Missing 'parameters' configuration option");

  validationHelper({ parameters: {} },
  "Error: Missing 'parameters.oauth_uri' parameter");

  validationHelper({ parameters: {
    oauth_uri: "http://oauth.test/v1"
  }},
  "Error: Missing 'parameters.client_id' parameter");

  validationHelper({ parameters: {
    oauth_uri: "http://oauth.test/v1",
    client_id: "client_id"
  }},
  "Error: Missing 'parameters.content_uri' parameter");

  validationHelper({ parameters: {
    oauth_uri: "http://oauth.test/v1",
    client_id: "client_id",
    content_uri: "http://content.test"
  }},
  "Error: Missing 'parameters.state' parameter");

  validationHelper({ parameters: {
    oauth_uri: "http://oauth.test/v1",
    client_id: "client_id",
    content_uri: "http://content.test",
    state: "complete",
    action: "force_auth"
  }},
  "Error: parameters.email is required for action 'force_auth'");

  run_next_test();
}

function validationHelper(params, expected) {
  try {
    new FxAccountsOAuthClient(params);
  } catch (e) {
    return do_check_eq(e.toString(), expected);
  }
  throw new Error("Validation helper error");
}
