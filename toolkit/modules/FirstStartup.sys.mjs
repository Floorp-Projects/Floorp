/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Normandy: "resource://normandy/Normandy.sys.mjs",
  TaskScheduler: "resource://gre/modules/TaskScheduler.sys.mjs",
});

const PREF_TIMEOUT = "first-startup.timeout";

/**
 * Service for blocking application startup, to be used on the first install. The intended
 * use case is for `FirstStartup` to be invoked when the application is called by an installer,
 * such as the Windows Stub Installer, to allow the application to do some first-install tasks
 * such as performance tuning and downloading critical data.
 *
 * In this scenario, the installer does not exit until the first application window appears,
 * which gives the user experience of the application starting up quickly on first install.
 */
export var FirstStartup = {
  NOT_STARTED: 0,
  IN_PROGRESS: 1,
  TIMED_OUT: 2,
  SUCCESS: 3,
  UNSUPPORTED: 4,

  _state: 0, // NOT_STARTED,
  /**
   * Initialize and run first-startup services. This will always run synchronously
   * and spin the event loop until either all required services have
   * completed, or until a timeout is reached.
   *
   * In the latter case, services are expected to run post-UI instead as usual.
   */
  init() {
    this._state = this.IN_PROGRESS;
    const timeout = Services.prefs.getIntPref(PREF_TIMEOUT, 30000); // default to 30 seconds
    let startingTime = Cu.now();
    let initialized = false;

    let promises = [];

    let normandyInitEndTime = null;
    if (AppConstants.MOZ_NORMANDY) {
      promises.push(
        lazy.Normandy.init({ runAsync: false }).finally(() => {
          normandyInitEndTime = Cu.now();
        })
      );
    }

    let deleteTasksEndTime = null;
    if (AppConstants.MOZ_UPDATE_AGENT) {
      // It's technically possible for a previous installation to leave an old
      // OS-level scheduled task around.  Start fresh.
      promises.push(
        lazy.TaskScheduler.deleteAllTasks()
          .catch(() => {})
          .finally(() => {
            deleteTasksEndTime = Cu.now();
          })
      );
    }

    if (promises.length) {
      Promise.all(promises).then(() => (initialized = true));

      this.elapsed = 0;
      Services.tm.spinEventLoopUntil("FirstStartup.sys.mjs:init", () => {
        this.elapsed = Math.round(Cu.now() - startingTime);
        if (this.elapsed >= timeout) {
          this._state = this.TIMED_OUT;
          return true;
        } else if (initialized) {
          this._state = this.SUCCESS;
          return true;
        }
        return false;
      });
    } else {
      this._state = this.UNSUPPORTED;
    }

    if (AppConstants.MOZ_NORMANDY) {
      Glean.firstStartup.normandyInitTime.set(
        Math.ceil(normandyInitEndTime || Cu.now() - startingTime)
      );
    }

    if (AppConstants.MOZ_UPDATE_AGENT) {
      Glean.firstStartup.deleteTasksTime.set(
        Math.ceil(deleteTasksEndTime || Cu.now() - startingTime)
      );
    }

    Glean.firstStartup.statusCode.set(this._state);
    Glean.firstStartup.elapsed.set(this.elapsed);
    GleanPings.firstStartup.submit();
  },

  get state() {
    return this._state;
  },
};
