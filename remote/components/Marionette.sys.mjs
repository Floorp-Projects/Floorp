/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Preferences: "resource://gre/modules/Preferences.sys.mjs",

  Deferred: "chrome://remote/content/shared/Sync.sys.mjs",
  EnvironmentPrefs: "chrome://remote/content/marionette/prefs.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  MarionettePrefs: "chrome://remote/content/marionette/prefs.sys.mjs",
  RecommendedPreferences:
    "chrome://remote/content/shared/RecommendedPreferences.sys.mjs",
  TCPListener: "chrome://remote/content/marionette/server.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.MARIONETTE)
);

ChromeUtils.defineLazyGetter(lazy, "textEncoder", () => new TextEncoder());

const NOTIFY_LISTENING = "marionette-listening";

// Complements -marionette flag for starting the Marionette server.
// We also set this if Marionette is running in order to start the server
// again after a Firefox restart.
const ENV_ENABLED = "MOZ_MARIONETTE";

// Besides starting based on existing prefs in a profile and a command
// line flag, we also support inheriting prefs out of an env var, and to
// start Marionette that way.
//
// This allows marionette prefs to persist when we do a restart into
// a different profile in order to test things like Firefox refresh.
// The environment variable itself, if present, is interpreted as a
// JSON structure, with the keys mapping to preference names in the
// "marionette." branch, and the values to the values of those prefs. So
// something like {"port": 4444} would result in the marionette.port
// pref being set to 4444.
const ENV_PRESERVE_PREFS = "MOZ_MARIONETTE_PREF_STATE_ACROSS_RESTARTS";

// Map of Marionette-specific preferences that should be set via
// RecommendedPreferences.
const RECOMMENDED_PREFS = new Map([
  // Automatically unload beforeunload alerts
  ["dom.disable_beforeunload", true],
]);

const isRemote =
  Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT;

class MarionetteParentProcess {
  #browserStartupFinished;

  constructor() {
    this.server = null;
    this._activePortPath;

    this.classID = Components.ID("{786a1369-dca5-4adc-8486-33d23c88010a}");
    this.helpInfo = "  --marionette       Enable remote control server.\n";

    // Initially set the enabled state based on the environment variable.
    this.enabled = Services.env.exists(ENV_ENABLED);

    Services.ppmm.addMessageListener("Marionette:IsRunning", this);

    this.#browserStartupFinished = lazy.Deferred();
  }

  /**
   * A promise that resolves when the initial application window has been opened.
   *
   * @returns {Promise}
   *     Promise that resolves when the initial application window is open.
   */
  get browserStartupFinished() {
    return this.#browserStartupFinished.promise;
  }

  get enabled() {
    return this._enabled;
  }

  set enabled(value) {
    // Return early if Marionette is already marked as being enabled.
    // There is also no possibility to disable Marionette once it got enabled.
    if (this._enabled || !value) {
      return;
    }

    this._enabled = value;
    lazy.logger.info(`Marionette enabled`);
  }

  get running() {
    return !!this.server && this.server.alive;
  }

  receiveMessage({ name }) {
    switch (name) {
      case "Marionette:IsRunning":
        return this.running;

      default:
        lazy.logger.warn("Unknown IPC message to parent process: " + name);
        return null;
    }
  }

  handle(cmdLine) {
    // `handle` is called too late in certain cases (eg safe mode, see comment
    // above "command-line-startup"). So the marionette command line argument
    // will always be processed in `observe`.
    // However it still needs to be consumed by the command-line-handler API,
    // to avoid issues on macos.
    // TODO: remove after Bug 1724251 is fixed.
    cmdLine.handleFlag("marionette", false);
  }

  async observe(subject, topic) {
    if (this.enabled) {
      lazy.logger.trace(`Received observer notification ${topic}`);
    }

    switch (topic) {
      case "profile-after-change":
        Services.obs.addObserver(this, "command-line-startup");
        break;

      // In safe mode the command line handlers are getting parsed after the
      // safe mode dialog has been closed. To allow Marionette to start
      // earlier, use the CLI startup observer notification for
      // special-cased handlers, which gets fired before the dialog appears.
      case "command-line-startup":
        Services.obs.removeObserver(this, topic);

        this.enabled = subject.handleFlag("marionette", false);

        if (this.enabled) {
          // Marionette needs to be initialized before any window is shown.
          Services.obs.addObserver(this, "final-ui-startup");

          // We want to suppress the modal dialog that's shown
          // when starting up in safe-mode to enable testing.
          if (Services.appinfo.inSafeMode) {
            Services.obs.addObserver(this, "domwindowopened");
          }

          lazy.RecommendedPreferences.applyPreferences(RECOMMENDED_PREFS);

          // Only set preferences to preserve in a new profile
          // when Marionette is enabled.
          for (let [pref, value] of lazy.EnvironmentPrefs.from(
            ENV_PRESERVE_PREFS
          )) {
            lazy.Preferences.set(pref, value);
          }
        }
        break;

      case "domwindowopened":
        Services.obs.removeObserver(this, topic);
        this.suppressSafeModeDialog(subject);
        break;

      case "final-ui-startup":
        Services.obs.removeObserver(this, topic);

        Services.obs.addObserver(this, "browser-idle-startup-tasks-finished");
        Services.obs.addObserver(this, "mail-idle-startup-tasks-finished");
        Services.obs.addObserver(this, "quit-application");

        await this.init();
        break;

      // Used to wait until the initial application window has been opened.
      case "browser-idle-startup-tasks-finished":
      case "mail-idle-startup-tasks-finished":
        Services.obs.removeObserver(
          this,
          "browser-idle-startup-tasks-finished"
        );
        Services.obs.removeObserver(this, "mail-idle-startup-tasks-finished");
        this.#browserStartupFinished.resolve();
        break;

      case "quit-application":
        Services.obs.removeObserver(this, topic);
        await this.uninit();
        break;
    }
  }

  suppressSafeModeDialog(win) {
    win.addEventListener(
      "load",
      () => {
        let dialog = win.document.getElementById("safeModeDialog");
        if (dialog) {
          // accept the dialog to start in safe-mode
          lazy.logger.trace("Safe mode detected, supressing dialog");
          win.setTimeout(() => {
            dialog.getButton("accept").click();
          });
        }
      },
      { once: true }
    );
  }

  async init() {
    if (!this.enabled || this.running) {
      lazy.logger.debug(
        `Init aborted (enabled=${this.enabled}, running=${this.running})`
      );
      return;
    }

    try {
      this.server = new lazy.TCPListener(lazy.MarionettePrefs.port);
      await this.server.start();
    } catch (e) {
      lazy.logger.fatal("Marionette server failed to start", e);
      Services.startup.quit(Ci.nsIAppStartup.eForceQuit);
      return;
    }

    Services.env.set(ENV_ENABLED, "1");
    Services.obs.notifyObservers(this, NOTIFY_LISTENING, true);
    lazy.logger.debug("Marionette is listening");

    // Write Marionette port to MarionetteActivePort file within the profile.
    this._activePortPath = PathUtils.join(
      PathUtils.profileDir,
      "MarionetteActivePort"
    );

    const data = `${this.server.port}`;
    try {
      await IOUtils.write(this._activePortPath, lazy.textEncoder.encode(data));
    } catch (e) {
      lazy.logger.warn(
        `Failed to create ${this._activePortPath} (${e.message})`
      );
    }
  }

  async uninit() {
    if (this.running) {
      await this.server.stop();
      Services.obs.notifyObservers(this, NOTIFY_LISTENING);
      lazy.logger.debug("Marionette stopped listening");

      try {
        await IOUtils.remove(this._activePortPath);
      } catch (e) {
        lazy.logger.warn(
          `Failed to remove ${this._activePortPath} (${e.message})`
        );
      }
    }
  }

  get QueryInterface() {
    return ChromeUtils.generateQI([
      "nsICommandLineHandler",
      "nsIMarionette",
      "nsIObserver",
    ]);
  }
}

class MarionetteContentProcess {
  constructor() {
    this.classID = Components.ID("{786a1369-dca5-4adc-8486-33d23c88010a}");
  }

  get running() {
    let reply = Services.cpmm.sendSyncMessage("Marionette:IsRunning");
    if (!reply.length) {
      lazy.logger.warn("No reply from parent process");
      return false;
    }
    return reply[0];
  }

  get QueryInterface() {
    return ChromeUtils.generateQI(["nsIMarionette"]);
  }
}

export var Marionette;
if (isRemote) {
  Marionette = new MarionetteContentProcess();
} else {
  Marionette = new MarionetteParentProcess();
}

// This is used by the XPCOM codepath which expects a constructor
export const MarionetteFactory = function () {
  return Marionette;
};
