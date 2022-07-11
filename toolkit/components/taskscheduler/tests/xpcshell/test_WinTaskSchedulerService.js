/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

// Unit tests for access to the Windows Task Scheduler via nsIWinTaskSchedulerService.

const svc = Cc["@mozilla.org/win-task-scheduler-service;1"].getService(
  Ci.nsIWinTaskSchedulerService
);

function randomName() {
  return (
    "moz-taskschd-test-" +
    Services.uuid
      .generateUUID()
      .toString()
      .slice(1, -1)
  );
}

const gParentFolderName = randomName();
const gParentFolderPath = `\\${gParentFolderName}`;
const gSubFolderName = randomName();
const gSubFolderPath = `\\${gParentFolderName}\\${gSubFolderName}`;
// This folder will not be created
const gMissingFolderName = randomName();
const gMissingFolderPath = `\\${gParentFolderName}\\${gMissingFolderName}`;

const gValidTaskXML = `<Task xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">
  <Triggers />
  <Settings>
    <Enabled>false</Enabled>
  </Settings>
  <Actions>
    <Exec>
      <Command>xyz123.exe</Command>
    </Exec>
  </Actions>
</Task>`;

// Missing actions
const gInvalidTaskXML = `<Task xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">
  <Triggers />
  <Settings>
    <Enabled>false</Enabled>
  </Settings>
</Task>`;

function cleanup() {
  let tasksToDelete = svc.getFolderTasks(gSubFolderPath);

  for (const task of tasksToDelete) {
    svc.deleteTask(gSubFolderPath, task);
  }

  svc.deleteFolder(gParentFolderPath, gSubFolderName);

  svc.deleteFolder("\\", gParentFolderPath);
}

registerCleanupFunction(() => {
  try {
    cleanup();
  } catch (_ex) {
    // Folders may not exist
  }
});

add_task(async function test_svc() {
  /***** FOLDERS *****/

  // Try creating subfolder before parent folder exists
  Assert.throws(
    () => svc.createFolder(gParentFolderPath, gSubFolderName),
    /NS_ERROR_FILE_NOT_FOUND/
  );

  // Create parent folder
  svc.createFolder("\\", gParentFolderName);

  // Create subfolder
  svc.createFolder(gParentFolderPath, gSubFolderName);

  // Try creating existing folder
  Assert.throws(
    () => svc.createFolder(gParentFolderPath, gSubFolderName),
    /NS_ERROR_FILE_ALREADY_EXISTS/
  );

  // Try deleting nonexistent subfolder
  Assert.throws(
    () => svc.deleteFolder(gParentFolderPath, gMissingFolderName),
    /NS_ERROR_FILE_NOT_FOUND/
  );

  /***** TASKS *****/
  const taskNames = [randomName(), randomName(), randomName()];

  // Try enumerating nonexistent subfolder
  Assert.throws(
    () => svc.getFolderTasks(gMissingFolderPath),
    /NS_ERROR_FILE_NOT_FOUND/
  );

  // List empty subfolder
  Assert.deepEqual(svc.getFolderTasks(gSubFolderPath), []);

  // Try to create task in nonexistent subfolder
  Assert.throws(
    () => svc.registerTask(gMissingFolderPath, taskNames[0], gValidTaskXML),
    /NS_ERROR_FILE_NOT_FOUND/
  );

  // Create task 0

  svc.registerTask(gSubFolderPath, taskNames[0], gValidTaskXML);

  // Try to recreate task 0
  Assert.throws(
    () => svc.registerTask(gSubFolderPath, taskNames[0], gValidTaskXML),
    /NS_ERROR_FILE_ALREADY_EXISTS/
  );

  // Update task 0
  svc.registerTask(
    gSubFolderPath,
    taskNames[0],
    gValidTaskXML,
    true /* aUpdateExisting */
  );

  // Read back XML
  Assert.ok(svc.getTaskXML(gSubFolderPath, taskNames[0]));

  // Create remaining tasks
  for (const task of taskNames.slice(1)) {
    svc.registerTask(gSubFolderPath, task, gValidTaskXML);
  }

  // Try to create with invalid XML
  Assert.throws(
    () => svc.registerTask(gSubFolderPath, randomName(), gInvalidTaskXML),
    /NS_ERROR_FAILURE/
  );

  // Validate XML
  Assert.equal(svc.validateTaskDefinition(gValidTaskXML), 0 /* S_OK */);

  // Try to validate invalid XML
  Assert.notEqual(svc.validateTaskDefinition(gInvalidTaskXML), 0 /* S_OK */);

  // Test enumeration
  {
    let foundTasks = svc.getFolderTasks(gSubFolderPath);
    foundTasks.sort();

    let allTasks = taskNames.slice();
    allTasks.sort();

    Assert.deepEqual(foundTasks, allTasks);
  }

  // Try deleting non-empty folder
  Assert.throws(
    () => svc.deleteFolder(gParentFolderPath, gSubFolderName),
    /NS_ERROR_FILE_DIR_NOT_EMPTY/
  );

  const missingTaskName = randomName();

  // Try deleting non-existent task
  Assert.throws(
    () => svc.deleteTask(gSubFolderName, missingTaskName),
    /NS_ERROR_FILE_NOT_FOUND/
  );

  // Try reading non-existent task
  Assert.throws(
    () => svc.getTaskXML(gSubFolderPath, missingTaskName),
    /NS_ERROR_FILE_NOT_FOUND/
  );

  /***** Cleanup *****/
  // Explicitly call cleanup() to test that it removes the folder without error.
  cleanup();
});
