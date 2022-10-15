/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This test exercises functionality and also ensures the exit codes,
// which are a public API, do not change over time.
const { EXIT_CODE } = ChromeUtils.importESModule(
  "resource://gre/modules/BackgroundTasksManager.sys.mjs"
);

add_task(async function test_success() {
  let exitCode = await do_backgroundtask("success");
  Assert.equal(0, exitCode);
  Assert.equal(EXIT_CODE.SUCCESS, exitCode);
});

add_task(async function test_failure() {
  let exitCode = await do_backgroundtask("failure");
  Assert.equal(1, exitCode);
  // There's no single exit code for failure.
  Assert.notEqual(EXIT_CODE.SUCCESS, exitCode);
});

add_task(async function test_exception() {
  let exitCode = await do_backgroundtask("exception");
  Assert.equal(3, exitCode);
  Assert.equal(EXIT_CODE.EXCEPTION, exitCode);
});

add_task(async function test_not_found() {
  let exitCode = await do_backgroundtask("not_found");
  Assert.equal(2, exitCode);
  Assert.equal(EXIT_CODE.NOT_FOUND, exitCode);
});

add_task(async function test_timeout() {
  const startTime = new Date();
  let exitCode = await do_backgroundtask("timeout");
  const endTime = new Date();
  const elapsedMs = endTime - startTime;
  const fiveMinutesInMs = 5 * 60 * 1000;
  Assert.less(elapsedMs, fiveMinutesInMs);
  Assert.equal(4, exitCode);
  Assert.equal(EXIT_CODE.TIMEOUT, exitCode);
});
