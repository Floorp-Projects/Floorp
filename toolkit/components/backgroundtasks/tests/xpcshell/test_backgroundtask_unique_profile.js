/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Run a background task which itself waits for a launched background task, which itself waits for a
// launched background task, etc.  This is an easy way to ensure that tasks are running concurrently
// without requiring concurrency primitives.
add_task(async function test_backgroundtask_unique_profile() {
  let sentinel = Services.uuid.generateUUID().toString();
  sentinel = sentinel.substring(1, sentinel.length - 1);

  let count = 3;
  let stdoutLines = [];
  let exitCode = await do_backgroundtask("unique_profile", {
    extraArgs: [sentinel, count.toString()],
    onStdoutLine: line => stdoutLines.push(line),
  });
  Assert.equal(0, exitCode);

  let infos = [];
  let profiles = new Set();
  for (let line of stdoutLines) {
    if (line.includes(sentinel)) {
      let info = JSON.parse(line.split(sentinel)[1]);
      infos.push(info);
      profiles.add(info[1]);
    }
  }

  Assert.equal(
    count,
    profiles.size,
    `Found ${count} distinct profiles: ${JSON.stringify(
      Array.from(profiles)
    )} in: ${JSON.stringify(infos)}`
  );
});
