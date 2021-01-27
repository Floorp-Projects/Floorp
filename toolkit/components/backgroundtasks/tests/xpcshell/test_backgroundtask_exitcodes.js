/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_success() {
  let exitCode = await do_backgroundtask("success");
  Assert.equal(0, exitCode);
});

add_task(async function test_failure() {
  let exitCode = await do_backgroundtask("failure");
  Assert.equal(1, exitCode);
});

add_task(async function test_exception() {
  let exitCode = await do_backgroundtask("exception");
  Assert.equal(3, exitCode);
});

add_task(async function test_not_found() {
  let exitCode = await do_backgroundtask("not_found");
  Assert.equal(2, exitCode);
});
