/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cu} = require("chrome");
const {setTimeout, clearTimeout} = require('sdk/timers');
const EventEmitter = require("devtools/shared/event-emitter");

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/devtools/dbg-client.jsm");
Cu.import("resource://gre/modules/devtools/dbg-server.jsm");

/**
 * Connection Manager.
 *
 * To use this module:
 * const {ConnectionManager} = require("devtools/client/connection-manager");
 *
 * # ConnectionManager
 *
 * Methods:
 *  ⬩ Connection createConnection(host, port)
 *  ⬩ void       destroyConnection(connection)
 *
 * Properties:
 *  ⬩ Array      connections
 *
 * # Connection
 *
 * A connection is a wrapper around a debugger client. It has a simple
 * API to instantiate a connection to a debugger server. Once disconnected,
 * no need to re-create a Connection object. Calling `connect()` again
 * will re-create a debugger client.
 *
 * Methods:
 *  ⬩ connect()         Connect to host:port. Expect a "connecting" event. If
 *                      host is not specified, a local pipe is used
 *  ⬩ disconnect()      Disconnect if connected. Expect a "disconnecting" event
 *
 * Properties:
 *  ⬩ host              IP address or hostname
 *  ⬩ port              Port
 *  ⬩ logs              Current logs. "newlog" event notifies new available logs
 *  ⬩ store             Reference to a local data store (see below)
 *  ⬩ status            Connection status:
 *                        Connection.Status.CONNECTED
 *                        Connection.Status.DISCONNECTED
 *                        Connection.Status.CONNECTING
 *                        Connection.Status.DISCONNECTING
 *                        Connection.Status.DESTROYED
 *
 * Events (as in event-emitter.js):
 *  ⬩ Connection.Events.CONNECTING      Trying to connect to host:port
 *  ⬩ Connection.Events.CONNECTED       Connection is successful
 *  ⬩ Connection.Events.DISCONNECTING   Trying to disconnect from server
 *  ⬩ Connection.Events.DISCONNECTED    Disconnected (at client request, or because of a timeout or connection error)
 *  ⬩ Connection.Events.STATUS_CHANGED  The connection status (connection.status) has changed
 *  ⬩ Connection.Events.TIMEOUT         Connection timeout
 *  ⬩ Connection.Events.HOST_CHANGED    Host has changed
 *  ⬩ Connection.Events.PORT_CHANGED    Port has changed
 *  ⬩ Connection.Events.NEW_LOG         A new log line is available
 *
 */

let ConnectionManager = {
  _connections: new Set(),
  createConnection: function(host, port) {
    let c = new Connection(host, port);
    c.once("destroy", (event) => this.destroyConnection(c));
    this._connections.add(c);
    this.emit("new", c);
    return c;
  },
  destroyConnection: function(connection) {
    if (this._connections.has(connection)) {
      this._connections.delete(connection);
      if (connection.status != Connection.Status.DESTROYED) {
        connection.destroy();
      }
    }
  },
  get connections() {
    return [c for (c of this._connections)];
  },
}

EventEmitter.decorate(ConnectionManager);

let lastID = -1;

function Connection(host, port) {
  EventEmitter.decorate(this);
  this.uid = ++lastID;
  this.host = host;
  this.port = port;
  this._setStatus(Connection.Status.DISCONNECTED);
  this._onDisconnected = this._onDisconnected.bind(this);
  this._onConnected = this._onConnected.bind(this);
  this._onTimeout = this._onTimeout.bind(this);
}

Connection.Status = {
  CONNECTED: "connected",
  DISCONNECTED: "disconnected",
  CONNECTING: "connecting",
  DISCONNECTING: "disconnecting",
  DESTROYED: "destroyed",
}

Connection.Events = {
  CONNECTED: Connection.Status.CONNECTED,
  DISCONNECTED: Connection.Status.DISCONNECTED,
  CONNECTING: Connection.Status.CONNECTING,
  DISCONNECTING: Connection.Status.DISCONNECTING,
  DESTROYED: Connection.Status.DESTROYED,
  TIMEOUT: "timeout",
  STATUS_CHANGED: "status-changed",
  HOST_CHANGED: "host-changed",
  PORT_CHANGED: "port-changed",
  NEW_LOG: "new_log"
}

Connection.prototype = {
  logs: "",
  log: function(str) {
    this.logs +=  "\n" + str;
    this.emit(Connection.Events.NEW_LOG, str);
  },

  get client() {
    return this._client
  },

  get host() {
    return this._host
  },

  set host(value) {
    if (this._host && this._host == value)
      return;
    this._host = value;
    this.emit(Connection.Events.HOST_CHANGED);
  },

  get port() {
    return this._port
  },

  set port(value) {
    if (this._port && this._port == value)
      return;
    this._port = value;
    this.emit(Connection.Events.PORT_CHANGED);
  },

  disconnect: function(force) {
    if (this.status == Connection.Status.DESTROYED) {
      return;
    }
    clearTimeout(this._timeoutID);
    if (this.status == Connection.Status.CONNECTED ||
        this.status == Connection.Status.CONNECTING) {
      this.log("disconnecting");
      this._setStatus(Connection.Status.DISCONNECTING);
      this._client.close();
    }
  },

  connect: function() {
    if (this.status == Connection.Status.DESTROYED) {
      return;
    }
    if (!this._client) {
      this.log("connecting to " + this.host + ":" + this.port);
      this._setStatus(Connection.Status.CONNECTING);
      let delay = Services.prefs.getIntPref("devtools.debugger.remote-timeout");
      this._timeoutID = setTimeout(this._onTimeout, delay);

      let transport;
      if (!this._host) {
        transport = DebuggerServer.connectPipe();
      } else {
        transport = debuggerSocketConnect(this._host, this._port);
      }
      this._client = new DebuggerClient(transport);
      this._client.addOneTimeListener("closed", this._onDisconnected);
      this._client.connect(this._onConnected);
    } else {
      let msg = "Can't connect. Client is not fully disconnected";
      this.log(msg);
      throw new Error(msg);
    }
  },

  destroy: function() {
    this.log("killing connection");
    clearTimeout(this._timeoutID);
    if (this._client) {
      this._client.close();
      this._client = null;
    }
    this._setStatus(Connection.Status.DESTROYED);
  },

  get status() {
    return this._status
  },

  _setStatus: function(value) {
    if (this._status && this._status == value)
      return;
    this._status = value;
    this.emit(value);
    this.emit(Connection.Events.STATUS_CHANGED, value);
  },

  _onDisconnected: function() {
    clearTimeout(this._timeoutID);
    switch (this.status) {
      case Connection.Status.CONNECTED:
        this.log("disconnected (unexpected)");
        break;
      case Connection.Status.CONNECTING:
        this.log("Connection error");
        break;
      default:
        this.log("disconnected");
    }
    this._client = null;
    this._setStatus(Connection.Status.DISCONNECTED);
  },

  _onConnected: function() {
    this.log("connected");
    clearTimeout(this._timeoutID);
    this._setStatus(Connection.Status.CONNECTED);
  },

  _onTimeout: function() {
    this.log("connection timeout");
    this.emit(Connection.Events.TIMEOUT, str);
    this.disconnect();
  },
}

exports.ConnectionManager = ConnectionManager;
exports.Connection = Connection;

