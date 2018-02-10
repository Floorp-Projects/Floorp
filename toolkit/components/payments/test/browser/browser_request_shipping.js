"use strict";

add_task(async function setup() {
  let onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                          (subject, data) => data == "add");

  let card = {
    "cc-exp-month": 1,
    "cc-exp-year": 9999,
    "cc-name": "John Doe",
    "cc-number": "999999999999",
  };

  formAutofillStorage.creditCards.add(card);
  await onChanged;
});

add_task(async function test_request_shipping_present() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    for (let [shippingKey, shippingString] of [
      [null, "Shipping Address"],
      ["shipping", "Shipping Address"],
      ["delivery", "Delivery Address"],
      ["pickup", "Pickup Address"],
    ]) {
      let options = {
        requestShipping: true,
      };
      if (shippingKey) {
        options.shippingType = shippingKey;
      }
      let {win, frame} =
        await setupPaymentDialog(browser, {
          methodData: [PTU.MethodData.basicCard],
          details: PTU.Details.twoShippingOptions,
          options,
          merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
        }
      );

      let isShippingOptionsVisible =
        await spawnPaymentDialogTask(frame,
                                     PTU.DialogContentTasks.isElementVisible,
                                     "shipping-option-picker");
      ok(isShippingOptionsVisible, "shipping-option-picker should be visible");
      let addressSelector = "address-picker[selected-state-key='selectedShippingAddress']";
      let isShippingAddressVisible =
        await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.isElementVisible,
                                     addressSelector);
      ok(isShippingAddressVisible, "shipping address picker should be visible");

      let shippingOptionText =
        await spawnPaymentDialogTask(frame,
                                     PTU.DialogContentTasks.getElementTextContent,
                                     "#shipping-type-label");
      is(shippingOptionText, shippingString,
         "Label should be match shipping type: " + shippingKey);

      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
      await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
    }
  });
});

add_task(async function test_request_shipping_not_present() {
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

    let isShippingOptionsVisible =
      await spawnPaymentDialogTask(frame,
                                   PTU.DialogContentTasks.isElementVisible,
                                   "shipping-option-picker");
    ok(!isShippingOptionsVisible, "shipping-option-picker should not be visible");
    let isShippingAddressVisible =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.isElementVisible,
                                   "address-picker[selected-state-key='selectedShippingAddress']");
    ok(!isShippingAddressVisible, "shipping address picker should not be visible");

    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});
