/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { WebSocketConnection } from "chrome://remote/content/shared/WebSocketConnection.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  processCapabilities:
    "chrome://remote/content/shared/webdriver/Capabilities.sys.mjs",
  quit: "chrome://remote/content/shared/Browser.sys.mjs",
  RemoteAgent: "chrome://remote/content/components/RemoteAgent.sys.mjs",
  WEBDRIVER_CLASSIC_CAPABILITIES:
    "chrome://remote/content/shared/webdriver/Capabilities.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.WEBDRIVER_BIDI)
);

export class WebDriverBiDiConnection extends WebSocketConnection {
  /**
   * @param {WebSocket} webSocket
   *     The WebSocket server connection to wrap.
   * @param {Connection} httpdConnection
   *     Reference to the httpd.js's connection needed for clean-up.
   */
  constructor(webSocket, httpdConnection) {
    super(webSocket, httpdConnection);

    // Each connection has only a single associated WebDriver session.
    this.session = null;
  }

  /**
   * Perform required steps to end the session.
   */
  endSession() {
    // TODO Bug 1838269. Implement session ending logic
    // for the case of classic + bidi session.
    // We currently only support one session, see Bug 1720707.
    lazy.RemoteAgent.webDriverBiDi.deleteSession();
  }

  /**
   * Register a new WebDriver Session to forward the messages to.
   *
   * @param {Session} session
   *     The WebDriverSession to register.
   */
  registerSession(session) {
    if (this.session) {
      throw new lazy.error.UnknownError(
        "A WebDriver session has already been set"
      );
    }

    this.session = session;
    lazy.logger.debug(
      `Connection ${this.id} attached to session ${session.id}`
    );
  }

  /**
   * Unregister the already set WebDriver session.
   */
  unregisterSession() {
    if (!this.session) {
      return;
    }

    this.session.removeConnection(this);
    this.session = null;
  }

  /**
   * Send an error back to the WebDriver BiDi client.
   *
   * @param {number} id
   *     Id of the packet which lead to an error.
   * @param {Error} err
   *     Error object with `status`, `message` and `stack` attributes.
   */
  sendError(id, err) {
    const webDriverError = lazy.error.wrap(err);

    this.send({
      type: "error",
      id,
      error: webDriverError.status,
      message: webDriverError.message,
      stacktrace: webDriverError.stack,
    });
  }

  /**
   * Send an event coming from a module to the WebDriver BiDi client.
   *
   * @param {string} method
   *     The event name. This is composed by a module name, a dot character
   *     followed by the event name, e.g. `log.entryAdded`.
   * @param {object} params
   *     A JSON-serializable object, which is the payload of this event.
   */
  sendEvent(method, params) {
    this.send({ type: "event", method, params });

    if (Services.profiler?.IsActive()) {
      ChromeUtils.addProfilerMarker(
        "BiDi: Event",
        { category: "Remote-Protocol" },
        method
      );
    }
  }

  /**
   * Send the result of a call to a module's method back to the
   * WebDriver BiDi client.
   *
   * @param {number} id
   *     The request id being sent by the client to call the module's method.
   * @param {object} result
   *     A JSON-serializable object, which is the actual result.
   */
  sendResult(id, result) {
    result = typeof result !== "undefined" ? result : {};
    this.send({ type: "success", id, result });
  }

  observe(subject, topic) {
    switch (topic) {
      case "quit-application-requested":
        this.endSession();
        break;
    }
  }

  // Transport hooks

  /**
   * Called by the `transport` when the connection is closed.
   */
  onConnectionClose() {
    this.unregisterSession();

    super.onConnectionClose();
  }

  /**
   * Receive a packet from the WebSocket layer.
   *
   * This packet is sent by a WebDriver BiDi client and is meant to execute
   * a particular method on a given module.
   *
   * @param {object} packet
   *     JSON-serializable object sent by the client
   */
  async onPacket(packet) {
    super.onPacket(packet);

    const { id, method, params } = packet;
    const startTime = Cu.now();

    try {
      // First check for mandatory field in the command packet
      lazy.assert.positiveInteger(id, "id: unsigned integer value expected");
      lazy.assert.string(method, "method: string value expected");
      lazy.assert.object(params, "params: object value expected");

      // Extract the module and the command name out of `method` attribute
      const { module, command } = splitMethod(method);
      let result;

      // Handle static commands first
      if (module === "session" && command === "new") {
        const processedCapabilities = lazy.processCapabilities(params);

        result = await lazy.RemoteAgent.webDriverBiDi.createSession(
          processedCapabilities,
          this
        );

        // Since in Capabilities class we setup default values also for capabilities which are
        // not relevant for bidi, we want to remove them from the payload before returning to a client.
        result.capabilities = Array.from(result.capabilities.entries()).reduce(
          (object, [key, value]) => {
            if (!lazy.WEBDRIVER_CLASSIC_CAPABILITIES.includes(key)) {
              object[key] = value;
            }

            return object;
          },
          {}
        );
      } else if (module === "session" && command === "status") {
        result = lazy.RemoteAgent.webDriverBiDi.getSessionReadinessStatus();
      } else {
        lazy.assert.session(this.session);

        // Bug 1741854 - Workaround to deny internal methods to be called
        if (command.startsWith("_")) {
          throw new lazy.error.UnknownCommandError(method);
        }

        // Finally, instruct the session to execute the command
        result = await this.session.execute(module, command, params);
      }

      this.sendResult(id, result);

      // Session clean up.
      if (module === "session" && command === "end") {
        this.endSession();
      }
      // Close the browser.
      // TODO Bug 1842018. Refactor this part to return the response
      // when the quitting of the browser is finished.
      else if (module === "browser" && command === "close") {
        // Register handler to run WebDriver BiDi specific shutdown code.
        Services.obs.addObserver(this, "quit-application-requested");

        // TODO Bug 1836282. Add as the third argument "moz:windowless" capability
        // from the session, when this capability is supported by Webdriver BiDi.
        await lazy.quit(["eForceQuit"], false);

        Services.obs.removeObserver(this, "quit-application-requested");
      }
    } catch (e) {
      this.sendError(id, e);
    }

    if (Services.profiler?.IsActive()) {
      ChromeUtils.addProfilerMarker(
        "BiDi: Command",
        { startTime, category: "Remote-Protocol" },
        `${method} (${id})`
      );
    }
  }
}

/**
 * Splits a WebDriver BiDi method into module and command components.
 *
 * @param {string} method
 *     Name of the method to split, e.g. "session.subscribe".
 *
 * @returns {Object<string, string>}
 *     Object with the module ("session") and command ("subscribe")
 *     as properties.
 */
export function splitMethod(method) {
  const parts = method.split(".");

  if (parts.length != 2 || !parts[0].length || !parts[1].length) {
    throw new TypeError(`Invalid method format: '${method}'`);
  }

  return {
    module: parts[0],
    command: parts[1],
  };
}
