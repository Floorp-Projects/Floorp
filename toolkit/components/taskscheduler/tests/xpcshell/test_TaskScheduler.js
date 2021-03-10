/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

// Cross-platform task scheduler tests.
//
// There's not much that can be done here without allowing the task to run, so this
// only touches on the basics of argument checking. On platforms without a task
// scheduler implementation, these interfaces currently do nothing else.

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { updateAppInfo } = ChromeUtils.import(
  "resource://testing-common/AppInfo.jsm"
);
updateAppInfo();

const { TaskScheduler } = ChromeUtils.import(
  "resource://gre/modules/TaskScheduler.jsm"
);

registerCleanupFunction(() => {
  TaskScheduler.deleteAllTasks();
});

add_task(async function test_gen() {
  TaskScheduler.registerTask("FOO", "xyz", TaskScheduler.MIN_INTERVAL_SECONDS, {
    disabled: true,
  });
  TaskScheduler.deleteTask("FOO");

  Assert.throws(
    () =>
      TaskScheduler.registerTask("BAR", "123", 1, {
        disabled: true,
      }),
    /Interval is too short/
  );
});
