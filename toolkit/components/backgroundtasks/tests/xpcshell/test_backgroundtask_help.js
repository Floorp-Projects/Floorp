/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test_backgroundtask_help_includes_jsdebugger_options() {
  let lines = [];
  let exitCode = await do_backgroundtask("success", {
    extraArgs: ["--help"],
    onStdoutLine: line => lines.push(line),
  });
  Assert.equal(0, exitCode);

  Assert.ok(lines.some(line => line.includes("--jsdebugger")));
  Assert.ok(lines.some(line => line.includes("--wait-for-jsdebugger")));
  Assert.ok(lines.some(line => line.includes("--start-debugger-server")));
});
