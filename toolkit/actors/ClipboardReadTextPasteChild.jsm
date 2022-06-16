/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ClipboardReadTextPasteChild"];

/**
 * Propagates "MozClipboardReadTextPaste" events from a content process to the
 * chrome process.
 * Receives messages from the chrome process.
 */
class ClipboardReadTextPasteChild extends JSWindowActorChild {
  constructor() {
    super();
  }

  // EventListener interface.
  handleEvent(aEvent) {
    if (aEvent.type == "MozClipboardReadTextPaste" && aEvent.isTrusted) {
      this.sendAsyncMessage("ClipboardReadTextPaste:ShowMenupopup", {});
    }
  }

  // For JSWindowActorChild.
  receiveMessage(value) {
    switch (value.name) {
      case "ClipboardReadTextPaste:PasteMenuItemClicked": {
        // TODO: notify C++ side.
        break;
      }
      case "ClipboardReadTextPaste:PasteMenuItemDismissed": {
        // TODO: notify C++ side.
        break;
      }
    }
  }
}
