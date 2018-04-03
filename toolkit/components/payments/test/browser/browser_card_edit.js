"use strict";

add_task(async function test_add_link() {
  const args = {
    methodData: [PTU.MethodData.basicCard],
    details: PTU.Details.total60USD,
  };
  await spawnInDialogForMerchantTask(PTU.ContentTasks.createRequest, async function check() {
    let {
      PaymentTestUtils: PTU,
    } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

    let addLink = content.document.querySelector("payment-method-picker a");
    is(addLink.textContent, "Add", "Add link text");

    addLink.click();

    let state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return state.page.id == "basic-card-page" && !state.page.guid;
    },
                                                          "Check add page state");

    let year = (new Date()).getFullYear();
    let card = {
      "cc-number": "4111111111111111",
      "cc-name": "J. Smith",
      "cc-exp-month": 11,
      "cc-exp-year": year,
    };

    info("filling fields");
    for (let [key, val] of Object.entries(card)) {
      let field = content.document.getElementById(key);
      field.value = val;
      ok(!field.disabled, `Field #${key} shouldn't be disabled`);
    }

    content.document.querySelector("basic-card-form button:last-of-type").click();

    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return Object.keys(state.savedBasicCards).length == 1;
    },
                                                      "Check card was added");

    let cardGUIDs = Object.keys(state.savedBasicCards);
    is(cardGUIDs.length, 1, "Check there is one card");
    let savedCard = state.savedBasicCards[cardGUIDs[0]];
    card["cc-number"] = "************1111"; // Card should be masked
    for (let [key, val] of Object.entries(card)) {
      is(savedCard[key], val, "Check " + key);
    }

    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return state.page.id == "payment-summary";
    },
                                                      "Switched back to payment-summary");
  }, args);
});

add_task(async function test_edit_link() {
  const args = {
    methodData: [PTU.MethodData.basicCard],
    details: PTU.Details.total60USD,
  };
  await spawnInDialogForMerchantTask(PTU.ContentTasks.createRequest, async function check() {
    let {
      PaymentTestUtils: PTU,
    } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

    let editLink = content.document.querySelector("payment-method-picker a:nth-of-type(2)");
    is(editLink.textContent, "Edit", "Edit link text");

    editLink.click();

    let state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return state.page.id == "basic-card-page" && !!state.page.guid;
    },
                                                          "Check edit page state");

    let nextYear = (new Date()).getFullYear() + 1;
    let card = {
      // cc-number cannot be modified
      "cc-name": "A. Nonymous",
      "cc-exp-month": 3,
      "cc-exp-year": nextYear,
    };

    info("overwriting field values");
    for (let [key, val] of Object.entries(card)) {
      let field = content.document.getElementById(key);
      field.value = val;
      ok(!field.disabled, `Field #${key} shouldn't be disabled`);
    }
    ok(content.document.getElementById("cc-number").disabled, "cc-number field should be disabled");

    content.document.querySelector("basic-card-form button:last-of-type").click();

    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return Object.keys(state.savedBasicCards).length == 1;
    },
                                                      "Check card was added");

    let cardGUIDs = Object.keys(state.savedBasicCards);
    is(cardGUIDs.length, 1, "Check there is still one card");
    let savedCard = state.savedBasicCards[cardGUIDs[0]];
    is(savedCard["cc-number"], "************1111", "Card number should be masked and unmodified.");
    for (let [key, val] of Object.entries(card)) {
      is(savedCard[key], val, "Check updated " + key);
    }

    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return state.page.id == "payment-summary";
    },
                                                      "Switched back to payment-summary");
  }, args);
});

