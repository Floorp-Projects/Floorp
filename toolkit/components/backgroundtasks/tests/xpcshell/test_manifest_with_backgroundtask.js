/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_manifest_with_backgroundtask() {
  let bts = Cc["@mozilla.org/backgroundtasks;1"].getService(
    Ci.nsIBackgroundTasks
  );

  Assert.equal(false, bts.isBackgroundTaskMode);
  Assert.equal(null, bts.backgroundTaskName());

  bts.overrideBackgroundTaskNameForTesting("test-task");

  Assert.equal(true, bts.isBackgroundTaskMode);
  Assert.equal("test-task", bts.backgroundTaskName());

  // Load test components.
  do_load_manifest("CatBackgroundTaskRegistrationComponents.manifest");

  const expectedEntries = new Map([
    [
      "CatBackgroundTaskRegisteredComponent",
      "@unit.test.com/cat-backgroundtask-registered-component;1",
    ],
    [
      "CatBackgroundTaskAlwaysRegisteredComponent",
      "@unit.test.com/cat-backgroundtask-alwaysregistered-component;1",
    ],
  ]);

  // Verify the correct entries are registered in the "test-cat" category.
  for (let { entry, value } of Services.catMan.enumerateCategory("test-cat")) {
    ok(expectedEntries.has(entry), `${entry} is expected`);
    Assert.equal(
      value,
      expectedEntries.get(entry),
      "${entry} has correct value."
    );
    expectedEntries.delete(entry);
  }
  print("Check that all of the expected entries have been deleted.");
  Assert.deepEqual(
    Array.from(expectedEntries.keys()),
    [],
    "All expected entries have been deleted."
  );
});
