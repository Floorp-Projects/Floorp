/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Target"];

const { Connection } = ChromeUtils.import(
  "chrome://remote/content/Connection.jsm"
);
const { WebSocketTransport } = ChromeUtils.import(
  "chrome://remote/content/server/WebSocketTransport.jsm"
);
const { WebSocketHandshake } = ChromeUtils.import(
  "chrome://remote/content/server/WebSocketHandshake.jsm"
);

const UUIDGen = Cc["@mozilla.org/uuid-generator;1"].getService(
  Ci.nsIUUIDGenerator
);

/**
 * Base class for all the Targets.
 */
class Target {
  /**
   * @param Targets targets
   * @param Class sessionClass
   */
  constructor(targets, sessionClass) {
    // Save a reference to Targets instance in order to expose it to:
    // domains/parent/Target.jsm
    this.targets = targets;

    // When a new connection is made to this target,
    // we will instantiate a new session based on this given class.
    // The session class is specific to each target's kind and is passed
    // by the inheriting class.
    this.sessionClass = sessionClass;

    // There can be more than one connection if multiple clients connect to the remote agent.
    this.connections = new Set();
    this.id = UUIDGen.generateUUID()
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
    const so = await WebSocketHandshake.upgrade(request, response);
    const transport = new WebSocketTransport(so);
    const conn = new Connection(transport, response._connection);
    const session = new this.sessionClass(conn, this);
    conn.registerSession(session);
    this.connections.add(conn);
  }

  // XPCOM

  get QueryInterface() {
    return ChromeUtils.generateQI([Ci.nsIHttpRequestHandler]);
  }
}
