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
    this.defineLazyLogGetter(this, "frameScript");
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
    let {messageType, requestId} = detail;
    this.log.debug(`received message from content: ${messageType} ... ${requestId}`);
    this.sendMessageToChrome(messageType, {
      requestId,
    });
  },

  defineLazyLogGetter(scope, logPrefix) {
    XPCOMUtils.defineLazyGetter(scope, "log", () => {
      let {ConsoleAPI} = Cu.import("resource://gre/modules/Console.jsm", {});
      return new ConsoleAPI({
        maxLogLevelPref: "dom.payments.loglevel",
        prefix: logPrefix,
      });
    });
  },

  sendToContent(messageType, detail = {}) {
    this.log.debug(`sendToContent (${messageType})`);
    let response = Object.assign({messageType}, detail);
    let event = new content.document.defaultView.CustomEvent("paymentChromeToContent", {
      bubbles: true,
      detail: Cu.cloneInto(response, content.document.defaultView),
    });
    content.document.dispatchEvent(event);
  },

  sendMessageToChrome(messageType, detail = {}) {
    sendAsyncMessage("paymentContentToChrome", Object.assign(detail, {messageType}));
  },
};

PaymentFrameScript.init();
