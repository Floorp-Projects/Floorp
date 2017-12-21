/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Runs in the privileged outer dialog. Each dialog loads this script in its
 * own scope.
 */

"use strict";

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;
const paymentSrv = Cc["@mozilla.org/dom/payments/payment-request-service;1"]
                     .getService(Ci.nsIPaymentRequestService);

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

var PaymentDialog = {
  componentsLoaded: new Map(),
  frame: null,
  mm: null,
  request: null,

  init(requestId, frame) {
    if (!requestId || typeof(requestId) != "string") {
      throw new Error("Invalid PaymentRequest ID");
    }
    this.request = paymentSrv.getPaymentRequestById(requestId);

    if (!this.request) {
      throw new Error(`PaymentRequest not found: ${requestId}`);
    }

    this.frame = frame;
    this.mm = frame.frameLoader.messageManager;
    this.mm.addMessageListener("paymentContentToChrome", this);
    this.mm.loadFrameScript("chrome://payments/content/paymentDialogFrameScript.js", true);
    this.frame.src = "resource://payments/paymentRequest.xhtml";
  },

  createShowResponse({
    acceptStatus,
    methodName = "",
    methodData = null,
    payerName = "",
    payerEmail = "",
    payerPhone = "",
  }) {
    let showResponse = this.createComponentInstance(Ci.nsIPaymentShowActionResponse);

    showResponse.init(this.request.requestId,
                      acceptStatus,
                      methodName,
                      methodData,
                      payerName,
                      payerEmail,
                      payerPhone);
    return showResponse;
  },

  createBasicCardResponseData({
    cardholderName = "",
    cardNumber,
    expiryMonth = "",
    expiryYear = "",
    cardSecurityCode = "",
    billingAddress = null,
  }) {
    const basicCardResponseData = Cc["@mozilla.org/dom/payments/basiccard-response-data;1"]
                                  .createInstance(Ci.nsIBasicCardResponseData);
    basicCardResponseData.initData(cardholderName,
                                   cardNumber,
                                   expiryMonth,
                                   expiryYear,
                                   cardSecurityCode,
                                   billingAddress);
    return basicCardResponseData;
  },

  createPaymentAddress({
    country = "",
    addressLines = [],
    region = "",
    city = "",
    dependentLocality = "",
    postalCode = "",
    sortingCode = "",
    languageCode = "",
    organization = "",
    recipient = "",
    phone = "",
  }) {
    const billingAddress = Cc["@mozilla.org/dom/payments/payment-address;1"]
                           .createInstance(Ci.nsIPaymentAddress);
    const addressLine = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    for (let line of addressLines) {
      const address = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
      address.data = line;
      addressLine.appendElement(address);
    }
    billingAddress.init(country,
                        addressLine,
                        region,
                        city,
                        dependentLocality,
                        postalCode,
                        sortingCode,
                        languageCode,
                        organization,
                        recipient,
                        phone);
    return billingAddress;
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

  onPaymentCancel() {
    const showResponse = this.createShowResponse({
      acceptStatus: Ci.nsIPaymentActionResponse.PAYMENT_REJECTED,
    });
    paymentSrv.respondPayment(showResponse);
    window.close();
  },

  pay({
    payerName,
    payerEmail,
    payerPhone,
    methodName,
    methodData,
  }) {
    let basicCardData = this.createBasicCardResponseData(methodData);
    const showResponse = this.createShowResponse({
      acceptStatus: Ci.nsIPaymentActionResponse.PAYMENT_ACCEPTED,
      payerName,
      payerEmail,
      payerPhone,
      methodName,
      methodData: basicCardData,
    });
    paymentSrv.respondPayment(showResponse);
  },

  receiveMessage({data}) {
    let {messageType} = data;

    switch (messageType) {
      case "initializeRequest": {
        let requestSerialized = JSON.parse(JSON.stringify(this.request));

        // Manually serialize the nsIPrincipal.
        let displayHost = this.request.topLevelPrincipal.URI.displayHost;
        requestSerialized.topLevelPrincipal = {
          URI: {
            displayHost,
          },
        };

        this.mm.sendAsyncMessage("paymentChromeToContent", {
          messageType: "showPaymentRequest",
          data: {
            request: requestSerialized,
          },
        });
        break;
      }
      case "paymentCancel": {
        this.onPaymentCancel();
        break;
      }
      case "pay": {
        this.pay(data);
        break;
      }
    }
  },
};

if ("document" in this) {
  // Running in a browser, not a unit test
  let frame = document.getElementById("paymentRequestFrame");
  let requestId = (new URLSearchParams(window.location.search)).get("requestId");
  PaymentDialog.init(requestId, frame);
}
