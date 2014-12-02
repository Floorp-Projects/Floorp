/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let { Ci, Cc, CC, Cr } = require("chrome");
let Services = require("Services");
let DevToolsUtils = require("devtools/toolkit/DevToolsUtils");
let { dumpn } = DevToolsUtils;
let { DebuggerTransport } = require("devtools/toolkit/transport/transport");

DevToolsUtils.defineLazyGetter(this, "ServerSocket", () => {
  return CC("@mozilla.org/network/server-socket;1",
            "nsIServerSocket",
            "initSpecialConnection");
});

DevToolsUtils.defineLazyGetter(this, "UnixDomainServerSocket", () => {
  return CC("@mozilla.org/network/server-socket;1",
            "nsIServerSocket",
            "initWithFilename");
});

DevToolsUtils.defineLazyGetter(this, "nsFile", () => {
  return CC("@mozilla.org/file/local;1", "nsIFile", "initWithPath");
});

DevToolsUtils.defineLazyGetter(this, "socketTransportService", () => {
  return Cc["@mozilla.org/network/socket-transport-service;1"]
         .getService(Ci.nsISocketTransportService);
});

/**
 * Connects to a debugger server socket and returns a DebuggerTransport.
 *
 * @param aHost string
 *        The host name or IP address of the debugger server.
 * @param aPort number
 *        The port number of the debugger server.
 */
function socketConnect(aHost, aPort)
{
  let s = socketTransportService.createTransport(null, 0, aHost, aPort, null);
  // By default the CONNECT socket timeout is very long, 65535 seconds,
  // so that if we race to be in CONNECT state while the server socket is still
  // initializing, the connection is stuck in connecting state for 18.20 hours!
  s.setTimeout(Ci.nsISocketTransport.TIMEOUT_CONNECT, 2);

  // openOutputStream may throw NS_ERROR_NOT_INITIALIZED if we hit some race
  // where the nsISocketTransport gets shutdown in between its instantiation and
  // the call to this method.
  let transport;
  try {
    transport = new DebuggerTransport(s.openInputStream(0, 0, 0),
                                      s.openOutputStream(0, 0, 0));
  } catch(e) {
    DevToolsUtils.reportException("socketConnect", e);
    throw e;
  }
  return transport;
}

/**
 * Creates a new socket listener for remote connections to a given
 * DebuggerServer.  This helps contain and organize the parts of the server that
 * may differ or are particular to one given listener mechanism vs. another.
 */
function SocketListener(server) {
  this._server = server;
}

SocketListener.prototype = {

  /**
   * Listens on the given port or socket file for remote debugger connections.
   *
   * @param portOrPath int, string
   *        If given an integer, the port to listen on.
   *        Otherwise, the path to the unix socket domain file to listen on.
   */
  open: function(portOrPath) {
    let flags = Ci.nsIServerSocket.KeepWhenOffline;
    // A preference setting can force binding on the loopback interface.
    if (Services.prefs.getBoolPref("devtools.debugger.force-local")) {
      flags |= Ci.nsIServerSocket.LoopbackOnly;
    }

    try {
      let backlog = 4;
      let port = Number(portOrPath);
      if (port) {
        this._socket = new ServerSocket(port, flags, backlog);
      } else {
        let file = nsFile(portOrPath);
        if (file.exists())
          file.remove(false);
        this._socket = new UnixDomainServerSocket(file, parseInt("666", 8),
                                                  backlog);
      }
      this._socket.asyncListen(this);
    } catch (e) {
      dumpn("Could not start debugging listener on '" + portOrPath + "': " + e);
      throw Cr.NS_ERROR_NOT_AVAILABLE;
    }
  },

  /**
   * Closes the SocketListener.  Notifies the server to remove the listener from
   * the set of active SocketListeners.
   */
  close: function() {
    this._socket.close();
    this._server._removeListener(this);
    this._server = null;
  },

  /**
   * Gets the port that a TCP socket listener is listening on, or null if this
   * is not a TCP socket (so there is no port).
   */
  get port() {
    if (!this._socket) {
      return null;
    }
    return this._socket.port;
  },

  // nsIServerSocketListener implementation

  onSocketAccepted:
  DevToolsUtils.makeInfallible(function(aSocket, aTransport) {
    if (Services.prefs.getBoolPref("devtools.debugger.prompt-connection") &&
        !this._server._allowConnection()) {
      return;
    }
    dumpn("New debugging connection on " +
          aTransport.host + ":" + aTransport.port);

    let input = aTransport.openInputStream(0, 0, 0);
    let output = aTransport.openOutputStream(0, 0, 0);
    let transport = new DebuggerTransport(input, output);
    this._server._onConnection(transport);
  }, "SocketListener.onSocketAccepted"),

  onStopListening: function(aSocket, status) {
    dumpn("onStopListening, status: " + status);
  }

};

// TODO: These high-level entry points will branch based on TLS vs. bare TCP as
// part of bug 1059001.
exports.DebuggerSocket = {
  createListener(server) {
    return new SocketListener(server);
  },
  connect(host, port) {
    return socketConnect(host, port);
  }
};
