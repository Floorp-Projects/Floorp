/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ControllersChild"];

ChromeUtils.import("resource://gre/modules/ActorChild.jsm");

class ControllersChild extends ActorChild {
  receiveMessage(message) {
    switch (message.name) {
      case "ControllerCommands:Do":
        if (this.docShell.isCommandEnabled(message.data))
          this.docShell.doCommand(message.data);
        break;

      case "ControllerCommands:DoWithParams":
        var data = message.data;
        if (this.docShell.isCommandEnabled(data.cmd)) {
          var params = Cc["@mozilla.org/embedcomp/command-params;1"].
                       createInstance(Ci.nsICommandParams);
          for (var name in data.params) {
            var value = data.params[name];
            if (value.type == "long") {
              params.setLongValue(name, parseInt(value.value));
            } else {
              throw Cr.NS_ERROR_NOT_IMPLEMENTED;
            }
          }
          this.docShell.doCommandWithParams(data.cmd, params);
        }
        break;
    }
  }
}
