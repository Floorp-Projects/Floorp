/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["splitMethod", "WebDriverBiDiConnection"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const { WebSocketConnection } = ChromeUtils.import(
  "chrome://remote/content/shared/WebSocketConnection.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.jsm",
  error: "chrome://remote/content/shared/webdriver/Errors.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
  RemoteAgent: "chrome://remote/content/components/RemoteAgent.jsm",
  truncate: "chrome://remote/content/shared/Format.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.WEBDRIVER_BIDI)
);

class WebDriverBiDiConnection extends WebSocketConnection {
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
   *
   * @param {Session} session
   *     The WebDriverSession to register.
   */
  unregisterSession() {
    if (!this.session) {
      return;
    }

    this.session.removeConnection(this);
    this.session = null;
  }

  /**
   * Send the JSON-serializable object to the WebDriver BiDi client.
   *
   * @param {Object} data
   *     The object to be sent.
   */
  send(data) {
    const payload = JSON.stringify(data, null, lazy.Log.verbose ? "\t" : null);
    lazy.logger.debug(
      lazy.truncate`${this.session?.id || "no session"} <- ${payload}`
    );

    super.send(data);
  }

  /**
   * Send an error back to the WebDriver BiDi client.
   *
   * @param {Number} id
   *     Id of the packet which lead to an error.
   * @param {Error} err
   *     Error object with `status`, `message` and `stack` attributes.
   */
  sendError(id, err) {
    const webDriverError = lazy.error.wrap(err);

    this.send({
      id,
      error: webDriverError.status,
      message: webDriverError.message,
      stacktrace: webDriverError.stack,
    });
  }

  /**
   * Send an event coming from a module to the WebDriver BiDi client.
   *
   * @param {String} method
   *     The event name. This is composed by a module name, a dot character
   *     followed by the event name, e.g. `log.entryAdded`.
   * @param {Object} params
   *     A JSON-serializable object, which is the payload of this event.
   */
  sendEvent(method, params) {
    this.send({ method, params });
  }

  /**
   * Send the result of a call to a module's method back to the
   * WebDriver BiDi client.
   *
   * @param {Number} id
   *     The request id being sent by the client to call the module's method.
   * @param {Object} result
   *     A JSON-serializable object, which is the actual result.
   */
  sendResult(id, result) {
    result = typeof result !== "undefined" ? result : {};
    this.send({ id, result });
  }

  // Transport hooks

  /**
   * Called by the `transport` when the connection is closed.
   */
  onClosed() {
    this.unregisterSession();

    super.onClosed();
  }

  /**
   * Receive a packet from the WebSocket layer.
   *
   * This packet is sent by a WebDriver BiDi client and is meant to execute
   * a particular method on a given module.
   *
   * @param Object packet
   *        JSON-serializable object sent by the client
   */
  async onPacket(packet) {
    const payload = JSON.stringify(
      packet,
      null,
      lazy.Log.verbose ? "\t" : null
    );
    lazy.logger.debug(
      lazy.truncate`${this.session?.id || "no session"} -> ${payload}`
    );

    const { id, method, params } = packet;

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
        // TODO: Needs capability matching code
        result = await lazy.RemoteAgent.webDriverBiDi.createSession(
          params,
          this
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
    } catch (e) {
      this.sendError(packet.id, e);
    }
  }
}

/**
 * Splits a WebDriver BiDi method into module and command components.
 *
 * @param {String} method
 *     Name of the method to split, e.g. "session.subscribe".
 *
 * @returns {Object<String, String>}
 *     Object with the module ("session") and command ("subscribe")
 *     as properties.
 */
function splitMethod(method) {
  const parts = method.split(".");

  if (parts.length != 2 || !parts[0].length || !parts[1].length) {
    throw new TypeError(`Invalid method format: '${method}'`);
  }

  return {
    module: parts[0],
    command: parts[1],
  };
}
