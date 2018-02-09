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
  let onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                          (subject, data) => data == "add");
  let address = {
    "given-name": "Timothy",
    "additional-name": "John",
    "family-name": "Berners-Lee",
    organization: "World Wide Web Consortium",
    "street-address": "32 Vassar Street\nMIT Room 32-G524",
    "address-level2": "Cambridge",
    "address-level1": "MA",
    "postal-code": "02139",
    country: "US",
    tel: "+16172535702",
    email: "timbl@example.org",
  };
  profileStorage.addresses.add(address);
  await onChanged;

  onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                      (subject, data) => data == "add");
  let card = {
    "cc-exp-month": 1,
    "cc-exp-year": 9999,
    "cc-name": "John Doe",
    "cc-number": "999999999999",
  };

  profileStorage.creditCards.add(card);
  await onChanged;

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
    info("entering CSC");
    await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.setSecurityCode, {
      securityCode: "999",
    });
    info("clicking pay");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.completePayment);

    // Add a handler to complete the payment above.
    info("acknowledging the completion from the merchant page");
    let result = await ContentTask.spawn(browser, {}, PTU.ContentTasks.addCompletionHandler);

    let addressLines = address["street-address"].split("\n");
    let actualShippingAddress = result.response.shippingAddress;
    is(actualShippingAddress.addressLine[0], addressLines[0], "Address line 1 should match");
    is(actualShippingAddress.addressLine[1], addressLines[1], "Address line 2 should match");
    is(actualShippingAddress.country, address.country, "Country should match");
    is(actualShippingAddress.region, address["address-level1"], "Region should match");
    is(actualShippingAddress.city, address["address-level2"], "City should match");
    is(actualShippingAddress.postalCode, address["postal-code"], "Zip code should match");
    is(actualShippingAddress.organization, address.organization, "Org should match");
    is(actualShippingAddress.recipient,
       `${address["given-name"]} ${address["additional-name"]} ${address["family-name"]}`,
       "Recipient country should match");
    is(actualShippingAddress.phone, address.tel, "Phone should match");

    is(result.response.methodName, "basic-card", "Check methodName");
    let methodDetails = result.methodDetails;
    is(methodDetails.cardholderName, "John Doe", "Check cardholderName");
    is(methodDetails.cardNumber, "999999999999", "Check cardNumber");
    is(methodDetails.expiryMonth, "01", "Check expiryMonth");
    is(methodDetails.expiryYear, "9999", "Check expiryYear");
    is(methodDetails.cardSecurityCode, "999", "Check cardSecurityCode");

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});
