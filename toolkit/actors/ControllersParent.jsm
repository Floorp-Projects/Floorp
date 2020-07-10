/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ControllersParent"];

class ControllersParent extends JSWindowActorParent {
  constructor() {
    super();

    // A map of commands that have had their enabled/disabled state assigned. The
    // value of each key will be true if enabled, and false if disabled.
    this.supportedCommands = {};
  }

  get browser() {
    return this.browsingContext.top.embedderElement;
  }

  // Update the set of enabled and disabled commands.
  enableDisableCommands(aAction, aEnabledCommands, aDisabledCommands) {
    // Clear the list first
    this.supportedCommands = {};

    for (let command of aEnabledCommands) {
      this.supportedCommands[command] = true;
    }

    for (let command of aDisabledCommands) {
      this.supportedCommands[command] = false;
    }

    let browser = this.browser;
    if (browser) {
      browser.ownerGlobal.updateCommands(aAction);
    }
  }

  isCommandEnabled(aCommand) {
    return this.supportedCommands[aCommand] || false;
  }

  supportsCommand(aCommand) {
    return aCommand in this.supportedCommands;
  }

  doCommand(aCommand) {
    this.sendAsyncMessage("ControllerCommands:Do", aCommand);
  }

  getCommandStateWithParams(aCommand, aCommandParams) {
    throw Components.Exception("Not implemented", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  doCommandWithParams(aCommand, aCommandParams) {
    let cmd = {
      cmd: aCommand,
      params: null,
    };
    if (aCommand == "cmd_lookUpDictionary") {
      // Although getBoundingClientRect of the element is logical pixel, but
      // x and y parameter of cmd_lookUpDictionary are device pixel.
      // So we need calculate child process's coordinate using correct unit.
      let browser = this.browser;
      let rect = browser.getBoundingClientRect();
      let scale = browser.ownerGlobal.devicePixelRatio;
      cmd.params = {
        x: {
          type: "long",
          value: aCommandParams.getLongValue("x") - rect.left * scale,
        },
        y: {
          type: "long",
          value: aCommandParams.getLongValue("y") - rect.top * scale,
        },
      };
    } else {
      throw Components.Exception(
        "Not implemented",
        Cr.NS_ERROR_NOT_IMPLEMENTED
      );
    }
    this.sendAsyncMessage("ControllerCommands:DoWithParams", cmd);
  }

  getSupportedCommands() {
    throw Components.Exception("Not implemented", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  onEvent() {}
}

ControllersParent.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIBrowserController",
  "nsIController",
  "nsICommandController",
]);
