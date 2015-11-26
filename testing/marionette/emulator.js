/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var {classes: Cc, interfaces: Ci} = Components;
const uuidGen = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);
this.EXPORTED_SYMBOLS = ["emulator", "Emulator", "EmulatorCallback"];

this.emulator = {};

/**
 * Determines if command ID is an emulator callback.
 */
this.emulator.isCallback = function(cmdId) {
  return cmdId < 0;
};

/**
 * Represents the connection between Marionette and the emulator it's
 * running on.
 *
 * When injected scripts call the JS routines {@code runEmulatorCmd} or
 * {@code runEmulatorShell}, the second argument to those is a callback
 * which is stored in cbs.  They are later retreived by their unique ID
 * using popCallback.
 *
 * @param {function(Object)} sendFn
 *     Callback function that sends a message to the emulator.
 */
this.Emulator = function(sendFn) {
  this.send = sendFn;
  this.cbs = [];
};

/**
 * Pops a callback off the stack if found.  Otherwise this is a no-op.
 *
 * @param {number} id
 *     Unique ID associated with the callback.
 *
 * @return {?function(Object)}
 *     Callback function that takes an emulator response message as
 *     an argument.
 */
Emulator.prototype.popCallback = function(id) {
  let f, fi;
  for (let i = 0; i < this.cbs.length; ++i) {
    if (this.cbs[i].id == id) {
      f = this.cbs[i];
      fi = i;
    }
  }

  if (!f) {
    return null;
  }

  this.cbs.splice(fi, 1);
  return f;
};

/**
 * Pushes callback on to the stack.
 *
 * @param {function(Object)} cb
 *     Callback function that takes an emulator response message as
 *     an argument.
 */
Emulator.prototype.pushCallback = function(cb) {
  cb.send_ = this.sendFn;
  this.cbs.push(cb);
};

/**
 * Encapsulates a callback to the emulator and provides an execution
 * environment for them.
 *
 * Each callback is assigned a unique identifier, id, that can be used
 * to retrieve them from Emulator's stack using popCallback.
 *
 * The onresult event listener is triggered when a result arrives on
 * the callback.
 *
 * The onerror event listener is triggered when an error occurs during
 * the execution of that callback.
 */
this.EmulatorCallback = function() {
  this.id = uuidGen.generateUUID().toString();
  this.onresult = null;
  this.onerror = null;
  this.send_ = null;
};

EmulatorCallback.prototype.command = function(cmd, cb) {
  this.onresult = cb;
  this.send_({emulator_cmd: cmd, id: this.id});
};

EmulatorCallback.prototype.shell = function(args, cb) {
  this.onresult = cb;
  this.send_({emulator_shell: args, id: this.id});
};

EmulatorCallback.prototype.result = function(msg) {
  if (this.send_ === null) {
    throw new TypeError(
      "EmulatorCallback must be registered with Emulator to fire");
  }

  try {
    if (!this.onresult) {
      return;
    }
    this.onresult(msg.result);
  } catch (e) {
    if (this.onerror) {
      this.onerror(e);
    }
  }
};
