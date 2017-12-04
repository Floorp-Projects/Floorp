/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Constructor: CC, interfaces: Ci, utils: Cu} = Components;

const ServerSocket = CC(
    "@mozilla.org/network/server-socket;1",
    "nsIServerSocket",
    "initSpecialConnection");

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("chrome://marionette/content/assert.js");
const {GeckoDriver} = Cu.import("chrome://marionette/content/driver.js", {});
const {WebElement} = Cu.import("chrome://marionette/content/element.js", {});
const {
  error,
  UnknownCommandError,
} = Cu.import("chrome://marionette/content/error.js", {});
const {
  Command,
  Message,
  Response,
} = Cu.import("chrome://marionette/content/message.js", {});
const {DebuggerTransport} = Cu.import("chrome://marionette/content/transport.js", {});

XPCOMUtils.defineLazyServiceGetter(
    this, "env", "@mozilla.org/process/environment;1", "nsIEnvironment");

const logger = Log.repository.getLogger("Marionette");

const {KeepWhenOffline, LoopbackOnly} = Ci.nsIServerSocket;

this.EXPORTED_SYMBOLS = ["server"];

/** @namespace */
this.server = {};

const PROTOCOL_VERSION = 3;

const ENV_ENABLED = "MOZ_MARIONETTE";

const PREF_CONTENT_LISTENER = "marionette.contentListener";
const PREF_PORT = "marionette.port";
const PREF_RECOMMENDED = "marionette.prefs.recommended";

const NOTIFY_RUNNING = "remote-active";

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

  // Start with a blank page (about:blank)
  ["browser.startup.page", 0],

  // Do not redirect user when a milstone upgrade of Firefox is detected
  ["browser.startup.homepage_override.mstone", "ignore"],

  // Disable browser animations
  ["toolkit.cosmeticAnimations.enabled", false],

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

  // Turn off search suggestions in the location bar so as not to trigger
  // network connections.
  ["browser.urlbar.suggest.searches", false],

  // Turn off the location bar search suggestions opt-in.  It interferes with
  // tests that don't expect it to be there.
  ["browser.urlbar.userMadeSearchSuggestionsChoice", true],

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

  // Make sure Shield doesn't hit the network.
  ["extensions.shield-recipe-client.api_url", ""],

  ["extensions.showMismatchUI", false],

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

  // No hang monitor
  ["hangmonitor.timeout", 0],

  // Show chrome errors and warnings in the error console
  ["javascript.options.showInConsole", true],

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
 * Starting the Marionette server will open a TCP socket sporting the
 * debugger transport interface on the provided |port|.  For every new
 * connection, a |server.TCPConnection| is created.
 */
server.TCPListener = class {
  /**
   * @param {number} port
   *     Port for server to listen to.
   */
  constructor(port) {
    this.port = port;
    this.socket = null;
    this.conns = new Set();
    this.nextConnID = 0;
    this.alive = false;
    this.alteredPrefs = new Set();
  }

  /**
   * Function produces a GeckoDriver.
   *
   * Determines the application to initialise the driver with.
   *
   * @return {GeckoDriver}
   *     A driver instance.
   */
  driverFactory() {
    Preferences.set(PREF_CONTENT_LISTENER, false);
    return new GeckoDriver(Services.appinfo.ID, this);
  }

  set acceptConnections(value) {
    if (value) {
      if (!this.socket) {
        const flags = KeepWhenOffline | LoopbackOnly;
        const backlog = 1;
        this.socket = new ServerSocket(this.port, flags, backlog);
        this.port = this.socket.port;

        this.socket.asyncListen(this);
        logger.debug("New connections are accepted");
      }

    } else if (this.socket) {
      this.socket.close();
      this.socket = null;
      logger.debug("New connections will no longer be accepted");
    }
  }

  /**
   * Bind this listener to |port| and start accepting incoming socket
   * connections on |onSocketAccepted|.
   *
   * The marionette.port preference will be populated with the value
   * of |this.port|.
   */
  start() {
    if (this.alive) {
      return;
    }

    Services.obs.notifyObservers(this, NOTIFY_RUNNING, true);

    if (Preferences.get(PREF_RECOMMENDED)) {
      // set recommended prefs if they are not already user-defined
      for (let [k, v] of RECOMMENDED_PREFS) {
        if (!Preferences.isSet(k)) {
          logger.debug(`Setting recommended pref ${k} to ${v}`);
          Preferences.set(k, v);
          this.alteredPrefs.add(k);
        }
      }
    }

    // Start socket server and listening for connection attempts
    this.acceptConnections = true;

    Preferences.set(PREF_PORT, this.port);
    env.set(ENV_ENABLED, "1");

    this.alive = true;
  }

  stop() {
    if (!this.alive) {
      return;
    }

    for (let k of this.alteredPrefs) {
      logger.debug(`Resetting recommended pref ${k}`);
      Preferences.reset(k);
    }
    this.alteredPrefs.clear();

    // Shutdown server socket, and no longer listen for new connections
    this.acceptConnections = false;

    Services.obs.notifyObservers(this, NOTIFY_RUNNING);
    this.alive = false;
  }

  onSocketAccepted(serverSocket, clientSocket) {
    let input = clientSocket.openInputStream(0, 0, 0);
    let output = clientSocket.openOutputStream(0, 0, 0);
    let transport = new DebuggerTransport(input, output);

    let conn = new server.TCPConnection(
        this.nextConnID++, transport, this.driverFactory.bind(this));
    conn.onclose = this.onConnectionClosed.bind(this);
    this.conns.add(conn);

    logger.debug(`Accepted connection ${conn.id} ` +
        `from ${clientSocket.host}:${clientSocket.port}`);
    conn.sayHello();
    transport.ready();
  }

  onConnectionClosed(conn) {
    logger.debug(`Closed connection ${conn.id}`);
    this.conns.delete(conn);
  }
};

/**
 * Marionette client connection.
 *
 * Dispatches packets received to their correct service destinations
 * and sends back the service endpoint's return values.
 *
 * @param {number} connID
 *     Unique identifier of the connection this dispatcher should handle.
 * @param {DebuggerTransport} transport
 *     Debugger transport connection to the client.
 * @param {function(): GeckoDriver} driverFactory
 *     Factory function that produces a |GeckoDriver|.
 */
server.TCPConnection = class {
  constructor(connID, transport, driverFactory) {
    this.id = connID;
    this.conn = transport;

    // transport hooks are TCPConnection#onPacket
    // and TCPConnection#onClosed
    this.conn.hooks = this;

    // callback for when connection is closed
    this.onclose = null;

    // last received/sent message ID
    this.lastID = 0;

    this.driver = driverFactory();

    // lookup of commands sent by server to client by message ID
    this.commands_ = new Map();
  }

  /**
   * Debugger transport callback that cleans up
   * after a connection is closed.
   */
  onClosed() {
    this.driver.deleteSession();
    if (this.onclose) {
      this.onclose(this);
    }
  }

  /**
   * Callback that receives data packets from the client.
   *
   * If the message is a Response, we look up the command previously
   * issued to the client and run its callback, if any.  In case of
   * a Command, the corresponding is executed.
   *
   * @param {Array.<number, number, ?, ?>} data
   *     A four element array where the elements, in sequence, signifies
   *     message type, message ID, method name or error, and parameters
   *     or result.
   */
  onPacket(data) {
    // unable to determine how to respond
    if (!Array.isArray(data)) {
      let e = new TypeError(
          "Unable to unmarshal packet data: " + JSON.stringify(data));
      error.report(e);
      return;
    }

    // return immediately with any error trying to unmarshal message
    let msg;
    try {
      msg = Message.fromPacket(data);
      msg.origin = Message.Origin.Client;
      this.log_(msg);
    } catch (e) {
      let resp = this.createResponse(data[1]);
      resp.sendError(e);
      return;
    }

    // look up previous command we received a response for
    if (msg instanceof Response) {
      let cmd = this.commands_.get(msg.id);
      this.commands_.delete(msg.id);
      cmd.onresponse(msg);

    // execute new command
    } else if (msg instanceof Command) {
      (async () => {
        await this.execute(msg);
      })();
    }
  }

  /**
   * Executes a Marionette command and sends back a response when it
   * has finished executing.
   *
   * If the command implementation sends the response itself by calling
   * <code>resp.send()</code>, the response is guaranteed to not be
   * sent twice.
   *
   * Errors thrown in commands are marshaled and sent back, and if they
   * are not {@link WebDriverError} instances, they are additionally
   * propagated and reported to {@link Components.utils.reportError}.
   *
   * @param {Command} cmd
   *     Command to execute.
   */
  async execute(cmd) {
    let resp = this.createResponse(cmd.id);
    let sendResponse = () => resp.sendConditionally(resp => !resp.sent);
    let sendError = resp.sendError.bind(resp);

    await this.despatch(cmd, resp)
        .then(sendResponse, sendError).catch(error.report);
  }

  /**
   * Despatches command to appropriate Marionette service.
   *
   * @param {Command} cmd
   *     Command to run.
   * @param {Response} resp
   *     Mutable response where the command's return value will be
   *     assigned.
   *
   * @throws {Error}
   *     A command's implementation may throw at any time.
   */
  async despatch(cmd, resp) {
    let fn = this.driver.commands[cmd.name];
    if (typeof fn == "undefined") {
      throw new UnknownCommandError(cmd.name);
    }

    if (!["newSession", "WebDriver:NewSession"].includes(cmd.name)) {
      assert.session(this.driver);
    }

    let rv = await fn.bind(this.driver)(cmd, resp);

    if (typeof rv != "undefined") {
      if (rv instanceof WebElement || typeof rv != "object") {
        resp.body = {value: rv};
      } else {
        resp.body = rv;
      }
    }
  }

  /**
   * Fail-safe creation of a new instance of |message.Response|.
   *
   * @param {number} msgID
   *     Message ID to respond to.  If it is not a number, -1 is used.
   *
   * @return {message.Response}
   *     Response to the message with |msgID|.
   */
  createResponse(msgID) {
    if (typeof msgID != "number") {
      msgID = -1;
    }
    return new Response(msgID, this.send.bind(this));
  }

  sendError(err, cmdID) {
    let resp = new Response(cmdID, this.send.bind(this));
    resp.sendError(err);
  }

  /**
   * When a client connects we send across a JSON Object defining the
   * protocol level.
   *
   * This is the only message sent by Marionette that does not follow
   * the regular message format.
   */
  sayHello() {
    let whatHo = {
      applicationType: "gecko",
      marionetteProtocol: PROTOCOL_VERSION,
    };
    this.sendRaw(whatHo);
  }

  /**
   * Delegates message to client based on the provided  {@code cmdID}.
   * The message is sent over the debugger transport socket.
   *
   * The command ID is a unique identifier assigned to the client's request
   * that is used to distinguish the asynchronous responses.
   *
   * Whilst responses to commands are synchronous and must be sent in the
   * correct order.
   *
   * @param {Message} msg
   *     The command or response to send.
   */
  send(msg) {
    msg.origin = Message.Origin.Server;
    if (msg instanceof Command) {
      this.commands_.set(msg.id, msg);
      this.sendToEmulator(msg);
    } else if (msg instanceof Response) {
      this.sendToClient(msg);
    }
  }

  // Low-level methods:

  /**
   * Send given response to the client over the debugger transport socket.
   *
   * @param {Response} resp
   *     The response to send back to the client.
   */
  sendToClient(resp) {
    this.driver.responseCompleted();
    this.sendMessage(resp);
  }

  /**
   * Marshal message to the Marionette message format and send it.
   *
   * @param {Message} msg
   *     The message to send.
   */
  sendMessage(msg) {
    this.log_(msg);
    let payload = msg.toPacket();
    this.sendRaw(payload);
  }

  /**
   * Send the given payload over the debugger transport socket to the
   * connected client.
   *
   * @param {Object.<string, ?>} payload
   *     The payload to ship.
   */
  sendRaw(payload) {
    this.conn.send(payload);
  }

  log_(msg) {
    let dir = (msg.origin == Message.Origin.Client ? "->" : "<-");
    logger.trace(`${this.id} ${dir} ${msg}`);
  }

  toString() {
    return `[object server.TCPConnection ${this.id}]`;
  }
};
