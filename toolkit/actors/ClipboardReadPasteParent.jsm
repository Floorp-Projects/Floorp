/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ClipboardReadPasteParent"];

const kMenuPopupId = "clipboardReadPasteMenuPopup";

// Exchanges messages with the child actor and handles events from the
// pasteMenuHandler.
class ClipboardReadPasteParent extends JSWindowActorParent {
  constructor() {
    super();

    this._menupopup = null;
    this._pasteMenuItemClicked = false;
  }

  // EventListener interface.
  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "command": {
        this.onCommand();
        break;
      }
      case "popuphiding": {
        this.onPopupHiding();
        break;
      }
    }
  }

  onCommand() {
    this._pasteMenuItemClicked = true;
    this.sendAsyncMessage("ClipboardReadPaste:PasteMenuItemClicked");
  }

  onPopupHiding() {
    // Remove the listeners before potentially sending the async message
    // below, because that might throw.
    this._removeMenupopupEventListeners();

    if (this._pasteMenuItemClicked) {
      // A message was already sent. Reset the state to handle further
      // click/dismiss events properly.
      this._pasteMenuItemClicked = false;
    } else {
      this.sendAsyncMessage("ClipboardReadPaste:PasteMenuItemDismissed");
    }
  }

  // For JSWindowActorParent.
  receiveMessage(value) {
    if (value.name == "ClipboardReadPaste:ShowMenupopup") {
      if (!this._menupopup) {
        this._menupopup = this._getMenupopup();
      }

      this._addMenupopupEventListeners();

      const browser = this.browsingContext.top.embedderElement;
      const window = browser.ownerGlobal;
      const windowUtils = window.windowUtils;

      let mouseXInCSSPixels = {};
      let mouseYInCSSPixels = {};
      windowUtils.getLastOverWindowPointerLocationInCSSPixels(
        mouseXInCSSPixels,
        mouseYInCSSPixels
      );

      // `openPopup` is a no-op if the popup is already opened.
      // That property is used when `navigator.clipboard.readText()` or
      // `navigator.clipboard.read()`is called from two different frames, e.g.
      // an iframe and the top level frame. In that scenario, the two frames
      // correspond to different `navigator.clipboard` instances. When
      // `readText()` or `read()` is called from both frames, an actor pair is
      // instantiated for each of them. Both actor parents will call `openPopup`
      // on the same `_menupopup` object. If the popup is already open,
      // `openPopup` is a no-op. When the popup is clicked or dismissed both
      // actor parents will receive the corresponding event.
      this._menupopup.openPopup(
        null,
        "overlap" /* options */,
        mouseXInCSSPixels.value,
        mouseYInCSSPixels.value,
        true /* isContextMenu */
      );
    }
  }

  _addMenupopupEventListeners() {
    this._menupopup.addEventListener("command", this);
    this._menupopup.addEventListener("popuphiding", this);
  }

  _removeMenupopupEventListeners() {
    this._menupopup.removeEventListener("command", this);
    this._menupopup.removeEventListener("popuphiding", this);
  }

  _createMenupopup(aChromeDoc) {
    let menuitem = aChromeDoc.createXULElement("menuitem");
    menuitem.id = "clipboardReadPasteMenuItem";
    menuitem.setAttribute("data-l10n-id", "text-action-paste");

    let menupopup = aChromeDoc.createXULElement("menupopup");
    menupopup.id = kMenuPopupId;
    menupopup.appendChild(menuitem);
    return menupopup;
  }

  _getMenupopup() {
    let browser = this.browsingContext.top.embedderElement;
    let window = browser.ownerGlobal;
    let chromeDoc = window.document;

    let menupopup = chromeDoc.getElementById(kMenuPopupId);
    if (menupopup == null) {
      menupopup = this._createMenupopup(chromeDoc);
      const parent =
        chromeDoc.querySelector("popupset") || chromeDoc.documentElement;
      parent.appendChild(menupopup);
    }

    return menupopup;
  }
}
