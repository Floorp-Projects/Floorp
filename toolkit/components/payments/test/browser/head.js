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


/**
 * Return the container (e.g. dialog or overlay) that the payment request contents are shown in.
 * This abstracts away the details of the widget used so that this can more earily transition from a
 * dialog to another kind of overlay.
 * Consumers shouldn't rely on a dialog window being returned.
 * @returns {Promise}
 */
async function getPaymentWidget() {
  let win;
  await BrowserTestUtils.waitForCondition(() => {
    win = Services.wm.getMostRecentWindow(null);
    return win.name.startsWith(paymentUISrv.REQUEST_ID_PREFIX);
  }, "payment dialog should be the most recent");

  return win;
}

async function getPaymentFrame(widget) {
  return widget.document.getElementById("paymentRequestFrame");
}

async function waitForMessageFromWidget(messageType, widget = null) {
  info("waitForMessageFromWidget: " + messageType);
  return new Promise(resolve => {
    Services.mm.addMessageListener("paymentContentToChrome", function onMessage({data, target}) {
      if (data.messageType != messageType) {
        return;
      }
      if (widget && widget != target) {
        return;
      }
      resolve();
      info(`Got ${messageType} from widget`);
      Services.mm.removeMessageListener("paymentContentToChrome", onMessage);
    });
  });
}

async function waitForWidgetReady(widget = null) {
  return waitForMessageFromWidget("paymentDialogReady", widget);
}

function spawnPaymentDialogTask(paymentDialogFrame, taskFn, args = null) {
  return ContentTask.spawn(paymentDialogFrame.frameLoader, args, taskFn);
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

  /**
   * Click the cancel button
   *
   * Don't await on this task since the cancel can close the dialog before
   * ContentTask can resolve the promise.
   *
   * @returns {undefined}
   */
  manuallyClickCancel: () => {
    content.document.getElementById("cancel").click();
  },
};


add_task(async function setup_head() {
  await SpecialPowers.pushPrefEnv({set: [[PREF_PAYMENT_ENABLED, true]]});
});
