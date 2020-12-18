/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["PrintingSelectionChild"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

class PrintingSelectionChild extends JSWindowActorChild {
  receiveMessage(message) {
    switch (message.name) {
      case "PrintingSelection:HasSelection":
        return this.hasSelection(this.document.ownerGlobal);
    }

    return undefined;
  }

  hasSelection(global) {
    const { content } = global;

    let focusedWindow = Services.focus.focusedWindow || content;
    let selection = focusedWindow.getSelection();
    return selection.type == "Range";
  }
}
