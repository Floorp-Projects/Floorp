/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "ConnectionHandshake",
  "SocketListener",
];

// This is an XPCOM-service-ified copy of ../devtools/shared/security/socket.js.

const {EventEmitter} = ChromeUtils.import("resource://gre/modules/EventEmitter.jsm");
const {Log} = ChromeUtils.import("chrome://remote/content/Log.jsm");
const {Preferences} = ChromeUtils.import("resource://gre/modules/Preferences.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  DebuggerTransport: "chrome://remote/content/server/Transport.jsm",
  WebSocketDebuggerTransport: "chrome://remote/content/server/WebSocketTransport.jsm",
  WebSocketServer: "chrome://remote/content/server/WebSocket.jsm",
});

XPCOMUtils.defineLazyGetter(this, "log", Log.get);
XPCOMUtils.defineLazyGetter(this, "nsFile", () => CC("@mozilla.org/file/local;1", "nsIFile", "initWithPath"));

const LOOPBACKS = ["localhost", "127.0.0.1"];

const {KeepWhenOffline, LoopbackOnly} = Ci.nsIServerSocket;

this.SocketListener = class SocketListener {
  constructor() {
    this.socket = null;
    this.network = null;

    this.nextConnID = 0;

    this.onConnectionCreated = null;
    this.onConnectionClose = null;

    EventEmitter.decorate(this);
  }

  get listening() {
    return !!this.socket;
  }

  /**
   * @param {String} addr
   *     [<network>][:<host>][:<port>]
   *     networks: ws, unix, tcp
   */
  listen(addr) {
    const [network, host, port] = addr.split(":");
    try {
      this._listen(network, host, port);
    } catch (e) {
      this.close();
      throw new Error(`Unable to start socket server on ${addr}: ${e.message}`, e);
    }
  }

  _listen(network = "tcp", host = "localhost", port = 0) {
    if (typeof network != "string" ||
        typeof host != "string" ||
        (network != "unix" && typeof port != "number")) {
      throw new TypeError();
    }
    if (network != "unix" && (!Number.isInteger(port) || port < 0)) {
      throw new RangeError();
    }
    if (!SocketListener.Networks.includes(network)) {
      throw new Error("Unexpected network: " + network);
    }
    if (!LOOPBACKS.includes(host)) {
      throw new Error("Restricted to listening on loopback devices");
    }

    const flags = KeepWhenOffline | LoopbackOnly;

    const backlog = 4;
    this.socket = createSocket();
    this.network = network;

    switch (this.network) {
      case "tcp":
      case "ws":
        // -1 means kernel-assigned port in Gecko
        if (port == 0) {
          port = -1;
        }
        
        this.socket.initSpecialConnection(port, flags, backlog);
        break;

      case "unix":
        // concrete Unix socket
        if (host.startsWith("/")) {
          const file = nsFile(host);
          if (file.exists()) {
            file.remove(false);
          }
          this.socket.initWithFilename(file, Number.parseInt("666", 8), backlog);
        // abstract Unix socket
        } else {
          this.socket.initWithAbstractAddress(host, backlog);
        }
        break;
    }

    this.socket.asyncListen(this);
  }

  close() {
    if (this.socket) {
      this.socket.close();
      this.socket = null;
    }
    // TODO(ato): removeSocketListener?
  }

  get host() {
    if (this.socket) {
      // TODO(ato): Don't hardcode:
      return "localhost";
    }
    return null;
  }

  get port() {
    if (this.socket) {
      return this.socket.port;
    }
    return null;
  }

  onAllowedConnection(eventName, transport) {
    this.emit("accepted", transport, this);
  }

  // nsIServerSocketListener implementation

  onSocketAccepted(socket, socketTransport) {
    const conn = new ConnectionHandshake(this, socketTransport);
    conn.once("allowed", this.onAllowedConnection.bind(this));
    conn.handle();
  }

  onStopListening(socket, status) {
    dump("onStopListening: " + status + "\n");
  }
};

SocketListener.Networks = ["tcp", "unix", "ws"];

/**
 * Created by SocketListener for each accepted incoming socket.
 * This is a short-lived object used to implement a handshake,
 * before the socket transport is handed back to RemoteAgent.
 */
this.ConnectionHandshake = class ConnectionHandshake {
  constructor(listener, socketTransport) {
    this.listener = listener;
    this.socket = socketTransport;
    this.transport = null;
    this.destroyed = false;

    EventEmitter.decorate(this);
  }

  destructor() {
    this.listener = null;
    this.socket = null;
    this.transport = null;
    this.destroyed = true;
  }

  get address() {
    return `${this.host}:${this.port}`;
  }

  get host() {
    return this.socket.host;
  }

  get port() {
    return this.socket.port;
  }

  async handle() {
    try {
      await this.createTransport();
      this.allow();
    } catch (e) {
      this.deny(e);
    }
  }

  async createTransport() {
    const rx = this.socket.openInputStream(0, 0, 0);
    const tx = this.socket.openOutputStream(0, 0, 0);

    if (this.listener.network == "ws") {
      const so = await WebSocketServer.accept(this.socket, rx, tx);
      this.transport = new WebSocketDebuggerTransport(so);
    } else {
      this.transport = new DebuggerTransport(rx, tx);
    }

    // This handles early disconnects from clients, primarily for failed TLS negotiation.
    // We don't support TLS connections in RDP, but might be useful for future blocklist.
    this.transport.hooks = {
      onClosed: reason => this.deny(reason),
    };
    // TODO(ato): Review if this is correct:
    this.transport.ready();
  }

  allow() {
    if (this.destroyed) {
      return;
    }
    log.debug(`Accepted connection from ${this.address}`);
    this.emit("allowed", this.transport);
    this.destructor();
  }

  deny(result) {
    if (this.destroyed) {
      return;
    }

    let err = legibleError(result);
    log.warn(`Connection from ${this.address} denied: ${err.message}`, err.stack);

    if (this.transport) {
      this.transport.hooks = null;
      this.transport.close(result); 
    }
    this.socket.close(result);
    this.destructor();
  }
};

function createSocket() {
  return Cc["@mozilla.org/network/server-socket;1"]
      .createInstance(Ci.nsIServerSocket);
}

// TODO(ato): Move to separate module
function legibleError(obj) {
  if (obj instanceof Ci.nsIException) {
    for (const result in Cr) {
      if (obj.result == Cr[result]) {
        return {
          message: result,
          stack: obj.location.formattedStack,
        };
      }
    }

    return {
      message: "nsIException",
      stack: obj,
    };
  } else {
    return obj;
  }
}
