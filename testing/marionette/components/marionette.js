/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const {
  EnvironmentPrefs,
  MarionettePrefs,
} = ChromeUtils.import("chrome://marionette/content/prefs.js", {});

XPCOMUtils.defineLazyModuleGetters(this, {
  Log: "chrome://marionette/content/log.js",
  Preferences: "resource://gre/modules/Preferences.jsm",
  TCPListener: "chrome://marionette/content/server.js",
});

XPCOMUtils.defineLazyGetter(this, "log", Log.get);

XPCOMUtils.defineLazyServiceGetter(
    this, "env", "@mozilla.org/process/environment;1", "nsIEnvironment");

const XMLURI_PARSE_ERROR = "http://www.mozilla.org/newlayout/xml/parsererror.xml";

const NOTIFY_RUNNING = "remote-active";

// Complements -marionette flag for starting the Marionette server.
// We also set this if Marionette is running in order to start the server
// again after a Firefox restart.
const ENV_ENABLED = "MOZ_MARIONETTE";
const PREF_ENABLED = "marionette.enabled";

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

// Marionette sets preferences recommended for automation when it starts,
// unless marionette.prefs.recommended has been set to false.
// Where noted, some prefs should also be set in the profile passed to
// Marionette to prevent them from affecting startup, since some of these
// are checked before Marionette initialises.
const RECOMMENDED_PREFS = new Map([

  // Make sure Shield doesn't hit the network.
  ["app.normandy.api_url", ""],

  // Disable automatic downloading of new releases.
  //
  // This should also be set in the profile prior to starting Firefox,
  // as it is picked up at runtime.
  ["app.update.auto", false],

  // Disable automatically upgrading Firefox.
  //
  // This should also be set in the profile prior to starting Firefox,
  // as it is picked up at runtime.
  ["app.update.enabled", false],

  // Increase the APZ content response timeout in tests to 1 minute.
  // This is to accommodate the fact that test environments tends to be
  // slower than production environments (with the b2g emulator being
  // the slowest of them all), resulting in the production timeout value
  // sometimes being exceeded and causing false-positive test failures.
  //
  // (bug 1176798, bug 1177018, bug 1210465)
  ["apz.content_response_timeout", 60000],

  // Indicate that the download panel has been shown once so that
  // whichever download test runs first doesn't show the popup
  // inconsistently.
  ["browser.download.panel.shown", true],

  // Background thumbnails in particular cause grief, and disabling
  // thumbnails in general cannot hurt
  ["browser.pagethumbnails.capturing_disabled", true],

  // Disable safebrowsing components.
  //
  // These should also be set in the profile prior to starting Firefox,
  // as it is picked up at runtime.
  ["browser.safebrowsing.blockedURIs.enabled", false],
  ["browser.safebrowsing.downloads.enabled", false],
  ["browser.safebrowsing.passwords.enabled", false],
  ["browser.safebrowsing.malware.enabled", false],
  ["browser.safebrowsing.phishing.enabled", false],

  // Disable updates to search engines.
  //
  // Should be set in profile.
  ["browser.search.update", false],

  // Do not restore the last open set of tabs if the browser has crashed
  ["browser.sessionstore.resume_from_crash", false],

  // Don't check for the default web browser during startup.
  //
  // These should also be set in the profile prior to starting Firefox,
  // as it is picked up at runtime.
  ["browser.shell.checkDefaultBrowser", false],

  // Do not redirect user when a milstone upgrade of Firefox is detected
  ["browser.startup.homepage_override.mstone", "ignore"],

  // Disable browser animations (tabs, fullscreen, sliding alerts)
  ["toolkit.cosmeticAnimations.enabled", false],

  // Do not close the window when the last tab gets closed
  ["browser.tabs.closeWindowWithLastTab", false],

  // Do not allow background tabs to be zombified on Android, otherwise for
  // tests that open additional tabs, the test harness tab itself might get
  // unloaded
  ["browser.tabs.disableBackgroundZombification", false],

  // Do not warn when closing all open tabs
  ["browser.tabs.warnOnClose", false],

  // Do not warn when closing all other open tabs
  ["browser.tabs.warnOnCloseOtherTabs", false],

  // Do not warn when multiple tabs will be opened
  ["browser.tabs.warnOnOpen", false],

  // Disable first run splash page on Windows 10
  ["browser.usedOnWindows10.introURL", ""],

  // Disable the UI tour.
  //
  // Should be set in profile.
  ["browser.uitour.enabled", false],

  // Turn off search suggestions in the location bar so as not to trigger
  // network connections.
  ["browser.urlbar.suggest.searches", false],

  // Do not warn on quitting Firefox
  ["browser.warnOnQuit", false],

  // Do not show datareporting policy notifications which can
  // interfere with tests
  [
    "datareporting.healthreport.documentServerURI",
    "http://%(server)s/dummy/healthreport/",
  ],
  ["datareporting.healthreport.logging.consoleEnabled", false],
  ["datareporting.healthreport.service.enabled", false],
  ["datareporting.healthreport.service.firstRun", false],
  ["datareporting.healthreport.uploadEnabled", false],
  ["datareporting.policy.dataSubmissionEnabled", false],
  ["datareporting.policy.dataSubmissionPolicyAccepted", false],
  ["datareporting.policy.dataSubmissionPolicyBypassNotification", true],

  // Automatically unload beforeunload alerts
  ["dom.disable_beforeunload", true],

  // Disable popup-blocker
  ["dom.disable_open_during_load", false],

  // Enabling the support for File object creation in the content process
  ["dom.file.createInChild", true],

  // Disable the ProcessHangMonitor
  ["dom.ipc.reportProcessHangs", false],

  // Disable slow script dialogues
  ["dom.max_chrome_script_run_time", 0],
  ["dom.max_script_run_time", 0],

  // Only load extensions from the application and user profile
  // AddonManager.SCOPE_PROFILE + AddonManager.SCOPE_APPLICATION
  //
  // Should be set in profile.
  ["extensions.autoDisableScopes", 0],
  ["extensions.enabledScopes", 5],

  // Disable metadata caching for installed add-ons by default
  ["extensions.getAddons.cache.enabled", false],

  // Disable installing any distribution extensions or add-ons.
  // Should be set in profile.
  ["extensions.installDistroAddons", false],

  // Turn off extension updates so they do not bother tests
  ["extensions.update.enabled", false],
  ["extensions.update.notifyUser", false],

  // Make sure opening about:addons will not hit the network
  [
    "extensions.webservice.discoverURL",
    "http://%(server)s/dummy/discoveryURL",
  ],

  // Allow the application to have focus even it runs in the background
  ["focusmanager.testmode", true],

  // Disable useragent updates
  ["general.useragent.updates.enabled", false],

  // Always use network provider for geolocation tests so we bypass the
  // macOS dialog raised by the corelocation provider
  ["geo.provider.testing", true],

  // Do not scan Wifi
  ["geo.wifi.scan", false],

  // Show chrome errors and warnings in the error console
  ["javascript.options.showInConsole", true],

  // Do not prompt with long usernames or passwords in URLs
  ["network.http.phishy-userpass-length", 255],

  // Do not prompt for temporary redirects
  ["network.http.prompt-temp-redirect", false],

  // Disable speculative connections so they are not reported as leaking
  // when they are hanging around
  ["network.http.speculative-parallel-limit", 0],

  // Do not automatically switch between offline and online
  ["network.manage-offline-status", false],

  // Make sure SNTP requests do not hit the network
  ["network.sntp.pools", "%(server)s"],

  // Local documents have access to all other local documents,
  // including directory listings
  ["security.fileuri.strict_origin_policy", false],

  // Tests do not wait for the notification button security delay
  ["security.notification_enable_delay", 0],

  // Ensure blocklist updates do not hit the network
  ["services.settings.server", "http://%(server)s/dummy/blocklist/"],

  // Do not automatically fill sign-in forms with known usernames and
  // passwords
  ["signon.autofillForms", false],

  // Disable password capture, so that tests that include forms are not
  // influenced by the presence of the persistent doorhanger notification
  ["signon.rememberSignons", false],

  // Disable first-run welcome page
  ["startup.homepage_welcome_url", "about:blank"],
  ["startup.homepage_welcome_url.additional", ""],

  // Prevent starting into safe mode after application crashes
  ["toolkit.startup.max_resumed_crashes", -1],

]);

const isRemote = Services.appinfo.processType ==
    Services.appinfo.PROCESS_TYPE_CONTENT;

class MarionetteParentProcess {
  constructor() {
    this.server = null;

    // holds reference to ChromeWindow
    // used to run GFX sanity tests on Windows
    this.gfxWindow = null;

    // indicates that all pending window checks have been completed
    // and that we are ready to start the Marionette server
    this.finalUIStartup = false;

    this.enabled = env.exists(ENV_ENABLED);
    this.alteredPrefs = new Set();

    Services.prefs.addObserver(PREF_ENABLED, this);
    Services.ppmm.addMessageListener("Marionette:IsRunning", this);
  }

  get running() {
    return !!this.server && this.server.alive;
  }

  set enabled(value) {
    MarionettePrefs.enabled = value;
  }

  get enabled() {
    return MarionettePrefs.enabled;
  }

  receiveMessage({name}) {
    switch (name) {
      case "Marionette:IsRunning":
        return this.running;

      default:
        log.warn("Unknown IPC message to parent process: " + name);
        return null;
    }
  }

  observe(subject, topic) {
    log.debug(`Received observer notification ${topic}`);

    switch (topic) {
      case "nsPref:changed":
        if (this.enabled) {
          this.init(false);
        } else {
          this.uninit();
        }
        break;

      case "profile-after-change":
        Services.obs.addObserver(this, "command-line-startup");
        Services.obs.addObserver(this, "sessionstore-windows-restored");
        Services.obs.addObserver(this, "toplevel-window-ready");

        for (let [pref, value] of EnvironmentPrefs.from(ENV_PRESERVE_PREFS)) {
          Preferences.set(pref, value);
        }
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

          Services.obs.addObserver(this, "xpcom-will-shutdown");
          this.finalUIStartup = true;
          this.init();
        }
        break;

      case "domwindowopened":
        Services.obs.removeObserver(this, topic);
        this.suppressSafeModeDialog(subject);
        break;

      case "toplevel-window-ready":
        subject.addEventListener("load", ev => {
          if (ev.target.documentElement.namespaceURI == XMLURI_PARSE_ERROR) {
            Services.obs.removeObserver(this, topic);

            let parserError = ev.target.querySelector("parsererror");
            log.fatal(parserError.textContent);
            this.uninit();
            Services.startup.quit(Ci.nsIAppStartup.eForceQuit);
          }
        }, {once: true});
        break;

      case "sessionstore-windows-restored":
        Services.obs.removeObserver(this, topic);
        Services.obs.removeObserver(this, "toplevel-window-ready");

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
          log.debug("GFX sanity window detected, waiting until it has been closed...");
          Services.obs.addObserver(this, "domwindowclosed");
        } else {
          Services.obs.addObserver(this, "xpcom-will-shutdown");
          this.finalUIStartup = true;
          this.init();
        }

        break;

      case "xpcom-will-shutdown":
        Services.obs.removeObserver(this, "xpcom-will-shutdown");
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

  init(quit = true) {
    if (this.running || !this.enabled || !this.finalUIStartup) {
      log.debug(`Init aborted (running=${this.running}, ` +
                `enabled=${this.enabled}, finalUIStartup=${this.finalUIStartup})`);
      return;
    }

    log.debug(`Waiting for delayed startup...`);
    Services.tm.idleDispatchToMainThread(async () => {
      let startupRecorder = Promise.resolve();
      if ("@mozilla.org/test/startuprecorder;1" in Cc) {
        log.debug(`Waiting for startup tests...`);
        startupRecorder = Cc["@mozilla.org/test/startuprecorder;1"]
            .getService().wrappedJSObject.done;
      }
      await startupRecorder;

      if (MarionettePrefs.recommendedPrefs) {
        for (let [k, v] of RECOMMENDED_PREFS) {
          if (!Preferences.isSet(k)) {
            log.debug(`Setting recommended pref ${k} to ${v}`);
            Preferences.set(k, v);
            this.alteredPrefs.add(k);
          }
        }
      }

      try {
        this.server = new TCPListener(MarionettePrefs.port);
        this.server.start();
      } catch (e) {
        log.fatal("Remote protocol server failed to start", e);
        this.uninit();
        if (quit) {
          Services.startup.quit(Ci.nsIAppStartup.eForceQuit);
        }
        return;
      }

      env.set(ENV_ENABLED, "1");
      Services.obs.notifyObservers(this, NOTIFY_RUNNING, true);
      log.debug("Remote service is active");
    });
  }

  uninit() {
    for (let k of this.alteredPrefs) {
      log.debug(`Resetting recommended pref ${k}`);
      Preferences.reset(k);
    }
    this.alteredPrefs.clear();

    if (this.running) {
      this.server.stop();
      Services.obs.notifyObservers(this, NOTIFY_RUNNING);
      log.debug("Remote service is inactive");
    }
  }

  get QueryInterface() {
    return ChromeUtils.generateQI([
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
      log.warn("No reply from parent process");
      return false;
    }
    return reply[0];
  }

  get QueryInterface() {
    return ChromeUtils.generateQI([Ci.nsIMarionette]);
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
