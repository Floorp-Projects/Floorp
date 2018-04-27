"use strict";

add_task(async function test_add_link() {
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

    let shippingAddressChangePromise = ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
    }, PTU.ContentTasks.awaitPaymentRequestEventPromise);

    const EXPECTED_ADDRESS = {
      "given-name": "Jared",
      "family-name": "Wein",
      "organization": "Mozilla",
      "street-address": "404 Internet Lane",
      "address-level2": "Firefoxity City",
      "address-level1": "CA",
      "postal-code": "31337",
      "country": "US",
      "tel": "+15555551212",
      "email": "test@example.com",
    };

    await spawnPaymentDialogTask(frame, async (address) => {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      let addLink = content.document.querySelector("address-picker a");
      is(addLink.textContent, "Add", "Add link text");

      addLink.click();

      let state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "address-page" && !state.page.guid;
      }, "Check add page state");

      let title = content.document.querySelector("address-form h1");
      is(title.textContent, "Add Shipping Address", "Page title should be set");

      info("filling fields");
      for (let [key, val] of Object.entries(address)) {
        let field = content.document.getElementById(key);
        if (!field) {
          ok(false, `${key} field not found`);
        }
        field.value = val;
        ok(!field.disabled, `Field #${key} shouldn't be disabled`);
      }

      content.document.querySelector("address-form button:last-of-type").click();
      state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return Object.keys(state.savedAddresses).length == 1;
      }, "Check address was added");

      let addressGUIDs = Object.keys(state.savedAddresses);
      is(addressGUIDs.length, 1, "Check there is one address");
      let savedAddress = state.savedAddresses[addressGUIDs[0]];
      for (let [key, val] of Object.entries(address)) {
        is(savedAddress[key], val, "Check " + key);
      }

      state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "payment-summary";
      }, "Switched back to payment-summary");
    }, EXPECTED_ADDRESS);

    await shippingAddressChangePromise;
    info("got shippingaddresschange event");

    info("clicking cancel");
    await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});

add_task(async function test_edit_link() {
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

    let shippingAddressChangePromise = ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
    }, PTU.ContentTasks.awaitPaymentRequestEventPromise);

    const EXPECTED_ADDRESS = {
      "given-name": "Jaws",
      "family-name": "swaJ",
      "organization": "aliizoM",
    };

    await spawnPaymentDialogTask(frame, async (address) => {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      let editLink = content.document.querySelector("address-picker a:nth-of-type(2)");
      is(editLink.textContent, "Edit", "Edit link text");

      editLink.click();

      let state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "address-page" && !!state.page.guid;
      }, "Check edit page state");

      let title = content.document.querySelector("address-form h1");
      is(title.textContent, "Edit Shipping Address", "Page title should be set");

      info("overwriting field values");
      for (let [key, val] of Object.entries(address)) {
        let field = content.document.getElementById(key);
        field.value = val;
        ok(!field.disabled, `Field #${key} shouldn't be disabled`);
      }

      content.document.querySelector("address-form button:last-of-type").click();

      state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return Object.keys(state.savedAddresses).length == 1;
      }, "Check address was edited");

      let addressGUIDs = Object.keys(state.savedAddresses);
      is(addressGUIDs.length, 1, "Check there is still one address");
      let savedAddress = state.savedAddresses[addressGUIDs[0]];
      for (let [key, val] of Object.entries(address)) {
        is(savedAddress[key], val, "Check updated " + key);
      }

      state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "payment-summary";
      }, "Switched back to payment-summary");
    }, EXPECTED_ADDRESS);

    await shippingAddressChangePromise;
    info("got shippingaddresschange event");

    info("clicking cancel");
    await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
  await cleanupFormAutofillStorage();
});
