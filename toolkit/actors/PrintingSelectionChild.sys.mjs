/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class PrintingSelectionChild extends JSWindowActorChild {
  receiveMessage(message) {
    switch (message.name) {
      case "PrintingSelection:HasSelection":
        return this.hasSelection();
    }

    return undefined;
  }

  hasSelection() {
    let selection = this.browsingContext.associatedWindow?.getSelection();
    return selection && selection.type == "Range";
  }
}
