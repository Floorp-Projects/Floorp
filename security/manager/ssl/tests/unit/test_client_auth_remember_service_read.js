/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// This tests that the nsIClientAuthRememberService correctly reads its backing
// state file.

function run_test() {
  let stateFile = do_get_profile();
  stateFile.append(CLIENT_AUTH_FILE_NAME);
  let outputStream = FileUtils.openFileOutputStream(stateFile);
  let keyValuePairs = [
    {
      key: "example.com,C9:65:33:89:EE:DC:4D:05:DA:16:3D:D0:12:61:BC:61:21:51:AF:2B:CC:C6:E1:72:B3:78:23:0F:13:B1:C7:4D,",
      value: "AAAA",
    },
    {
      key: "example.com,C9:65:33:89:EE:DC:4D:05:DA:16:3D:D0:12:61:BC:61:21:51:AF:2B:CC:C6:E1:72:B3:78:23:0F:13:B1:C7:4D,^partitionKey=%28https%2Cexample.com%29",
      value: "BBBB",
    },
    { key: "example.test,,", value: "CCCC" },
  ];
  for (let keyValuePair of keyValuePairs) {
    append_line_to_data_storage_file(
      outputStream,
      1,
      1,
      keyValuePair.key,
      keyValuePair.value,
      1024
    );
  }

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
