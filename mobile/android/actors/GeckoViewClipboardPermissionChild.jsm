/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { GeckoViewActorChild } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewActorChild.jsm"
);

const EXPORTED_SYMBOLS = ["GeckoViewClipboardPermissionChild"];

class GeckoViewClipboardPermissionChild extends GeckoViewActorChild {
  constructor() {
    super();
    this._pendingPromise = null;
  }

  async promptPermissionForClipboardRead() {
    const uri = this.contentWindow.location.href;

    const { x, y } = await this.sendQuery(
      "ClipboardReadTextPaste:GetLastPointerLocation"
    );

    const promise = this.eventDispatcher.sendRequestForResult({
      type: "GeckoView:ClipboardPermissionRequest",
      uri,
      screenPoint: {
        x,
        y,
      },
    });

    this._pendingPromise = promise;

    try {
      const allowOrDeny = await promise;
      if (this._pendingPromise !== promise) {
        // Current pending promise is newer. So it means that this promise
        // is already resolved or rejected. Do nothing.
        return;
      }
      this.contentWindow.navigator.clipboard.onUserReactedToPasteMenuPopup(
        allowOrDeny
      );
      this._pendingPromise = null;
    } catch (error) {
      debug`Permission error: ${error}`;

      if (this._pendingPromise !== promise) {
        // Current pending promise is newer. So it means that this promise
        // is already resolved or rejected. Do nothing.
        return;
      }

      this.contentWindow.navigator.clipboard.onUserReactedToPasteMenuPopup(
        false
      );
      this._pendingPromise = null;
    }
  }

  handleEvent(aEvent) {
    debug`handleEvent: ${aEvent.type}`;

    switch (aEvent.type) {
      case "MozClipboardReadPaste":
        if (aEvent.isTrusted) {
          this.promptPermissionForClipboardRead();
        }
        break;

      // page hide or deactivate cancel clipboard permission.
      case "pagehide":
      // fallthrough for the next three events.
      case "deactivate":
      case "mousedown":
      case "mozvisualscroll":
        // Gecko desktop uses XUL popup to show clipboard permission prompt.
        // So it will be closed automatically by scroll and other user
        // activation. So GeckoView has to close permission prompt by some user
        // activations, too.

        this.eventDispatcher.sendRequest({
          type: "GeckoView:DismissClipboardPermissionRequest",
        });
        if (this._pendingPromise) {
          this.contentWindow.navigator.clipboard.onUserReactedToPasteMenuPopup(
            false
          );
          this._pendingPromise = null;
        }
        break;
    }
  }
}

const { debug, warn } = GeckoViewClipboardPermissionChild.initLogging(
  "GeckoViewClipboardPermissionChild"
);
