/**
 * Modify page elements and verify that they are found as options in the save/update doorhanger.
 */

const USERNAME_SELECTOR = "#form-expanded-username";
const PASSWORD_SELECTOR = "#form-expanded-password";
const SEARCH_SELECTOR = "#form-expanded-search";
const CAPTCHA_SELECTOR = "#form-expanded-captcha";
const NON_FORM_SELECTOR = "#form-expanded-non-form-input";

const AUTOCOMPLETE_POPUP_SELECTOR = "#PopupAutoComplete";
const USERNAME_DROPMARKER_SELECTOR =
  "#password-notification-username-dropmarker";

const TEST_CASES = [
  {
    description: "a modified username should be included in the popup",
    modifiedFields: [
      { [USERNAME_SELECTOR]: "new_username" },
      { [PASSWORD_SELECTOR]: "myPassword" },
    ],
    expectUsernameDropmarker: true,
    expectedValues: ["new_username"],
  },
  {
    description:
      "if no non-password fields are modified, no popup should be available",
    modifiedFields: [{ [PASSWORD_SELECTOR]: "myPassword" }],
    expectUsernameDropmarker: false,
    expectedValues: [],
  },
  {
    description: "all modified username fields should be included in the popup",
    modifiedFields: [
      { [USERNAME_SELECTOR]: "new_username" },
      { [SEARCH_SELECTOR]: "unrelated search query" },
      { [CAPTCHA_SELECTOR]: "someCaptcha" },
      { [PASSWORD_SELECTOR]: "myPassword" },
    ],
    expectUsernameDropmarker: true,
    expectedValues: ["new_username", "unrelated search query", "someCaptcha"],
  },
  {
    description:
      "any modified fields that don't look like usernames or passwords should not be included in the popup",
    modifiedFields: [
      { [PASSWORD_SELECTOR]: "myPassword" },
      { [NON_FORM_SELECTOR]: "I dont even know what this one is" },
    ],
    expectUsernameDropmarker: false,
    expectedValues: [],
  },
  {
    description:
      "when a field is modified multiple times, all CHANGE event values should be included in the popup",
    modifiedFields: [
      { [USERNAME_SELECTOR]: "new_username1" },
      { [USERNAME_SELECTOR]: "new_username2" },
      { [USERNAME_SELECTOR]: "new_username3" },
      { [PASSWORD_SELECTOR]: "myPassword" },
    ],
    expectUsernameDropmarker: true,
    expectedValues: ["new_username1", "new_username2", "new_username3"],
  },
  {
    description: "empty strings should not be displayed in popup",
    modifiedFields: [
      { [PASSWORD_SELECTOR]: "myPassword" },
      { [USERNAME_SELECTOR]: "new_username" },
      { [USERNAME_SELECTOR]: "" },
    ],
    expectUsernameDropmarker: true,
    expectedValues: ["new_username"],
  },
  {
    description: "saved logins should be displayed in popup",
    modifiedFields: [
      { [USERNAME_SELECTOR]: "new_username" },
      { [PASSWORD_SELECTOR]: "myPassword" },
    ],
    savedLogins: [
      {
        username: "savedUn1",
        password: "somePass",
      },
      {
        username: "savedUn2",
        password: "otherPass",
      },
    ],
    expectUsernameDropmarker: true,
    expectedValues: ["new_username", "savedUn1", "savedUn2"],
  },
  {
    description: "duplicated page usernames should only be displayed once",
    modifiedFields: [
      { [PASSWORD_SELECTOR]: "myPassword" },
      { [USERNAME_SELECTOR]: "new_username1" },
      { [USERNAME_SELECTOR]: "new_username2" },
      { [USERNAME_SELECTOR]: "new_username1" },
    ],
    expectUsernameDropmarker: true,
    expectedValues: ["new_username1", "new_username2"],
  },
  {
    description: "non-un/pw fields also prompt doorhanger updates",
    modifiedFields: [
      { [PASSWORD_SELECTOR]: "myPassword" },
      { [USERNAME_SELECTOR]: "new_username1" },
      { [SEARCH_SELECTOR]: "search" },
      { [CAPTCHA_SELECTOR]: "captcha" },
    ],
    expectUsernameDropmarker: true,
    expectedValues: ["new_username1", "search", "captcha"],
  },
  // {
  //   description: "duplicated saved/page usernames should TODO https://mozilla.invisionapp.com/share/XGXL6WZVKFJ#/screens/420547613/comments",
  // },
];

function _validateTestCase(tc) {
  if (tc.expectUsernameDropmarker) {
    ok(
      !!tc.expectedValues.length,
      "Validate test case.  A visible dropmarker implies expected values"
    );
  } else {
    ok(
      !tc.expectedValues.length,
      "Validate test case.  A hidden dropmarker implies no expected values"
    );
  }
}

async function _setPrefs() {
  await SpecialPowers.pushPrefEnv({
    set: [["signon.capture.inputChanges.enabled", true]],
  });
}

function _addSavedLogin(username) {
  Services.logins.addLogin(
    LoginTestUtils.testData.formLogin({
      origin: "https://example.com",
      formActionOrigin: "https://example.com",
      username,
      password: "Saved login passwords not used in this test",
    })
  );
}

async function _clickDropmarker(document, notificationElement) {
  let acPopup = document.querySelector(AUTOCOMPLETE_POPUP_SELECTOR);
  let acPopupShown = BrowserTestUtils.waitForEvent(acPopup, "popupshown");

  notificationElement.querySelector(USERNAME_DROPMARKER_SELECTOR).click();
  await acPopupShown;
}

function _getSuggestedValues(document) {
  let suggestedValues = [];
  let autocompletePopup = document.querySelector(AUTOCOMPLETE_POPUP_SELECTOR);
  let numRows = autocompletePopup.view.matchCount;
  for (let i = 0; i < numRows; i++) {
    suggestedValues.push(autocompletePopup.view.getValueAt(i));
  }
  return suggestedValues;
}

add_task(async function test_edit_password() {
  await _setPrefs();
  for (let testCase of TEST_CASES) {
    info("Test case: " + JSON.stringify(testCase));
    _validateTestCase(testCase);

    // Clean state before the test case is executed.
    await LoginTestUtils.clearData();
    await cleanupDoorhanger();
    await cleanupPasswordNotifications();
    Services.logins.removeAllLogins();

    // Create the pre-existing logins when needed.
    info("Adding any saved logins");
    if (testCase.savedLogins) {
      for (let login of testCase.savedLogins) {
        info("Adding login " + JSON.stringify(login));
        _addSavedLogin(login.username);
      }
    }

    info("Opening tab");
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url:
          "https://example.com/browser/toolkit/components/" +
          "passwordmgr/test/browser/form_expanded.html",
      },
      async function(browser) {
        info("Editing the form");
        for (const change of testCase.modifiedFields) {
          for (const selector in change) {
            let newValue = change[selector];
            info(`Setting field '${selector}' to '${newValue}'`);
            await changeContentFormValues(browser, change);
          }
        }

        let notif = getCaptureDoorhanger("any");

        let { panel } = PopupNotifications;

        let promiseShown = BrowserTestUtils.waitForEvent(panel, "popupshown");

        EventUtils.synthesizeMouseAtCenter(notif.anchorElement, {});

        await promiseShown;

        let notificationElement = panel.childNodes[0];

        let usernameDropmarker = notificationElement.querySelector(
          USERNAME_DROPMARKER_SELECTOR
        );
        ok(
          BrowserTestUtils.is_visible(usernameDropmarker) ==
            testCase.expectUsernameDropmarker,
          "Confirm dropmarker visibility"
        );

        if (testCase.expectUsernameDropmarker) {
          info("Opening autocomplete popup");
          await _clickDropmarker(document, notificationElement);
        }

        let suggestedValues = _getSuggestedValues(document);

        let expectedNotFound = testCase.expectedValues.filter(
          expected => !suggestedValues.includes(expected)
        );
        let foundNotExpected = suggestedValues.filter(
          actual => !testCase.expectedValues.includes(actual)
        );

        // Log expected/actual inconsistencies
        ok(
          !expectedNotFound.length,
          `All expected values should be found\nCase: "${
            testCase.description
          }"\nExpected: ${JSON.stringify(
            testCase.expectedValues
          )}\nActual: ${JSON.stringify(
            suggestedValues
          )}\nExpected not found: ${JSON.stringify(expectedNotFound)}
        `
        );
        ok(
          !foundNotExpected.length,
          `All actual values should be expected\nCase: "${
            testCase.description
          }"\nExpected: ${JSON.stringify(
            testCase.expectedValues
          )}\nActual: ${JSON.stringify(
            suggestedValues
          )}\nFound not expected: ${JSON.stringify(foundNotExpected)}
        `
        );

        // Clean up state
        await cleanupDoorhanger();
        await cleanupPasswordNotifications();
        await clearMessageCache(browser);
        Services.logins.removeAllLogins();
      }
    );
  }
});
