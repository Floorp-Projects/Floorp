/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["WebDriverSession"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  accessibility: "chrome://remote/content/marionette/accessibility.js",
  allowAllCerts: "chrome://remote/content/marionette/cert.js",
  Capabilities: "chrome://remote/content/shared/webdriver/Capabilities.jsm",
  error: "chrome://remote/content/shared/webdriver/Errors.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
  MessageHandlerRegistry:
    "chrome://remote/content/shared/messagehandler/MessageHandlerRegistry.jsm",
  RootMessageHandler:
    "chrome://remote/content/shared/messagehandler/RootMessageHandler.jsm",
  WebDriverBiDiConnection:
    "chrome://remote/content/webdriver-bidi/WebDriverBiDiConnection.jsm",
  WebSocketHandshake: "chrome://remote/content/server/WebSocketHandshake.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "uuidGen",
  "@mozilla.org/uuid-generator;1",
  "nsIUUIDGenerator"
);

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());

/**
 * Representation of WebDriver session.
 */
class WebDriverSession {
  /**
   * Construct a new WebDriver session.
   *
   * It is expected that the caller performs the necessary checks on
   * the requested capabilities to be WebDriver conforming.  The WebDriver
   * service offered by Marionette does not match or negotiate capabilities
   * beyond type- and bounds checks.
   *
   * <h3>Capabilities</h3>
   *
   * <dl>
   *  <dt><code>acceptInsecureCerts</code> (boolean)
   *  <dd>Indicates whether untrusted and self-signed TLS certificates
   *   are implicitly trusted on navigation for the duration of the session.
   *
   *  <dt><code>pageLoadStrategy</code> (string)
   *  <dd>The page load strategy to use for the current session.  Must be
   *   one of "<tt>none</tt>", "<tt>eager</tt>", and "<tt>normal</tt>".
   *
   *  <dt><code>proxy</code> (Proxy object)
   *  <dd>Defines the proxy configuration.
   *
   *  <dt><code>setWindowRect</code> (boolean)
   *  <dd>Indicates whether the remote end supports all of the resizing
   *   and repositioning commands.
   *
   *  <dt><code>timeouts</code> (Timeouts object)
   *  <dd>Describes the timeouts imposed on certian session operations.
   *
   *  <dt><code>strictFileInteractability</code> (boolean)
   *  <dd>Defines the current session’s strict file interactability.
   *
   *  <dt><code>unhandledPromptBehavior</code> (string)
   *  <dd>Describes the current session’s user prompt handler.  Must be one of
   *   "<tt>accept</tt>", "<tt>accept and notify</tt>", "<tt>dismiss</tt>",
   *   "<tt>dismiss and notify</tt>", and "<tt>ignore</tt>".  Defaults to the
   *   "<tt>dismiss and notify</tt>" state.
   *
   *  <dt><code>moz:accessibilityChecks</code> (boolean)
   *  <dd>Run a11y checks when clicking elements.
   *
   *  <dt><code>moz:debuggerAddress</code> (boolean)
   *  <dd>Indicate that the Chrome DevTools Protocol (CDP) has to be enabled.
   *
   *  <dt><code>moz:useNonSpecCompliantPointerOrigin</code> (boolean)
   *  <dd>Use the not WebDriver conforming calculation of the pointer origin
   *   when the origin is an element, and the element center point is used.
   *
   *  <dt><code>moz:webdriverClick</code> (boolean)
   *  <dd>Use a WebDriver conforming <i>WebDriver::ElementClick</i>.
   * </dl>
   *
   * <h4>Timeouts object</h4>
   *
   * <dl>
   *  <dt><code>script</code> (number)
   *  <dd>Determines when to interrupt a script that is being evaluates.
   *
   *  <dt><code>pageLoad</code> (number)
   *  <dd>Provides the timeout limit used to interrupt navigation of the
   *   browsing context.
   *
   *  <dt><code>implicit</code> (number)
   *  <dd>Gives the timeout of when to abort when locating an element.
   * </dl>
   *
   * <h4>Proxy object</h4>
   *
   * <dl>
   *  <dt><code>proxyType</code> (string)
   *  <dd>Indicates the type of proxy configuration.  Must be one
   *   of "<tt>pac</tt>", "<tt>direct</tt>", "<tt>autodetect</tt>",
   *   "<tt>system</tt>", or "<tt>manual</tt>".
   *
   *  <dt><code>proxyAutoconfigUrl</code> (string)
   *  <dd>Defines the URL for a proxy auto-config file if
   *   <code>proxyType</code> is equal to "<tt>pac</tt>".
   *
   *  <dt><code>httpProxy</code> (string)
   *  <dd>Defines the proxy host for HTTP traffic when the
   *   <code>proxyType</code> is "<tt>manual</tt>".
   *
   *  <dt><code>noProxy</code> (string)
   *  <dd>Lists the adress for which the proxy should be bypassed when
   *   the <code>proxyType</code> is "<tt>manual</tt>".  Must be a JSON
   *   List containing any number of any of domains, IPv4 addresses, or IPv6
   *   addresses.
   *
   *  <dt><code>sslProxy</code> (string)
   *  <dd>Defines the proxy host for encrypted TLS traffic when the
   *   <code>proxyType</code> is "<tt>manual</tt>".
   *
   *  <dt><code>socksProxy</code> (string)
   *  <dd>Defines the proxy host for a SOCKS proxy traffic when the
   *   <code>proxyType</code> is "<tt>manual</tt>".
   *
   *  <dt><code>socksVersion</code> (string)
   *  <dd>Defines the SOCKS proxy version when the <code>proxyType</code> is
   *   "<tt>manual</tt>".  It must be any integer between 0 and 255
   *   inclusive.
   * </dl>
   *
   * <h3>Example</h3>
   *
   * Input:
   *
   * <pre><code>
   *     {"capabilities": {"acceptInsecureCerts": true}}
   * </code></pre>
   *
   * @param {Object.<string, *>=} capabilities
   *     JSON Object containing any of the recognised capabilities listed
   *     above.
   *
   * @param {WebDriverBiDiConnection=} connection
   *     An optional existing WebDriver BiDi connection to associate with the
   *     new session.
   *
   * @throws {SessionNotCreatedError}
   *     If, for whatever reason, a session could not be created.
   */
  constructor(capabilities, connection) {
    // WebSocket connections that use this session. This also accounts for
    // possible disconnects due to network outages, which require clients
    // to reconnect.
    this._connections = new Set();

    this.id = uuidGen
      .generateUUID()
      .toString()
      .slice(1, -1);

    // Define the HTTP path to query this session via WebDriver BiDi
    this.path = `/session/${this.id}`;

    try {
      this.capabilities = Capabilities.fromJSON(capabilities, this.path);
    } catch (e) {
      throw new error.SessionNotCreatedError(e);
    }

    if (this.capabilities.get("acceptInsecureCerts")) {
      logger.warn("TLS certificate errors will be ignored for this session");
      allowAllCerts.enable();
    }

    if (this.proxy.init()) {
      logger.info(`Proxy settings initialised: ${JSON.stringify(this.proxy)}`);
    }

    // If we are testing accessibility with marionette, start a11y service in
    // chrome first. This will ensure that we do not have any content-only
    // services hanging around.
    if (this.a11yChecks && accessibility.service) {
      logger.info("Preemptively starting accessibility service in Chrome");
    }

    // If a connection without an associated session has been specified
    // immediately register the newly created session for it.
    if (connection) {
      connection.registerSession(this);
      this._connections.add(connection);
    }
  }

  destroy() {
    allowAllCerts.disable();

    // Close all open connections
    this._connections.forEach(connection => connection.close());
    this._connections.clear();

    // Destroy the dedicated MessageHandler instance if we created one.
    if (this._messageHandler) {
      this._messageHandler.off(
        "message-handler-event",
        this._onMessageHandlerEvent
      );
      this._messageHandler.destroy();
    }
  }

  execute(module, command, params) {
    return this.messageHandler.handleCommand({
      moduleName: module,
      commandName: command,
      params,

      // XXX: At the moment, commands do not describe consistently their destination,
      // so we will need a translation step based on a specific command and its params
      // in order to extract a destination that can be understood by the MessageHandler.
      //
      // For now, an option is to send all commands to ROOT, and all BiDi MessageHandler
      // modules will therefore need to implement this translation step in the root
      // implementation of their module.
      destination: {
        type: RootMessageHandler.type,
      },
    });
  }

  get a11yChecks() {
    return this.capabilities.get("moz:accessibilityChecks");
  }

  get messageHandler() {
    if (!this._messageHandler) {
      this._messageHandler = MessageHandlerRegistry.getOrCreateMessageHandler(
        this.id,
        RootMessageHandler.type
      );
      this._onMessageHandlerEvent = this._onMessageHandlerEvent.bind(this);
      this._messageHandler.on(
        "message-handler-event",
        this._onMessageHandlerEvent
      );
    }

    return this._messageHandler;
  }

  get pageLoadStrategy() {
    return this.capabilities.get("pageLoadStrategy");
  }

  get proxy() {
    return this.capabilities.get("proxy");
  }

  get strictFileInteractability() {
    return this.capabilities.get("strictFileInteractability");
  }

  get timeouts() {
    return this.capabilities.get("timeouts");
  }

  set timeouts(timeouts) {
    this.capabilities.set("timeouts", timeouts);
  }

  get unhandledPromptBehavior() {
    return this.capabilities.get("unhandledPromptBehavior");
  }

  toString() {
    return `[object ${this.constructor.name} ${this.id}]`;
  }

  // nsIHttpRequestHandler

  /**
   * Handle new WebSocket connection requests.
   *
   * WebSocket clients will attempt to connect to this session at
   * `/session/:id`.  Hereby a WebSocket upgrade will automatically
   * be performed.
   *
   * @param {Request} request
   *     HTTP request (httpd.js)
   * @param {Response} response
   *     Response to an HTTP request (httpd.js)
   */
  async handle(request, response) {
    const webSocket = await WebSocketHandshake.upgrade(request, response);
    const conn = new WebDriverBiDiConnection(webSocket, response._connection);
    conn.registerSession(this);
    this._connections.add(conn);
  }

  _onMessageHandlerEvent(eventName, messageHandlerEvent) {
    const { method, params } = messageHandlerEvent;
    this._connections.forEach(connection =>
      connection.sendEvent(method, params)
    );
  }

  // XPCOM

  get QueryInterface() {
    return ChromeUtils.generateQI(["nsIHttpRequestHandler"]);
  }
}
