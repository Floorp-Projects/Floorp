"use strict";

const methodData = [PTU.MethodData.basicCard];
const details = PTU.Details.twoShippingOptions;

add_task(async function test_show_abort_dialog() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win} =
      await setupPaymentDialog(browser, {
        methodData,
        details,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

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
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData,
        details,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

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
  let addressGUID = await formAutofillStorage.addresses.add(address);
  await onChanged;

  onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                      (subject, data) => data == "add");
  let card = {
    billingAddressGUID: addressGUID,
    "cc-exp-month": 1,
    "cc-exp-year": 9999,
    "cc-name": "John Doe",
    "cc-number": "999999999999",
  };

  formAutofillStorage.creditCards.add(card);
  await onChanged;

  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData,
        details,
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

    info("entering CSC");
    await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.setSecurityCode, {
      securityCode: "999",
    });
    info("clicking pay");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.completePayment);

    // Add a handler to complete the payment above.
    info("acknowledging the completion from the merchant page");
    let result = await ContentTask.spawn(browser, {}, PTU.ContentTasks.addCompletionHandler);

    let actualShippingAddress = result.response.shippingAddress;
    checkPaymentAddressMatchesStorageAddress(actualShippingAddress, address, "Shipping address");
    is(result.response.methodName, "basic-card", "Check methodName");
    let methodDetails = result.methodDetails;
    is(methodDetails.cardholderName, "John Doe", "Check cardholderName");
    is(methodDetails.cardNumber, "999999999999", "Check cardNumber");
    is(methodDetails.expiryMonth, "01", "Check expiryMonth");
    is(methodDetails.expiryYear, "9999", "Check expiryYear");
    is(methodDetails.cardSecurityCode, "999", "Check cardSecurityCode");
    is(typeof methodDetails.methodName, "undefined", "Check methodName wasn't included");
    checkPaymentAddressMatchesStorageAddress(methodDetails.billingAddress, address,
                                             "Billing address");

    is(result.response.shippingOption, "2", "Check shipping option");

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});

add_task(async function test_show_completePayment() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData,
        details,
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

    await ContentTask.spawn(browser, {
      eventName: "shippingoptionchange",
    }, PTU.ContentTasks.promisePaymentRequestEvent);

    info("changing shipping option to '1' from default selected option of '2'");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.selectShippingOptionById, "1");

    await ContentTask.spawn(browser, {
      eventName: "shippingoptionchange",
    }, PTU.ContentTasks.awaitPaymentRequestEventPromise);
    info("got shippingoptionchange event");

    info("clicking pay");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.completePayment);

    // Add a handler to complete the payment above.
    info("acknowledging the completion from the merchant page");
    let result = await ContentTask.spawn(browser, {}, PTU.ContentTasks.addCompletionHandler);

    is(result.response.shippingOption, "1", "Check shipping option");

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});
