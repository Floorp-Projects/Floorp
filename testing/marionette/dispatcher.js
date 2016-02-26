/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Task.jsm");

Cu.import("chrome://marionette/content/driver.js");
Cu.import("chrome://marionette/content/emulator.js");
Cu.import("chrome://marionette/content/error.js");
Cu.import("chrome://marionette/content/message.js");

this.EXPORTED_SYMBOLS = ["Dispatcher"];

const PROTOCOL_VERSION = 3;

const logger = Log.repository.getLogger("Marionette");

/**
 * Manages a Marionette connection, and dispatches packets received to
 * their correct destinations.
 *
 * @param {number} connId
 *     Unique identifier of the connection this dispatcher should handle.
 * @param {DebuggerTransport} transport
 *     Debugger transport connection to the client.
 * @param {function(EmulatorService): GeckoDriver} driverFactory
 *     A factory function that takes an EmulatorService and produces
 *     a GeckoDriver.
 */
this.Dispatcher = function(connId, transport, driverFactory) {
  this.connId = connId;
  this.conn = transport;

  // transport hooks are Dispatcher#onPacket
  // and Dispatcher#onClosed
  this.conn.hooks = this;

  // callback for when connection is closed
  this.onclose = null;

  // last received/sent message ID
  this.lastId = 0;

  this.emulator = new emulator.EmulatorService(this.sendEmulator.bind(this));
  this.driver = driverFactory(this.emulator);

  // lookup of commands sent by server to client by message ID
  this.commands_ = new Map();
};

/**
 * Debugger transport callback that cleans up
 * after a connection is closed.
 */
Dispatcher.prototype.onClosed = function(reason) {
  this.driver.sessionTearDown();
  if (this.onclose) {
    this.onclose(this);
  }
};

/**
 * Callback that receives data packets from the client.
 *
 * If the message is a Response, we look up the command previously issued
 * to the client and run its callback, if any.  In case of a Command,
 * the corresponding is executed.
 *
 * @param {Array.<number, number, ?, ?>} data
 *     A four element array where the elements, in sequence, signifies
 *     message type, message ID, method name or error, and parameters
 *     or result.
 */
Dispatcher.prototype.onPacket = function(data) {
  let msg = Message.fromMsg(data);
  msg.origin = MessageOrigin.Client;
  this.log_(msg);

  if (msg instanceof Response) {
    let cmd = this.commands_.get(msg.id);
    this.commands_.delete(msg.id);
    cmd.onresponse(msg);
  } else if (msg instanceof Command) {
    this.lastId = msg.id;
    this.execute(msg);
  }
};

/**
 * Executes a WebDriver command and sends back a response when it has
 * finished executing.
 *
 * Commands implemented in GeckoDriver and registered in its
 * {@code GeckoDriver.commands} attribute.  The return values from
 * commands are expected to be Promises.  If the resolved value of said
 * promise is not an object, the response body will be wrapped in an object
 * under a "value" field.
 *
 * If the command implementation sends the response itself by calling
 * {@code resp.send()}, the response is guaranteed to not be sent twice.
 *
 * Errors thrown in commands are marshaled and sent back, and if they
 * are not WebDriverError instances, they are additionally propagated and
 * reported to {@code Components.utils.reportError}.
 *
 * @param {Command} cmd
 *     The requested command to execute.
 */
Dispatcher.prototype.execute = function(cmd) {
  let resp = new Response(cmd.id, this.send.bind(this));
  let sendResponse = () => resp.sendConditionally(resp => !resp.sent);
  let sendError = resp.sendError.bind(resp);

  let req = Task.spawn(function*() {
    let fn = this.driver.commands[cmd.name];
    if (typeof fn == "undefined") {
      throw new UnknownCommandError(cmd.name);
    }

    let rv = yield fn.bind(this.driver)(cmd, resp);

    if (typeof rv != "undefined") {
      if (typeof rv != "object") {
        resp.body = {value: rv};
      } else {
        resp.body = rv;
      }
    }
  }.bind(this));

  req.then(sendResponse, sendError).catch(error.report);
};

Dispatcher.prototype.sendError = function(err, cmdId) {
  let resp = new Response(cmdId, this.send.bind(this));
  resp.sendError(err);
};

// Convenience methods:

/**
 * When a client connects we send across a JSON Object defining the
 * protocol level.
 *
 * This is the only message sent by Marionette that does not follow
 * the regular message format.
 */
Dispatcher.prototype.sayHello = function() {
  let whatHo = {
    applicationType: "gecko",
    marionetteProtocol: PROTOCOL_VERSION,
  };
  this.sendRaw(whatHo);
};

Dispatcher.prototype.sendEmulator = function(name, params, resCb, errCb) {
  let cmd = new Command(++this.lastId, name, params);
  cmd.onresult = resCb;
  cmd.onerror = errCb;
  this.send(cmd);
};

/**
 * Delegates message to client or emulator based on the provided
 * {@code cmdId}.  The message is sent over the debugger transport socket.
 *
 * The command ID is a unique identifier assigned to the client's request
 * that is used to distinguish the asynchronous responses.
 *
 * Whilst responses to commands are synchronous and must be sent in the
 * correct order, emulator callbacks are more transparent and can be sent
 * at any time.  These callbacks won't change the current command state.
 *
 * @param {Command,Response} msg
 *     The command or response to send.
 */
Dispatcher.prototype.send = function(msg) {
  msg.origin = MessageOrigin.Server;
  if (msg instanceof Command) {
    this.commands_.set(msg.id, msg);
    this.sendToEmulator(msg);
  } else if (msg instanceof Response) {
    this.sendToClient(msg);
  }
};

// Low-level methods:

/**
 * Send command to emulator over the debugger transport socket.
 *
 * @param {Command} cmd
 *     The command to issue to the emulator.
 */
Dispatcher.prototype.sendToEmulator = function(cmd) {
  this.sendMessage(cmd);
};

/**
 * Send given response to the client over the debugger transport socket.
 *
 * @param {Response} resp
 *     The response to send back to the client.
 */
Dispatcher.prototype.sendToClient = function(resp) {
  this.driver.responseCompleted();
  this.sendMessage(resp);
};

/**
 * Marshal message to the Marionette message format and send it.
 *
 * @param {Command,Response} msg
 *     The message to send.
 */
Dispatcher.prototype.sendMessage = function(msg) {
  this.log_(msg);
  let payload = msg.toMsg();
  this.sendRaw(payload);
};

/**
 * Send the given payload over the debugger transport socket to the
 * connected client.
 *
 * @param {Object} payload
 *     The payload to ship.
 */
Dispatcher.prototype.sendRaw = function(payload) {
  this.conn.send(payload);
};

Dispatcher.prototype.log_ = function(msg) {
  let a = (msg.origin == MessageOrigin.Client ? " -> " : " <- ");
  let s = JSON.stringify(msg.toMsg());
  logger.trace(this.connId + a + s);
};
