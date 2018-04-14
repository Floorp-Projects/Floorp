"use strict";

async function setup() {
  await setupFormAutofillStorage();
  await addSampleAddressesAndBasicCard();
}

add_task(async function test_change_shipping() {
  await setup();
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: PTU.Details.twoShippingOptions,
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

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

    await ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
    }, PTU.ContentTasks.promisePaymentRequestEvent);
    info("added shipping change handler");

    await spawnPaymentDialogTask(frame,
                                 PTU.DialogContentTasks.selectShippingAddressByCountry,
                                 "DE");
    info("changed shipping address to DE country");

    await ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
    }, PTU.ContentTasks.awaitPaymentRequestEventPromise);
    info("got shippingaddresschange event");

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

    let addressLines = PTU.Addresses.TimBL2["street-address"].split("\n");
    let actualShippingAddress = result.response.shippingAddress;
    let expectedAddress = PTU.Addresses.TimBL2;
    is(actualShippingAddress.addressLine[0], addressLines[0], "Address line 1 should match");
    is(actualShippingAddress.addressLine[1], addressLines[1], "Address line 2 should match");
    is(actualShippingAddress.country, expectedAddress.country, "Country should match");
    is(actualShippingAddress.region, expectedAddress["address-level1"], "Region should match");
    is(actualShippingAddress.city, expectedAddress["address-level2"], "City should match");
    is(actualShippingAddress.postalCode, expectedAddress["postal-code"], "Zip code should match");
    is(actualShippingAddress.organization, expectedAddress.organization, "Org should match");
    is(actualShippingAddress.recipient,
       `${expectedAddress["given-name"]} ${expectedAddress["additional-name"]} ` +
       `${expectedAddress["family-name"]}`,
       "Recipient should match");
    is(actualShippingAddress.phone, expectedAddress.tel, "Phone should match");

    let methodDetails = result.methodDetails;
    is(methodDetails.cardholderName, "John Doe", "Check cardholderName");
    is(methodDetails.cardNumber, "999999999999", "Check cardNumber");
    is(methodDetails.expiryMonth, "01", "Check expiryMonth");
    is(methodDetails.expiryYear, "9999", "Check expiryYear");

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
  await cleanupFormAutofillStorage();
});

add_task(async function test_no_shippingchange_without_shipping() {
  await setup();
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: PTU.Details.twoShippingOptions,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

    ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
    }, PTU.ContentTasks.ensureNoPaymentRequestEvent);
    info("added shipping change handler");

    info("clicking pay");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.completePayment);

    // Add a handler to complete the payment above.
    info("acknowledging the completion from the merchant page");
    let result = await ContentTask.spawn(browser, {}, PTU.ContentTasks.addCompletionHandler);
    is(result.response.methodName, "basic-card", "Check methodName");

    let actualShippingAddress = result.response.shippingAddress;
    ok(actualShippingAddress === null,
       "Check that shipping address is null with requestShipping:false");

    let methodDetails = result.methodDetails;
    is(methodDetails.cardholderName, "John Doe", "Check cardholderName");
    is(methodDetails.cardNumber, "999999999999", "Check cardNumber");
    is(methodDetails.expiryMonth, "01", "Check expiryMonth");
    is(methodDetails.expiryYear, "9999", "Check expiryYear");

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
  await cleanupFormAutofillStorage();
});

add_task(async function test_address_edit() {
  await setup();
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: PTU.Details.twoShippingOptions,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
        options: PTU.Options.requestShippingOption,
      }
    );

    let addressOptions =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getShippingAddresses);
    info("initial addressOptions: " + JSON.stringify(addressOptions));
    let selectedIndex = addressOptions.selectedOptionIndex;
    let selectedAddressGuid = addressOptions.options[selectedIndex].guid;
    let selectedAddress = formAutofillStorage.addresses.get(selectedAddressGuid);

    is(selectedIndex, 0, "First address should be selected");
    ok(selectedAddress, "Selected address does exist in the address collection");
    is(selectedAddress.country, "US", "Expected initial country value");

    info("Updating the address directly in the store");
    await formAutofillStorage.addresses.update(selectedAddress.guid, {
      country: "CA",
    }, true);

    await ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
    }, PTU.ContentTasks.promisePaymentRequestEvent);

    addressOptions =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getShippingAddresses);
    info("updated addressOptions: " + JSON.stringify(addressOptions));
    selectedIndex = addressOptions.selectedOptionIndex;
    let newSelectedAddressGuid = addressOptions.options[selectedIndex].guid;

    is(newSelectedAddressGuid, selectedAddressGuid, "Selected guid hasnt changed");
    selectedAddress = formAutofillStorage.addresses.get(selectedAddressGuid);

    is(selectedIndex, 0, "First address should be selected");
    is(selectedAddress.country, "CA", "Expected changed country value");

    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });

  await cleanupFormAutofillStorage();
});

add_task(async function test_address_removal() {
  await setup();
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: PTU.Details.twoShippingOptions,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
        options: PTU.Options.requestShippingOption,
      }
    );

    let addressOptions =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getShippingAddresses);
    info("initial addressOptions: " + JSON.stringify(addressOptions));
    let selectedIndex = addressOptions.selectedOptionIndex;
    let selectedAddressGuid = addressOptions.options[selectedIndex].guid;

    is(selectedIndex, 0, "First address should be selected");
    is(addressOptions.options.length, 2, "Should be 2 address options initially");

    info("Remove the selected address from the store");
    await formAutofillStorage.addresses.remove(selectedAddressGuid);

    await ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
    }, PTU.ContentTasks.promisePaymentRequestEvent);

    addressOptions =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getShippingAddresses);
    info("updated addressOptions: " + JSON.stringify(addressOptions));
    selectedIndex = addressOptions.selectedOptionIndex;

    is(selectedIndex, 0, "First address should be selected");
    is(addressOptions.options.length, 1, "Should now be 1 address option");

    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });

  await cleanupFormAutofillStorage();
});
