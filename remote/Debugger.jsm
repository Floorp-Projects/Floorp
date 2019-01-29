/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Debugger"];

const {Connection} = ChromeUtils.import("chrome://remote/content/Connection.jsm");
const {Session} = ChromeUtils.import("chrome://remote/content/Session.jsm");
const {SocketListener} = ChromeUtils.import("chrome://remote/content/server/Socket.jsm");

/**
 * Represents a debuggee target (a browsing context, typically a tab)
 * that clients can connect to and debug.
 *
 * Debugger#listen starts a WebSocket listener,
 * and for each accepted connection a new Session is created.
 * There can be multiple sessions per target.
 * The session's lifetime is equal to the lifetime of the debugger connection.
 */
this.Debugger = class {
  constructor(target) {
    this.target = target;
    this.listener = null;
    this.sessions = new Map();
    this.nextConnID = 0;
  }

  get connected() {
    return !!this.listener && this.listener.listening;
  }

  listen() {
    if (this.listener) {
      return;
    }

    this.listener = new SocketListener();
    this.listener.on("accepted", this.onConnectionAccepted.bind(this));

    this.listener.listen("ws", 0 /* atomically allocated port */);
  }

  close() {
    this.listener.off("accepted", this.onConnectionAccepted.bind(this));
    for (const [conn, session] of this.sessions) {
      session.destructor();
      conn.close();
    }
    this.listener.close();
    this.listener = null;
    this.sessions.clear();
  }

  onConnectionAccepted(transport, listener) {
    const conn = new Connection(this.nextConnID++, transport, listener);
    transport.ready();
    this.sessions.set(conn, new Session(conn, this.target));
  }

  get url() {
    if (this.connected) {
      const {network, host, port} = this.listener;
      return `${network}://${host}:${port}/`;
    }
    return null;
  }

  toString() {
    return `[object Debugger ${this.url || "disconnected"}]`;
  }
};
