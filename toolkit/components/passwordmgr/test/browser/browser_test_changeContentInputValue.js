/**
 * Tests head.js#changeContentInputValue.
 */

"use strict";

// The origin for the test URIs.
const TEST_ORIGIN = "https://example.com";
const BASIC_FORM_PAGE_PATH = DIRECTORY_PATH + "form_basic.html";
const USERNAME_INPUT_SELECTOR = "#form-basic-username";

let testCases = [
  {
    name: "blank string should clear input value",
    originalValue: "start text",
    inputEvent: "",
    expectedKeypresses: ["Backspace"],
  },
  {
    name:
      "input value that adds to original string should only add the difference",
    originalValue: "start text",
    inputEvent: "start text!!!",
    expectedKeypresses: ["!", "!", "!"],
  },
  {
    name:
      "input value that is a subset of original string should only delete the difference",
    originalValue: "start text",
    inputEvent: "start",
    expectedKeypresses: ["Backspace"],
  },
  {
    name:
      "input value that is unrelated to the original string should replace it",
    originalValue: "start text",
    inputEvent: "wut?",
    expectedKeypresses: ["w", "u", "t", "?"],
  },
];

for (let testData of testCases) {
  let tmp = {
    async [testData.name]() {
      await testStringChange(testData);
    },
  };
  add_task(tmp[testData.name]);
}

async function testStringChange({
  name,
  originalValue,
  inputEvent,
  expectedKeypresses,
}) {
  info("Starting test " + name);
  await LoginTestUtils.clearData();

  await LoginTestUtils.addLogin({
    username: originalValue,
    password: "password",
  });

  let formProcessedPromise = listenForTestNotification("FormProcessed");
  let url = TEST_ORIGIN + BASIC_FORM_PAGE_PATH;
  info("Opening tab with url: " + url);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function(browser) {
      info(`Opened tab with url: ${url}, waiting for focus`);
      await SimpleTest.promiseFocus(browser.ownerGlobal);
      info("Waiting for form-processed message");
      await formProcessedPromise;
      await checkForm(browser, originalValue);
      info("form checked");

      await ContentTask.spawn(
        browser,
        { USERNAME_INPUT_SELECTOR, expectedKeypresses },
        async function({ USERNAME_INPUT_SELECTOR, expectedKeypresses }) {
          let input = content.document.querySelector(USERNAME_INPUT_SELECTOR);

          let verifyKeyListener = event => {
            is(
              expectedKeypresses[0],
              event.key,
              "Key press matches expected value"
            );
            expectedKeypresses.shift();

            if (!expectedKeypresses.length) {
              input.removeEventListner("keydown", verifyKeyListener);
              input.addEventListener("keydown", () => {
                throw new Error("Unexpected keypress encountered");
              });
            }
          };

          input.addEventListener("keydown", verifyKeyListener);
        }
      );

      changeContentInputValue(browser, USERNAME_INPUT_SELECTOR, inputEvent);
    }
  );
}

async function checkForm(browser, expectedUsername) {
  await ContentTask.spawn(
    browser,
    {
      expectedUsername,
      USERNAME_INPUT_SELECTOR,
    },
    async function contentCheckForm({
      expectedUsername,
      USERNAME_INPUT_SELECTOR,
    }) {
      let field = content.document.querySelector(USERNAME_INPUT_SELECTOR);
      is(
        field.value,
        expectedUsername,
        `Username field has teh expected initial value '${expectedUsername}'`
      );
    }
  );
}
