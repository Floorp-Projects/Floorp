/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Task.jsm");

Cu.import("chrome://marionette/content/error.js");

this.EXPORTED_SYMBOLS = ["CommandProcessor", "Response"];
const logger = Log.repository.getLogger("Marionette");

/**
 * Represents the response returned from the remote end after execution
 * of its corresponding command.
 *
 * The Response is a mutable object passed to each command for
 * modification through the available setters.  The response is sent
 * implicitly by CommandProcessor when a command is finished executing,
 * and any modifications made subsequent to this will have no effect.
 *
 * @param {number} cmdId
 *     UUID tied to the corresponding command request this is
 *     a response for.
 * @param {function(number)} okHandler
 *     Callback function called on successful responses with no body.
 * @param {function(Object, number)} respHandler
 *     Callback function called on successful responses with body.
 * @param {Object=} msg
 *     A message to populate the response, containing the properties
 *     "sessionId", "status", and "value".
 * @param {function(Map)=} sanitizer
 *     Run before sending message.
 */
this.Response = function(cmdId, okHandler, respHandler, msg, sanitizer) {
  const removeEmpty = function(map) {
    let rv = {};
    for (let [key, value] of map) {
      if (typeof value == "undefined") {
        value = null;
      }
      rv[key] = value;
    }
    return rv;
  };

  this.id = cmdId;
  this.ok = true;
  this.okHandler = okHandler;
  this.respHandler = respHandler;
  this.sanitizer = sanitizer || removeEmpty;

  this.data = new Map([
    ["sessionId", msg.sessionId ? msg.sessionId : null],
    ["status", msg.status ? msg.status : "success"],
    ["value", msg.value ? msg.value : undefined],
  ]);
};

Response.prototype = {
  get name() { return this.data.get("name"); },
  set name(n) { this.data.set("name", n); },
  get sessionId() { return this.data.get("sessionId"); },
  set sessionId(id) { this.data.set("sessionId", id); },
  get status() { return this.data.get("status"); },
  set status(ns) { this.data.set("status", ns); },
  get value() { return this.data.get("value"); },
  set value(val) {
    this.data.set("value", val);
    this.ok = false;
  }
};

Response.prototype.send = function() {
  if (this.sent) {
    logger.warn("Skipped sending response to command ID " +
      this.id + " because response has already been sent");
    return;
  }

  if (this.ok) {
    this.okHandler(this.id);
  } else {
    let rawData = this.sanitizer(this.data);
    this.respHandler(rawData, this.id);
  }
};

/**
 * @param {(Error|Object)} err
 *     The error to send, either an instance of the Error prototype,
 *     or an object with the properties "message", "status", and "stack".
 */
Response.prototype.sendError = function(err) {
  this.status = "status" in err ? err.status : new UnknownError().status;
  this.value = error.toJSON(err);
  this.send();

  // propagate errors that are implementation problems
  if (!error.isWebDriverError(err)) {
    throw err;
  }
};

/**
 * The command processor receives messages on execute(payload, …)
 * from the dispatcher, processes them, and wraps the functions that
 * it executes from the WebDriver implementation, driver.
 *
 * @param {GeckoDriver} driver
 *     Reference to the driver implementation.
 */
this.CommandProcessor = function(driver) {
  this.driver = driver;
};

/**
 * Executes a WebDriver command based on the received payload,
 * which is expected to be an object with a "parameters" property
 * that is a simple key/value collection of arguments.
 *
 * The respHandler function will be called with the JSON object to
 * send back to the client.
 *
 * The cmdId is the UUID tied to this request that prevents
 * the dispatcher from sending responses in the wrong order.
 *
 * @param {Object} payload
 *     Message as received from client.
 * @param {function(number)} okHandler
 *     Callback function called on successful responses with no body.
 * @param {function(Object, number)} respHandler
 *     Callback function called on successful responses with body.
 * @param {number} cmdId
 *     The unique identifier for the command to execute.
 */
CommandProcessor.prototype.execute = function(payload, okHandler, respHandler, cmdId) {
  let cmd = payload;
  let resp = new Response(
    cmdId, okHandler, respHandler, {sessionId: this.driver.sessionId});
  let sendResponse = resp.send.bind(resp);
  let sendError = resp.sendError.bind(resp);

  // Ideally handlers shouldn't have to care about the command ID,
  // but some methods (newSession, executeScript, et al.) have not
  // yet been converted to use the new form of request dispatching.
  cmd.id = cmdId;

  let req = Task.spawn(function*() {
    let fn = this.driver.commands[cmd.name];
    if (typeof fn == "undefined") {
      throw new UnknownCommandError(cmd.name);
    }

    yield fn.bind(this.driver)(cmd, resp);
  }.bind(this));

  req.then(sendResponse, sendError).catch(error.report);
};
