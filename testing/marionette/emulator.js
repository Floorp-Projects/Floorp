/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const logger = Log.repository.getLogger("Marionette");

this.EXPORTED_SYMBOLS = ["Emulator"];

/**
 * Represents the connection between Marionette and the emulator it's
 * running on.
 *
 * When injected scripts call the JS routines {@code runEmulatorCmd} or
 * {@code runEmulatorShell}, the second argument to those is a callback
 * which is stored in cbs.  They are later retreived by their unique ID
 * using popCallback.
 *
 * @param {function(Object)} sendToEmulatorFn
 *     Callback function that sends a message to the emulator.
 * @param {function(Object)} sendToEmulatorFn
 *     Callback function that sends a message asynchronously to the
 *     current listener.
 */
this.Emulator = function(sendToEmulatorFn) {
  this.sendToEmulator = sendToEmulatorFn;
};

/**
 * Instruct the client to run an Android emulator command.
 *
 * @param {string} cmd
 *     The command to run.
 * @param {function(?)} resCb
 *     Callback on a result response from the emulator.
 * @param {function(?)} errCb
 *     Callback on an error in running the command.
 */
Emulator.prototype.command = function(cmd, resCb, errCb) {
  assertDefined(cmd, "runEmulatorCmd");
  this.sendToEmulator(
      "runEmulatorCmd", {emulator_cmd: cmd}, resCb, errCb);
};

/**
 * Instruct the client to execute Android emulator shell arguments.
 *
 * @param {Array.<string>} args
 *     The shell instruction for the emulator to execute.
 * @param {function(?)} resCb
 *     Callback on a result response from the emulator.
 * @param {function(?)} errCb
 *     Callback on an error in executing the shell arguments.
 */
Emulator.prototype.shell = function(args, resCb, errCb) {
  assertDefined(args, "runEmulatorShell");
  this.sendToEmulator(
      "runEmulatorShell", {emulator_shell: args}, resCb, errCb);
};

Emulator.prototype.processMessage = function(msg) {
  let resCb = this.resultCallback(msg.json.id);
  let errCb = this.errorCallback(msg.json.id);

  switch (msg.name) {
    case "Marionette:runEmulatorCmd":
      this.command(msg.json.command, resCb, errCb);
      break;

    case "Marionette:runEmulatorShell":
      this.shell(msg.json.arguments, resCb, errCb);
      break;
  }
};

Emulator.prototype.resultCallback = function(msgId) {
  return res => this.sendResult({result: res, id: msgId});
};

Emulator.prototype.errorCallback = function(msgId) {
  return err => this.sendResult({error: err, id: msgId});
};

Emulator.prototype.sendResult = function(msg) {
  // sendToListener set explicitly in GeckoDriver's ctor
  this.sendToListener("emulatorCmdResult", msg);
};

/** Receives IPC messages from the listener. */
Emulator.prototype.receiveMessage = function(msg) {
  try {
    this.processMessage(msg);
  } catch (e) {
    this.sendResult({error: `${e.name}: ${e.message}`, id: msg.json.id});
  }
};

Emulator.prototype.QueryInterface = XPCOMUtils.generateQI(
    [Ci.nsIMessageListener, Ci.nsISupportsWeakReference]);

function assertDefined(arg, action) {
  if (typeof arg == "undefined") {
    throw new TypeError("Not enough arguments to " + action);
  }
}
