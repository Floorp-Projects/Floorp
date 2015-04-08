/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

Cu.import("chrome://marionette/content/command.js");
Cu.import("chrome://marionette/content/emulator.js");
Cu.import("chrome://marionette/content/error.js");
Cu.import("chrome://marionette/content/driver.js");

this.EXPORTED_SYMBOLS = ["Dispatcher"];

const logger = Log.repository.getLogger("Marionette");
const uuidGen = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);

/**
 * Manages a Marionette connection, and dispatches packets received to
 * their correct destinations.
 *
 * @param {number} connId
 *     Unique identifier of the connection this dispatcher should handle.
 * @param {DebuggerTransport} transport
 *     Debugger transport connection to the client.
 * @param {function(Emulator): GeckoDriver} driverFactory
 *     A factory function that takes an Emulator as argument and produces
 *     a GeckoDriver.
 * @param {function()} stopSignal
 *     Signal to stop the Marionette server.
 */
this.Dispatcher = function(connId, transport, driverFactory, stopSignal) {
  this.id = connId;
  this.conn = transport;

  // Marionette uses a protocol based on the debugger server, which
  // requires passing back actor ID's with responses.  Unlike the debugger
  // server, we don't actually have multiple actors, so just use a dummy
  // value of "0".
  this.actorId = "0";

  // callback for when connection is closed
  this.onclose = null;

  // transport hooks are Dispatcher.prototype.onPacket
  // and Dispatcher.prototype.onClosed
  this.conn.hooks = this;

  this.emulator = new Emulator(msg => this.sendResponse(msg, -1));
  this.driver = driverFactory(this.emulator);
  this.commandProcessor = new CommandProcessor(this.driver);

  this.stopSignal_ = stopSignal;
};

/**
 * Debugger transport callback that dispatches the request.
 * Request handlers defined in this.requests take presedence
 * over those defined in this.driver.commands.
 */
Dispatcher.prototype.onPacket = function(packet) {
  // Avoid using toSource and template strings (or touching the payload at all
  // if not necessary) for the sake of memory use.
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=1150170
  if (logger.level <= Log.Level.Debug) {
    logger.debug(this.id + " -> (" + JSON.stringify(packet) + ")");
  }

  if (this.requests && this.requests[packet.name]) {
    this.requests[packet.name].bind(this)(packet);
  } else {
    let id = this.beginNewCommand();
    let ok = this.sendOk.bind(this);
    let send = this.send.bind(this);
    this.commandProcessor.execute(packet, ok, send, id);
  }
};

/**
 * Debugger transport callback that cleans up
 * after a connection is closed.
 */
Dispatcher.prototype.onClosed = function(status) {
  this.driver.sessionTearDown();
  if (this.onclose) {
    this.onclose(this);
  }
};

// Dispatcher specific command handlers:

Dispatcher.prototype.getMarionetteID = function() {
  let id = this.beginNewCommand();
  this.sendResponse({from: "root", id: this.actorId}, id);
};

Dispatcher.prototype.emulatorCmdResult = function(msg) {
  switch (this.driver.context) {
    case Context.CONTENT:
      this.driver.sendAsync("emulatorCmdResult", msg);
      break;
    case Context.CHROME:
      let cb = this.emulator.popCallback(msg.id);
      if (!cb) {
        return;
      }
      cb.result(msg);
      break;
  }
};

/**
 * Quits Firefox with the provided flags and tears down the current
 * session.
 */
Dispatcher.prototype.quitApplication = function(msg) {
  let id = this.beginNewCommand();

  if (this.driver.appName != "Firefox") {
    this.sendError({
      "message": "In app initiated quit only supported on Firefox",
      "status": "webdriver error",
    }, id);
    return;
  }

  let flags = Ci.nsIAppStartup.eAttemptQuit;
  for (let k of msg.parameters.flags) {
    flags |= Ci.nsIAppStartup[k];
  }

  this.stopSignal_();
  this.sendOk(id);

  this.driver.sessionTearDown();
  Services.startup.quit(flags);
};

// Convenience methods:

Dispatcher.prototype.sayHello = function() {
  let id = this.beginNewCommand();
  let yo = {from: "root", applicationType: "gecko", traits: []};
  this.sendResponse(yo, id);
};

Dispatcher.prototype.sendOk = function(cmdId) {
  this.sendResponse({from: this.actorId, ok: true}, cmdId);
};

Dispatcher.prototype.sendError = function(err, cmdId) {
  let packet = {
    from: this.actorId,
    status: err.status,
    sessionId: this.driver.sessionId,
    error: err
  };
  this.sendResponse(packet, cmdId);
};

/**
 * Marshals and sends message to either client or emulator based on the
 * provided {@code cmdId}.
 *
 * This routine produces a Marionette protocol packet, which is different
 * to a WebDriver protocol response in that it contains an extra key
 * {@code from} for the debugger transport actor ID.  It also replaces the
 * key {@code value} with {@code error} when {@code msg.status} isn't
 * {@code 0}.
 *
 * @param {Object} msg
 *     Object with the properties {@code value}, {@code status}, and
 *     {@code sessionId}.
 * @param {UUID} cmdId
 *     The unique identifier for the command the message is a response to.
 */
Dispatcher.prototype.send = function(msg, cmdId) {
  let packet = {
    from: this.actorId,
    value: msg.value,
    status: msg.status,
    sessionId: msg.sessionId,
  };

  if (typeof packet.value == "undefined") {
    packet.value = null;
  }

  // the Marionette protocol sends errors using the "error"
  // key instead of, as Selenium, "value"
  if (!error.isSuccess(msg.status)) {
    packet.error = packet.value;
    delete packet.value;
  }

  this.sendResponse(packet, cmdId);
};

// Low-level methods:

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
 * @param {Object} payload
 *     The payload to send.
 * @param {UUID} cmdId
 *     The unique identifier for this payload.  {@code -1} signifies
 *     that it's an emulator callback.
 */
Dispatcher.prototype.sendResponse = function(payload, cmdId) {
  if (emulator.isCallback(cmdId)) {
    this.sendToEmulator(payload);
  } else {
    this.sendToClient(payload, cmdId);
    this.commandId = null;
  }
};

/**
 * Send message to emulator over the debugger transport socket.
 * Notably this skips out-of-sync command checks.
 */
Dispatcher.prototype.sendToEmulator = function(payload) {
  this.sendRaw("emulator", payload);
};

/**
 * Send given payload as-is to the connected client over the debugger
 * transport socket.
 *
 * If {@code cmdId} evaluates to false, the current command state isn't
 * set, or the response is out-of-sync, a warning is logged and this
 * routine will return (no-op).
 */
Dispatcher.prototype.sendToClient = function(payload, cmdId) {
  if (!cmdId) {
    logger.warn("Got response with no command ID");
    return;
  } else if (this.commandId === null) {
    logger.warn(`No current command, ignoring response: ${payload.toSource}`);
    return;
  } else if (this.isOutOfSync(cmdId)) {
    logger.warn(`Ignoring out-of-sync response with command ID: ${cmdId}`);
    return;
  }
  this.driver.responseCompleted();
  this.sendRaw("client", payload);
};

/**
 * Sends payload as-is over debugger transport socket to client,
 * and logs it.
 */
Dispatcher.prototype.sendRaw = function(dest, payload) {
  // Avoid using toSource and template strings (or touching the payload at all
  // if not necessary) for the sake of memory use.
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=1150170
  if (logger.level <= Log.Level.Debug) {
    logger.debug(this.id + " " + dest + " <- (" + JSON.stringify(payload) + ")");
  }
  this.conn.send(payload);
};

/**
 * Begins a new command by generating a unique identifier and assigning
 * it to the current command state {@code Dispatcher.prototype.commandId}.
 *
 * @return {UUID}
 *     The generated unique identifier for the current command.
 */
Dispatcher.prototype.beginNewCommand = function() {
  let uuid = uuidGen.generateUUID().toString();
  this.commandId = uuid;
  return uuid;
};

Dispatcher.prototype.isOutOfSync = function(cmdId) {
  return this.commandId !== cmdId;
};

Dispatcher.prototype.requests = {
  getMarionetteID: Dispatcher.prototype.getMarionetteID,
  emulatorCmdResult: Dispatcher.prototype.emulatorCmdResult,
  quitApplication: Dispatcher.prototype.quitApplication
};
