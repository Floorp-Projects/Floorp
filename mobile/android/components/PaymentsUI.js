/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/JNI.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

function paymentSuccess(aRequestId) {
  return function(aResult) {
    closePaymentTab(aRequestId, function() {
      cpmm.sendAsyncMessage("Payment:Success", { result: aResult,
                                    requestId: aRequestId });
    });
  }
}

function paymentFailed(aRequestId) {
  return function(aErrorMsg) {
    closePaymentTab(aRequestId, function() {
      cpmm.sendAsyncMessage("Payment:Failed", { errorMsg: aErrorMsg,
                                    requestId: aRequestId });
    });
  }
}

let paymentTabs = {};
let cancelTabCallbacks = {};
function paymentCanceled(aRequestId) {
  return function() {
    paymentFailed(aRequestId)();
  }
}
function closePaymentTab(aId, aCallback) {
  if (paymentTabs[aId]) {
    paymentTabs[aId].browser.removeEventListener("TabClose", cancelTabCallbacks[aId]);
    delete cancelTabCallbacks[aId];

    // We ask the UI to close the selected payment flow.
    let content = Services.wm.getMostRecentWindow("navigator:browser");
    if (content) {
      content.BrowserApp.closeTab(paymentTabs[aId]);
    }

    paymentTabs[aId] = null;
  }

  aCallback();
}

function PaymentUI() {
}

PaymentUI.prototype = {
  get bundle() {
    delete this.bundle;
    return this.bundle = Services.strings.createBundle("chrome://browser/locale/payments.properties");
  },

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

  _error: function(aCallback) {
    return function _error(id, msg) {
      if (aCallback) {
        aCallback.onresult(id, msg);
      }
    };
  },

  showPaymentFlow: function showPaymentFlow(aRequestId,
                                            aPaymentFlowInfo,
                                            aErrorCb) {
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
    let tab = content.BrowserApp.addTab(aPaymentFlowInfo.uri + aPaymentFlowInfo.jwt);

    // Inject paymentSuccess and paymentFailed methods into the document after its loaded.
    tab.browser.addEventListener("DOMWindowCreated", function loadPaymentShim() {
      let frame = tab.browser.contentDocument.defaultView;
      try {
        frame.wrappedJSObject.mozPaymentProvider = {
          __exposedProps__: {
            paymentSuccess: 'r',
            paymentFailed: 'r',
            mnc: 'r',
            mcc: 'r',
          },

          _getNetworkInfo: function(type) {
            let jni = new JNI();
            let cls = jni.findClass("org/mozilla/gecko/GeckoNetworkManager");
            let method = jni.getStaticMethodID(cls, "get" + type.toUpperCase(), "()I");
            let val = jni.callStaticIntMethod(cls, method);
            jni.close();

            if (val < 0)
              return null;
            return val;
          },

          get mnc() {
            delete this.mnc;
            return this.mnc = this._getNetworkInfo("mnc");
          },

          get mcc() {
            delete this.mcc;
            return this.mcc = this._getNetworkInfo("mcc");
          },

          paymentSuccess: paymentSuccess(aRequestId),
          paymentFailed: paymentFailed(aRequestId)
        };
      } catch (e) {
        _error(aRequestId, "ERROR_ADDING_METHODS");
      } finally {
        tab.browser.removeEventListener("DOMWindowCreated", loadPaymentShim);
      }
    }, true);

    // Store a reference to the tab so that we can close it when the payment succeeds or fails.
    paymentTabs[aRequestId] = tab;
    cancelTabCallbacks[aRequestId] = paymentCanceled(aRequestId);

    // Fail the payment if the tab is closed on its own
    tab.browser.addEventListener("TabClose", cancelTabCallbacks[aRequestId]);
  },

  cleanup: function cleanup() {
    // Nothing to do here.
  },

  classID: Components.ID("{3c6c9575-f57e-427b-a8aa-57bc3cbff48f}"), 
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPaymentUIGlue])
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PaymentUI]);
