/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["WebDriverBiDiConnection"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  error: "chrome://remote/content/shared/webdriver/Errors.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
  truncate: "chrome://remote/content/shared/Format.jsm",
  WebSocketConnection: "chrome://remote/content/shared/WebSocketConnection.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () =>
  Log.get(Log.TYPES.WEBDRIVER_BIDI)
);

class WebDriverBiDiConnection extends WebSocketConnection {
  /**
   * @param {WebSocket} webSocket
   *     The WebSocket server connection to wrap.
   * @param {HttpServer} httpdConnection
   *     Reference to the httpd.js's connection needed for clean-up.
   */
  constructor(webSocket, httpdConnection) {
    super(webSocket, httpdConnection);

    // Each connection has only a single WebDriver session.
    this.session = null;
  }

  /**
   * Register a new Session to forward the messages to.
   *
   * @param {Session} session
   *     The WebDriverSession to register.
   */
  registerSession(session) {
    if (this.session) {
      throw new error.UnknownError("A session has already been set");
    }

    this.session = session;
  }

  /**
   * Send the JSON-serializable object to the WebDriver BiDi client.
   *
   * @param {Object} data
   *     The object to be sent.
   */
  send(data) {
    const payload = JSON.stringify(data, null, Log.verbose ? "\t" : null);
    logger.trace(truncate`${this.constructor.name} ${this.id} <- ${payload}`);

    super.send(data);
  }

  /**
   * Send an error back to the WebDriver BiDi client.
   *
   * @param {Number} id
   *     Id of the packet which lead to an error.
   * @param {Error} err
   *     Error object with `message` and `stack` attributes.
   */
  sendError(id, error) {}

  /**
   * Send an event coming from a module to the WebDriver BiDi client.
   *
   * @param {String} method
   *     The event name. This is composed by a module name, a dot character
   *     followed by the event name, e.g. `log.entryAdded`.
   * @param {Object} params
   *     A JSON-serializable object, which is the payload of this event.
   */
  sendEvent(method, params) {}

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
    this.send({ id, result });
  }

  // Transport hooks

  /**
   * Called by the `transport` when the connection is closed.
   */
  onClosed() {
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
    const payload = JSON.stringify(packet, null, Log.verbose ? "\t" : null);
    logger.trace(truncate`${this.constructor.name} ${this.id} -> ${payload}`);

    const { id, method /* params */ } = packet;

    try {
      // First check for mandatory field in the packets
      if (typeof id == "undefined") {
        throw new TypeError("Message missing 'id' field");
      }
      if (typeof method == "undefined") {
        throw new TypeError("Message missing 'method' field");
      }

      let result;

      this.sendResult(id, result);
    } catch (e) {
      this.sendError(packet.id, e);
    }
  }
}
