/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BrowserUtils: "resource://gre/modules/BrowserUtils.sys.mjs",
});

export class FindBarChild extends JSWindowActorChild {
  constructor() {
    super();

    this._findKey = null;

    this.inQuickFind = false;
    this.inPassThrough = false;

    ChromeUtils.defineLazyGetter(this, "FindBarContent", () => {
      const { FindBarContent } = ChromeUtils.importESModule(
        "resource://gre/modules/FindBarContent.sys.mjs"
      );

      let findBarContent = new FindBarContent(this);

      Object.defineProperties(this, {
        inQuickFind: {
          get() {
            return findBarContent.inQuickFind;
          },
        },
        inPassThrough: {
          get() {
            return findBarContent.inPassThrough;
          },
        },
      });

      return findBarContent;
    });
  }

  receiveMessage(msg) {
    if (msg.name == "Findbar:UpdateState") {
      this.FindBarContent.updateState(msg.data);
    }
  }

  /**
   * Check whether this key event will start the findbar in the parent,
   * in which case we should pass any further key events to the parent to avoid
   * them being lost.
   * @param aEvent the key event to check.
   */
  eventMatchesFindShortcut(aEvent) {
    if (!this._findKey) {
      this._findKey = Services.cpmm.sharedData.get("Findbar:Shortcut");
      if (!this._findKey) {
        return false;
      }
    }
    for (let k in this._findKey) {
      if (this._findKey[k] != aEvent[k]) {
        return false;
      }
    }
    return true;
  }

  handleEvent(event) {
    if (event.type == "keypress") {
      this.onKeypress(event);
    }
  }

  onKeypress(event) {
    if (!this.inPassThrough && this.eventMatchesFindShortcut(event)) {
      return this.FindBarContent.start(event);
    }

    // disable FAYT in about:blank to prevent FAYT opening unexpectedly.
    let location = this.document.location.href;
    if (location == "about:blank") {
      return null;
    }

    if (
      event.ctrlKey ||
      event.altKey ||
      event.metaKey ||
      event.defaultPrevented ||
      !lazy.BrowserUtils.mimeTypeIsTextBased(this.document.contentType) ||
      !lazy.BrowserUtils.canFindInPage(location)
    ) {
      return null;
    }

    if (this.inPassThrough || this.inQuickFind) {
      return this.FindBarContent.onKeypress(event);
    }

    if (event.charCode && this.shouldFastFind(event.target)) {
      let key = String.fromCharCode(event.charCode);
      if ((key == "/" || key == "'") && FindBarChild.manualFAYT) {
        return this.FindBarContent.startQuickFind(event);
      }
      if (key != " " && FindBarChild.findAsYouType) {
        return this.FindBarContent.startQuickFind(event, true);
      }
    }
    return null;
  }

  /**
   * Return true if we should FAYT for this node:
   *
   * @param elt
   *        The element that is focused
   */
  shouldFastFind(elt) {
    if (elt) {
      let win = elt.ownerGlobal;
      if (win.HTMLInputElement.isInstance(elt) && elt.mozIsTextField(false)) {
        return false;
      }

      if (elt.isContentEditable || win.document.designMode == "on") {
        return false;
      }

      if (
        win.HTMLTextAreaElement.isInstance(elt) ||
        win.HTMLSelectElement.isInstance(elt) ||
        win.HTMLObjectElement.isInstance(elt) ||
        win.HTMLEmbedElement.isInstance(elt)
      ) {
        return false;
      }

      if (
        (win.HTMLIFrameElement.isInstance(elt) && elt.mozbrowser) ||
        win.XULFrameElement.isInstance(elt)
      ) {
        // If we're targeting a mozbrowser iframe or an embedded XULFrameElement
        // (e.g. about:addons extensions inline options page), do not activate
        // fast find.
        return false;
      }
    }

    return true;
  }
}

XPCOMUtils.defineLazyPreferenceGetter(
  FindBarChild,
  "findAsYouType",
  "accessibility.typeaheadfind"
);
XPCOMUtils.defineLazyPreferenceGetter(
  FindBarChild,
  "manualFAYT",
  "accessibility.typeaheadfind.manual"
);
