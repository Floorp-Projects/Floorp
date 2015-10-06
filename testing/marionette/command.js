/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var {utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Task.jsm");

Cu.import("chrome://marionette/content/error.js");

this.EXPORTED_SYMBOLS = ["CommandProcessor", "Response"];
const logger = Log.repository.getLogger("Marionette");

const validator = {
  exclusionary: {
    "capabilities": ["error", "value"],
    "error": ["value", "sessionId", "capabilities"],
    "sessionId": ["error", "value"],
    "value": ["error", "sessionId", "capabilities"],
  },

  set: function(obj, prop, val) {
    let tests = this.exclusionary[prop];
    if (tests) {
      for (let t of tests) {
        if (obj.hasOwnProperty(t)) {
          throw new TypeError(`${t} set, cannot set ${prop}`);
        }
      }
    }

    obj[prop] = val;
    return true;
  },
};

/**
 * The response body is exposed as an argument to commands.
 * Commands can set fields on the body through defining properties.
 *
 * Setting properties invokes a validator that performs tests for
 * mutually exclusionary fields on the input against the existing data
 * in the body.
 *
 * For example setting the {@code error} property on the body when
 * {@code value}, {@code sessionId}, or {@code capabilities} have been
 * set previously will cause an error.
 */
this.ResponseBody = () => new Proxy({}, validator);

/**
 * Represents the response returned from the remote end after execution
 * of its corresponding command.
 *
 * The response is a mutable object passed to each command for
 * modification through the available setters.  To send data in a response,
 * you modify the body property on the response.  The body property can
 * also be replaced completely.
 *
 * The response is sent implicitly by CommandProcessor when a command
 * has finished executing, and any modifications made subsequent to that
 * will have no effect.
 *
 * @param {number} cmdId
 *     UUID tied to the corresponding command request this is
 *     a response for.
 * @param {function(Object, number)} respHandler
 *     Callback function called on responses.
 */
this.Response = function(cmdId, respHandler) {
  this.id = cmdId;
  this.respHandler = respHandler;
  this.sent = false;
  this.body = ResponseBody();
};

Response.prototype.send = function() {
  if (this.sent) {
    throw new RangeError("Response has already been sent: " + this.toString());
  }
  this.respHandler(this.body, this.id);
  this.sent = true;
};

Response.prototype.sendError = function(err) {
  let wd = error.isWebDriverError(err);
  let we = wd ? err : new WebDriverError(err.message);

  this.body.error = we.status;
  this.body.message = we.message || null;
  this.body.stacktrace = we.stack || null;

  this.send();

  // propagate errors that are implementation problems
  if (!wd) {
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
 * @param {function(Object, number)} respHandler
 *     Callback function called on responses.
 * @param {number} cmdId
 *     The unique identifier for the command to execute.
 */
CommandProcessor.prototype.execute = function(payload, respHandler, cmdId) {
  let cmd = payload;
  let resp = new Response(cmdId, respHandler);
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
