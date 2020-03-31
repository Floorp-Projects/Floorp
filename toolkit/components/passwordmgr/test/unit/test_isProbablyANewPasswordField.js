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
    description: "Basic login form",
    document: `
      <h1>Sign in</h1>
      <form>
        <label>Username: <input type="text" name="username"></label>
        <label>Password: <input type="password" name="password"></label>
        <input type="submit" value="Sign in">
      </form>
    `,
    expectedResult: [false],
  },
  {
    description: "Basic registration form",
    document: `
      <h1>Create account</h1>
      <form>
        <label>Username: <input type="text" name="username"></label>
        <label>Password: <input type="password" name="new-password"></label>
        <input type="submit" value="Register">
      </form>
    `,
    expectedResult: [true],
  },
  {
    description: "Basic password change form",
    document: `
      <h1>Change password</h1>
      <form>
        <label>Current Password: <input type="password" name="current-password"></label>
        <label>New Password: <input type="password" name="new-password"></label>
        <label>Confirm Password: <input type="password" name="confirm-password"></label>
        <input type="submit" value="Save">
      </form>
    `,
    expectedResult: [false, true, true],
  },
  {
    description: "Basic login 'form' without a form element",
    document: `
      <h1>Sign in</h1>
      <label>Username: <input type="text" name="username"></label>
      <label>Password: <input type="password" name="password"></label>
      <input type="submit" value="Sign in">
    `,
    expectedResult: [false],
  },
  {
    description: "Basic registration 'form' without a form element",
    document: `
      <h1>Create account</h1>
      <label>Username: <input type="text" name="username"></label>
      <label>Password: <input type="password" name="new-password"></label>
      <input type="submit" value="Register">
    `,
    expectedResult: [true],
  },
  {
    description: "Basic password change 'form' without a form element",
    document: `
      <h1>Change password</h1>
      <label>Current Password: <input type="password" name="current-password"></label>
      <label>New Password: <input type="password" name="new-password"></label>
      <label>Confirm Password: <input type="password" name="confirm-password"></label>
      <input type="submit" value="Save">
    `,
    expectedResult: [false, true, true],
  },
];

add_task(async function test_returns_false_when_pref_disabled() {
  const threshold = Services.prefs.getStringPref(
    NEW_PASSWORD_HEURISTIC_ENABLED_PREF
  );

  info("Temporarily disabling new-password heuristic pref");
  Services.prefs.setStringPref(NEW_PASSWORD_HEURISTIC_ENABLED_PREF, "-1");

  // Use registration form test case, where we know it should return true if enabled
  const testcase = TESTCASES[1];
  info("Starting testcase: " + testcase.description);
  const document = MockDocument.createTestDocument(
    "http://localhost:8080/test/",
    testcase.document
  );
  const input = document.querySelectorAll(`input[type="password"]`);
  const result = LoginAutoComplete._isProbablyANewPasswordField(input);
  Assert.strictEqual(
    result,
    false,
    `When the pref is set to disable, the result is always false, e.g. for the testcase, ${testcase.description} `
  );

  info("Re-enabling new-password heuristic pref");
  Services.prefs.setStringPref(NEW_PASSWORD_HEURISTIC_ENABLED_PREF, threshold);
});

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
          `In the test case, ${testcase.description}, check if password field #${i} is a new password field.`
        );
      }
    });
  })();
}
