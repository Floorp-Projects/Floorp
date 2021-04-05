/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_backgroundtask_deletes_profile() {
  let sentinel = Cc["@mozilla.org/uuid-generator;1"]
    .getService(Ci.nsIUUIDGenerator)
    .generateUUID()
    .toString();
  sentinel = sentinel.substring(1, sentinel.length - 1);

  let stdoutLines = [];
  let exitCode = await do_backgroundtask("unique_profile", {
    extraArgs: [sentinel],
    stdoutLines,
  });
  Assert.equal(0, exitCode);

  let profile;
  for (let line of stdoutLines) {
    if (line.includes(sentinel)) {
      let info = JSON.parse(line.split(sentinel)[1]);
      profile = info[1];
    }
  }
  Assert.ok(!!profile, `Found profile: ${profile}`);

  let dir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  dir.initWithPath(profile);
  Assert.ok(
    !dir.exists(),
    `Temporary profile directory does not exist: ${profile}`
  );
});
