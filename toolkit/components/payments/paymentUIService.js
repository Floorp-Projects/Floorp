/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;
// eslint-disable-next-line no-unused-vars
const DIALOG_URL = "chrome://payments/content/paymentRequest.xhtml";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function PaymentUIService() {}

PaymentUIService.prototype = {
  classID: Components.ID("{01f8bd55-9017-438b-85ec-7c15d2b35cdc}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPaymentUIService]),

  canMakePayment(requestId) {
    let canMakeResponse = Cc["@mozilla.org/dom/payments/payment-canmake-action-response;1"]
                            .createInstance(Ci.nsIPaymentCanMakeActionResponse);
    canMakeResponse.init(requestId, false);
    return canMakeResponse.QueryInterface(Ci.nsIPaymentActionResponse);
  },

  showPayment(requestId) {
    return null;
  },

  abortPayment(requestId) {
    let abortResponse = Cc["@mozilla.org/dom/payments/payment-abort-action-response;1"]
                          .createInstance(Ci.nsIPaymentAbortActionResponse);
    abortResponse.init(requestId, Ci.nsIPaymentActionResponse.ABORT_SUCCEEDED);
    return abortResponse.QueryInterface(Ci.nsIPaymentActionResponse);
  },

  completePayment(requestId) {
    return null;
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PaymentUIService]);
