/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["WebDriverBiDi"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  error: "chrome://remote/content/shared/webdriver/Errors.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
  WebDriverNewSessionHandler:
    "chrome://remote/content/webdriver-bidi/NewSessionHandler.jsm",
  WebDriverSession: "chrome://remote/content/shared/webdriver/Session.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.WEBDRIVER_BIDI)
);
XPCOMUtils.defineLazyGetter(lazy, "textEncoder", () => new TextEncoder());

/**
 * Entry class for the WebDriver BiDi support.
 *
 * @see https://w3c.github.io/webdriver-bidi
 */
class WebDriverBiDi {
  /**
   * Creates a new instance of the WebDriverBiDi class.
   *
   * @param {RemoteAgent} agent
   *     Reference to the Remote Agent instance.
   */
  constructor(agent) {
    this.agent = agent;
    this._running = false;

    this._session = null;
    this._sessionlessConnections = new Set();
  }

  get address() {
    return `ws://${this.agent.host}:${this.agent.port}`;
  }

  get session() {
    return this._session;
  }

  /**
   * Add a new connection that is not yet attached to a WebDriver session.
   *
   * @param {WebDriverBiDiConnection} connection
   *     The connection without an accociated WebDriver session.
   */
  addSessionlessConnection(connection) {
    this._sessionlessConnections.add(connection);
  }

  /**
   * Create a new WebDriver session.
   *
   * @param {Object.<string, *>=} capabilities
   *     JSON Object containing any of the recognised capabilities as listed
   *     on the `WebDriverSession` class.
   *
   * @param {WebDriverBiDiConnection=} sessionlessConnection
   *     Optional connection that is not yet accociated with a WebDriver
   *     session, and has to be associated with the new WebDriver session.
   *
   * @return {Object<String, Capabilities>}
   *     Object containing the current session ID, and all its capabilities.
   *
   * @throws {SessionNotCreatedError}
   *     If, for whatever reason, a session could not be created.
   */
  async createSession(capabilities, sessionlessConnection) {
    if (this.session) {
      throw new lazy.error.SessionNotCreatedError(
        "Maximum number of active sessions"
      );
    }

    const session = new lazy.WebDriverSession(
      capabilities,
      sessionlessConnection
    );

    // When the Remote Agent is listening, and a BiDi WebSocket connection
    // has been requested, register a path handler for the session.
    let webSocketUrl = null;
    if (
      this.agent.running &&
      (session.capabilities.get("webSocketUrl") || sessionlessConnection)
    ) {
      // Creating a WebDriver BiDi session too early can cause issues with
      // clients in not being able to find any available browsing context.
      // Also when closing the application while it's still starting up can
      // cause shutdown hangs. As such WebDriver BiDi will return a new session
      // once the initial application window has finished initializing.
      lazy.logger.debug(`Waiting for initial application window`);
      await this.agent.browserStartupFinished;

      this.agent.server.registerPathHandler(session.path, session);
      webSocketUrl = `${this.address}${session.path}`;

      lazy.logger.debug(`Registered session handler: ${session.path}`);

      if (sessionlessConnection) {
        // Remove temporary session-less connection
        this._sessionlessConnections.delete(sessionlessConnection);
      }
    }

    // Also update the webSocketUrl capability to contain the session URL if
    // a path handler has been registered. Otherwise set its value to null.
    session.capabilities.set("webSocketUrl", webSocketUrl);

    this._session = session;

    return {
      sessionId: this.session.id,
      capabilities: this.session.capabilities,
    };
  }

  /**
   * Delete the current WebDriver session.
   */
  deleteSession() {
    if (!this.session) {
      return;
    }

    // When the Remote Agent is listening, and a BiDi WebSocket is active,
    // unregister the path handler for the session.
    if (this.agent.running && this.session.capabilities.get("webSocketUrl")) {
      this.agent.server.registerPathHandler(this.session.path, null);
      lazy.logger.debug(`Unregistered session handler: ${this.session.path}`);
    }

    this.session.destroy();
    this._session = null;
  }

  /**
   * Retrieve the readiness state of the remote end, regarding the creation of
   * new WebDriverBiDi sessions.
   *
   * See https://w3c.github.io/webdriver-bidi/#command-session-status
   *
   * @return {Object}
   *     The readiness state.
   */
  getSessionReadinessStatus() {
    if (this.session) {
      // We currently only support one session, see Bug 1720707.
      return {
        ready: false,
        message: "Session already started",
      };
    }

    return {
      ready: true,
      message: "",
    };
  }

  /**
   * Starts the WebDriver BiDi support.
   */
  async start() {
    if (this._running) {
      return;
    }

    this._running = true;

    // Install a HTTP handler for direct WebDriver BiDi connection requests.
    this.agent.server.registerPathHandler(
      "/session",
      new lazy.WebDriverNewSessionHandler(this)
    );

    Cu.printStderr(`WebDriver BiDi listening on ${this.address}\n`);

    // Write WebSocket port to WebDriverBiDiActivePort file within the profile.
    this._activePortPath = PathUtils.join(
      PathUtils.profileDir,
      "WebDriverBiDiActivePort"
    );

    const data = `${this.agent.port}`;
    try {
      await IOUtils.write(this._activePortPath, lazy.textEncoder.encode(data));
    } catch (e) {
      lazy.logger.warn(
        `Failed to create ${this._activePortPath} (${e.message})`
      );
    }
  }

  /**
   * Stops the WebDriver BiDi support.
   */
  async stop() {
    if (!this._running) {
      return;
    }

    try {
      try {
        await IOUtils.remove(this._activePortPath);
      } catch (e) {
        lazy.logger.warn(
          `Failed to remove ${this._activePortPath} (${e.message})`
        );
      }

      this.deleteSession();

      this.agent.server.registerPathHandler("/session", null);

      // Close all open session-less connections
      this._sessionlessConnections.forEach(connection => connection.close());
      this._sessionlessConnections.clear();
    } finally {
      this._running = false;
    }
  }
}
