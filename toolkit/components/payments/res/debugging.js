/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const requestStore = window.parent.document.querySelector("payment-dialog").requestStore;

let REQUEST_1 = {
  tabId: 9,
  topLevelPrincipal: {URI: {displayHost: "tschaeff.github.io"}},
  requestId: "3797081f-a96b-c34b-a58b-1083c6e66e25",
  paymentMethods: [],
  paymentDetails: {
    id: "",
    totalItem: {label: "Demo total", amount: {currency: "EUR", value: "1.00"}, pending: false},
    displayItems: [
      {
        label: "Square",
        amount: {
          currency: "USD",
          value: "5",
        },
      },
    ],
    shippingOptions: [
      {
        id: "123",
        label: "Fast",
        amount: {
          currency: "USD",
          value: 10,
        },
        selected: false,
      },
      {
        id: "456",
        label: "Faster (default)",
        amount: {
          currency: "USD",
          value: 20,
        },
        selected: true,
      },
    ],
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
};

let REQUEST_2 = {
  tabId: 9,
  topLevelPrincipal: {URI: {displayHost: "example.com"}},
  requestId: "3797081f-a96b-c34b-a58b-1083c6e66e25",
  paymentMethods: [],
  paymentDetails: {
    id: "",
    totalItem: {label: "Demo total", amount: {currency: "CAD", value: "25.75"}, pending: false},
    displayItems: [
      {
        label: "Triangle",
        amount: {
          currency: "CAD",
          value: "3",
        },
      },
      {
        label: "Circle",
        amount: {
          currency: "EUR",
          value: "10.50",
        },
      },
    ],
    shippingOptions: [
      {
        id: "123",
        label: "Fast (default)",
        amount: {
          currency: "USD",
          value: 10,
        },
        selected: true,
      },
      {
        id: "947",
        label: "Slow",
        amount: {
          currency: "USD",
          value: 10,
        },
        selected: false,
      },
    ],
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
};

let ADDRESSES_1 = {
  "48bnds6854t": {
    "address-level1": "MI",
    "address-level2": "Some City",
    "country": "US",
    "guid": "48bnds6854t",
    "name": "Mr. Foo",
    "postal-code": "90210",
    "street-address": "123 Sesame Street,\nApt 40",
    "tel": "+1 519 555-5555",
  },
  "68gjdh354j": {
    "address-level1": "CA",
    "address-level2": "Mountain View",
    "country": "US",
    "guid": "68gjdh354j",
    "name": "Mrs. Bar",
    "postal-code": "94041",
    "street-address": "P.O. Box 123",
    "tel": "+1 650 555-5555",
  },
};

let buttonActions = {
  delete1Address() {
    let savedAddresses = Object.assign({}, requestStore.getState().savedAddresses);
    delete savedAddresses[Object.keys(savedAddresses)[0]];
    requestStore.setState({
      savedAddresses,
    });
  },

  logState() {
    let state = requestStore.getState();
    // eslint-disable-next-line no-console
    console.log(state);
    dump(`${JSON.stringify(state, null, 2)}\n`);
  },

  refresh() {
    window.parent.location.reload(true);
  },

  setAddresses1() {
    requestStore.setState({savedAddresses: ADDRESSES_1});
  },

  setRequest1() {
    requestStore.setState({request: REQUEST_1});
  },

  setRequest2() {
    requestStore.setState({request: REQUEST_2});
  },
};

window.addEventListener("click", function onButtonClick(evt) {
  let id = evt.target.id;
  if (!id || typeof(buttonActions[id]) != "function") {
    return;
  }

  buttonActions[id]();
});
