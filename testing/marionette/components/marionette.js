/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Constructor: CC, interfaces: Ci, utils: Cu, classes: Cc} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(
    this, "env", "@mozilla.org/process/environment;1", "nsIEnvironment");

const MARIONETTE_CONTRACT_ID = "@mozilla.org/remote/marionette;1";
const MARIONETTE_CID = Components.ID("{786a1369-dca5-4adc-8486-33d23c88010a}");

const PREF_PORT = "marionette.port";
const PREF_PORT_FALLBACK = "marionette.defaultPrefs.port";
const PREF_LOG_LEVEL = "marionette.log.level";
const PREF_LOG_LEVEL_FALLBACK = "marionette.logging";

const DEFAULT_LOG_LEVEL = "info";
const LOG_LEVELS = new class extends Map {
  constructor() {
    super([
      ["fatal", Log.Level.Fatal],
      ["error", Log.Level.Error],
      ["warn", Log.Level.Warn],
      ["info", Log.Level.Info],
      ["config", Log.Level.Config],
      ["debug", Log.Level.Debug],
      ["trace", Log.Level.Trace],
    ]);
  }

  get(level) {
    let s = new String(level).toLowerCase();
    if (!this.has(s)) {
      return DEFAULT_LOG_LEVEL;
    }
    return super.get(s);
  }
};

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

const ServerSocket = CC("@mozilla.org/network/server-socket;1",
    "nsIServerSocket",
    "initSpecialConnection");

// Get preference value of |preferred|, falling back to |fallback|
// if |preferred| is not user-modified and |fallback| exists.
function getPref(preferred, fallback) {
  if (!Preferences.isSet(preferred) && Preferences.has(fallback)) {
    return Preferences.get(fallback, Preferences.get(preferred));
  }
  return Preferences.get(preferred);
}

// Marionette preferences recently changed names.  This is an abstraction
// that first looks for the new name, but falls back to using the old name
// if the new does not exist.
//
// This shim can be removed when Firefox 55 ships.
const prefs = {
  get port() {
    return getPref(PREF_PORT, PREF_PORT_FALLBACK);
  },

  get logLevel() {
    let s = getPref(PREF_LOG_LEVEL, PREF_LOG_LEVEL_FALLBACK);
    return LOG_LEVELS.get(s);
  },

  readFromEnvironment(key) {
    const env = Cc["@mozilla.org/process/environment;1"]
        .getService(Ci.nsIEnvironment);

    if (env.exists(key)) {
      let prefs;
      try {
        prefs = JSON.parse(env.get(key));
      } catch (e) {
        Cu.reportError(
            "Invalid Marionette preferences in environment; " +
            "preferences will not have been applied");
        Cu.reportError(e);
      }

      if (prefs) {
        for (let prefName of Object.keys(prefs)) {
          Preferences.set(prefName, prefs[prefName]);
        }
      }
    }
  },
};

function MarionetteComponent() {
  this.running = false;
  this.server = null;

  // holds reference to ChromeWindow
  // used to run GFX sanity tests on Windows
  this.gfxWindow = null;

  // indicates that all pending window checks have been completed
  // and that we are ready to start the Marionette server
  this.finalUIStartup = false;

  this.logger = this.setupLogger(prefs.logLevel);

  this.enabled = env.exists(ENV_ENABLED);
  if (this.enabled) {
    this.logger.info(`Enabled via ${ENV_ENABLED}`);
  }
}

MarionetteComponent.prototype = {
  classDescription: "Marionette component",
  classID: MARIONETTE_CID,
  contractID: MARIONETTE_CONTRACT_ID,
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsICommandLineHandler,
    Ci.nsIMarionette,
  ]),
  _xpcom_categories: [
    {category: "command-line-handler", entry: "b-marionette"},
    {category: "profile-after-change", service: true},
  ],
  helpInfo: "  --marionette       Enable remote control server.\n",
};

// Handle -marionette flag
MarionetteComponent.prototype.handle = function(cmdLine) {
  if (!this.enabled && cmdLine.handleFlag("marionette", false)) {
    this.enabled = true;
    this.logger.info("Enabled via --marionette");
  }
};

MarionetteComponent.prototype.observe = function(subject, topic, data) {
  this.logger.debug(`Received observer notification "${topic}"`);

  switch (topic) {
    case "command-line-startup":
      Services.obs.removeObserver(this, topic);
      this.handle(subject);

    case "profile-after-change":
      // Using sessionstore-windows-restored as the xpcom category doesn't
      // seem to work, so we wait for that by adding an observer here.
      Services.obs.addObserver(this, "sessionstore-windows-restored");

      // In safe mode the command line handlers are getting parsed after the
      // safe mode dialog has been closed. To allow Marionette to start
      // earlier, register the CLI startup observer notification for
      // special-cased handlers, which gets fired before the dialog appears.
      Services.obs.addObserver(this, "command-line-startup");

      prefs.readFromEnvironment(ENV_PRESERVE_PREFS);

      if (this.enabled) {
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

        Services.obs.addObserver(this, "xpcom-shutdown");
        this.finalUIStartup = true;
        this.init();
      }
      break;

    case "domwindowopened":
      Services.obs.removeObserver(this, topic);
      this.suppressSafeModeDialog(subject);
      break;

    case "sessionstore-windows-restored":
      Services.obs.removeObserver(this, topic);

      // When Firefox starts on Windows, an additional GFX sanity test
      // window may appear off-screen.  Marionette should wait for it
      // to close.
      let winEn = Services.wm.getEnumerator(null);
      while (winEn.hasMoreElements()) {
        let win = winEn.getNext();
        if (win.document.documentURI == "chrome://gfxsanity/content/sanityparent.html") {
          this.gfxWindow = win;
          break;
        }
      }

      if (this.gfxWindow) {
        Services.obs.addObserver(this, "domwindowclosed");
      } else {
        Services.obs.addObserver(this, "xpcom-shutdown");
        this.finalUIStartup = true;
        this.init();
      }

      break;

    case "xpcom-shutdown":
      Services.obs.removeObserver(this, "xpcom-shutdown");
      this.uninit();
      break;
  }
};

MarionetteComponent.prototype.setupLogger = function(level) {
  let logger = Log.repository.getLogger("Marionette");
  logger.level = level;
  logger.addAppender(new Log.DumpAppender());
  return logger;
};

MarionetteComponent.prototype.suppressSafeModeDialog = function(win) {
  win.addEventListener("load", () => {
    if (win.document.getElementById("safeModeDialog")) {
      // accept the dialog to start in safe-mode
      this.logger.debug("Safe Mode detected. Going to suspress the dialog now.");
      win.setTimeout(() => {
        win.document.documentElement.getButton("accept").click();
      });
    }
  }, {once: true});
};

MarionetteComponent.prototype.init = function() {
  if (this.running || !this.enabled || !this.finalUIStartup) {
    return;
  }

  // Delay initialization until we are done with delayed startup...
  Services.tm.idleDispatchToMainThread(() => {
    // ... and with startup tests.
    let promise = Promise.resolve();
    if ("@mozilla.org/test/startuprecorder;1" in Cc)
      promise = Cc["@mozilla.org/test/startuprecorder;1"].getService().wrappedJSObject.done;
    promise.then(() => {
      let s;
      try {
        Cu.import("chrome://marionette/content/server.js");
        s = new server.TCPListener(prefs.port);
        s.start();
        this.logger.info(`Listening on port ${s.port}`);
      } finally {
        if (s) {
          this.server = s;
          this.running = true;
        }
      }
    });
  });
};

MarionetteComponent.prototype.uninit = function() {
  if (!this.running) {
    return;
  }
  this.server.stop();
  this.running = false;
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([MarionetteComponent]);
