"use strict";

/* eslint
  "no-unused-vars": ["error", {
    vars: "local",
    args: "none",
    varsIgnorePattern: "^(Cc|Ci|Cr|Cu|EXPORTED_SYMBOLS)$",
  }],
*/


const BLANK_PAGE_PATH = "/browser/toolkit/components/payments/test/browser/blank_page.html";
const BLANK_PAGE_URL = "https://example.com" + BLANK_PAGE_PATH;

const paymentSrv = Cc["@mozilla.org/dom/payments/payment-request-service;1"]
                     .getService(Ci.nsIPaymentRequestService);
const paymentUISrv = Cc["@mozilla.org/dom/payments/payment-ui-service;1"]
                     .getService().wrappedJSObject;
const {profileStorage} = Cu.import("resource://formautofill/ProfileStorage.jsm", {});
const {PaymentTestUtils: PTU} = Cu.import("resource://testing-common/PaymentTestUtils.jsm", {});

function getPaymentRequests() {
  let requestsEnum = paymentSrv.enumerate();
  let requests = [];
  while (requestsEnum.hasMoreElements()) {
    requests.push(requestsEnum.getNext().QueryInterface(Ci.nsIPaymentRequest));
  }
  return requests;
}

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

function waitForMessageFromWidget(messageType, widget = null) {
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

async function withMerchantTab({browser = gBrowser, url = BLANK_PAGE_URL} = {
  browser: gBrowser,
  url: BLANK_PAGE_URL,
}, taskFn) {
  await BrowserTestUtils.withNewTab({
    gBrowser: browser,
    url,
  }, taskFn);

  paymentSrv.cleanup(); // Temporary measure until bug 1408234 is fixed.

  await new Promise(resolve => {
    SpecialPowers.exactGC(resolve);
  });
}

/**
 * Load the privileged payment dialog wrapper document in a new tab and run the
 * task function.
 *
 * @param {string} requestId of the PaymentRequest
 * @param {Function} taskFn to run in the dialog with the frame as an argument.
 * @returns {Promise} which resolves when the dialog document is loaded
 */
function withNewDialogFrame(requestId, taskFn) {
  async function dialogTabTask(dialogBrowser) {
    let paymentRequestFrame = dialogBrowser.contentDocument.getElementById("paymentRequestFrame");
    // Ensure the inner frame is loaded
    await spawnPaymentDialogTask(paymentRequestFrame, async function ensureLoaded() {
      await ContentTaskUtils.waitForCondition(() => content.document.readyState == "complete",
                                              "Waiting for the unprivileged frame to load");
    });
    await taskFn(paymentRequestFrame);
  }

  let args = {
    gBrowser,
    url: `chrome://payments/content/paymentDialog.xhtml?requestId=${requestId}`,
  };
  return BrowserTestUtils.withNewTab(args, dialogTabTask);
}

/**
 * Spawn a content task inside the inner unprivileged frame of a privileged Payment Request dialog.
 *
 * @param {string} requestId
 * @param {Function} contentTaskFn
 * @param {object?} [args = null] for the content task
 * @returns {Promise}
 */
function spawnTaskInNewDialog(requestId, contentTaskFn, args = null) {
  return withNewDialogFrame(requestId, async function spawnTaskInNewDialog_tabTask(reqFrame) {
    await spawnPaymentDialogTask(reqFrame, contentTaskFn, args);
  });
}

/**
 * Open a merchant tab with the given merchantTaskFn to create a PaymentRequest
 * and then open the associated PaymentRequest dialog in a new tab and run the
 * associated dialogTaskFn. The same taskArgs are passed to both functions.
 *
 * @param {Function} merchantTaskFn
 * @param {Function} dialogTaskFn
 * @param {Object} taskArgs
 * @param {Object} options
 * @param {string} options.origin
 */
async function spawnInDialogForMerchantTask(merchantTaskFn, dialogTaskFn, taskArgs, {
  origin = "https://example.com",
} = {
  origin: "https://example.com",
}) {
  await withMerchantTab({
    url: origin + BLANK_PAGE_PATH,
  }, async merchBrowser => {
    await ContentTask.spawn(merchBrowser, taskArgs, merchantTaskFn);

    const requests = getPaymentRequests();
    is(requests.length, 1, "Should have one payment request");
    let request = requests[0];
    ok(!!request.requestId, "Got a payment request with an ID");

    await spawnTaskInNewDialog(request.requestId, dialogTaskFn, taskArgs);
  });
}

add_task(async function setup_head() {
  await profileStorage.initialize();

  SimpleTest.registerCleanupFunction(function cleanup() {
    paymentSrv.cleanup();
    profileStorage.addresses._nukeAllRecords();
    profileStorage.creditCards._nukeAllRecords();
  });
});
