/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["BrowserElementChild"];

class BrowserElementChild extends JSWindowActorChild {
  handleEvent(event) {
    if (
      event.type == "DOMWindowClose" &&
      !this.manager.browsingContext.parent
    ) {
      this.sendAsyncMessage("DOMWindowClose", {});
    }
  }

  receiveMessage(message) {
    let context = this.manager.browsingContext;
    let docShell = context.docShell;

    switch (message.name) {
      case "PermitUnload": {
        this.sendAsyncMessage("Running", {});

        let permitUnload = true;
        if (docShell && docShell.contentViewer) {
          permitUnload = docShell.contentViewer.permitUnload(
            message.data.flags
          );
        }

        this.sendAsyncMessage("Done", { permitUnload });
        break;
      }

      case "EnterModalState": {
        this.contentWindow.windowUtils.enterModalState();
        break;
      }

      case "LeaveModalState": {
        this.contentWindow.windowUtils.leaveModalState();
        break;
      }
    }
  }
}
