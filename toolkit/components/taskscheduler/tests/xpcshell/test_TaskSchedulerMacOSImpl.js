/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

// Unit tests for macOS scheduled task generation.

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

const { updateAppInfo } = ChromeUtils.import(
  "resource://testing-common/AppInfo.jsm"
);
updateAppInfo();

const { TaskScheduler } = ChromeUtils.import(
  "resource://gre/modules/TaskScheduler.jsm"
);
const { _TaskSchedulerMacOSImpl: MacOSImpl } = ChromeUtils.import(
  "resource://gre/modules/TaskSchedulerMacOSImpl.jsm"
);

function getFirefoxExecutableFilename() {
  if (AppConstants.platform === "win") {
    return AppConstants.MOZ_APP_NAME + ".exe";
  }
  return AppConstants.MOZ_APP_NAME;
}

// Returns a nsIFile to the firefox.exe (really, application) executable file.
function getFirefoxExecutableFile() {
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file = Services.dirsvc.get("GreBinD", Ci.nsIFile);

  file.append(getFirefoxExecutableFilename());
  return file;
}

const uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(
  Ci.nsIUUIDGenerator
);

function randomName() {
  return (
    "moz-taskschd-test-" +
    uuidGenerator
      .generateUUID()
      .toString()
      .slice(1, -1)
  );
}

add_task(async function test_all() {
  let labels;
  Assert.notEqual(await MacOSImpl._uid(), 0, "Should not be running as root");

  let id1 = randomName();
  let id2 = randomName();
  Assert.notEqual(id1, id2, "Random labels should not collide");

  await MacOSImpl.registerTask(
    id1,
    getFirefoxExecutableFile().path,
    TaskScheduler.MIN_INTERVAL_SECONDS,
    { disabled: true }
  );

  await MacOSImpl.registerTask(
    id2,
    getFirefoxExecutableFile().path,
    TaskScheduler.MIN_INTERVAL_SECONDS,
    { disabled: true }
  );

  let label1 = MacOSImpl._formatLabelForThisApp(id1);
  let label2 = MacOSImpl._formatLabelForThisApp(id2);

  // We don't assert equality because there may be existing tasks, concurrent
  // tests, etc.  This also means we can't reasonably tests `deleteAllTasks()`.
  labels = await MacOSImpl._listAllLabelsForThisApp();
  Assert.ok(
    labels.includes(label1),
    `Task ${label1} should have been registered in ${JSON.stringify(labels)}`
  );
  Assert.ok(
    labels.includes(label2),
    `Task ${label2} should have been registered in ${JSON.stringify(labels)}`
  );

  Assert.ok(await MacOSImpl.deleteTask(id1));

  labels = await MacOSImpl._listAllLabelsForThisApp();
  Assert.ok(
    !labels.includes(label1),
    `Task ${label1} should no longer be registered in ${JSON.stringify(labels)}`
  );
  Assert.ok(
    labels.includes(label2),
    `Task ${label2} should still be registered in ${JSON.stringify(labels)}`
  );

  Assert.ok(await MacOSImpl.deleteTask(id2));

  labels = await MacOSImpl._listAllLabelsForThisApp();
  Assert.ok(
    !labels.includes(label1),
    `Task ${label1} should no longer be registered in ${JSON.stringify(labels)}`
  );
  Assert.ok(
    !labels.includes(label2),
    `Task ${label2} should no longer be registered in ${JSON.stringify(labels)}`
  );
});
