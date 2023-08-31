/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// This tests that the nsIClientAuthRememberService correctly reads its backing
// state file.

function run_test() {
  let profile = do_get_profile();
  let clientAuthRememberFile = do_get_file(
    `test_client_auth_remember_service/${CLIENT_AUTH_FILE_NAME}`
  );
  clientAuthRememberFile.copyTo(profile, CLIENT_AUTH_FILE_NAME);

  let clientAuthRememberService = Cc[
    "@mozilla.org/security/clientAuthRememberService;1"
  ].getService(Ci.nsIClientAuthRememberService);

  let dbKey = {};
  ok(
    clientAuthRememberService.hasRememberedDecisionScriptable(
      "example.com",
      {},
      dbKey
    )
  );
  equal(dbKey.value, "AAAA");

  dbKey = {};
  ok(
    clientAuthRememberService.hasRememberedDecisionScriptable(
      "example.com",
      { partitionKey: "(https,example.com)" },
      dbKey
    )
  );
  equal(dbKey.value, "BBBB");

  ok(
    !clientAuthRememberService.hasRememberedDecisionScriptable(
      "example.org",
      {},
      {}
    )
  );
  ok(
    !clientAuthRememberService.hasRememberedDecisionScriptable(
      "example.com",
      { partitionKey: "(https,example.org)" },
      {}
    )
  );

  dbKey = {};
  ok(
    clientAuthRememberService.hasRememberedDecisionScriptable(
      "example.test",
      {},
      dbKey
    )
  );
  equal(dbKey.value, "CCCC");
}
