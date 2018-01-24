"use strict";

const methodData = [PTU.MethodData.basicCard];
const details = PTU.Details.total60USD;

add_task(async function test_show_abort_dialog() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    // start by creating a PaymentRequest, and show it
    await ContentTask.spawn(browser, {methodData, details}, PTU.ContentTasks.createAndShowRequest);

    // get a reference to the UI dialog and the requestId
    let win = await getPaymentWidget();
    let requestId = paymentUISrv.requestIdForWindow(win);
    ok(requestId, "requestId should be defined");
    is(win.closed, false, "dialog should not be closed");

    // abort the payment request
    ContentTask.spawn(browser, null, async () => content.rq.abort());
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});

add_task(async function test_show_manualAbort_dialog() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let dialogReadyPromise = waitForWidgetReady();
    // start by creating a PaymentRequest, and show it
    await ContentTask.spawn(browser, {methodData, details}, PTU.ContentTasks.createAndShowRequest);

    // get a reference to the UI dialog and the requestId
    let [win] = await Promise.all([getPaymentWidget(), dialogReadyPromise]);
    ok(win, "Got payment widget");
    let requestId = paymentUISrv.requestIdForWindow(win);
    ok(requestId, "requestId should be defined");
    is(win.closed, false, "dialog should not be closed");

    // abort the payment request manually
    let frame = await getPaymentFrame(win);
    ok(frame, "Got payment frame");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});

add_task(async function test_show_completePayment() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let dialogReadyPromise = waitForWidgetReady();
    // start by creating a PaymentRequest, and show it
    await ContentTask.spawn(browser, {methodData, details}, PTU.ContentTasks.createAndShowRequest);

    // get a reference to the UI dialog and the requestId
    let [win] = await Promise.all([getPaymentWidget(), dialogReadyPromise]);
    ok(win, "Got payment widget");
    let requestId = paymentUISrv.requestIdForWindow(win);
    ok(requestId, "requestId should be defined");
    is(win.closed, false, "dialog should not be closed");

    let frame = await getPaymentFrame(win);
    ok(frame, "Got payment frame");
    info("clicking pay");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.completePayment);

    // Add a handler to complete the payment above.
    info("acknowledging the completion from the merchant page");
    let result = await ContentTask.spawn(browser, {}, PTU.ContentTasks.addCompletionHandler);
    is(result.response.methodName, "basic-card", "Check methodName");

    let methodDetails = result.methodDetails;
    is(methodDetails.cardholderName, "John Doe", "Check cardholderName");
    is(methodDetails.cardNumber, "9999999999", "Check cardNumber");
    is(methodDetails.expiryMonth, "01", "Check expiryMonth");
    is(methodDetails.expiryYear, "9999", "Check expiryYear");
    is(methodDetails.cardSecurityCode, "999", "Check cardSecurityCode");

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});
