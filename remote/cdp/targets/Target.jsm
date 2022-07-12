/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Target"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  CDPConnection: "chrome://remote/content/cdp/CDPConnection.jsm",
  WebSocketHandshake: "chrome://remote/content/server/WebSocketHandshake.jsm",
});

/**
 * Base class for all the targets.
 */
class Target {
  /**
   * @param TargetList targetList
   * @param Class sessionClass
   */
  constructor(targetList, sessionClass) {
    // Save a reference to TargetList instance in order to expose it to:
    // domains/parent/Target.jsm
    this.targetList = targetList;

    // When a new connection is made to this target,
    // we will instantiate a new session based on this given class.
    // The session class is specific to each target's kind and is passed
    // by the inheriting class.
    this.sessionClass = sessionClass;

    // There can be more than one connection if multiple clients connect to the remote agent.
    this.connections = new Set();
    this.id = Services.uuid
      .generateUUID()
      .toString()
      .slice(1, -1);
  }

  /**
   * Close all active connections made to this target.
   */
  destructor() {
    for (const conn of this.connections) {
      conn.close();
    }
  }

  // nsIHttpRequestHandler

  async handle(request, response) {
    const webSocket = await lazy.WebSocketHandshake.upgrade(request, response);
    const conn = new lazy.CDPConnection(webSocket, response._connection);
    const session = new this.sessionClass(conn, this);
    conn.registerSession(session);
    this.connections.add(conn);
  }

  // XPCOM

  get QueryInterface() {
    return ChromeUtils.generateQI(["nsIHttpRequestHandler"]);
  }
}
