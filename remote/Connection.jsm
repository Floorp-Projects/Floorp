/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Connection"];

const {Log} = ChromeUtils.import("chrome://remote/content/Log.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "log", Log.get);
XPCOMUtils.defineLazyServiceGetter(this, "UUIDGen", "@mozilla.org/uuid-generator;1", "nsIUUIDGenerator");

class Connection {
  /**
   * @param WebSocketDebuggerTransport transport
   * @param httpd.js's Connection httpdConnection
   */
  constructor(transport, httpdConnection) {
    this.id = UUIDGen.generateUUID().toString();
    this.transport = transport;
    this.httpdConnection = httpdConnection;

    this.transport.hooks = this;
    this.transport.ready();

    this.defaultSession = null;
    this.sessions = new Map();
  }

  /**
   * Register a new Session to forward the messages to.
   * Session without any `id` attribute will be considered to be the
   * default one, to which messages without `sessionId` attribute are
   * forwarded to. Only one such session can be registered.
   *
   * @param Session session
   */
  registerSession(session) {
    if (!session.id) {
      if (this.defaultSession) {
        throw new Error("Default session is already set on Connection," +
                        "can't register another one.");
      }
      this.defaultSession = session;
    }
    this.sessions.set(session.id, session);
  }

  send(message) {
    log.trace(`<-(connection ${this.id}) ${JSON.stringify(message)}`);
    this.transport.send(message);
  }

  error(id, e) {
    const error = {
      message: e.message,
      data: e.stack,
    };
    this.send({id, error});
  }

  deserialize(data) {
    const id = data.id;
    const method = data.method;
    const params = data.params || {};
    return {id, method, params};
  }

  // transport hooks

  onPacket(packet) {
    log.trace(`(connection ${this.id})-> ${JSON.stringify(packet)}`);

    let message = {id: null};
    try {
      message = this.deserialize(packet);
      const { sessionId } = packet;
      if (!sessionId) {
        if (!this.defaultSession) {
          throw new Error(`Connection is missing a default Session.`);
        }
        this.defaultSession.onMessage(message);
      } else {
        const session = this.sessions.get(sessionId);
        if (!session) {
          throw new Error(`Session '${sessionId}' doesn't exists.`);
        }
        session.onMessage(message);
      }
    } catch (e) {
      log.warn(e);
      this.error(message.id, e);
    }
  }

  close() {
    this.transport.close();
    this.sessions.clear();

    // In addition to the WebSocket transport, we also have to close the Connection
    // used internaly within httpd.js. Otherwise the server doesn't shut down correctly
    // and keep these Connection instances alive.
    this.httpdConnection.close();
  }

  onClosed(status) {}

  toString() {
    return `[object Connection ${this.id}]`;
  }
}
