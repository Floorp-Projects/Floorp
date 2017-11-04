/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This frame script only exists to mediate communications between the
 * unprivileged frame in a content process and the privileged dialog wrapper
 * in the UI process on the main thread.
 *
 * `paymentChromeToContent` messages from the privileged wrapper are converted
 * into DOM events of the same name.
 * `paymentContentToChrome` custom DOM events from the unprivileged frame are
 * converted into messages of the same name.
 *
 * Business logic should stay out of this shim.
 */

"use strict";

/* eslint-env mozilla/frame-script */

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

let PaymentFrameScript = {
  init() {
    XPCOMUtils.defineLazyGetter(this, "log", () => {
      let {ConsoleAPI} = Cu.import("resource://gre/modules/Console.jsm", {});
      return new ConsoleAPI({
        maxLogLevelPref: "dom.payments.loglevel",
        prefix: "paymentDialogFrameScript",
      });
    });

    addEventListener("paymentContentToChrome", this, false, true);

    addMessageListener("paymentChromeToContent", this);
  },

  handleEvent(event) {
    this.sendToChrome(event);
  },

  receiveMessage({data: {messageType, data}}) {
    this.sendToContent(messageType, data);
  },

  sendToChrome({detail}) {
    let {messageType} = detail;
    this.log.debug("sendToChrome:", messageType, detail);
    this.sendMessageToChrome(messageType, detail);
  },

  sendToContent(messageType, detail = {}) {
    this.log.debug("sendToContent", messageType, detail);
    let response = Object.assign({messageType}, detail);
    let event = new content.CustomEvent("paymentChromeToContent", {
      detail: Cu.cloneInto(response, content),
    });
    content.dispatchEvent(event);
  },

  sendMessageToChrome(messageType, data = {}) {
    sendAsyncMessage("paymentContentToChrome", Object.assign(data, {messageType}));
  },
};

PaymentFrameScript.init();
