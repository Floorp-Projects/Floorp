/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;
const paymentSrv = Cc["@mozilla.org/dom/payments/payment-request-service;1"]
                     .getService(Ci.nsIPaymentRequestService);

let PaymentDialog = {
  componentsLoaded: new Map(),
  frame: null,
  mm: null,

  init(frame) {
    this.frame = frame;
    this.mm = frame.frameLoader.messageManager;
    this.mm.addMessageListener("paymentContentToChrome", this);
    this.mm.loadFrameScript("chrome://payments/content/paymentDialogFrameScript.js", true);
  },

  createShowResponse({requestId, acceptStatus, methodName = "", data = null,
                      payerName = "", payerEmail = "", payerPhone = ""}) {
    let showResponse = this.createComponentInstance(Ci.nsIPaymentShowActionResponse);
    let methodData = this.createComponentInstance(Ci.nsIGeneralResponseData);

    showResponse.init(requestId,
                      acceptStatus,
                      methodName,
                      methodData,
                      payerName,
                      payerEmail,
                      payerPhone);
    return showResponse;
  },

  createComponentInstance(componentInterface) {
    let componentName;
    switch (componentInterface) {
      case Ci.nsIPaymentShowActionResponse: {
        componentName = "@mozilla.org/dom/payments/payment-show-action-response;1";
        break;
      }
      case Ci.nsIGeneralResponseData: {
        componentName = "@mozilla.org/dom/payments/general-response-data;1";
        break;
      }
    }
    let component = this.componentsLoaded.get(componentName);

    if (!component) {
      component = Cc[componentName];
      this.componentsLoaded.set(componentName, component);
    }

    return component.createInstance(componentInterface);
  },

  onPaymentCancel(requestId) {
    const showResponse = this.createShowResponse({
      requestId,
      acceptStatus: Ci.nsIPaymentActionResponse.PAYMENT_REJECTED,
    });
    paymentSrv.respondPayment(showResponse);
    window.close();
  },

  receiveMessage({data}) {
    let {messageType, requestId} = data;

    switch (messageType) {
      case "initializeRequest": {
        this.mm.sendAsyncMessage("paymentChromeToContent", {
          messageType: "showPaymentRequest",
          data: window.arguments[0],
        });
        break;
      }
      case "paymentCancel": {
        this.onPaymentCancel(requestId);
        break;
      }
    }
  },
};

let frame = document.getElementById("paymentRequestFrame");
PaymentDialog.init(frame);
