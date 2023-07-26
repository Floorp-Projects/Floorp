/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  WebDriverNewSessionHandler:
    "chrome://remote/content/webdriver-bidi/NewSessionHandler.sys.mjs",
  WebDriverSession: "chrome://remote/content/shared/webdriver/Session.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.WEBDRIVER_BIDI)
);
ChromeUtils.defineLazyGetter(lazy, "textEncoder", () => new TextEncoder());

/**
 * Entry class for the WebDriver BiDi support.
 *
 * @see https://w3c.github.io/webdriver-bidi
 */
export class WebDriverBiDi {
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
   * @param {Object<string, *>=} capabilities
   *     JSON Object containing any of the recognised capabilities as listed
   *     on the `WebDriverSession` class.
   *
   * @param {WebDriverBiDiConnection=} sessionlessConnection
   *     Optional connection that is not yet accociated with a WebDriver
   *     session, and has to be associated with the new WebDriver session.
   *
   * @returns {Object<string, Capabilities>}
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
   * @returns {object}
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

    // Write WebSocket connection details to the WebDriverBiDiServer.json file
    // located within the application's profile.
    this._bidiServerPath = PathUtils.join(
      PathUtils.profileDir,
      "WebDriverBiDiServer.json"
    );

    const data = {
      ws_host: this.agent.host,
      ws_port: this.agent.port,
    };

    try {
      await IOUtils.write(
        this._bidiServerPath,
        lazy.textEncoder.encode(JSON.stringify(data, undefined, "  "))
      );
    } catch (e) {
      lazy.logger.warn(
        `Failed to create ${this._bidiServerPath} (${e.message})`
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
      await IOUtils.remove(this._bidiServerPath);
    } catch (e) {
      lazy.logger.warn(
        `Failed to remove ${this._bidiServerPath} (${e.message})`
      );
    }

    try {
      // Close open session
      this.deleteSession();
      this.agent.server.registerPathHandler("/session", null);

      // Close all open session-less connections
      this._sessionlessConnections.forEach(connection => connection.close());
      this._sessionlessConnections.clear();
    } catch (e) {
      lazy.logger.error("Failed to stop protocol", e);
    } finally {
      this._running = false;
    }
  }
}
