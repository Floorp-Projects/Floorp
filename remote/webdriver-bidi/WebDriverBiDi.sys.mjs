/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  RecommendedPreferences:
    "chrome://remote/content/shared/RecommendedPreferences.sys.mjs",
  WebDriverNewSessionHandler:
    "chrome://remote/content/webdriver-bidi/NewSessionHandler.sys.mjs",
  WebDriverSession: "chrome://remote/content/shared/webdriver/Session.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.WEBDRIVER_BIDI)
);
ChromeUtils.defineLazyGetter(lazy, "textEncoder", () => new TextEncoder());

const RECOMMENDED_PREFS = new Map([
  // Enables permission isolation by user context.
  // It should be enabled by default in Nightly in the scope of the bug 1641584.
  ["permissions.isolateBy.userContext", true],
]);

/**
 * Entry class for the WebDriver BiDi support.
 *
 * @see https://w3c.github.io/webdriver-bidi
 */
export class WebDriverBiDi {
  #agent;
  #bidiServerPath;
  #running;
  #session;
  #sessionlessConnections;

  /**
   * Creates a new instance of the WebDriverBiDi class.
   *
   * @param {RemoteAgent} agent
   *     Reference to the Remote Agent instance.
   */
  constructor(agent) {
    this.#agent = agent;
    this.#running = false;

    this.#bidiServerPath;
    this.#session = null;
    this.#sessionlessConnections = new Set();
  }

  get address() {
    return `ws://${this.#agent.host}:${this.#agent.port}`;
  }

  get session() {
    return this.#session;
  }

  #newSessionAlgorithm(session, flags) {
    if (!this.#agent.running) {
      // With the Remote Agent not running WebDriver BiDi is not supported.
      return;
    }

    if (flags.has(lazy.WebDriverSession.SESSION_FLAG_BIDI)) {
      // It's already a WebDriver BiDi session.
      return;
    }

    const webSocketUrl = session.capabilities.get("webSocketUrl");
    if (webSocketUrl === undefined) {
      return;
    }

    // Start listening for BiDi connections.
    this.#agent.server.registerPathHandler(session.path, session);
    lazy.logger.debug(`Registered session handler: ${session.path}`);

    session.capabilities.set("webSocketUrl", `${this.address}${session.path}`);

    session.bidi = true;
    flags.add("bidi");
  }

  /**
   * Add a new connection that is not yet attached to a WebDriver session.
   *
   * @param {WebDriverBiDiConnection} connection
   *     The connection without an associated WebDriver session.
   */
  addSessionlessConnection(connection) {
    this.#sessionlessConnections.add(connection);
  }

  /**
   * Create a new WebDriver session.
   *
   * @param {Object<string, *>=} capabilities
   *     JSON Object containing any of the recognised capabilities as listed
   *     on the `WebDriverSession` class.
   * @param {Set} flags
   *     Session configuration flags.
   * @param {WebDriverBiDiConnection=} sessionlessConnection
   *     Optional connection that is not yet associated with a WebDriver
   *     session, and has to be associated with the new WebDriver session.
   *
   * @returns {Object<string, Capabilities>}
   *     Object containing the current session ID, and all its capabilities.
   *
   * @throws {SessionNotCreatedError}
   *     If, for whatever reason, a session could not be created.
   */
  async createSession(capabilities, flags, sessionlessConnection) {
    if (this.#session) {
      throw new lazy.error.SessionNotCreatedError(
        "Maximum number of active sessions"
      );
    }

    this.#session = new lazy.WebDriverSession(
      capabilities,
      flags,
      sessionlessConnection
    );

    // Run new session steps for WebDriver BiDi.
    this.#newSessionAlgorithm(this.#session, flags);

    if (sessionlessConnection) {
      // Connection is now registered with a WebDriver session
      this.#sessionlessConnections.delete(sessionlessConnection);
    }

    if (this.#session.bidi) {
      // Creating a WebDriver BiDi session too early can cause issues with
      // clients in not being able to find any available browsing context.
      // Also when closing the application while it's still starting up can
      // cause shutdown hangs. As such WebDriver BiDi will return a new session
      // once the initial application window has finished initializing.
      lazy.logger.debug(`Waiting for initial application window`);
      await this.#agent.browserStartupFinished;
    }

    return {
      sessionId: this.#session.id,
      capabilities: this.#session.capabilities,
    };
  }

  /**
   * Delete the current WebDriver session.
   */
  deleteSession() {
    if (!this.#session) {
      return;
    }

    // When the Remote Agent is listening, and a BiDi WebSocket is active,
    // unregister the path handler for the session.
    if (this.#agent.running && this.#session.capabilities.get("webSocketUrl")) {
      this.#agent.server.registerPathHandler(this.#session.path, null);
      lazy.logger.debug(`Unregistered session handler: ${this.#session.path}`);
    }

    this.#session.destroy();
    this.#session = null;
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
    if (this.#session) {
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
    if (this.#running) {
      return;
    }

    this.#running = true;

    lazy.RecommendedPreferences.applyPreferences(RECOMMENDED_PREFS);

    // Install a HTTP handler for direct WebDriver BiDi connection requests.
    this.#agent.server.registerPathHandler(
      "/session",
      new lazy.WebDriverNewSessionHandler(this)
    );

    Cu.printStderr(`WebDriver BiDi listening on ${this.address}\n`);

    try {
      // Write WebSocket connection details to the WebDriverBiDiServer.json file
      // located within the application's profile.
      this.#bidiServerPath = PathUtils.join(
        PathUtils.profileDir,
        "WebDriverBiDiServer.json"
      );

      const data = {
        ws_host: this.#agent.host,
        ws_port: this.#agent.port,
      };

      await IOUtils.write(
        this.#bidiServerPath,
        lazy.textEncoder.encode(JSON.stringify(data, undefined, "  "))
      );
    } catch (e) {
      lazy.logger.warn(
        `Failed to create ${this.#bidiServerPath} (${e.message})`
      );
    }
  }

  /**
   * Stops the WebDriver BiDi support.
   */
  async stop() {
    if (!this.#running) {
      return;
    }

    try {
      await IOUtils.remove(this.#bidiServerPath);
    } catch (e) {
      lazy.logger.warn(
        `Failed to remove ${this.#bidiServerPath} (${e.message})`
      );
    }

    try {
      // Close open session
      this.deleteSession();
      this.#agent.server.registerPathHandler("/session", null);

      // Close all open session-less connections
      this.#sessionlessConnections.forEach(connection => connection.close());
      this.#sessionlessConnections.clear();
    } catch (e) {
      lazy.logger.error("Failed to stop protocol", e);
    } finally {
      this.#running = false;
    }
  }
}
