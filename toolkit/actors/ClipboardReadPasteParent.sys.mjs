/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const kMenuPopupId = "clipboardReadPasteMenuPopup";

// Exchanges messages with the child actor and handles events from the
// pasteMenuHandler.
export class ClipboardReadPasteParent extends JSWindowActorParent {
  constructor() {
    super();

    this._menupopup = null;
    this._menuitem = null;
    this._delayTimer = null;
    this._pasteMenuItemClicked = false;
    this._lastBeepTime = 0;
  }

  didDestroy() {
    if (this._menupopup) {
      this._menupopup.hidePopup(true);
    }
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
      case "keydown": {
        this.onKeyDown(aEvent);
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
    this._clearDelayTimer();
    this._stopWatchingForSpammyActivation();

    if (this._pasteMenuItemClicked) {
      // A message was already sent. Reset the state to handle further
      // click/dismiss events properly.
      this._pasteMenuItemClicked = false;
    } else {
      this.sendAsyncMessage("ClipboardReadPaste:PasteMenuItemDismissed");
    }
  }

  onKeyDown(aEvent) {
    if (!this._menuitem.disabled) {
      return;
    }

    let accesskey = this._menuitem.getAttribute("accesskey");
    if (
      aEvent.key == accesskey.toLowerCase() ||
      aEvent.key == accesskey.toUpperCase()
    ) {
      if (Date.now() - this._lastBeepTime > 1000) {
        Cc["@mozilla.org/sound;1"].getService(Ci.nsISound).beep();
        this._lastBeepTime = Date.now();
      }
      this._refreshDelayTimer();
    }
  }

  // For JSWindowActorParent.
  receiveMessage(value) {
    if (value.name == "ClipboardReadPaste:ShowMenupopup") {
      if (!this._menupopup) {
        this._menupopup = this._getMenupopup();
        this._menuitem = this._menupopup.firstElementChild;
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

      this._menuitem.disabled = true;
      this._startWatchingForSpammyActivation();
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

      this._refreshDelayTimer();
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
    aChromeDoc.l10n.setAttributes(menuitem, "text-action-paste");

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

  _startWatchingForSpammyActivation() {
    let doc = this._menuitem.ownerDocument;
    Services.els.addSystemEventListener(doc, "keydown", this, true);
  }

  _stopWatchingForSpammyActivation() {
    let doc = this._menuitem.ownerDocument;
    Services.els.removeSystemEventListener(doc, "keydown", this, true);
  }

  _clearDelayTimer() {
    if (this._delayTimer) {
      let window = this._menuitem.ownerGlobal;
      window.clearTimeout(this._delayTimer);
      this._delayTimer = null;
    }
  }

  _refreshDelayTimer() {
    this._clearDelayTimer();

    let window = this._menuitem.ownerGlobal;
    let delay = Services.prefs.getIntPref("security.dialog_enable_delay");
    this._delayTimer = window.setTimeout(() => {
      this._menuitem.disabled = false;
      this._stopWatchingForSpammyActivation();
      this._delayTimer = null;
    }, delay);
  }
}
