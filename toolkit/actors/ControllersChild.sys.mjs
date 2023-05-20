/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class ControllersChild extends JSWindowActorChild {
  receiveMessage(message) {
    switch (message.name) {
      case "ControllerCommands:Do":
        if (this.docShell && this.docShell.isCommandEnabled(message.data)) {
          this.docShell.doCommand(message.data);
        }
        break;

      case "ControllerCommands:DoWithParams":
        var data = message.data;
        if (this.docShell && this.docShell.isCommandEnabled(data.cmd)) {
          var params = Cu.createCommandParams();
          let substituteXY = false;
          let x = 0;
          let y = 0;
          if (
            data.cmd == "cmd_lookUpDictionary" &&
            "x" in data.params &&
            "y" in data.params &&
            data.params.x.type == "long" &&
            data.params.y.type == "long"
          ) {
            substituteXY = true;
            x = parseInt(data.params.x.value);
            y = parseInt(data.params.y.value);

            let rect =
              this.contentWindow.windowUtils.convertFromParentProcessWidgetToLocal(
                x,
                y,
                1,
                1
              );
            x = Math.round(rect.x);
            y = Math.round(rect.y);
          }

          for (var name in data.params) {
            var value = data.params[name];
            if (value.type == "long") {
              if (substituteXY && name === "x") {
                params.setLongValue(name, x);
              } else if (substituteXY && name === "y") {
                params.setLongValue(name, y);
              } else {
                params.setLongValue(name, parseInt(value.value));
              }
            } else {
              throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
            }
          }
          this.docShell.doCommandWithParams(data.cmd, params);
        }
        break;
    }
  }
}
