/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(
    this, "env", "@mozilla.org/process/environment;1", "nsIEnvironment");
ChromeUtils.defineModuleGetter(this, "Log",
    "resource://gre/modules/Log.jsm");
ChromeUtils.defineModuleGetter(this, "Preferences",
    "resource://gre/modules/Preferences.jsm");
XPCOMUtils.defineLazyGetter(this, "log", () => {
  let log = Log.repository.getLogger("Marionette");
  log.addAppender(new Log.DumpAppender());
  return log;
});

const PREF_ENABLED = "marionette.enabled";
const PREF_PORT = "marionette.port";
const PREF_PORT_FALLBACK = "marionette.defaultPrefs.port";
const PREF_LOG_LEVEL = "marionette.log.level";
const PREF_LOG_LEVEL_FALLBACK = "marionette.logging";

const DEFAULT_LOG_LEVEL = "info";
const NOTIFY_RUNNING = "remote-active";

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

const isRemote = Services.appinfo.processType ==
    Services.appinfo.PROCESS_TYPE_CONTENT;

const LogLevel = {
  get(level) {
    let levels = new Map([
      ["fatal", Log.Level.Fatal],
      ["error", Log.Level.Error],
      ["warn", Log.Level.Warn],
      ["info", Log.Level.Info],
      ["config", Log.Level.Config],
      ["debug", Log.Level.Debug],
      ["trace", Log.Level.Trace],
    ]);

    let s = String(level).toLowerCase();
    if (!levels.has(s)) {
      return DEFAULT_LOG_LEVEL;
    }
    return levels.get(s);
  },
};

function getPrefVal(pref) {
  const {PREF_STRING, PREF_BOOL, PREF_INT, PREF_INVALID} = Ci.nsIPrefBranch;

  let type = Services.prefs.getPrefType(pref);
  switch (type) {
    case PREF_STRING:
      return Services.prefs.getStringPref(pref);

    case PREF_BOOL:
      return Services.prefs.getBoolPref(pref);

    case PREF_INT:
      return Services.prefs.getIntPref(pref);

    case PREF_INVALID:
      return undefined;

    default:
      throw new TypeError(`Unexpected preference type (${type}) for ${pref}`);
  }
}

// Get preference value of |preferred|, falling back to |fallback|
// if |preferred| is not user-modified and |fallback| exists.
function getPref(preferred, fallback) {
  if (!Services.prefs.prefHasUserValue(preferred) &&
      Services.prefs.getPrefType(fallback) != Ci.nsIPrefBranch.PREF_INVALID) {
    return getPrefVal(fallback, getPrefVal(preferred));
  }
  return getPrefVal(preferred);
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
    return LogLevel.get(s);
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

class MarionetteMainProcess {
  constructor() {
    this.server = null;

    // holds reference to ChromeWindow
    // used to run GFX sanity tests on Windows
    this.gfxWindow = null;

    // indicates that all pending window checks have been completed
    // and that we are ready to start the Marionette server
    this.finalUIStartup = false;

    log.level = prefs.logLevel;

    this.enabled = env.exists(ENV_ENABLED);

    Services.prefs.addObserver(PREF_ENABLED, this);
    Services.ppmm.addMessageListener("Marionette:IsRunning", this);
  }

  get running() {
    return this.server && this.server.alive;
  }

  set enabled(value) {
    Services.prefs.setBoolPref(PREF_ENABLED, value);
  }

  get enabled() {
    return Services.prefs.getBoolPref(PREF_ENABLED);
  }

  receiveMessage({name}) {
    switch (name) {
      case "Marionette:IsRunning":
        return this.running;

      default:
        log.warn("Unknown IPC message to main process: " + name);
        return null;
    }
  }

  observe(subject, topic) {
    log.debug(`Received observer notification ${topic}`);

    switch (topic) {
      case "nsPref:changed":
        if (Services.prefs.getBoolPref(PREF_ENABLED)) {
          this.init();
        } else {
          this.uninit();
        }
        break;

      case "profile-after-change":
        Services.obs.addObserver(this, "command-line-startup");
        Services.obs.addObserver(this, "sessionstore-windows-restored");

        prefs.readFromEnvironment(ENV_PRESERVE_PREFS);
        break;

      // In safe mode the command line handlers are getting parsed after the
      // safe mode dialog has been closed. To allow Marionette to start
      // earlier, use the CLI startup observer notification for
      // special-cased handlers, which gets fired before the dialog appears.
      case "command-line-startup":
        Services.obs.removeObserver(this, topic);

        if (!this.enabled && subject.handleFlag("marionette", false)) {
          this.enabled = true;
        }

        // We want to suppress the modal dialog that's shown
        // when starting up in safe-mode to enable testing.
        if (this.enabled && Services.appinfo.inSafeMode) {
          Services.obs.addObserver(this, "domwindowopened");
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
  }

  suppressSafeModeDialog(win) {
    win.addEventListener("load", () => {
      if (win.document.getElementById("safeModeDialog")) {
        // accept the dialog to start in safe-mode
        log.debug("Safe mode detected, supressing dialog");
        win.setTimeout(() => {
          win.document.documentElement.getButton("accept").click();
        });
      }
    }, {once: true});
  }

  init() {
    if (this.running || !this.enabled || !this.finalUIStartup) {
      return;
    }

    // wait for delayed startup...
    Services.tm.idleDispatchToMainThread(async () => {
      // ... and for startup tests
      let startupRecorder = Promise.resolve();
      if ("@mozilla.org/test/startuprecorder;1" in Cc) {
        startupRecorder = Cc["@mozilla.org/test/startuprecorder;1"]
            .getService().wrappedJSObject.done;
      }
      await startupRecorder;

      try {
        ChromeUtils.import("chrome://marionette/content/server.js");
        let listener = new server.TCPListener(prefs.port);
        listener.start();
        this.server = listener;
      } catch (e) {
        log.fatal("Remote protocol server failed to start", e);
        Services.startup.quit(Ci.nsIAppStartup.eForceQuit);
      }

      Services.obs.notifyObservers(this, NOTIFY_RUNNING, true);
      log.info(`Listening on port ${this.server.port}`);
    });
  }

  uninit() {
    if (this.running) {
      this.server.stop();
      Services.obs.notifyObservers(this, NOTIFY_RUNNING);
    }
  }

  get QueryInterface() {
    return XPCOMUtils.generateQI([
      Ci.nsICommandLineHandler,
      Ci.nsIMarionette,
      Ci.nsIObserver,
    ]);
  }
}

class MarionetteContentProcess {
  get running() {
    let reply = Services.cpmm.sendSyncMessage("Marionette:IsRunning");
    if (reply.length == 0) {
      log.warn("No reply from main process");
      return false;
    }
    return reply[0];
  }

  get QueryInterface() {
    return XPCOMUtils.generateQI([Ci.nsIMarionette]);
  }
}

const MarionetteFactory = {
  instance_: null,

  createInstance(outer, iid) {
    if (outer) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }

    if (!this.instance_) {
      if (isRemote) {
        this.instance_ = new MarionetteContentProcess();
      } else {
        this.instance_ = new MarionetteMainProcess();
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

  /* eslint-disable camelcase */
  _xpcom_factory: MarionetteFactory,

  _xpcom_categories: [
    {category: "command-line-handler", entry: "b-marionette"},
    {category: "profile-after-change", service: true},
  ],
  /* eslint-enable camelcase */

  helpInfo: "  --marionette       Enable remote control server.\n",
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([Marionette]);
