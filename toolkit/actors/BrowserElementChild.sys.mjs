/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class BrowserElementChild extends JSWindowActorChild {
  handleEvent(event) {
    if (
      event.type == "DOMWindowClose" &&
      !this.manager.browsingContext.parent
    ) {
      this.sendAsyncMessage("DOMWindowClose", {});
    }
  }

  receiveMessage(message) {
    switch (message.name) {
      case "EnterModalState": {
        this.contentWindow.windowUtils.enterModalState();
        break;
      }

      case "LeaveModalState": {
        if (
          !message.data.forceLeave &&
          !this.contentWindow.windowUtils.isInModalState()
        ) {
          break;
        }
        this.contentWindow.windowUtils.leaveModalState();
        break;
      }
    }
  }
}
