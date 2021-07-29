/**
 * Test for LoginManagerChild.getUsernameFieldFromUsernameOnlyForm
 */

"use strict";

const { LoginManagerChild } = ChromeUtils.import(
  "resource://gre/modules/LoginManagerChild.jsm"
);

const TESTCASES = [
  {
    description: "1 text input field",
    document: `<form>
      <input id="un1" type="text">
      </form>`,
    expectations: [false, true],
  },
  {
    description: "1 text input field & 1 hidden input fields",
    document: `<form>
      <input id="un1" type="text">
      <input id="un2" type="hidden">
      </form>`,
    expectations: [false, true],
  },
  {
    description: "1 username field",
    document: `<form>
      <input id="un1" autocomplete="username">
      </form>`,
    expectations: [true, true],
  },
  {
    description: "1 username field & 1 hidden input fields",
    document: `<form>
      <input id="un1" autocomplete="username">
      <input id="un2" type="hidden">
      </form>`,
    expectations: [true, true],
  },
  {
    description: "1 username field, 1 hidden input field, & 1 password field",
    document: `<form>
      <input id="un1" autocomplete="username">
      <input id="un2" type="hidden">
      <input id="pw1" type=password>
      </form>`,
    expectations: [false, false],
  },
  {
    description: "1 password field",
    document: `<form>
      <input id="pw1" type=password>
      </form>`,
    expectations: [false, false],
  },
  {
    description: "1 username & password field",
    document: `<form>
      <input id="un1" autocomplete="username">
      <input id="pw1" type=password>
      </form>`,
    expectations: [false, false],
  },
  {
    description: "1 username & text field",
    document: `<form>
      <input id="un1" autocomplete="username">
      <input id="un2" type="text">
      </form>`,
    expectations: [false, false],
  },
  {
    description: "2 text input fields",
    document: `<form>
      <input id="un1" type="text">
      <input id="un2" type="text">
      </form>`,
    expectations: [false, false],
  },
  {
    description: "2 username fields",
    document: `<form>
      <input id="un1" autocomplete="username">
      <input id="un2" autocomplete="username">
      </form>`,
    expectations: [false, false],
  },
];

function _setPrefs() {
  Services.prefs.setBoolPref("signon.usernameOnlyForm.enabled", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("signon.usernameOnlyForm.enabled");
  });
}

this._setPrefs();

for (let tc of TESTCASES) {
  info("Sanity checking the testcase: " + tc.description);

  // A form is considered a username-only form
  for (let formHasSigninKeyword of [false, true]) {
    (function() {
      const testcase = tc;
      add_task(async function() {
        if (formHasSigninKeyword) {
          testcase.decription += " (form has a login keyword)";
        }
        info("Starting testcase: " + testcase.description);
        info("Document string: " + testcase.document);
        const document = MockDocument.createTestDocument(
          "http://localhost:8080/test/",
          testcase.document
        );

        let form = document.querySelector("form");
        if (formHasSigninKeyword) {
          form.setAttribute("name", "login");
        }

        let lmc = new LoginManagerChild();
        let element = lmc.getUsernameFieldFromUsernameOnlyForm(form);
        Assert.strictEqual(
          testcase.expectations[formHasSigninKeyword ? 1 : 0],
          element != null,
          "TODO: add description"
        );
      });
    })();
  }
}
