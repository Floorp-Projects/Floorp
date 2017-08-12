"use strict";

/* eslint
  "no-unused-vars": ["error", {
    vars: "local",
    args: "none",
    varsIgnorePattern: "^(Cc|Ci|Cr|Cu|EXPORTED_SYMBOLS)$",
  }],
*/

const REQUEST_ID_PREFIX = "paymentRequest-";
const BLANK_PAGE_URL = "https://example.com/browser/toolkit/components/" +
                       "payments/test/browser/blank_page.html";
const PREF_PAYMENT_ENABLED = "dom.payments.request.enabled";


async function getDialogWindow() {
  let win;
  await BrowserTestUtils.waitForCondition(() => {
    win = Services.wm.getMostRecentWindow(null);
    return win.name.startsWith(REQUEST_ID_PREFIX);
  }, "payment dialog should be the most recent");

  return win;
}

function requestIdForWindow(window) {
  let windowName = window.name;

  return windowName.startsWith(REQUEST_ID_PREFIX) ?
    windowName.replace(REQUEST_ID_PREFIX, "") : // returns suffix, which is the requestId
    null;
}

/**
 * Common content tasks functions to be used with ContentTask.spawn.
 */
let ContentTasks = {
  createAndShowRequest: async ({methodData, details, options}) => {
    let rq = new content.PaymentRequest(methodData, details, options);
    content.rq = rq; // assign it so we can retrieve it later
    rq.show();
  },
};


add_task(async function setup_head() {
  await SpecialPowers.pushPrefEnv({set: [[PREF_PAYMENT_ENABLED, true]]});
});
