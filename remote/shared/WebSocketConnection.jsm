/* This Source Code Form is subject to the terms of the Mozilla Public

 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["WebSocketConnection"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  Log: "chrome://remote/content/shared/Log.jsm",
  WebSocketTransport: "chrome://remote/content/server/WebSocketTransport.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

class WebSocketConnection {
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
   * @param Session session
   *     The session to register.
   */
  registerSession(session) {
    throw new Error("Not implemented");
  }

  /**
   * Send the JSON-serializable object to the client.
   *
   * @param {Object} data
   *     The object to be sent.
   */
  send(data) {
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
   * @param {Object} packet
   *     JSON-serializable object sent by the client.
   */
  async onPacket(packet) {
    throw new Error("Not implemented");
  }
}
