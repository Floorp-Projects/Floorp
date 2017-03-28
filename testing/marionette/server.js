/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Constructor: CC, classes: Cc, interfaces: Ci, utils: Cu} = Components;

const loader = Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Ci.mozIJSSubScriptLoader);
const ServerSocket = CC("@mozilla.org/network/server-socket;1", "nsIServerSocket", "initSpecialConnection");

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

Cu.import("chrome://marionette/content/assert.js");
Cu.import("chrome://marionette/content/driver.js");
Cu.import("chrome://marionette/content/error.js");
Cu.import("chrome://marionette/content/message.js");

// Bug 1083711: Load transport.js as an SDK module instead of subscript
loader.loadSubScript("resource://devtools/shared/transport/transport.js");

const logger = Log.repository.getLogger("Marionette");

this.EXPORTED_SYMBOLS = ["server"];
this.server = {};

const PROTOCOL_VERSION = 3;

const PREF_CONTENT_LISTENER = "marionette.contentListener";
const PREF_RECOMMENDED = "marionette.prefs.recommended";

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
 * Starting the Marionette server will open a TCP socket sporting the
 * debugger transport interface on the provided |port|.  For every new
 * connection, a |server.TCPConnection| is created.
 */
server.TCPListener = class {
  /**
   * @param {number} port
   *     Port for server to listen to.
   * @param {boolean=} forceLocal
   *     Listen only to connections from loopback if true (default).
   *     When false, accept all connections.
   */
  constructor (port, forceLocal = true) {
    this.port = port;
    this.forceLocal = forceLocal;
    this.conns = new Set();
    this.nextConnID = 0;
    this.alive = false;
    this._acceptConnections = false;
    this.alteredPrefs = new Set();
  }

  /**
   * Function produces a GeckoDriver.
   *
   * Determines application nameto initialise the driver with.
   *
   * @return {GeckoDriver}
   *     A driver instance.
   */
  driverFactory () {
    Preferences.set(PREF_CONTENT_LISTENER, false);
    return new GeckoDriver(Services.appinfo.name, this);
  }

  set acceptConnections (value) {
    if (!value) {
      logger.info("New connections will no longer be accepted");
    } else {
      logger.info("New connections are accepted again");
    }

    this._acceptConnections = value;
  }

  start () {
    if (this.alive) {
      return;
    }

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

    let flags = Ci.nsIServerSocket.KeepWhenOffline;
    if (this.forceLocal) {
      flags |= Ci.nsIServerSocket.LoopbackOnly;
    }
    this.listener = new ServerSocket(this.port, flags, 1);
    this.listener.asyncListen(this);

    this.alive = true;
    this._acceptConnections = true;
  }

  stop () {
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
  }

  closeListener () {
    this.listener.close();
    this.listener = null;
  }

  onSocketAccepted (serverSocket, clientSocket) {
    if (!this._acceptConnections) {
      logger.warn("New connections are currently not accepted");
      return;
    }

    let input = clientSocket.openInputStream(0, 0, 0);
    let output = clientSocket.openOutputStream(0, 0, 0);
    let transport = new DebuggerTransport(input, output);

    let conn = new server.TCPConnection(
        this.nextConnID++, transport, this.driverFactory.bind(this));
    conn.onclose = this.onConnectionClosed.bind(this);
    this.conns.add(conn);

    logger.debug(`Accepted connection ${conn.id} from ${clientSocket.host}:${clientSocket.port}`);
    conn.sayHello();
    transport.ready();
  }

  onConnectionClosed (conn) {
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
  constructor (connID, transport, driverFactory) {
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
  onClosed (reason) {
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
  onPacket (data) {
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
      msg = Message.fromMsg(data);
      msg.origin = MessageOrigin.Client;
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
      this.lastID = msg.id;
      this.execute(msg);
    }
  }

  /**
   * Executes a WebDriver command and sends back a response when it has
   * finished executing.
   *
   * Commands implemented in GeckoDriver and registered in its
   * {@code GeckoDriver.commands} attribute.  The return values from
   * commands are expected to be Promises.  If the resolved value of said
   * promise is not an object, the response body will be wrapped in
   * an object under a "value" field.
   *
   * If the command implementation sends the response itself by calling
   * {@code resp.send()}, the response is guaranteed to not be sent twice.
   *
   * Errors thrown in commands are marshaled and sent back, and if they
   * are not WebDriverError instances, they are additionally propagated
   * and reported to {@code Components.utils.reportError}.
   *
   * @param {Command} cmd
   *     The requested command to execute.
   */
  execute (cmd) {
    let resp = this.createResponse(cmd.id);
    let sendResponse = () => resp.sendConditionally(resp => !resp.sent);
    let sendError = resp.sendError.bind(resp);

    let req = Task.spawn(function* () {
      let fn = this.driver.commands[cmd.name];
      if (typeof fn == "undefined") {
        throw new UnknownCommandError(cmd.name);
      }

      if (cmd.name !== "newSession") {
        assert.session(this.driver);
      }

      let rv = yield fn.bind(this.driver)(cmd, resp);

      if (typeof rv != "undefined") {
        if (typeof rv != "object") {
          resp.body = {value: rv};
        } else {
          resp.body = rv;
        }
      }
    }.bind(this));

    req.then(sendResponse, sendError).catch(error.report);
  }

  /**
   * Fail-safe creation of a new instance of |message.Response|.
   *
   * @param {?} msgID
   *     Message ID to respond to.  If it is not a number, -1 is used.
   *
   * @return {message.Response}
   *     Response to the message with |msgID|.
   */
  createResponse (msgID) {
    if (typeof msgID != "number") {
      msgID = -1;
    }
    return new Response(msgID, this.send.bind(this));
  }

  sendError (err, cmdID) {
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
  sayHello () {
    let whatHo = {
      applicationType: "gecko",
      marionetteProtocol: PROTOCOL_VERSION,
    };
    this.sendRaw(whatHo);
  };

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
   * @param {Command,Response} msg
   *     The command or response to send.
   */
  send (msg) {
    msg.origin = MessageOrigin.Server;
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
  sendToClient (resp) {
    this.driver.responseCompleted();
    this.sendMessage(resp);
  };

  /**
   * Marshal message to the Marionette message format and send it.
   *
   * @param {Command,Response} msg
   *     The message to send.
   */
  sendMessage (msg) {
    this.log_(msg);
    let payload = msg.toMsg();
    this.sendRaw(payload);
  }

  /**
   * Send the given payload over the debugger transport socket to the
   * connected client.
   *
   * @param {Object} payload
   *     The payload to ship.
   */
  sendRaw (payload) {
    this.conn.send(payload);
  }

  log_ (msg) {
    let a = (msg.origin == MessageOrigin.Client ? " -> " : " <- ");
    let s = JSON.stringify(msg.toMsg());
    logger.trace(this.id + a + s);
  }

  toString () {
    return `[object server.TCPConnection ${this.id}]`;
  }
};
