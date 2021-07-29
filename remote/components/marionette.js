/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ComponentUtils: "resource://gre/modules/ComponentUtils.jsm",

  EnvironmentPrefs: "chrome://remote/content/marionette/prefs.js",
  Log: "chrome://remote/content/shared/Log.jsm",
  MarionettePrefs: "chrome://remote/content/marionette/prefs.js",
  Preferences: "resource://gre/modules/Preferences.jsm",
  RecommendedPreferences:
    "chrome://remote/content/shared/RecommendedPreferences.jsm",
  TCPListener: "chrome://remote/content/marionette/server.js",
});

XPCOMUtils.defineLazyGetter(this, "logger", () =>
  Log.get(Log.TYPES.MARIONETTE)
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "env",
  "@mozilla.org/process/environment;1",
  "nsIEnvironment"
);

const XMLURI_PARSE_ERROR =
  "http://www.mozilla.org/newlayout/xml/parsererror.xml";

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
  constructor() {
    this.server = null;

    // holds reference to ChromeWindow
    // used to run GFX sanity tests on Windows
    this.gfxWindow = null;

    // indicates that all pending window checks have been completed
    // and that we are ready to start the Marionette server
    this.finalUIStartup = false;

    // Initially set the enabled state based on the environment variable.
    this.enabled = env.exists(ENV_ENABLED);

    Services.ppmm.addMessageListener("Marionette:IsRunning", this);
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
    logger.info(`Marionette enabled`);
  }

  get running() {
    return !!this.server && this.server.alive;
  }

  receiveMessage({ name }) {
    switch (name) {
      case "Marionette:IsRunning":
        return this.running;

      default:
        logger.warn("Unknown IPC message to parent process: " + name);
        return null;
    }
  }

  observe(subject, topic) {
    if (this.enabled) {
      logger.trace(`Received observer notification ${topic}`);
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
          Services.obs.addObserver(this, "toplevel-window-ready");
          Services.obs.addObserver(this, "marionette-startup-requested");

          // Only set preferences to preserve in a new profile
          // when Marionette is enabled.
          for (let [pref, value] of EnvironmentPrefs.from(ENV_PRESERVE_PREFS)) {
            Preferences.set(pref, value);
          }

          // We want to suppress the modal dialog that's shown
          // when starting up in safe-mode to enable testing.
          if (Services.appinfo.inSafeMode) {
            Services.obs.addObserver(this, "domwindowopened");
          }
        }

        break;

      case "domwindowclosed":
        if (this.gfxWindow === null || subject === this.gfxWindow) {
          Services.obs.removeObserver(this, topic);
          Services.obs.removeObserver(this, "toplevel-window-ready");

          Services.obs.addObserver(this, "quit-application");

          this.finalUIStartup = true;
          this.init();
        }
        break;

      case "domwindowopened":
        Services.obs.removeObserver(this, topic);
        this.suppressSafeModeDialog(subject);
        break;

      case "toplevel-window-ready":
        subject.addEventListener(
          "load",
          ev => {
            if (ev.target.documentElement.namespaceURI == XMLURI_PARSE_ERROR) {
              Services.obs.removeObserver(this, topic);

              let parserError = ev.target.querySelector("parsererror");
              logger.fatal(parserError.textContent);
              this.uninit();
              Services.startup.quit(Ci.nsIAppStartup.eForceQuit);
            }
          },
          { once: true }
        );
        break;

      case "marionette-startup-requested":
        Services.obs.removeObserver(this, topic);

        // When Firefox starts on Windows, an additional GFX sanity test
        // window may appear off-screen.  Marionette should wait for it
        // to close.
        for (let win of Services.wm.getEnumerator(null)) {
          if (
            win.document.documentURI ==
            "chrome://gfxsanity/content/sanityparent.html"
          ) {
            this.gfxWindow = win;
            break;
          }
        }

        if (this.gfxWindow) {
          logger.trace(
            "GFX sanity window detected, waiting until it has been closed..."
          );
          Services.obs.addObserver(this, "domwindowclosed");
        } else {
          Services.obs.removeObserver(this, "toplevel-window-ready");

          Services.obs.addObserver(this, "quit-application");

          this.finalUIStartup = true;
          this.init();
        }

        break;

      case "quit-application":
        Services.obs.removeObserver(this, "quit-application");
        this.uninit();
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
          logger.trace("Safe mode detected, supressing dialog");
          win.setTimeout(() => {
            dialog.getButton("accept").click();
          });
        }
      },
      { once: true }
    );
  }

  init(quit = true) {
    if (this.running || !this.enabled || !this.finalUIStartup) {
      logger.debug(
        `Init aborted (running=${this.running}, ` +
          `enabled=${this.enabled}, finalUIStartup=${this.finalUIStartup})`
      );
      return;
    }

    logger.trace(
      `Waiting until startup recorder finished recording startup scripts...`
    );
    Services.tm.idleDispatchToMainThread(async () => {
      let startupRecorder = Promise.resolve();
      if ("@mozilla.org/test/startuprecorder;1" in Cc) {
        startupRecorder = Cc["@mozilla.org/test/startuprecorder;1"].getService()
          .wrappedJSObject.done;
      }
      await startupRecorder;
      logger.trace(`All scripts recorded.`);

      RecommendedPreferences.applyPreferences(RECOMMENDED_PREFS);

      try {
        this.server = new TCPListener(MarionettePrefs.port);
        this.server.start();
      } catch (e) {
        logger.fatal("Remote protocol server failed to start", e);
        this.uninit();
        if (quit) {
          Services.startup.quit(Ci.nsIAppStartup.eForceQuit);
        }
        return;
      }

      env.set(ENV_ENABLED, "1");
      Services.obs.notifyObservers(this, NOTIFY_LISTENING, true);
      logger.debug("Marionette is listening");
    });
  }

  uninit() {
    if (this.running) {
      this.server.stop();
      Services.obs.notifyObservers(this, NOTIFY_LISTENING);
      logger.debug("Marionette stopped listening");
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
  get running() {
    let reply = Services.cpmm.sendSyncMessage("Marionette:IsRunning");
    if (reply.length == 0) {
      logger.warn("No reply from parent process");
      return false;
    }
    return reply[0];
  }

  get QueryInterface() {
    return ChromeUtils.generateQI(["nsIMarionette"]);
  }
}

const MarionetteFactory = {
  instance_: null,

  createInstance(outer, iid) {
    if (outer) {
      throw Components.Exception("", Cr.NS_ERROR_NO_AGGREGATION);
    }

    if (!this.instance_) {
      if (isRemote) {
        this.instance_ = new MarionetteContentProcess();
      } else {
        this.instance_ = new MarionetteParentProcess();
      }
    }

    return this.instance_.QueryInterface(iid);
  },
};

function Marionette() {}

Marionette.prototype = {
  classDescription: "Marionette component",
  classID: Components.ID("{786a1369-dca5-4adc-8486-33d23c88010a}"),
  contractID: "@mozilla.org/remote/marionette;1",

  /* eslint-disable-next-line camelcase */
  _xpcom_factory: MarionetteFactory,

  helpInfo: "  --marionette       Enable remote control server.\n",
};

this.NSGetFactory = ComponentUtils.generateNSGetFactory([Marionette]);
