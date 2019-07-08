/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test preventive maintenance checkAndFixDatabase.
 */

add_task(async function() {
  // We must initialize places first, or we won't have a db to check.
  Assert.equal(
    PlacesUtils.history.databaseStatus,
    PlacesUtils.history.DATABASE_STATUS_CREATE
  );

  let tasksStatusMap = await PlacesDBUtils.checkAndFixDatabase();
  let numberOfTasksRun = tasksStatusMap.size;
  let successfulTasks = [];
  let failedTasks = [];
  tasksStatusMap.forEach(val => {
    if (val.succeeded && val.logs) {
      successfulTasks.push(val);
    } else {
      failedTasks.push(val);
    }
  });
  Assert.equal(numberOfTasksRun, 8, "Check that we have run all tasks.");
  Assert.equal(
    successfulTasks.length,
    8,
    "Check that we have run all tasks successfully"
  );
  Assert.equal(failedTasks.length, 0, "Check that no task is failing");
});
