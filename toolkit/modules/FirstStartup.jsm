/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["FirstStartup"];

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Normandy: "resource://normandy/Normandy.jsm",
});

const PREF_TIMEOUT = "first-startup.timeout";
const PROBE_NAME = "firstStartup.statusCode";

/**
 * Service for blocking application startup, to be used on the first install. The intended
 * use case is for `FirstStartup` to be invoked when the application is called by an installer,
 * such as the Windows Stub Installer, to allow the application to do some first-install tasks
 * such as performance tuning and downloading critical data.
 *
 * In this scenario, the installer does not exit until the first application window appears,
 * which gives the user experience of the application starting up quickly on first install.
 */
var FirstStartup = {
  NOT_STARTED: 0,
  IN_PROGRESS: 1,
  TIMED_OUT: 2,
  SUCCESS: 3,
  UNSUPPORTED: 4,

  _state: this.NOT_STARTED,
  /**
   * Initialize and run first-startup services. This will always run synchronously
   * and spin the event loop until either all required services have
   * completed, or until a timeout is reached.
   *
   * In the latter case, services are expected to run post-UI instead as usual.
   */
  init() {
    this._state = this.IN_PROGRESS;
    const timeout = Services.prefs.getIntPref(PREF_TIMEOUT, 5000); // default to 5 seconds
    let expiredTime = Date.now() + timeout;

    if (AppConstants.MOZ_NORMANDY) {
      let normandyInitialized = false;

      Normandy.init({ runAsync: false }).then(
        () => (normandyInitialized = true)
      );

      Services.tm.spinEventLoopUntil(() => {
        if (Date.now() >= expiredTime) {
          this._state = this.TIMED_OUT;
          return true;
        } else if (normandyInitialized) {
          this._state = this.SUCCESS;
          return true;
        }
        return false;
      });
    } else {
      this._state = this.UNSUPPORTED;
    }

    Services.telemetry.scalarSet(PROBE_NAME, this._state);
  },

  get state() {
    return this._state;
  },
};
