/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ClipboardReadPasteChild"];

/**
 * Propagates "MozClipboardReadPaste" events from a content process to the
 * chrome process.
 * Receives messages from the chrome process.
 */
class ClipboardReadPasteChild extends JSWindowActorChild {
  constructor() {
    super();
  }

  // EventListener interface.
  handleEvent(aEvent) {
    if (aEvent.type == "MozClipboardReadPaste" && aEvent.isTrusted) {
      this.sendAsyncMessage("ClipboardReadPaste:ShowMenupopup", {});
    }
  }

  // For JSWindowActorChild.
  receiveMessage(value) {
    switch (value.name) {
      case "ClipboardReadPaste:PasteMenuItemClicked": {
        this.contentWindow.navigator.clipboard.onUserReactedToPasteMenuPopup(
          true
        );
        break;
      }
      case "ClipboardReadPaste:PasteMenuItemDismissed": {
        this.contentWindow.navigator.clipboard.onUserReactedToPasteMenuPopup(
          false
        );
        break;
      }
    }
  }
}
