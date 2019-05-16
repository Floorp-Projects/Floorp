// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function RemoteController(browser) {
  this._browser = browser;

  // A map of commands that have had their enabled/disabled state assigned. The
  // value of each key will be true if enabled, and false if disabled.
  this._supportedCommands = { };
}

RemoteController.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIController,
                                          Ci.nsICommandController]),

  isCommandEnabled(aCommand) {
    return this._supportedCommands[aCommand] || false;
  },

  supportsCommand(aCommand) {
    return aCommand in this._supportedCommands;
  },

  doCommand(aCommand) {
    this._browser.messageManager.sendAsyncMessage("ControllerCommands:Do", aCommand);
  },

  getCommandStateWithParams(aCommand, aCommandParams) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  doCommandWithParams(aCommand, aCommandParams) {
    let cmd = {
      cmd: aCommand,
      params: null,
    };
    if (aCommand == "cmd_lookUpDictionary") {
      // Although getBoundingClientRect of the element is logical pixel, but
      // x and y parameter of cmd_lookUpDictionary are device pixel.
      // So we need calculate child process's coordinate using correct unit.
      let rect = this._browser.getBoundingClientRect();
      let scale = this._browser.ownerGlobal.devicePixelRatio;
      cmd.params = {
        x:  {
          type: "long",
          value: aCommandParams.getLongValue("x") - rect.left * scale,
        },
        y: {
          type: "long",
          value: aCommandParams.getLongValue("y") - rect.top * scale,
        },
      };
    } else {
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;
    }
    this._browser.messageManager.sendAsyncMessage(
      "ControllerCommands:DoWithParams", cmd);
  },

  getSupportedCommands(aCount, aCommands) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  onEvent() {},

  // This is intended to be called from the browser binding to update
  // the enabled and disabled commands.
  enableDisableCommands(aAction, aEnabledCommands, aDisabledCommands) {
    // Clear the list first
    this._supportedCommands = { };

    for (let command of aEnabledCommands) {
      this._supportedCommands[command] = true;
    }

    for (let command of aDisabledCommands) {
      this._supportedCommands[command] = false;
    }

    // Don't update anything if we're not the active element
    if (this._browser != this._browser.ownerDocument.activeElement) {
      return;
    }
    this._browser.ownerGlobal.updateCommands(aAction);
  },
};
