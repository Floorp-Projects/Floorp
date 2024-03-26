/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class MegalistChild extends JSWindowActorChild {
  receiveMessage(message) {
    // Forward message to the View
    const win = this.document.defaultView;
    const ev = new win.CustomEvent("MessageFromViewModel", {
      detail: message,
    });
    win.dispatchEvent(ev);
  }

  // Prevent TypeError: Property 'handleEvent' is not callable.
  handleEvent() {}
}
