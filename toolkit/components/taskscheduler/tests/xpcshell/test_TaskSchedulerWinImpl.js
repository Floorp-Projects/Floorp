/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

// Unit tests for Windows scheduled task generation.

const { updateAppInfo } = ChromeUtils.importESModule(
  "resource://testing-common/AppInfo.sys.mjs"
);
updateAppInfo();

const { TaskScheduler } = ChromeUtils.importESModule(
  "resource://gre/modules/TaskScheduler.sys.mjs"
);

const { WinImpl } = ChromeUtils.importESModule(
  "resource://gre/modules/TaskSchedulerWinImpl.sys.mjs"
);

const WinSvc = Cc["@mozilla.org/win-task-scheduler-service;1"].getService(
  Ci.nsIWinTaskSchedulerService
);

const uuidGenerator = Services.uuid;

function randomName() {
  return (
    "moz-taskschd-test-" + uuidGenerator.generateUUID().toString().slice(1, -1)
  );
}

const gFolderName = randomName();

// Override task folder name, to prevent colliding with other tests.
WinImpl._taskFolderName = function () {
  return gFolderName;
};
WinImpl._taskFolderNameParts = function () {
  return {
    parentName: "\\",
    subName: gFolderName,
  };
};

registerCleanupFunction(async () => {
  await TaskScheduler.deleteAllTasks();
});

add_task(async function test_create() {
  const taskName = "test-task-1";
  const rawTaskName = WinImpl._formatTaskName(taskName);
  const folderName = WinImpl._taskFolderName();
  const exePath = "C:\\Program Files\\XYZ\\123.exe";
  const workingDir = "C:\\Program Files\\XYZ";
  const argsIn = [
    "x.txt",
    "c:\\x.txt",
    'C:\\"HELLO WORLD".txt',
    "only space.txt",
  ];
  const expectedArgsOutStr = [
    "x.txt",
    "c:\\x.txt",
    '"C:\\\\\\"HELLO WORLD\\".txt"',
    '"only space.txt"',
  ].join(" ");
  const description = "Entities: < &. Non-ASCII: abc😀def.";
  const intervalSecsIn = 2 * 60 * 60; // 2 hours
  const expectedIntervalOutWin10 = "PT2H"; // Windows 10 regroups by hours and minutes
  const expectedIntervalOutWin7 = `PT${intervalSecsIn}S`; // Windows 7 doesn't regroup

  await TaskScheduler.registerTask(taskName, exePath, intervalSecsIn, {
    disabled: true,
    args: argsIn,
    description,
    workingDirectory: workingDir,
  });

  // Read back the task
  const readBackXML = WinSvc.getTaskXML(folderName, rawTaskName);
  const parser = new DOMParser();
  const doc = parser.parseFromString(readBackXML, "text/xml");
  Assert.equal(doc.documentElement.tagName, "Task");

  // Check for the values set above
  Assert.equal(doc.querySelector("Actions Exec Command").textContent, exePath);
  Assert.equal(
    doc.querySelector("Actions Exec WorkingDirectory").textContent,
    workingDir
  );
  Assert.equal(
    doc.querySelector("Actions Exec Arguments").textContent,
    expectedArgsOutStr
  );
  Assert.equal(
    doc.querySelector("RegistrationInfo Description").textContent,
    description
  );
  Assert.equal(
    doc.querySelector("RegistrationInfo Author").textContent,
    Services.appinfo.vendor
  );

  Assert.equal(doc.querySelector("Settings Enabled").textContent, "false");

  // Note: It's a little too tricky to check for a specific StartBoundary value reliably here, given
  // that it gets set relative to Date.now(), so I'm skipping that.
  const intervalOut = doc.querySelector(
    "Triggers TimeTrigger Repetition Interval"
  ).textContent;
  Assert.ok(
    intervalOut == expectedIntervalOutWin7 ||
      intervalOut == expectedIntervalOutWin10
  );

  // Validate the XML
  WinSvc.validateTaskDefinition(readBackXML);

  // Update
  const updatedExePath = "C:\\Program Files (x86)\\ABC\\foo.exe";
  const updatedIntervalSecsIn = 3 * 60 * 60; // 3 hours
  const expectedUpdatedIntervalOutWin10 = "PT3H";
  const expectedUpdatedIntervalOutWin7 = `PT${updatedIntervalSecsIn}S`;

  await TaskScheduler.registerTask(
    taskName,
    updatedExePath,
    updatedIntervalSecsIn,
    {
      disabled: true,
      args: argsIn,
      description,
      workingDirectory: workingDir,
    }
  );

  // Read back the updated task
  const readBackUpdatedXML = WinSvc.getTaskXML(folderName, rawTaskName);
  const updatedDoc = parser.parseFromString(readBackUpdatedXML, "text/xml");
  Assert.equal(updatedDoc.documentElement.tagName, "Task");

  // Check for updated values
  Assert.equal(
    updatedDoc.querySelector("Actions Exec Command").textContent,
    updatedExePath
  );

  Assert.notEqual(
    doc.querySelector("Triggers TimeTrigger StartBoundary").textContent,
    updatedDoc.querySelector("Triggers TimeTrigger StartBoundary").textContent
  );
  const updatedIntervalOut = updatedDoc.querySelector(
    "Triggers TimeTrigger Repetition Interval"
  ).textContent;
  Assert.ok(
    updatedIntervalOut == expectedUpdatedIntervalOutWin7 ||
      updatedIntervalOut == expectedUpdatedIntervalOutWin10
  );

  // Check that the folder really was there
  {
    const { parentName, subName } = WinImpl._taskFolderNameParts();
    let threw;
    try {
      WinSvc.deleteFolder(parentName, subName);
    } catch (ex) {
      threw = ex;
    }
    Assert.equal(threw.result, Cr.NS_ERROR_FILE_DIR_NOT_EMPTY);
  }

  // Delete
  await TaskScheduler.deleteAllTasks();

  // Check that the folder is gone
  {
    const { parentName, subName } = WinImpl._taskFolderNameParts();
    let threw;
    try {
      WinSvc.deleteFolder(parentName, subName);
    } catch (ex) {
      threw = ex;
    }
    Assert.equal(threw.result, Cr.NS_ERROR_FILE_NOT_FOUND);
  }

  // Format and validate the XML with the task not disabled
  const enabledXML = WinImpl._formatTaskDefinitionXML(exePath, intervalSecsIn, {
    args: argsIn,
    description,
    workingDirectory: workingDir,
  });
  Assert.equal(WinSvc.validateTaskDefinition(enabledXML), 0 /* S_OK */);

  // Format and validate with no options
  const basicXML = WinImpl._formatTaskDefinitionXML(
    "foo",
    TaskScheduler.MIN_INTERVAL_SECONDS
  );
  Assert.equal(WinSvc.validateTaskDefinition(basicXML), 0 /* S_OK */);
});

add_task(async function test_migrate() {
  // Create task name with nameVersion1
  const taskName = "test-task-1";
  const rawTaskNameV1 = WinImpl._formatTaskName(taskName, { nameVersion: 1 });
  const rawTaskNameV2 = WinImpl._formatTaskName(taskName, { nameVersion: 2 });
  const folderName = WinImpl._taskFolderName();
  const exePath = "C:\\Program Files\\XYZ\\123.exe";
  const workingDir = "C:\\Program Files\\XYZ";
  const argsIn = [
    "x.txt",
    "c:\\x.txt",
    'C:\\"HELLO WORLD".txt',
    "only space.txt",
  ];
  const expectedArgsOutStr = [
    "x.txt",
    "c:\\x.txt",
    '"C:\\\\\\"HELLO WORLD\\".txt"',
    '"only space.txt"',
  ].join(" ");
  const description = "Entities: < &. Non-ASCII: abc😀def.";
  const intervalSecsIn = 2 * 60 * 60; // 2 hours
  const expectedIntervalOut = "PT2H"; // 2 hours

  const queries = [
    ["Actions Exec Command", exePath],
    ["Actions Exec WorkingDirectory", workingDir],
    ["Actions Exec Arguments", expectedArgsOutStr],
    ["RegistrationInfo Description", description],
    ["RegistrationInfo Author", Services.appinfo.vendor],
    ["Settings Enabled", "false"],
    ["Triggers TimeTrigger Repetition Interval", expectedIntervalOut],
  ];

  await TaskScheduler.registerTask(taskName, exePath, intervalSecsIn, {
    disabled: true,
    args: argsIn,
    description,
    workingDirectory: workingDir,
    nameVersion: 1,
  });

  ok(
    WinImpl.taskExists(taskName, { nameVersion: 1 }),
    "Task exists with nameVersion1"
  );
  const originalTaskXML = WinSvc.getTaskXML(folderName, rawTaskNameV1);
  const parser = new DOMParser();
  const docV1 = parser.parseFromString(originalTaskXML, "text/xml");

  Assert.equal(docV1.documentElement.tagName, "Task");

  // Check for the values set above
  for (let [sel, expected] of queries) {
    Assert.equal(
      docV1.querySelector(sel).textContent,
      expected,
      `Task V1 ${sel} had expected textContent`
    );
  }

  // Update task name format to nameVersion2
  WinImpl._updateTaskNameFormat(taskName);
  ok(
    WinImpl.taskExists(taskName, { nameVersion: 2 }),
    "Task exists with nameVersion2"
  );
  ok(
    !WinImpl.taskExists(taskName, { nameVersion: 1 }),
    "Task with nameVersion1 successfully deleted"
  );

  // Check that the new task XML is still valid
  const newTaskXML = WinSvc.getTaskXML(folderName, rawTaskNameV2);
  Assert.equal(WinSvc.validateTaskDefinition(newTaskXML), 0 /* S_OK */);
  const docV2 = parser.parseFromString(newTaskXML, "text/xml");

  Assert.equal(docV2.documentElement.tagName, "Task");

  // Check that the updated values still match the provided ones.
  for (let [sel, expected] of queries) {
    Assert.equal(
      docV2.querySelector(sel).textContent,
      expected,
      `Task V2 ${sel} had expected textContent`
    );
  }
});
