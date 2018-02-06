"use strict";

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
  let address2 = {
    "given-name": "Timothy",
    "additional-name": "John",
    "family-name": "Berners-Lee",
    organization: "World Wide Web Consortium",
    "street-address": "1 Pommes Frittes Place",
    "address-level2": "Berlin",
    "address-level1": "BE",
    "postal-code": "02138",
    country: "DE",
    tel: "+16172535702",
    email: "timbl@example.org",
  };
  profileStorage.addresses.add(address2);
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
    await ContentTask.spawn(browser,
                            {
                              methodData: [PTU.MethodData.basicCard],
                              details: PTU.Details.twoShippingOptions,
                              options: PTU.Options.requestShippingOption,
                            },
                            PTU.ContentTasks.createAndShowRequest);

    // get a reference to the UI dialog and the requestId
    let [win] = await Promise.all([getPaymentWidget(), dialogReadyPromise]);
    ok(win, "Got payment widget");
    let requestId = paymentUISrv.requestIdForWindow(win);
    ok(requestId, "requestId should be defined");
    is(win.closed, false, "dialog should not be closed");

    let frame = await getPaymentFrame(win);
    ok(frame, "Got payment frame");

    let shippingOptions =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getShippingOptions);
    is(shippingOptions.selectedOptionCurrency, "USD", "Shipping options should be in USD");
    is(shippingOptions.optionCount, 2, "there should be two shipping options");
    is(shippingOptions.selectedOptionValue, "2", "default selected should be '2'");

    await spawnPaymentDialogTask(frame,
                                 PTU.DialogContentTasks.selectShippingOptionById,
                                 "1");

    shippingOptions =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getShippingOptions);
    is(shippingOptions.optionCount, 2, "there should be two shipping options");
    is(shippingOptions.selectedOptionValue, "1", "selected should be '1'");

    await ContentTask.spawn(browser,
                            {
                              details: PTU.Details.twoShippingOptionsEUR,
                            },
                            PTU.ContentTasks.addShippingChangeHandler);
    info("added shipping change handler");
    await spawnPaymentDialogTask(frame,
                                 PTU.DialogContentTasks.selectShippingAddressByCountry,
                                 "DE");
    info("changed shipping address to DE country");

    shippingOptions =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getShippingOptions);
    todo_is(shippingOptions.selectedOptionCurrency, "EUR",
            "Shipping options should be in EUR when shippingaddresschange is implemented");

    info("clicking pay");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.completePayment);

    // Add a handler to complete the payment above.
    info("acknowledging the completion from the merchant page");
    let result = await ContentTask.spawn(browser, {}, PTU.ContentTasks.addCompletionHandler);
    is(result.response.methodName, "basic-card", "Check methodName");

    let addressLines = address2["street-address"].split("\n");
    let actualShippingAddress = result.response.shippingAddress;
    is(actualShippingAddress.addressLine[0], addressLines[0], "Address line 1 should match");
    is(actualShippingAddress.addressLine[1], addressLines[1], "Address line 2 should match");
    is(actualShippingAddress.country, address2.country, "Country should match");
    is(actualShippingAddress.region, address2["address-level1"], "Region should match");
    is(actualShippingAddress.city, address2["address-level2"], "City should match");
    is(actualShippingAddress.postalCode, address2["postal-code"], "Zip code should match");
    is(actualShippingAddress.organization, address2.organization, "Org should match");
    is(actualShippingAddress.recipient,
       `${address2["given-name"]} ${address2["additional-name"]} ${address2["family-name"]}`,
       "Recipient country should match");
    is(actualShippingAddress.phone, address2.tel, "Phone should match");

    let methodDetails = result.methodDetails;
    is(methodDetails.cardholderName, "John Doe", "Check cardholderName");
    is(methodDetails.cardNumber, "999999999999", "Check cardNumber");
    is(methodDetails.expiryMonth, "01", "Check expiryMonth");
    is(methodDetails.expiryYear, "9999", "Check expiryYear");

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});
