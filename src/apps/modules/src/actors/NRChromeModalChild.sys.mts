/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { TForm } from "../../../main/core/common/modal-parent/utils/type.ts";
export class NRChromeModalChild extends JSWindowActorChild {
  actorCreated() {
    console.log("NRChromeModalChild actor created");
  }

  async receiveMessage(message: ReceiveMessageArgument) {
    const window = this.contentWindow as Window;
    switch (message.name) {
      case "NRChromeModal:show": {
        console.log("appendChildToform", window.appendChildToform);
        await this.renderContent(window, message.data);
        break;
      }
    }
    return null;
  }

  renderContent(win: Window, from: TForm) {
    win.buildFormFromConfig(from);
  }
}
