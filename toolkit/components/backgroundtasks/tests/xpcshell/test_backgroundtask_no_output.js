/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Run a background task which itself waits for a launched background task,
// which itself waits for a launched background task, etc.  Verify no output is
// produced.
add_task(async function test_backgroundtask_no_output() {
  let sentinel = Services.uuid.generateUUID().toString();
  sentinel = sentinel.substring(1, sentinel.length - 1);

  let count = 2;
  let outputLines = [];
  let exitCode = await do_backgroundtask("no_output", {
    extraArgs: [sentinel, count.toString()],
    // This is a misnomer: stdout is redirected to stderr, so this is _any_
    // output line.
    onStdoutLine: line => outputLines.push(line),
  });
  Assert.equal(0, exitCode);

  if (AppConstants.platform !== "win") {
    // Check specific logs because there can still be some logs in certain conditions,
    // e.g. in code coverage (see bug 1831778 and bug 1804833)
    ok(
      outputLines.every(l => !l.includes("*** You are running in")),
      "Should not have logs by default"
    );
  }
});

// Run a background task which itself waits for a launched background task,
// which itself waits for a launched background task, etc.  Since we ignore the
// no output restriction, verify that output is produced.
add_task(async function test_backgroundtask_ignore_no_output() {
  let sentinel = Services.uuid.generateUUID().toString();
  sentinel = sentinel.substring(1, sentinel.length - 1);

  let count = 2;
  let outputLines = [];
  let exitCode = await do_backgroundtask("no_output", {
    extraArgs: [sentinel, count.toString()],
    extraEnv: { MOZ_BACKGROUNDTASKS_IGNORE_NO_OUTPUT: "1" },
    // This is a misnomer: stdout is redirected to stderr, so this is _any_
    // output line.
    onStdoutLine: line => {
      if (line.includes(sentinel)) {
        outputLines.push(line);
      }
    },
  });
  Assert.equal(0, exitCode);

  Assert.equal(count, outputLines.length);
});
