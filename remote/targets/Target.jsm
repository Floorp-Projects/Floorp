/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Target"];

const {Connection} = ChromeUtils.import("chrome://remote/content/Connection.jsm");
const {WebSocketDebuggerTransport} = ChromeUtils.import("chrome://remote/content/server/WebSocketTransport.jsm");
const {WebSocketServer} = ChromeUtils.import("chrome://remote/content/server/WebSocket.jsm");

/**
 * Base class for all the Targets.
 */
class Target {
  /**
   * @param Targets targets
   * @param Class sessionClass
   */
  constructor(targets, sessionClass) {
    this.targets = targets;
    this.sessionClass = sessionClass;
    this.sessions = new Map();
  }

  /**
   * Close all pending connections to this target.
   */
  disconnect() {
    for (const [conn] of this.sessions) {
      conn.close();
    }
  }

  // nsIHttpRequestHandler

  async handle(request, response) {
    const so = await WebSocketServer.upgrade(request, response);
    const transport = new WebSocketDebuggerTransport(so);
    const conn = new Connection(transport, response._connection);
    this.sessions.set(conn, new this.sessionClass(conn, this));
  }

  // XPCOM

  get QueryInterface() {
    return ChromeUtils.generateQI([
      Ci.nsIHttpRequestHandler,
    ]);
  }
}
