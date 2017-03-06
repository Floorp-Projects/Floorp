/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var {Constructor: CC, classes: Cc, interfaces: Ci, utils: Cu} = Components;

var loader = Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Ci.mozIJSSubScriptLoader);
const ServerSocket = CC("@mozilla.org/network/server-socket;1", "nsIServerSocket", "initSpecialConnection");

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");

Cu.import("chrome://marionette/content/dispatcher.js");
Cu.import("chrome://marionette/content/driver.js");
Cu.import("chrome://marionette/content/element.js");
Cu.import("chrome://marionette/content/simpletest.js");

// Bug 1083711: Load transport.js as an SDK module instead of subscript
loader.loadSubScript("resource://devtools/shared/transport/transport.js");

const logger = Log.repository.getLogger("Marionette");

this.EXPORTED_SYMBOLS = ["MarionetteServer"];

const CONTENT_LISTENER_PREF = "marionette.contentListener";

// Marionette sets preferences recommended for automation when it starts,
// unless |marionette.prefs.recommended| has been set to false.
// Where noted, some prefs should also be set in the profile passed to
// Marionette to prevent them from affecting startup, since some of these
// are checked before Marionette initialises.
const RECOMMENDED_PREFS = new Map([

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

  // Do not show the EULA notification.
  //
  // This should also be set in the profile prior to starting Firefox,
  // as it is picked up at runtime.
  ["browser.EULA.override", true],

  // Turn off about:newtab and make use of about:blank instead for new
  // opened tabs.
  //
  // This should also be set in the profile prior to starting Firefox,
  // as it is picked up at runtime.
  ["browser.newtabpage.enabled", false],

  // Assume the about:newtab page's intro panels have been shown to not
  // depend on which test runs first and happens to open about:newtab
  ["browser.newtabpage.introShown", true],

  // Never start the browser in offline mode
  //
  // This should also be set in the profile prior to starting Firefox,
  // as it is picked up at runtime.
  ["browser.offline", false],

  // Background thumbnails in particular cause grief, and disabling
  // thumbnails in general cannot hurt
  ["browser.pagethumbnails.capturing_disabled", true],

  // Avoid performing Reader Mode intros during tests
  ["browser.reader.detectedFirstArticle", true],

  // Disable safebrowsing components.
  //
  // These should also be set in the profile prior to starting Firefox,
  // as it is picked up at runtime.
  ["browser.safebrowsing.blockedURIs.enabled", false],
  ["browser.safebrowsing.downloads.enabled", false],
  ["browser.safebrowsing.enabled", false],
  ["browser.safebrowsing.forbiddenURIs.enabled", false],
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

  // Start with a blank page (about:blank)
  ["browser.startup.page", 0],

  // Disable tab animation
  ["browser.tabs.animate", false],

  // Do not allow background tabs to be zombified, otherwise for tests
  // that open additional tabs, the test harness tab itself might get
  // unloaded
  ["browser.tabs.disableBackgroundZombification", false],

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

  // Do not show datareporting policy notifications which can
  // interfere with tests
  ["datareporting.healthreport.about.reportUrl", "http://%(server)s/dummy/abouthealthreport/"],
  ["datareporting.healthreport.documentServerURI", "http://%(server)s/dummy/healthreport/"],
  ["datareporting.healthreport.logging.consoleEnabled", false],
  ["datareporting.healthreport.service.enabled", false],
  ["datareporting.healthreport.service.firstRun", false],
  ["datareporting.healthreport.uploadEnabled", false],
  ["datareporting.policy.dataSubmissionEnabled", false],
  ["datareporting.policy.dataSubmissionPolicyAccepted", false],
  ["datareporting.policy.dataSubmissionPolicyBypassNotification", true],

  // Disable popup-blocker
  ["dom.disable_open_during_load", false],

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

  // Do not block add-ons for e10s
  ["extensions.e10sBlocksEnabling", false],

  // Disable metadata caching for installed add-ons by default
  ["extensions.getAddons.cache.enabled", false],

  // Disable intalling any distribution extensions or add-ons.
  // Should be set in profile.
  ["extensions.installDistroAddons", false],
  ["extensions.showMismatchUI", false],

  // Turn off extension updates so they do not bother tests
  ["extensions.update.enabled", false],
  ["extensions.update.notifyUser", false],

  // Make sure opening about:addons will not hit the network
  ["extensions.webservice.discoverURL", "http://%(server)s/dummy/discoveryURL"],

  // Allow the application to have focus even it runs in the background
  ["focusmanager.testmode", true],

  // Disable useragent updates
  ["general.useragent.updates.enabled", false],

  // Always use network provider for geolocation tests so we bypass the
  // macOS dialog raised by the corelocation provider
  ["geo.provider.testing", true],

  // Do not scan Wifi
  ["geo.wifi.scan", false],

  // No hang monitor
  ["hangmonitor.timeout", 0],

  // Show chrome errors and warnings in the error console
  ["javascript.options.showInConsole", true],

  // Make sure the disk cache doesn't get auto disabled
  ["network.http.bypass-cachelock-threshold", 200000],

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

/**
 * Bootstraps Marionette and handles incoming client connections.
 *
 * Once started, it opens a TCP socket sporting the debugger transport
 * protocol on the provided port.  For every new client a Dispatcher is
 * created.
 *
 * @param {number} port
 *     Port for server to listen to.
 * @param {boolean} forceLocal
 *     Listen only to connections from loopback if true.  If false,
 *     accept all connections.
 */
this.MarionetteServer = function (port, forceLocal) {
  this.port = port;
  this.forceLocal = forceLocal;
  this.conns = {};
  this.nextConnId = 0;
  this.alive = false;
  this._acceptConnections = false;
  this.alteredPrefs = new Set();
};

/**
 * Function produces a GeckoDriver.
 *
 * Determines application nameto initialise the driver with.
 *
 * @return {GeckoDriver}
 *     A driver instance.
 */
MarionetteServer.prototype.driverFactory = function() {
  Preferences.set(CONTENT_LISTENER_PREF, false);
  return new GeckoDriver(Services.appinfo.name, this);
};

MarionetteServer.prototype.__defineSetter__("acceptConnections", function (value) {
  if (!value) {
    logger.info("New connections will no longer be accepted");
  } else {
    logger.info("New connections are accepted again");
  }

  this._acceptConnections = value;
});

MarionetteServer.prototype.start = function() {
  if (this.alive) {
    return;
  }

  // set recommended preferences if they are not already user-defined
  for (let [k, v] of RECOMMENDED_PREFS) {
    if (!Preferences.isSet(k)) {
      logger.debug(`Setting recommended pref ${k} to ${v}`);
      Preferences.set(k, v);
      this.alteredPrefs.add(k);
    }
  }

  let flags = Ci.nsIServerSocket.KeepWhenOffline;
  if (this.forceLocal) {
    flags |= Ci.nsIServerSocket.LoopbackOnly;
  }
  this.listener = new ServerSocket(this.port, flags, 1);
  this.listener.asyncListen(this);

  this.alive = true;
  this._acceptConnections = true;
};

MarionetteServer.prototype.stop = function() {
  if (!this.alive) {
    return;
  }

  for (let k of this.alteredPrefs) {
    logger.debug(`Resetting recommended pref ${k}`);
    Preferences.reset(k);
  }
  this.closeListener();

  this.alteredPrefs.clear();
  this.alive = false;
  this._acceptConnections = false;
};

MarionetteServer.prototype.closeListener = function() {
  this.listener.close();
  this.listener = null;
};

MarionetteServer.prototype.onSocketAccepted = function (
    serverSocket, clientSocket) {
  if (!this._acceptConnections) {
    logger.warn("New connections are currently not accepted");
    return;
  }

  let input = clientSocket.openInputStream(0, 0, 0);
  let output = clientSocket.openOutputStream(0, 0, 0);
  let transport = new DebuggerTransport(input, output);
  let connId = "conn" + this.nextConnId++;

  let dispatcher = new Dispatcher(connId, transport, this.driverFactory.bind(this));
  dispatcher.onclose = this.onConnectionClosed.bind(this);
  this.conns[connId] = dispatcher;

  logger.debug(`Accepted connection ${connId} from ${clientSocket.host}:${clientSocket.port}`);
  dispatcher.sayHello();
  transport.ready();
};

MarionetteServer.prototype.onConnectionClosed = function (conn) {
  let id = conn.connId;
  delete this.conns[id];
  logger.debug(`Closed connection ${id}`);
};
