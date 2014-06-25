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
}

RemoteController.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIController]),

  isCommandEnabled: function(aCommand) {
    // We can't synchronously ask content if a command is enabled,
    // so we always pretend is.
    // The right way forward would be to never use nsIController
    // to ask if something in content is enabled. Maybe even
    // by replacing the nsIController architecture by something else.
    // See bug 905768.
    return true;
  },

  supportsCommand: function(aCommand) {
    // Optimize the lookup a bit.
    if (!aCommand.startsWith("cmd_"))
      return false;

    // For now only support the commands used in "browser-context.inc"
    let commands = [
      "cmd_copyLink",
      "cmd_copyImage",
      "cmd_undo",
      "cmd_cut",
      "cmd_copy",
      "cmd_paste",
      "cmd_delete",
      "cmd_selectAll",
      "cmd_switchTextDirection"
    ];

    return commands.indexOf(aCommand) >= 0;
  },

  doCommand: function(aCommand) {
    this._browser.messageManager.sendAsyncMessage("ControllerCommands:Do", aCommand);
  },

  onEvent: function () {}
};
