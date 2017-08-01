"use strict";

/* eslint
  "no-unused-vars": ["error", {
    vars: "local",
    args: "none",
    varsIgnorePattern: "^(Cc|Ci|Cr|Cu|EXPORTED_SYMBOLS)$",
  }],
*/

const BLANK_PAGE_URL = "https://example.com/browser/toolkit/components/" +
                       "payments/test/browser/blank_page.html";
const PREF_PAYMENT_ENABLED = "dom.payments.request.enabled";
const paymentUISrv = Cc["@mozilla.org/dom/payments/payment-ui-service;1"]
                     .getService().wrappedJSObject;


async function getDialogWindow() {
  let win;
  await BrowserTestUtils.waitForCondition(() => {
    win = Services.wm.getMostRecentWindow(null);
    return win.name.startsWith(paymentUISrv.REQUEST_ID_PREFIX);
  }, "payment dialog should be the most recent");

  return win;
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

  manuallyClickCancel: async () => {
    content.document.getElementById("cancel").click();
  },
};


add_task(async function setup_head() {
  await SpecialPowers.pushPrefEnv({set: [[PREF_PAYMENT_ENABLED, true]]});
});
