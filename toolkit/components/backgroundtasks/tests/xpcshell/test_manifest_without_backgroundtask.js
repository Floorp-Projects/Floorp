/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_without_backgroundtask() {
  let bts = Cc["@mozilla.org/backgroundtasks;1"].getService(
    Ci.nsIBackgroundTasks
  );
  bts.overrideBackgroundTaskNameForTesting(null);

  Assert.equal(false, bts.isBackgroundTaskMode);
  Assert.equal(null, bts.backgroundTaskName());

  // Load test components.
  do_load_manifest("CatBackgroundTaskRegistrationComponents.manifest");

  const EXPECTED_ENTRIES = new Map([
    ["CatRegisteredComponent", "@unit.test.com/cat-registered-component;1"],
    [
      "CatBackgroundTaskNotRegisteredComponent",
      "@unit.test.com/cat-backgroundtask-notregistered-component;1",
    ],
  ]);

  // Verify the correct entries are registered in the "test-cat" category.
  for (let { entry, value } of Services.catMan.enumerateCategory("test-cat")) {
    ok(EXPECTED_ENTRIES.has(entry), `${entry} is expected`);
    Assert.equal(EXPECTED_ENTRIES.get(entry), value);
    EXPECTED_ENTRIES.delete(entry);
  }
  Assert.deepEqual(
    Array.from(EXPECTED_ENTRIES.keys()),
    [],
    "All expected entries have been deleted."
  );
});
