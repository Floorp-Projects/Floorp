/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Run a background task which itself waits for a launched background task, which itself waits for a
// launched background task, etc.  This is an easy way to ensure that tasks are running concurrently
// without requiring concurrency primitives.
add_task(async function test_backgroundtask_automatic_restart() {
  let restartTimeoutSec = 30;
  const path = await IOUtils.createUniqueFile(
    PathUtils.tempDir,
    "automatic_restart.txt"
  );
  let fileExists = await IOUtils.exists(path);
  ok(fileExists, `File at ${path} was created`);
  let stdoutLines = [];

  // Test restart functionality.
  let exitCode = await do_backgroundtask("automaticrestart", {
    extraArgs: [`-no-wait`, path, `-attach-console`, `-no-remote`],
    onStdoutLine: line => stdoutLines.push(line),
  });
  Assert.equal(0, exitCode);

  let pid = -1;
  for (let line of stdoutLines) {
    if (line.includes("*** ApplyUpdate: launched")) {
      let lineArr = line.split(" ");
      pid = Number.parseInt(lineArr[lineArr.length - 2]);
    }
  }
  console.log(`found launched pid ${pid}`);

  let updateProcessor = Cc[
    "@mozilla.org/updates/update-processor;1"
  ].createInstance(Ci.nsIUpdateProcessor);
  updateProcessor.waitForProcessExit(pid, restartTimeoutSec * 1000);
  let finalMessage = await IOUtils.readUTF8(path);
  ok(finalMessage.includes(`${pid}`), `New process message: ${finalMessage}`);
  await IOUtils.remove(path, { ignoreAbsent: true });
  fileExists = await IOUtils.exists(path);
  ok(!fileExists, `File at ${path} was removed`);
});
