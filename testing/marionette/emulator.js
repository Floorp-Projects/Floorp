/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("chrome://marionette/content/error.js");

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");

const logger = Log.repository.getLogger("Marionette");

this.EXPORTED_SYMBOLS = ["emulator"];

this.emulator = {};

/**
 * Provides a service for instructing the emulator attached to the
 * currently connected client to perform shell- and command instructions.
 */
emulator.EmulatorService = class {
  /*
   * @param {function(Object)} sendToEmulatorFn
   *     Callback function that sends a message to the emulator.
   */
  constructor(sendToEmulatorFn) {
    this.sendToEmulator = sendToEmulatorFn;
  }

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
  command(cmd, resCb, errCb) {
    if (arguments.length < 1) {
      throw new ValueError("Not enough arguments");
    }
    this.sendToEmulator(
        "runEmulatorCmd", {emulator_cmd: cmd}, resCb, errCb);
  }

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
  shell(args, resCb, errCb) {
    if (arguments.length < 1) {
      throw new ValueError("Not enough arguments");
    }
    this.sendToEmulator(
        "runEmulatorShell", {emulator_shell: args}, resCb, errCb);
  }

  processMessage(msg) {
    let resCb = this.resultCallback(msg.json.id);
    let errCb = this.errorCallback(msg.json.id);

    switch (msg.name) {
      case "Marionette:runEmulatorCmd":
        this.command(msg.json.arguments, resCb, errCb);
        break;

      case "Marionette:runEmulatorShell":
        this.shell(msg.json.arguments, resCb, errCb);
        break;
    }
  }

  resultCallback(uuid) {
    return res => this.sendResult({value: res, id: uuid});
  }

  errorCallback(uuid) {
    return err => this.sendResult({error: err, id: uuid});
  }

  sendResult(msg) {
    // sendToListener set explicitly in GeckoDriver's ctor
    this.sendToListener("listenerResponse", msg);
  }

  /** Receives IPC messages from the listener. */
  // TODO(ato): The idea of services in chrome space
  // can be generalised at some later time.
  receiveMessage(msg) {
    let uuid = msg.json.id;
    try {
      this.processMessage(msg);
    } catch (e) {
      this.sendResult({error: `${e.name}: ${e.message}`, id: uuid});
    }
  }
};

emulator.EmulatorService.prototype.QueryInterface =
    XPCOMUtils.generateQI([
      Ci.nsIMessageListener,
      Ci.nsISupportsWeakReference
    ]);

emulator.EmulatorServiceClient = class {
  constructor(chromeProxy) {
    this.chrome = chromeProxy;
  }

  *command(cmd, cb) {
    let res = yield this.chrome.runEmulatorCmd(cmd);
    if (cb) {
      cb(res);
    }
  }

  *shell(args, cb) {
    let res = yield this.chrome.runEmulatorShell(args);
    if (cb) {
      cb(res);
    }
  }
};

/**
 * Adapts EmulatorService for use with sandboxes that scripts are
 * evaluated in.  Is consumed by sandbox.augment.
 */
emulator.Adapter = class {
  constructor(emulator) {
    this.emulator = emulator;
  }

  get exports() {
    return new Map([
      ["runEmulatorCmd", this.runEmulatorCmd.bind(this)],
      ["runEmulatorShell", this.runEmulatorShell.bind(this)],
    ]);
  }

  runEmulatorCmd(cmd, cb) {
    this.yield(this.emulator.command(cmd, cb));
  }

  runEmulatorShell(args, cb) {
    this.yield(this.emulator.shell(args, cb));
  }

  yield(promise) {
    Task.spawn(function() {
      yield promise;
    });
  }
};
