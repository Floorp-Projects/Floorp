/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-unused-vars: ["error", {args: "none"}] */

var EXPORTED_SYMBOLS = ["PopupBlocking"];

class PopupBlocking {
  constructor(mm) {
    this.mm = mm;

    this.popupData = null;
    this.popupDataInternal = null;

    mm.addEventListener("pageshow", this, true);
    mm.addEventListener("pagehide", this, true);

    mm.addMessageListener("PopupBlocking:UnblockPopup", this);
    mm.addMessageListener("PopupBlocking:GetBlockedPopupList", this);
  }

  receiveMessage(msg) {
    switch (msg.name) {
      case "PopupBlocking:UnblockPopup": {
        let i = msg.data.index;
        if (this.popupData && this.popupData[i]) {
          let data = this.popupData[i];
          let internals = this.popupDataInternal[i];
          let dwi = internals.requestingWindow;

          // If we have a requesting window and the requesting document is
          // still the current document, open the popup.
          if (dwi && dwi.document == internals.requestingDocument) {
            dwi.open(data.popupWindowURIspec, data.popupWindowName, data.popupWindowFeatures);
          }
        }
        break;
      }

      case "PopupBlocking:GetBlockedPopupList": {
        let popupData = [];
        let length = this.popupData ? this.popupData.length : 0;

        // Limit 15 popup URLs to be reported through the UI
        length = Math.min(length, 15);

        for (let i = 0; i < length; i++) {
          let popupWindowURIspec = this.popupData[i].popupWindowURIspec;

          if (popupWindowURIspec == this.mm.content.location.href) {
            popupWindowURIspec = "<self>";
          } else {
            // Limit 500 chars to be sent because the URI will be cropped
            // by the UI anyway, and data: URIs can be significantly larger.
            popupWindowURIspec = popupWindowURIspec.substring(0, 500);
          }

          popupData.push({popupWindowURIspec});
        }

        this.mm.sendAsyncMessage("PopupBlocking:ReplyGetBlockedPopupList", {popupData});
        break;
      }
    }
  }

  handleEvent(ev) {
    switch (ev.type) {
      case "DOMPopupBlocked":
        return this.onPopupBlocked(ev);
      case "pageshow":
        return this._removeIrrelevantPopupData();
      case "pagehide":
        return this._removeIrrelevantPopupData(ev.target);
    }
    return undefined;
  }

  onPopupBlocked(ev) {
    if (!this.popupData) {
      this.popupData = [];
      this.popupDataInternal = [];
    }

    let obj = {
      popupWindowURIspec: ev.popupWindowURI ? ev.popupWindowURI.spec : "about:blank",
      popupWindowFeatures: ev.popupWindowFeatures,
      popupWindowName: ev.popupWindowName
    };

    let internals = {
      requestingWindow: ev.requestingWindow,
      requestingDocument: ev.requestingWindow.document,
    };

    this.popupData.push(obj);
    this.popupDataInternal.push(internals);
    this.updateBlockedPopups(true);
  }

  _removeIrrelevantPopupData(removedDoc = null) {
    if (this.popupData) {
      let i = 0;
      let oldLength = this.popupData.length;
      while (i < this.popupData.length) {
        let {requestingWindow, requestingDocument} = this.popupDataInternal[i];
        // Filter out irrelevant reports.
        if (requestingWindow && requestingWindow.document == requestingDocument &&
            requestingDocument != removedDoc) {
          i++;
        } else {
          this.popupData.splice(i, 1);
          this.popupDataInternal.splice(i, 1);
        }
      }
      if (this.popupData.length == 0) {
        this.popupData = null;
        this.popupDataInternal = null;
      }
      if (!this.popupData || oldLength > this.popupData.length) {
        this.updateBlockedPopups(false);
      }
    }
  }

  updateBlockedPopups(freshPopup) {
    this.mm.sendAsyncMessage("PopupBlocking:UpdateBlockedPopups",
      {
        count: this.popupData ? this.popupData.length : 0,
        freshPopup
      });
  }
}
