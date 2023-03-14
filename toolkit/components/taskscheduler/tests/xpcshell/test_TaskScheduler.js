/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

// Cross-platform task scheduler tests.
//
// There's not much that can be done here without allowing the task to run, so this
// only touches on the basics of argument checking. On platforms without a task
// scheduler implementation, these interfaces currently do nothing else.

const { updateAppInfo } = ChromeUtils.importESModule(
  "resource://testing-common/AppInfo.sys.mjs"
);
updateAppInfo();

const { TaskScheduler } = ChromeUtils.importESModule(
  "resource://gre/modules/TaskScheduler.sys.mjs"
);

registerCleanupFunction(async () => {
  await TaskScheduler.deleteAllTasks();
});

add_task(async function test_gen() {
  await TaskScheduler.registerTask(
    "FOO",
    "xyz",
    TaskScheduler.MIN_INTERVAL_SECONDS,
    {
      disabled: true,
    }
  );

  Assert.equal(
    await TaskScheduler.taskExists("FOO"),
    true,
    "Task should exist after we created it."
  );

  await TaskScheduler.deleteTask("FOO");

  Assert.equal(
    await TaskScheduler.taskExists("FOO"),
    false,
    "Task should not exist after we deleted it."
  );

  await Assert.rejects(
    TaskScheduler.registerTask("BAR", "123", 1, {
      disabled: true,
    }),
    /Interval is too short/
  );
});
