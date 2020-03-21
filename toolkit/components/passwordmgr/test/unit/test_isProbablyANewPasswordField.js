/**
 * Test for LoginAutoComplete._isProbablyANewPasswordField.
 */

"use strict";

const LoginAutoComplete = Cc[
  "@mozilla.org/login-manager/autocompletesearch;1"
].getService(Ci.nsILoginAutoCompleteSearch).wrappedJSObject;

const TESTCASES = [
  // Note there is no test case for `<input type="password" autocomplete="new-password">`
  // since _isProbablyANewPasswordField explicitly does not run in that case.
  {
    description: "Password fields outside of a form",
    document: `<input type="password"><input type="password" autocomplete="current-password">`,
    expectedResult: [false, false],
  },
  {
    description: "Basic login form",
    document: `<form><input type="text" name="username"><input type="password" name="password"><input type="submit" value="sign in"></form>`,
    expectedResult: [false],
  },
  {
    description: "Basic registration form",
    document: `<form><input type="text" name="username"><input type="password" name="new-password"><input type="submit" value="register"></form>`,
    expectedResult: [true],
  },
  {
    description: "Basic password change form",
    document: `<form><input type="password" name="new-password"><input type="password" name="confirm-password"></form>`,
    expectedResult: [true, true],
  },
];

for (let testcase of TESTCASES) {
  info("Sanity checking the testcase: " + testcase.description);

  (function() {
    add_task(async function() {
      info("Starting testcase: " + testcase.description);
      let document = MockDocument.createTestDocument(
        "http://localhost:8080/test/",
        testcase.document
      );

      const results = [];
      for (let input of document.querySelectorAll(`input[type="password"]`)) {
        const result = LoginAutoComplete._isProbablyANewPasswordField(input);
        results.push(result);
      }

      for (let i = 0; i < testcase.expectedResult.length; i++) {
        let expectedResult = testcase.expectedResult[i];
        Assert.strictEqual(
          results[i],
          expectedResult,
          `Check if password field #${i} is a new password field`
        );
      }
    });
  })();
}
