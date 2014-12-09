// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

this.EXPORTED_SYMBOLS = ["RemoteController"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function RemoteController(browser)
{
  this._browser = browser;

  // A map of commands that have had their enabled/disabled state assigned. The
  // value of each key will be true if enabled, and false if disabled.
  this._supportedCommands = { };
}

RemoteController.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIController]),

  isCommandEnabled: function(aCommand) {
    return this._supportedCommands[aCommand] || false;
  },

  supportsCommand: function(aCommand) {
    return aCommand in this._supportedCommands;
  },

  doCommand: function(aCommand) {
    this._browser.messageManager.sendAsyncMessage("ControllerCommands:Do", aCommand);
  },

  onEvent: function () {},

  // This is intended to be called from the remote-browser binding to update
  // the enabled and disabled commands.
  enableDisableCommands: function(aAction,
                                  aEnabledLength, aEnabledCommands,
                                  aDisabledLength, aDisabledCommands) {
    // Clear the list first
    this._supportedCommands = { };

    for (let c = 0; c < aEnabledLength; c++) {
      this._supportedCommands[aEnabledCommands[c]] = true;
    }

    for (let c = 0; c < aDisabledLength; c++) {
      this._supportedCommands[aDisabledCommands[c]] = false;
    }

    this._browser.ownerDocument.defaultView.updateCommands(aAction);
  }
};
