/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import PaymentsStore from "../PaymentsStore.js";

/**
 * A mixin for a custom element to observe store changes to information about a payment request.
 */

/**
 * State of the payment request dialog.
 */
export let requestStore = new PaymentsStore({
  changesPrevented: false,
  completionState: "initial",
  orderDetailsShowing: false,
  page: {
    id: "payment-summary",
  },
  request: {
    tabId: null,
    topLevelPrincipal: {URI: {displayHost: null}},
    requestId: null,
    paymentMethods: [],
    paymentDetails: {
      id: null,
      totalItem: {label: null, amount: {currency: null, value: 0}},
      displayItems: [],
      shippingOptions: [],
      modifiers: null,
      error: "",
    },
    paymentOptions: {
      requestPayerName: false,
      requestPayerEmail: false,
      requestPayerPhone: false,
      requestShipping: false,
      shippingType: "shipping",
    },
    shippingOption: null,
  },
  selectedPayerAddress: null,
  selectedPaymentCard: null,
  selectedPaymentCardSecurityCode: null,
  selectedShippingAddress: null,
  selectedShippingOption: null,
  savedAddresses: {},
  savedBasicCards: {},
  tempBasicCards: {},
});


/**
 * A mixin to render UI based upon the requestStore and get updated when that store changes.
 *
 * Attaches `requestStore` to the element to give access to the store.
 * @param {class} superClass The class to extend
 * @returns {class}
 */
export default function PaymentStateSubscriberMixin(superClass) {
  return class PaymentStateSubscriber extends superClass {
    constructor() {
      super();
      this.requestStore = requestStore;
    }

    connectedCallback() {
      this.requestStore.subscribe(this);
      this.render(this.requestStore.getState());
      if (super.connectedCallback) {
        super.connectedCallback();
      }
    }

    disconnectedCallback() {
      this.requestStore.unsubscribe(this);
      if (super.disconnectedCallback) {
        super.disconnectedCallback();
      }
    }

    /**
     * Called by the store upon state changes.
     * @param {object} state The current state
     */
    stateChangeCallback(state) {
      this.render(state);
    }
  };
}
