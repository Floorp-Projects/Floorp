/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;
// eslint-disable-next-line no-unused-vars
const DIALOG_URL = "chrome://payments/content/paymentDialog.xhtml";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this,
                                   "paymentSrv",
                                   "@mozilla.org/dom/payments/payment-request-service;1",
                                   "nsIPaymentRequestService");

function defineLazyLogGetter(scope, logPrefix) {
  XPCOMUtils.defineLazyGetter(scope, "log", () => {
    let {ConsoleAPI} = Cu.import("resource://gre/modules/Console.jsm", {});
    return new ConsoleAPI({
      maxLogLevelPref: "dom.payments.loglevel",
      prefix: logPrefix,
    });
  });
}

function PaymentUIService() {
  this.wrappedJSObject = this;
  defineLazyLogGetter(this, "Payment UI Service");
  this.log.debug("constructor");
}

PaymentUIService.prototype = {
  classID: Components.ID("{01f8bd55-9017-438b-85ec-7c15d2b35cdc}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPaymentUIService]),
  REQUEST_ID_PREFIX: "paymentRequest-",

  showPayment(requestId) {
    this.log.debug("showPayment");
    let chromeWindow = Services.wm.getMostRecentWindow("navigator:browser");
    chromeWindow.openDialog(DIALOG_URL,
                            `${this.REQUEST_ID_PREFIX}${requestId}`,
                            "modal,dialog,centerscreen",
                            {requestId});
  },

  abortPayment(requestId) {
    this.log.debug(`abortPayment: ${requestId}`);
    let abortResponse = Cc["@mozilla.org/dom/payments/payment-abort-action-response;1"]
                          .createInstance(Ci.nsIPaymentAbortActionResponse);

    let enu = Services.wm.getEnumerator(null);
    let win;
    while ((win = enu.getNext())) {
      if (win.name == `${this.REQUEST_ID_PREFIX}${requestId}`) {
        this.log.debug(`closing: ${win.name}`);
        win.close();
        break;
      }
    }

    // if `win` is falsy, then we haven't found the dialog, so the abort fails
    // otherwise, the abort is successful
    let response = win ?
      Ci.nsIPaymentActionResponse.ABORT_SUCCEEDED :
      Ci.nsIPaymentActionResponse.ABORT_FAILED;

    abortResponse.init(requestId, response);
    paymentSrv.respondPayment(abortResponse);
  },

  completePayment(requestId) {
    let completeResponse = Cc["@mozilla.org/dom/payments/payment-complete-action-response;1"]
                             .createInstance(Ci.nsIPaymentCompleteActionResponse);
    completeResponse.init(requestId, Ci.nsIPaymentActionResponse.COMPLTETE_SUCCEEDED);
    paymentSrv.respondPayment(completeResponse.QueryInterface(Ci.nsIPaymentActionResponse));
  },

  updatePayment(requestId) {
  },

  // other helper methods

  requestIdForWindow(window) {
    let windowName = window.name;

    return windowName.startsWith(this.REQUEST_ID_PREFIX) ?
      windowName.replace(this.REQUEST_ID_PREFIX, "") : // returns suffix, which is the requestId
      null;
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PaymentUIService]);
