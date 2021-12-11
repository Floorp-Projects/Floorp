/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test preventive maintenance runTasks.
 */

add_task(async function() {
  let tasksStatusMap = await PlacesDBUtils.runTasks([
    PlacesDBUtils.invalidateCaches,
  ]);
  let numberOfTasksRun = tasksStatusMap.size;
  let successfulTasks = [];
  let failedTasks = [];
  tasksStatusMap.forEach(val => {
    if (val.succeeded) {
      successfulTasks.push(val);
    } else {
      failedTasks.push(val);
    }
  });

  Assert.equal(numberOfTasksRun, 1, "Check that we have run all tasks.");
  Assert.equal(
    successfulTasks.length,
    1,
    "Check that we have run all tasks successfully"
  );
  Assert.equal(failedTasks.length, 0, "Check that no task is failing");
});
