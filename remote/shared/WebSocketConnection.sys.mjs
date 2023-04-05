/* This Source Code Form is subject to the terms of the Mozilla Public

 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  truncate: "chrome://remote/content/shared/Format.sys.mjs",
  WebSocketTransport:
    "chrome://remote/content/server/WebSocketTransport.sys.mjs",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

export class WebSocketConnection {
  /**
   * @param {WebSocket} webSocket
   *     The WebSocket server connection to wrap.
   * @param {Connection} httpdConnection
   *     Reference to the httpd.js's connection needed for clean-up.
   */
  constructor(webSocket, httpdConnection) {
    this.id = Services.uuid
      .generateUUID()
      .toString()
      .slice(1, -1);

    this.httpdConnection = httpdConnection;

    this.transport = new lazy.WebSocketTransport(webSocket);
    this.transport.hooks = this;
    this.transport.ready();

    lazy.logger.debug(`${this.constructor.name} ${this.id} accepted`);
  }

  _log(direction, data) {
    function replacer(key, value) {
      if (typeof value === "string") {
        return lazy.truncate`${value}`;
      }
      return value;
    }

    const payload = JSON.stringify(
      data,
      replacer,
      lazy.Log.verbose ? "\t" : null
    );

    lazy.logger.trace(
      `${this.constructor.name} ${this.id} ${direction} ${payload}`
    );
  }

  /**
   * Close the WebSocket connection.
   */
  close() {
    this.transport.close();

    // In addition to the WebSocket transport, we also have to close the
    // connection used internally within httpd.js. Otherwise the server doesn't
    // shut down correctly, and keeps these Connection instances alive.
    this.httpdConnection.close();
  }

  /**
   * Register a new Session to forward the messages to.
   *
   * Needs to be implemented in the sub class.
   *
   * @param {Session} session
   *     The session to register.
   */
  registerSession(session) {
    throw new Error("Not implemented");
  }

  /**
   * Send the JSON-serializable object to the client.
   *
   * @param {object} data
   *     The object to be sent.
   */
  send(data) {
    this._log("<-", data);
    this.transport.send(data);
  }

  /**
   * Send an error back to the client.
   *
   * Needs to be implemented in the sub class.
   */
  sendError() {
    throw new Error("Not implemented");
  }

  /**
   * Send an event back to the client.
   *
   * Needs to be implemented in the sub class.
   */
  sendEvent() {
    throw new Error("Not implemented");
  }

  /**
   * Send the result of a call to a method back to the client.
   *
   * Needs to be implemented in the sub class.
   */
  sendResult() {
    throw new Error("Not implemented");
  }

  toString() {
    return `[object ${this.constructor.name} ${this.id}]`;
  }

  // Transport hooks

  /**
   * Called by the `transport` when the connection is closed.
   */
  onClosed(status) {
    lazy.logger.debug(`${this.constructor.name} ${this.id} closed`);
  }

  /**
   * Receive a packet from the WebSocket layer.
   *
   * This packet is sent by a WebSocket client and is meant to execute
   * a particular method.
   *
   * Needs to be implemented in the sub class.
   *
   * @param {object} packet
   *     JSON-serializable object sent by the client.
   */
  async onPacket(packet) {
    this._log("->", packet);
  }
}
