/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var {utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Task.jsm");

Cu.import("chrome://marionette/content/error.js");

this.EXPORTED_SYMBOLS = [
  "Command",
  "Message",
  "MessageOrigin",
  "Response",
];

const logger = Log.repository.getLogger("Marionette");

this.MessageOrigin = {
  Client: 0,
  Server: 1,
};

this.Message = {};

/**
 * Converts a data packet into a Command or Response type.
 *
 * @param {Array.<number, number, ?, ?>} data
 *     A four element array where the elements, in sequence, signifies
 *     message type, message ID, method name or error, and parameters
 *     or result.
 *
 * @return {(Command,Response)}
 *     Based on the message type, a Command or Response instance.
 *
 * @throws {TypeError}
 *     If the message type is not recognised.
 */
Message.fromMsg = function(data) {
  switch (data[0]) {
    case Command.TYPE:
      return Command.fromMsg(data);

    case Response.TYPE:
      return Response.fromMsg(data);

    default:
      throw new TypeError(
          "Unrecognised message type in packet: " + JSON.stringify(data));
  }
};

/**
 * A command is a request from the client to run a series of remote end
 * steps and return a fitting response.
 *
 * The command can be synthesised from the message passed over the
 * Marionette socket using the {@code fromMsg} function.  The format of
 * a message is:
 *
 *     [type, id, name, params]
 *
 * where
 *
 *   type:
 *     Must be zero (integer). Zero means that this message is a command.
 *
 *   id:
 *     Number used as a sequence number.  The server replies with a
 *     requested id.
 *
 *   name:
 *     String representing the command name with an associated set of
 *     remote end steps.
 *
 *   params:
 *     Object of command function arguments.  The keys of this object
 *     must be strings, but the values can be arbitrary values.
 *
 * A command has an associated message {@code id} that prevents the
 * dispatcher from sending responses in the wrong order.
 *
 * The command may also have optional error- and result handlers that
 * are called when the client returns with a response.  These are
 * {@code function onerror({Object})}, {@code function onresult({Object})},
 * and {@code function onresult({Response})}.
 *
 * @param {number} msgId
 *     Message ID unique identifying this message.
 * @param {string} name
 *     Command name.
 * @param {Object<string, ?>} params
 *     Command parameters.
 */
this.Command = class {
  constructor(msgId, name, params={}) {
    this.id = msgId;
    this.name = name;
    this.parameters = params;

    this.onerror = null;
    this.onresult = null;

    this.origin = MessageOrigin.Client;
    this.sent = false;
  }

  /**
   * Calls the error- or result handler associated with this command.
   * This function can be replaced with a custom response handler.
   *
   * @param {Response} resp
   *     The response to pass on to the result or error to the
   *     {@code onerror} or {@code onresult} handlers to.
   */
  onresponse(resp) {
    if (resp.error && this.onerror) {
      this.onerror(resp.error);
    } else if (resp.body && this.onresult) {
      this.onresult(resp.body);
    }
  }

  toMsg() {
    return [Command.TYPE, this.id, this.name, this.parameters];
  }

  toString() {
    return "Command {id: " + this.id + ", " +
        "name: " + JSON.stringify(this.name) + ", " +
        "parameters: " + JSON.stringify(this.parameters) + "}"
  }

  static fromMsg(msg) {
    return new Command(msg[1], msg[2], msg[3]);
  }
};

Command.TYPE = 0;


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
 * @param {number} msgId
 *     Message ID tied to the corresponding command request this is a
 *     response for.
 * @param {function(Response|Message)} respHandler
 *     Function callback called on sending the response.
 */
this.Response = class {
  constructor(msgId, respHandler) {
    this.id = msgId;

    this.error = null;
    this.body = ResponseBody();

    this.origin = MessageOrigin.Server;
    this.sent = false;

    this.respHandler_ = respHandler;
  }

  /**
   * Sends response conditionally, given a predicate.
   *
   * @param {function(Response): boolean} predicate
   *     A predicate taking a Response object and returning a boolean.
   */
  sendConditionally(predicate) {
    if (predicate(this)) {
      this.send();
    }
  }

  /**
   * Sends response using the response handler provided on construction.
   *
   * @throws {RangeError}
   *     If the response has already been sent.
   */
  send() {
    if (this.sent) {
      throw new RangeError("Response has already been sent: " + this);
    }
    this.respHandler_(this);
    this.sent = true;
  }

  /**
   * Send given Error to client.
   *
   * Turns the response into an error response, clears any previously
   * set body data, and sends it using the response handler provided
   * on construction.
   *
   * @param {Error} err
   *     The Error instance to send.
   *
   * @throws {Error}
   *     If the {@code error} is not a WebDriverError, the error is
   *     propagated.
   */
  sendError(err) {
    let wd = error.isWebDriverError(err);
    let we = wd ? err : new WebDriverError(err.message);

    this.error = error.toJson(we);
    this.body = null;
    this.send();

    // propagate errors that are implementation problems
    if (!wd) {
      throw err;
    }
  }

  toMsg() {
    return [Response.TYPE, this.id, this.error, this.body];
  }

  toString() {
    return "Response {id: " + this.id + ", " +
        "error: " + JSON.stringify(this.error) + ", " +
        "body: " + JSON.stringify(this.body) + "}";
  }

  static fromMsg(msg) {
    let resp = new Response(msg[1], null);
    resp.error = msg[2];
    resp.body = msg[3];
    return resp;
  }
};

Response.TYPE = 1;
