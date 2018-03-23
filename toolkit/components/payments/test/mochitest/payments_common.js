"use strict";

/* exported asyncElementRendered, promiseStateChange, deepClone, PTU */

const PTU = SpecialPowers.Cu.import("resource://testing-common/PaymentTestUtils.jsm", {})
                            .PaymentTestUtils;

/**
 * A helper to await on while waiting for an asynchronous rendering of a Custom
 * Element.
 * @returns {Promise}
 */
function asyncElementRendered() {
  return Promise.resolve();
}

function promiseStateChange(store) {
  return new Promise(resolve => {
    store.subscribe({
      stateChangeCallback(state) {
        store.unsubscribe(this);
        resolve(state);
      },
    });
  });
}

function deepClone(obj) {
  return JSON.parse(JSON.stringify(obj));
}
