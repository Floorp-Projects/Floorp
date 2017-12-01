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


let buttonActions = {
  logState() {
    let state = requestStore.getState();
    // eslint-disable-next-line no-console
    console.log(state);
    dump(`${JSON.stringify(state, null, 2)}\n`);
  },

  refresh() {
    window.parent.location.reload(true);
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
