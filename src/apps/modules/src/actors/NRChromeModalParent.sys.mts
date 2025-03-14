/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { ModalOptions, ModalResponse } from "../common/NRModalTypes.ts";

export class NRChromeModalParent extends JSWindowActorParent {
  receiveMessage(
    message: ReceiveMessageArgument,
  ): ModalResponse | null {
    switch (message.name) {
      case "NRChromeModal:show": {
        const options: ModalOptions = message.data;
        this.sendAsyncMessage("NRChromeModal:RenderContent", {
          jsx: options.jsx,
        });
        break;
      }
    }
    return null;
  }
}
