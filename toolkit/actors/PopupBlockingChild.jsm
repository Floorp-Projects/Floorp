/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-unused-vars: ["error", {args: "none"}] */

var EXPORTED_SYMBOLS = ["PopupBlockingChild"];

// The maximum number of popup information we'll send to the parent.
const MAX_SENT_POPUPS = 15;

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

class PopupBlockingChild extends JSWindowActorChild {
  constructor() {
    super();
    this.weakDocStates = new WeakMap();
  }

  actorCreated() {
    this.contentWindow.addEventListener("pageshow", this);
  }

  willDestroy() {
    this.contentWindow.removeEventListener("pageshow", this);
  }

  /**
   * Returns the state for the current document referred to via
   * this.document. If no such state exists, creates it, stores it
   * and returns it.
   */
  get docState() {
    let state = this.weakDocStates.get(this.document);
    if (!state) {
      state = {
        popupData: [],
      };
      this.weakDocStates.set(this.document, state);
    }

    return state;
  }

  receiveMessage(msg) {
    switch (msg.name) {
      case "UnblockPopup": {
        let i = msg.data.index;
        let state = this.docState;
        let popupData = state.popupData[i];
        if (popupData) {
          let dwi = popupData.requestingWindow;

          // If we have a requesting window and the requesting document is
          // still the current document, open the popup.
          if (dwi && dwi.document == popupData.requestingDocument) {
            dwi.open(
              popupData.popupWindowURISpec,
              popupData.popupWindowName,
              popupData.popupWindowFeatures
            );
          }
        }
        break;
      }

      case "GetBlockedPopupList": {
        let state = this.docState;
        let length = Math.min(state.popupData.length, MAX_SENT_POPUPS);

        let result = [];

        for (let i = 0; i < length; ++i) {
          let popup = state.popupData[i];

          let popupWindowURISpec = popup.popupWindowURISpec;

          if (this.contentWindow.location.href == popupWindowURISpec) {
            popupWindowURISpec = "<self>";
          } else {
            // Limit 500 chars to be sent because the URI will be cropped
            // by the UI anyway, and data: URIs can be significantly larger.
            popupWindowURISpec = popupWindowURISpec.substring(0, 500);
          }

          result.push({
            popupWindowURISpec,
          });
        }

        return result;
      }
    }

    return null;
  }

  handleEvent(event) {
    switch (event.type) {
      case "DOMPopupBlocked":
        this.onPopupBlocked(event);
        break;
      case "pageshow": {
        this.onPageShow(event);
        break;
      }
    }
  }

  onPopupBlocked(event) {
    if (event.target != this.document) {
      return;
    }

    let state = this.docState;

    // Avoid spamming the parent process with too many blocked popups.
    if (state.popupData.length >= PopupBlockingChild.maxReportedPopups) {
      return;
    }

    let popup = {
      popupWindowURISpec: event.popupWindowURI
        ? event.popupWindowURI.spec
        : "about:blank",
      popupWindowFeatures: event.popupWindowFeatures,
      popupWindowName: event.popupWindowName,
      requestingWindow: event.requestingWindow,
      requestingDocument: event.requestingWindow.document,
    };

    state.popupData.push(popup);
    this.updateBlockedPopups(true);
  }

  onPageShow(event) {
    if (event.target != this.document) {
      return;
    }

    this.updateBlockedPopups(false);
  }

  updateBlockedPopups(shouldNotify) {
    this.sendAsyncMessage("UpdateBlockedPopups", {
      shouldNotify,
      count: this.docState.popupData.length,
    });
  }
}

XPCOMUtils.defineLazyPreferenceGetter(
  PopupBlockingChild,
  "maxReportedPopups",
  "privacy.popups.maxReported"
);
