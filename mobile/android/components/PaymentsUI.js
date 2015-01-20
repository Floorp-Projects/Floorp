/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/JNI.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

let paymentTabs = {};
let cancelTabCallbacks = {};

function PaymentUI() {
}

PaymentUI.prototype = {
  get bundle() {
    if (!this._bundle) {
      this._bundle = Services.strings.createBundle("chrome://browser/locale/payments.properties");
    }
    return this._bundle;
  },

  _error: function(aCallback) {
    return function _error(id, msg) {
      if (aCallback) {
        aCallback.onresult(id, msg);
      }
    };
  },

  // nsIPaymentUIGlue

  confirmPaymentRequest: function confirmPaymentRequest(aRequestId,
                                                        aRequests,
                                                        aSuccessCb,
                                                        aErrorCb) {
    let _error = this._error(aErrorCb);

    let listItems = [];

    // If there's only one payment provider that will work, just move on without prompting the user.
    if (aRequests.length == 1) {
      aSuccessCb.onresult(aRequestId, aRequests[0].type);
      return;
    }

    // Otherwise, let the user select a payment provider from a list.
    for (let i = 0; i < aRequests.length; i++) {
      let request = aRequests[i];
      let requestText = request.providerName;
      if (request.productPrice) {
        requestText += " (" + request.productPrice[0].amount + " " +
                              request.productPrice[0].currency + ")";
      }
      listItems.push({ label: requestText });
    }

    let p = new Prompt({
      window: null,
      title: this.bundle.GetStringFromName("payments.providerdialog.title"),
    }).setSingleChoiceItems(listItems).show(function(data) {
      if (data.button > -1 && aSuccessCb) {
        aSuccessCb.onresult(aRequestId, aRequests[data.button].type);
      } else {
        _error(aRequestId, "USER_CANCELED");
      }
    });
  },

  showPaymentFlow: function showPaymentFlow(aRequestId,
                                            aPaymentFlowInfo,
                                            aErrorCb) {
    function paymentCanceled(aErrorCb, aRequestId) {
      return function() {
        aErrorCb.onresult(aRequestId, "DIALOG_CLOSED_BY_USER");
      }
    }

    let _error = this._error(aErrorCb);

    // We ask the UI to browse to the selected payment flow.
    let content = Services.wm.getMostRecentWindow("navigator:browser");
    if (!content) {
      _error(aRequestId, "NO_CONTENT_WINDOW");
      return;
    }

    // TODO: For now, known payment providers (BlueVia and Mozilla Market)
    // only accepts the JWT by GET, so we just add it to the URI.
    // https://github.com/mozilla-b2g/gaia/blob/master/apps/system/js/payment.js
    let tab = content.BrowserApp
                     .addTab("chrome://browser/content/payment.xhtml");
    tab.browser.addEventListener("DOMContentLoaded", function onloaded() {
      tab.browser.removeEventListener("DOMContentLoaded", onloaded);
      let document = tab.browser.contentDocument;
      let frame = document.getElementById("payflow");
      frame.setAttribute("mozbrowser", true);
      let docshell = frame.contentWindow
                          .QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIWebNavigation)
                          .QueryInterface(Ci.nsIDocShell);
      docshell.paymentRequestId = aRequestId;
      frame.src = aPaymentFlowInfo.uri + aPaymentFlowInfo.jwt;
      document.title = aPaymentFlowInfo.description || aPaymentFlowInfo.name ||
                       frame.src;
    });
    paymentTabs[aRequestId] = tab;
    cancelTabCallbacks[aRequestId] = paymentCanceled(aErrorCb, aRequestId);

    // Fail the payment if the tab is closed on its own
    tab.browser.addEventListener("TabClose", cancelTabCallbacks[aRequestId]);
  },

  closePaymentFlow: function closePaymentFlow(aRequestId) {
    if (!paymentTabs[aRequestId]) {
      return Promise.reject();
    }

    let deferred = Promise.defer();

    paymentTabs[aRequestId].browser.removeEventListener(
      "TabClose",
      cancelTabCallbacks[aRequestId]
    );
    delete cancelTabCallbacks[aRequestId];

    // We ask the UI to close the selected payment flow.
    let content = Services.wm.getMostRecentWindow("navigator:browser");
    if (content) {
      content.BrowserApp.closeTab(paymentTabs[aRequestId]);
    }
    paymentTabs[aRequestId] = null;

    deferred.resolve();

    return deferred.promise;
  },

  classID: Components.ID("{3c6c9575-f57e-427b-a8aa-57bc3cbff48f}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPaymentUIGlue])
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PaymentUI]);
