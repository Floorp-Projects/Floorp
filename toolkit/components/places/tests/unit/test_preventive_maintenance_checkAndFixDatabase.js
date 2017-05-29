/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /**
  * Test preventive maintenance checkAndFixDatabase.
  */

// Include PlacesDBUtils module.
Components.utils.import("resource://gre/modules/PlacesDBUtils.jsm");

add_task(async function() {
  let tasksStatusMap = await PlacesDBUtils.checkAndFixDatabase();
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
    Assert.equal(numberOfTasksRun, 6, "Check that we have run all tasks.");
    Assert.equal(successfulTasks.length, 6, "Check that we have run all tasks successfully");
    Assert.equal(failedTasks.length, 0, "Check that no task is failing");
});
