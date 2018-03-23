/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../../../../browser/extensions/formautofill/content/autofillEditForms.js*/
/* import-globals-from ../mixins/PaymentStateSubscriberMixin.js */
/* import-globals-from ../unprivileged-fallbacks.js */

"use strict";

/**
 * <basic-card-form></basic-card-form>
 *
 * XXX: Bug 1446164 - This form isn't localized when used via this custom element
 * as it will be much easier to share the logic once we switch to Fluent.
 */

class BasicCardForm extends PaymentStateSubscriberMixin(HTMLElement) {
  constructor() {
    super();

    this.backButton = document.createElement("button");
    this.backButton.addEventListener("click", this);

    // The markup is shared with form autofill preferences.
    let url = "formautofill/editCreditCard.xhtml";
    this.promiseReady = this._fetchMarkup(url).then(doc => {
      this.form = doc.getElementById("form");
      return this.form;
    });
  }

  _fetchMarkup(url) {
    return new Promise((resolve, reject) => {
      let xhr = new XMLHttpRequest();
      xhr.responseType = "document";
      xhr.addEventListener("error", reject);
      xhr.addEventListener("load", evt => {
        resolve(xhr.response);
      });
      xhr.open("GET", url);
      xhr.send();
    });
  }

  connectedCallback() {
    this.promiseReady.then(form => {
      this.appendChild(form);

      let record = {};
      this.formHandler = new EditCreditCard({
        form,
      }, record, {
        isCCNumber: PaymentDialogUtils.isCCNumber,
      });

      this.appendChild(this.backButton);
      // Only call the connected super callback(s) once our markup is fully
      // connected, including the shared form fetched asynchronously.
      super.connectedCallback();
    });
  }

  render(state) {
    this.backButton.textContent = this.dataset.backButtonLabel;

    let record = {};
    let {
      selectedPaymentCard,
      savedBasicCards,
    } = state;

    let editing = !!state.selectedPaymentCard;
    this.form.querySelector("#cc-number").disabled = editing;

    // If a card is selected we want to edit it.
    if (editing) {
      record = savedBasicCards[selectedPaymentCard];
      if (!record) {
        throw new Error("Trying to edit a non-existing card: " + selectedPaymentCard);
      }
    }

    this.formHandler.loadRecord(record);
  }

  handleEvent(event) {
    switch (event.type) {
      case "click": {
        this.onClick(event);
        break;
      }
    }
  }

  onClick(evt) {
    this.requestStore.setState({
      page: {
        id: "payment-summary",
      },
    });
  }
}

customElements.define("basic-card-form", BasicCardForm);
