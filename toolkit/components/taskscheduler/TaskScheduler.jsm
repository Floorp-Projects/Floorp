/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TaskScheduler"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetter(
  this,
  "WinImpl",
  "resource://gre/modules/TaskSchedulerWinImpl.jsm",
  "_TaskSchedulerWinImpl"
);

XPCOMUtils.defineLazyGetter(this, "gImpl", () => {
  if (AppConstants.platform == "win") {
    return WinImpl;
  }

  // Stubs for unsupported platforms
  return {
    registerTask() {},
    deleteTask() {},
    deleteAllTasks() {},
  };
});

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

/**
 * Interface to a system task scheduler, capable of running a command line at an interval
 * independent of the application.
 *
 * Currently only implemented for Windows, on other platforms these calls do nothing.
 *
 * The implementation will only interact with tasks from the same install of this application.
 * - On Windows the native tasks are named like "\<vendor>\<id> <install path hash>",
 *   e.g. "\Mozilla\Task Identifier 308046B0AF4A39CB"
 */
var TaskScheduler = {
  MIN_INTERVAL_SECONDS: 1800,

  /**
   * Create a scheduled task that will run a command indepedent of the application.
   *
   * It will run every intervalSeconds seconds, starting intervalSeconds seconds from now.
   *
   * If the task is unable to run one or more scheduled times (e.g. if the computer is
   * off, or the owning user is not logged in), then the next time a run is possible the task
   * will be run once.
   *
   * An existing task with the same `id` will be replaced.
   *
   * Only one instance of the task will run at once, though this does not affect different
   * tasks from the same application.
   *
   * @param id
   *        A unique string (including a UUID is recommended) to distinguish the task among
   *        other tasks from this installation.
   *        This string will also be visible to system administrators, so it should be a legible
   *        description, but it does not need to be localized.
   *
   * @param command
   *        Full path to the executable to run.
   *
   * @param intervalSeconds
   *        Interval at which to run the command, in seconds. Minimum 1800 (30 minutes).
   *
   * @param options
   *        Optional, as as all of its properties:
   *        {
   *          args
   *            Array of arguments to pass on the command line. Does not include the command
   *            itself even if that is considered part of the command line. If missing, no
   *            argument list is generated.
   *
   *          workingDirectory
   *            Working directory for the command. If missing, no working directory is set.
   *
   *          description
   *            A description string that will be visible to system administrators. This should
   *            be localized. If missing, no description is set.
   *
   *          disabled
   *            If true the task will be created disabled, so that it will not be run.
   *            Default false, intended for tests.
   *        }
   * }
   */
  registerTask(id, command, intervalSeconds, options) {
    if (!Number.isInteger(intervalSeconds)) {
      throw new Error("Interval is not an integer");
    }
    if (intervalSeconds < this.MIN_INTERVAL_SECONDS) {
      throw new Error("Interval is too short");
    }

    return gImpl.registerTask(id, command, intervalSeconds, options);
  },

  /**
   * Delete a scheduled task previously created with registerTask.
   *
   * @throws NS_ERROR_FILE_NOT_FOUND if the task does not exist.
   */
  deleteTask(id) {
    return gImpl.deleteTask(id);
  },

  /**
   * Delete all tasks registered by this application.
   */
  deleteAllTasks() {
    return gImpl.deleteAllTasks();
  },
};
