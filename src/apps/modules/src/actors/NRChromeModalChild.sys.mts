/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRChromeModalChild extends JSWindowActorChild {
  actorCreated() {
    console.log("NRChromeModalChild actor created");
  }

  async receiveMessage(message: ReceiveMessageArgument) {
    const window = this.contentWindow as Window;
    switch (message.name) {
      case "NRChromeModal:show": {
        await this.renderContent(window, message.data);
        break;
      }
    }
    return null;
  }

  renderContent(win: Window, jsx: string) {
    try {
      win.appendChildToform(jsx);
      return;
    } catch (_e) {
      const element = this.createElmFromStr(win, jsx);
      const modal = element;
      if (modal) {
        const form = win.document?.querySelector("#dynamic-form");
        if (form) {
          while (form.firstChild) {
            form.removeChild(form.firstChild);
          }
          form.appendChild(modal);
        }
        win.document?.querySelector("#dynamic-form")?.appendChild(modal);
      }
    }
  }

  createElmFromStr(win: Window, str: string) {
    return new win.DOMParser().parseFromString(str, "text/html").body
      .firstElementChild;
  }
}
