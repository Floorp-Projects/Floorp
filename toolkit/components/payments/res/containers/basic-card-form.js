/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../../../../browser/extensions/formautofill/content/autofillEditForms.js*/
import PaymentStateSubscriberMixin from "../mixins/PaymentStateSubscriberMixin.js";
/* import-globals-from ../unprivileged-fallbacks.js */
/* import-globals-from ../paymentRequest.js */

/**
 * <basic-card-form></basic-card-form>
 *
 * XXX: Bug 1446164 - This form isn't localized when used via this custom element
 * as it will be much easier to share the logic once we switch to Fluent.
 */

export default class BasicCardForm extends PaymentStateSubscriberMixin(HTMLElement) {
  constructor() {
    super();

    this.genericErrorText = document.createElement("div");

    this.backButton = document.createElement("button");
    this.backButton.addEventListener("click", this);

    this.saveButton = document.createElement("button");
    this.saveButton.addEventListener("click", this);

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
      let addresses = [];
      this.formHandler = new EditCreditCard({
        form,
      }, record, addresses, {
        isCCNumber: PaymentDialogUtils.isCCNumber,
        getAddressLabel: PaymentDialogUtils.getAddressLabel,
      });

      this.appendChild(this.genericErrorText);
      this.appendChild(this.backButton);
      this.appendChild(this.saveButton);
      // Only call the connected super callback(s) once our markup is fully
      // connected, including the shared form fetched asynchronously.
      super.connectedCallback();
    });
  }

  render(state) {
    this.backButton.textContent = this.dataset.backButtonLabel;
    this.saveButton.textContent = this.dataset.saveButtonLabel;

    let record = {};
    let {
      page,
      savedAddresses,
      savedBasicCards,
    } = state;

    this.genericErrorText.textContent = page.error;

    let editing = !!page.guid;
    this.form.querySelector("#cc-number").disabled = editing;

    // If a card is selected we want to edit it.
    if (editing) {
      record = savedBasicCards[page.guid];
      if (!record) {
        throw new Error("Trying to edit a non-existing card: " + page.guid);
      }
    }

    this.formHandler.loadRecord(record, savedAddresses);
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
    switch (evt.target) {
      case this.backButton: {
        this.requestStore.setState({
          page: {
            id: "payment-summary",
          },
        });
        break;
      }
      case this.saveButton: {
        this.saveRecord();
        break;
      }
      default: {
        throw new Error("Unexpected click target");
      }
    }
  }

  saveRecord() {
    let record = this.formHandler.buildFormObject();
    let {
      page,
    } = this.requestStore.getState();

    for (let editableFieldName of ["cc-name", "cc-exp-month", "cc-exp-year"]) {
      record[editableFieldName] = record[editableFieldName] || "";
    }

    // Only save the card number if we're saving a new record, otherwise we'd
    // overwrite the unmasked card number with the masked one.
    if (!page.guid) {
      record["cc-number"] = record["cc-number"] || "";
    }

    paymentRequest.updateAutofillRecord("creditCards", record, page.guid, {
      errorStateChange: {
        page: {
          id: "basic-card-page",
          error: this.dataset.errorGenericSave,
        },
      },
      preserveOldProperties: true,
      selectedStateKey: "selectedPaymentCard",
      successStateChange: {
        page: {
          id: "payment-summary",
        },
      },
    });
  }
}

customElements.define("basic-card-form", BasicCardForm);
