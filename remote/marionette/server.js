/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["TCPConnection", "TCPListener"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.jsm",
  Command: "chrome://remote/content/marionette/message.js",
  DebuggerTransport: "chrome://remote/content/marionette/transport.js",
  error: "chrome://remote/content/shared/webdriver/Errors.jsm",
  GeckoDriver: "chrome://remote/content/marionette/driver.js",
  Log: "chrome://remote/content/shared/Log.jsm",
  MarionettePrefs: "chrome://remote/content/marionette/prefs.js",
  Message: "chrome://remote/content/marionette/message.js",
  Response: "chrome://remote/content/marionette/message.js",
  WebReference: "chrome://remote/content/marionette/element.js",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.MARIONETTE)
);
XPCOMUtils.defineLazyGetter(lazy, "ServerSocket", () => {
  return Components.Constructor(
    "@mozilla.org/network/server-socket;1",
    "nsIServerSocket",
    "initSpecialConnection"
  );
});

const { KeepWhenOffline, LoopbackOnly } = Ci.nsIServerSocket;

const PROTOCOL_VERSION = 3;

/**
 * Bootstraps Marionette and handles incoming client connections.
 *
 * Starting the Marionette server will open a TCP socket sporting the
 * debugger transport interface on the provided `port`.  For every
 * new connection, a {@link TCPConnection} is created.
 */
class TCPListener {
  /**
   * @param {number} port
   *     Port for server to listen to.
   */
  constructor(port) {
    this.port = port;
    this.socket = null;
    this.conns = new Set();
    this.nextConnID = 0;
    this.alive = false;
  }

  /**
   * Function produces a {@link GeckoDriver}.
   *
   * Determines the application to initialise the driver with.
   *
   * @return {GeckoDriver}
   *     A driver instance.
   */
  driverFactory() {
    return new lazy.GeckoDriver(this);
  }

  set acceptConnections(value) {
    if (value) {
      if (!this.socket) {
        try {
          const flags = KeepWhenOffline | LoopbackOnly;
          const backlog = 1;
          this.socket = new lazy.ServerSocket(this.port, flags, backlog);
        } catch (e) {
          throw new Error(`Could not bind to port ${this.port} (${e.name})`);
        }

        this.port = this.socket.port;

        this.socket.asyncListen(this);
        lazy.logger.info(`Listening on port ${this.port}`);
      }
    } else if (this.socket) {
      // Note that closing the server socket will not close currently active
      // connections.
      this.socket.close();
      this.socket = null;
      lazy.logger.info(`Stopped listening on port ${this.port}`);
    }
  }

  /**
   * Bind this listener to {@link #port} and start accepting incoming
   * socket connections on {@link #onSocketAccepted}.
   *
   * The marionette.port preference will be populated with the value
   * of {@link #port}.
   */
  start() {
    if (this.alive) {
      return;
    }

    // Start socket server and listening for connection attempts
    this.acceptConnections = true;
    lazy.MarionettePrefs.port = this.port;
    this.alive = true;
  }

  stop() {
    if (!this.alive) {
      return;
    }

    // Shutdown server socket, and no longer listen for new connections
    this.acceptConnections = false;
    this.alive = false;
  }

  onSocketAccepted(serverSocket, clientSocket) {
    let input = clientSocket.openInputStream(0, 0, 0);
    let output = clientSocket.openOutputStream(0, 0, 0);
    let transport = new lazy.DebuggerTransport(input, output);

    // Only allow a single active WebDriver session at a time
    const hasActiveSession = [...this.conns].find(
      conn => !!conn.driver.currentSession
    );
    if (hasActiveSession) {
      lazy.logger.warn(
        "Connection attempt denied because an active session has been found"
      );

      // Ideally we should stop the server to listen for new connection
      // attempts, but the current architecture doesn't allow us to do that.
      // As such just close the transport if no further connections are allowed.
      transport.close();
      return;
    }

    let conn = new TCPConnection(
      this.nextConnID++,
      transport,
      this.driverFactory.bind(this)
    );
    conn.onclose = this.onConnectionClosed.bind(this);
    this.conns.add(conn);

    lazy.logger.debug(
      `Accepted connection ${conn.id} ` +
        `from ${clientSocket.host}:${clientSocket.port}`
    );
    conn.sayHello();
    transport.ready();
  }

  onConnectionClosed(conn) {
    lazy.logger.debug(`Closed connection ${conn.id}`);
    this.conns.delete(conn);
  }
}

/**
 * Marionette client connection.
 *
 * Dispatches packets received to their correct service destinations
 * and sends back the service endpoint's return values.
 *
 * @param {number} connID
 *     Unique identifier of the connection this dispatcher should handle.
 * @param {DebuggerTransport} transport
 *     Debugger transport connection to the client.
 * @param {function(): GeckoDriver} driverFactory
 *     Factory function that produces a {@link GeckoDriver}.
 */
class TCPConnection {
  constructor(connID, transport, driverFactory) {
    this.id = connID;
    this.conn = transport;

    // transport hooks are TCPConnection#onPacket
    // and TCPConnection#onClosed
    this.conn.hooks = this;

    // callback for when connection is closed
    this.onclose = null;

    // last received/sent message ID
    this.lastID = 0;

    this.driver = driverFactory();
  }

  /**
   * Debugger transport callback that cleans up
   * after a connection is closed.
   */
  onClosed() {
    this.driver.deleteSession();
    if (this.onclose) {
      this.onclose(this);
    }
  }

  /**
   * Callback that receives data packets from the client.
   *
   * If the message is a Response, we look up the command previously
   * issued to the client and run its callback, if any.  In case of
   * a Command, the corresponding is executed.
   *
   * @param {Array.<number, number, ?, ?>} data
   *     A four element array where the elements, in sequence, signifies
   *     message type, message ID, method name or error, and parameters
   *     or result.
   */
  onPacket(data) {
    // unable to determine how to respond
    if (!Array.isArray(data)) {
      let e = new TypeError(
        "Unable to unmarshal packet data: " + JSON.stringify(data)
      );
      lazy.error.report(e);
      return;
    }

    // return immediately with any error trying to unmarshal message
    let msg;
    try {
      msg = lazy.Message.fromPacket(data);
      msg.origin = lazy.Message.Origin.Client;
      this.log_(msg);
    } catch (e) {
      let resp = this.createResponse(data[1]);
      resp.sendError(e);
      return;
    }

    // execute new command
    if (msg instanceof lazy.Command) {
      (async () => {
        await this.execute(msg);
      })();
    } else {
      lazy.logger.fatal("Cannot process messages other than Command");
    }
  }

  /**
   * Executes a Marionette command and sends back a response when it
   * has finished executing.
   *
   * If the command implementation sends the response itself by calling
   * <code>resp.send()</code>, the response is guaranteed to not be
   * sent twice.
   *
   * Errors thrown in commands are marshaled and sent back, and if they
   * are not {@link WebDriverError} instances, they are additionally
   * propagated and reported to {@link Components.utils.reportError}.
   *
   * @param {Command} cmd
   *     Command to execute.
   */
  async execute(cmd) {
    let resp = this.createResponse(cmd.id);
    let sendResponse = () => resp.sendConditionally(resp => !resp.sent);
    let sendError = resp.sendError.bind(resp);

    await this.despatch(cmd, resp)
      .then(sendResponse, sendError)
      .catch(lazy.error.report);
  }

  /**
   * Despatches command to appropriate Marionette service.
   *
   * @param {Command} cmd
   *     Command to run.
   * @param {Response} resp
   *     Mutable response where the command's return value will be
   *     assigned.
   *
   * @throws {Error}
   *     A command's implementation may throw at any time.
   */
  async despatch(cmd, resp) {
    let fn = this.driver.commands[cmd.name];
    if (typeof fn == "undefined") {
      throw new lazy.error.UnknownCommandError(cmd.name);
    }

    if (cmd.name != "WebDriver:NewSession") {
      lazy.assert.session(this.driver.currentSession);
    }

    let rv = await fn.bind(this.driver)(cmd);

    if (rv != null) {
      if (rv instanceof lazy.WebReference || typeof rv != "object") {
        resp.body = { value: rv };
      } else {
        resp.body = rv;
      }
    }
  }

  /**
   * Fail-safe creation of a new instance of {@link Response}.
   *
   * @param {number} msgID
   *     Message ID to respond to.  If it is not a number, -1 is used.
   *
   * @return {Response}
   *     Response to the message with `msgID`.
   */
  createResponse(msgID) {
    if (typeof msgID != "number") {
      msgID = -1;
    }
    return new lazy.Response(msgID, this.send.bind(this));
  }

  sendError(err, cmdID) {
    let resp = new lazy.Response(cmdID, this.send.bind(this));
    resp.sendError(err);
  }

  /**
   * When a client connects we send across a JSON Object defining the
   * protocol level.
   *
   * This is the only message sent by Marionette that does not follow
   * the regular message format.
   */
  sayHello() {
    let whatHo = {
      applicationType: "gecko",
      marionetteProtocol: PROTOCOL_VERSION,
    };
    this.sendRaw(whatHo);
  }

  /**
   * Delegates message to client based on the provided  {@code cmdID}.
   * The message is sent over the debugger transport socket.
   *
   * The command ID is a unique identifier assigned to the client's request
   * that is used to distinguish the asynchronous responses.
   *
   * Whilst responses to commands are synchronous and must be sent in the
   * correct order.
   *
   * @param {Message} msg
   *     The command or response to send.
   */
  send(msg) {
    msg.origin = lazy.Message.Origin.Server;
    if (msg instanceof lazy.Response) {
      this.sendToClient(msg);
    } else {
      lazy.logger.fatal("Cannot send messages other than Response");
    }
  }

  // Low-level methods:

  /**
   * Send given response to the client over the debugger transport socket.
   *
   * @param {Response} resp
   *     The response to send back to the client.
   */
  sendToClient(resp) {
    this.sendMessage(resp);
  }

  /**
   * Marshal message to the Marionette message format and send it.
   *
   * @param {Message} msg
   *     The message to send.
   */
  sendMessage(msg) {
    this.log_(msg);
    let payload = msg.toPacket();
    this.sendRaw(payload);
  }

  /**
   * Send the given payload over the debugger transport socket to the
   * connected client.
   *
   * @param {Object.<string, ?>} payload
   *     The payload to ship.
   */
  sendRaw(payload) {
    this.conn.send(payload);
  }

  log_(msg) {
    let dir = msg.origin == lazy.Message.Origin.Client ? "->" : "<-";
    lazy.logger.debug(`${this.id} ${dir} ${msg}`);
  }

  toString() {
    return `[object TCPConnection ${this.id}]`;
  }
}
