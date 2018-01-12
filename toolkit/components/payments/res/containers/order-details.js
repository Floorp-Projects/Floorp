/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global PaymentStateSubscriberMixin, PaymentRequest */

"use strict";

/**
 * <order-details></order-details>
 */

class OrderDetails extends PaymentStateSubscriberMixin(HTMLElement) {
  connectedCallback() {
    if (!this._contents) {
      let template = document.getElementById("order-details-template");
      let contents = this._contents = document.importNode(template.content, true);

      this._mainItemsList = contents.querySelector(".main-list");
      this._footerItemsList = contents.querySelector(".footer-items-list");
      this._totalAmount = contents.querySelector(".details-total > currency-amount");

      this.appendChild(this._contents);
    }
    super.connectedCallback();
  }

  get mainItemsList() {
    return this._mainItemsList;
  }

  get footerItemsList() {
    return this._footerItemsList;
  }

  get totalAmountElem() {
    return this._totalAmount;
  }

  static _emptyList(listEl) {
    while (listEl.lastChild) {
      listEl.removeChild(listEl.lastChild);
    }
  }

  static _populateList(listEl, items) {
    let fragment = document.createDocumentFragment();
    for (let item of items) {
      let row = document.createElement("payment-details-item");
      row.label = item.label;
      row.amountValue = item.amount.value;
      row.amountCurrency = item.amount.currency;
      fragment.appendChild(row);
    }
    listEl.appendChild(fragment);
    return listEl;
  }

  render(state) {
    let request = state.request;
    let totalItem = request.paymentDetails.totalItem;

    OrderDetails._emptyList(this.mainItemsList);
    OrderDetails._emptyList(this.footerItemsList);

    let mainItems = OrderDetails._getMainListItems(request);
    if (mainItems.length) {
      OrderDetails._populateList(this.mainItemsList, mainItems);
    }

    let footerItems = OrderDetails._getFooterListItems(request);
    if (footerItems.length) {
      OrderDetails._populateList(this.footerItemsList, footerItems);
    }

    this.totalAmountElem.value = totalItem.amount.value;
    this.totalAmountElem.currency = totalItem.amount.currency;
  }

  /**
   * Determine if a display item should belong in the footer list.
   * This uses the proposed "type" property, tracked at:
   * https://github.com/w3c/payment-request/issues/163
   *
   * @param {object} item - Data representing a PaymentItem
   * @returns {boolean}
   */
  static isFooterItem(item) {
    return item.type == "tax";
  }

  static _getMainListItems(request) {
    let items = request.paymentDetails.displayItems;
    if (Array.isArray(items) && items.length) {
      let predicate = item => !OrderDetails.isFooterItem(item);
      return request.paymentDetails.displayItems.filter(predicate);
    }
    return [];
  }

  static _getFooterListItems(request) {
    let items = request.paymentDetails.displayItems;
    if (Array.isArray(items) && items.length) {
      let predicate = OrderDetails.isFooterItem;
      return request.paymentDetails.displayItems.filter(predicate);
    }
    return [];
  }
}

customElements.define("order-details", OrderDetails);
