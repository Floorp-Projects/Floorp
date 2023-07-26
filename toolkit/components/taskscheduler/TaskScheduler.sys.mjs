/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  WinImpl: "resource://gre/modules/TaskSchedulerWinImpl.sys.mjs",
  MacOSImpl: "resource://gre/modules/TaskSchedulerMacOSImpl.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "gImpl", () => {
  if (AppConstants.platform == "win") {
    return lazy.WinImpl;
  }

  if (AppConstants.platform == "macosx") {
    return lazy.MacOSImpl;
  }

  // Stubs for unsupported platforms
  return {
    registerTask() {},
    deleteTask() {},
    deleteAllTasks() {},
  };
});

/**
 * Interface to a system task scheduler, capable of running a command line at an interval
 * independent of the application.
 *
 * The expected consumer of this component wants to run a periodic short-lived maintenance task.
 * These periodic maintenance tasks will be per-OS-level user and run with the OS-level user's
 * permissions.  (These still need to work across systems with multiple users and the various
 * ownership and permission combinations that we see.)  This component does not help schedule
 * maintenance daemons, meaning long-lived processes.
 *
 * Currently only implemented for Windows and macOS, on other platforms these calls do nothing.
 *
 * The implementation will only interact with tasks from the same install of this application.
 * - On Windows the native tasks are named like "\<vendor>\<id> <install path hash>",
 *   e.g. "\Mozilla\Task Identifier 308046B0AF4A39CB"
 * - On macOS the native tasks are labeled like "<macOS bundle ID>.<install path hash>.<id>",
 *   e.g. "org.mozilla.nightly.308046B0AF4A39CB.Task Identifier".
 */
export var TaskScheduler = {
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
   *        Optional, as are all of its properties:
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
   *            Ignored on macOS: see comments in TaskSchedulerMacOSImpl.jsm.
   *            Default false, intended for tests.
   *
   *          executionTimeoutSec
   *            Specifies how long (in seconds) the scheduled task can execute for before it is
   *            automatically stopped by the task scheduler. If a value <= 0 is given, it will be
   *            ignored.
   *            This is not currently implemented on macOS.
   *            On Windows, the default timeout is 72 hours.
   *        }
   * }
   */
  async registerTask(id, command, intervalSeconds, options) {
    if (typeof id !== "string") {
      throw new Error("id is not a string");
    }
    if (!Number.isInteger(intervalSeconds)) {
      throw new Error("Interval is not an integer");
    }
    if (intervalSeconds < this.MIN_INTERVAL_SECONDS) {
      throw new Error("Interval is too short");
    }

    return lazy.gImpl.registerTask(id, command, intervalSeconds, options);
  },

  /**
   * Delete a scheduled task previously created with registerTask.
   *
   * @throws NS_ERROR_FILE_NOT_FOUND if the task does not exist.
   */
  async deleteTask(id) {
    return lazy.gImpl.deleteTask(id);
  },

  /**
   * Delete all tasks registered by this application.
   */
  async deleteAllTasks() {
    return lazy.gImpl.deleteAllTasks();
  },

  /**
   * Checks if a task exists.
   *
   * @param id
   *        A string representing the identifier of the task to look for.
   *
   * @return
   *        true if the task exists, otherwise false.
   */
  async taskExists(id) {
    return lazy.gImpl.taskExists(id);
  },
};
