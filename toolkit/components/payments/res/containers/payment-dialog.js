/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global PaymentStateSubscriberMixin, paymentRequest */

"use strict";

/**
 * <payment-dialog></payment-dialog>
 */

class PaymentDialog extends PaymentStateSubscriberMixin(HTMLElement) {
  constructor() {
    super();
    this._template = document.getElementById("payment-dialog-template");
    this._cachedState = {};
  }

  connectedCallback() {
    let contents = document.importNode(this._template.content, true);
    this._hostNameEl = contents.querySelector("#host-name");

    this._cancelButton = contents.querySelector("#cancel");
    this._cancelButton.addEventListener("click", this.cancelRequest);

    this._payButton = contents.querySelector("#pay");
    this._payButton.addEventListener("click", this);

    this._viewAllButton = contents.querySelector("#view-all");
    this._viewAllButton.addEventListener("click", this);

    this._orderDetailsOverlay = contents.querySelector("#order-details-overlay");

    this.appendChild(contents);

    super.connectedCallback();
  }

  disconnectedCallback() {
    this._cancelButton.removeEventListener("click", this.cancelRequest);
    this._payButton.removeEventListener("click", this.pay);
    this._viewAllButton.removeEventListener("click", this);
    super.disconnectedCallback();
  }

  handleEvent(event) {
    if (event.type == "click") {
      switch (event.target) {
        case this._viewAllButton:
          let orderDetailsShowing = !this.requestStore.getState().orderDetailsShowing;
          this.requestStore.setState({ orderDetailsShowing });
          break;
        case this._payButton:
          this.pay();
          break;
      }
    }
  }

  cancelRequest() {
    paymentRequest.cancel();
  }

  pay() {
    let {
      selectedPaymentCard,
      selectedPaymentCardSecurityCode,
    } = this.requestStore.getState();

    paymentRequest.pay({
      selectedPaymentCardGUID: selectedPaymentCard,
      selectedPaymentCardSecurityCode,
    });
  }

  changeShippingAddress(shippingAddressGUID) {
    paymentRequest.changeShippingAddress({
      shippingAddressGUID,
    });
  }

  /**
   * Set some state from the privileged parent process.
   * Other elements that need to set state should use their own `this.requestStore.setState`
   * method provided by the `PaymentStateSubscriberMixin`.
   *
   * @param {object} state - See `PaymentsStore.setState`
   */
  setStateFromParent(state) {
    this.requestStore.setState(state);

    // Check if any foreign-key constraints were invalidated.
    let {
      savedAddresses,
      savedBasicCards,
      selectedPaymentCard,
      selectedShippingAddress,
    } = this.requestStore.getState();

    // Ensure `selectedShippingAddress` never refers to a deleted address and refers
    // to an address if one exists.
    if (!savedAddresses[selectedShippingAddress]) {
      this.requestStore.setState({
        selectedShippingAddress: Object.keys(savedAddresses)[0] || null,
      });
    }

    // Ensure `selectedPaymentCard` never refers to a deleted payment card and refers
    // to a payment card if one exists.
    if (!savedBasicCards[selectedPaymentCard]) {
      this.requestStore.setState({
        selectedPaymentCard: Object.keys(savedBasicCards)[0] || null,
        selectedPaymentCardSecurityCode: null,
      });
    }
  }

  stateChangeCallback(state) {
    super.stateChangeCallback(state);

    if (state.selectedShippingAddress != this._cachedState.selectedShippingAddress) {
      this.changeShippingAddress(state.selectedShippingAddress);
    }

    this._cachedState.selectedShippingAddress = state.selectedShippingAddress;
  }

  render(state) {
    let request = state.request;
    this._hostNameEl.textContent = request.topLevelPrincipal.URI.displayHost;

    let totalItem = request.paymentDetails.totalItem;
    let totalAmountEl = this.querySelector("#total > currency-amount");
    totalAmountEl.value = totalItem.amount.value;
    totalAmountEl.currency = totalItem.amount.currency;

    this._orderDetailsOverlay.hidden = !state.orderDetailsShowing;
  }
}

customElements.define("payment-dialog", PaymentDialog);
