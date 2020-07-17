let { LoginManagerPrompter } = ChromeUtils.import(
  "resource://gre/modules/LoginManagerPrompter.jsm"
);

const TEST_CASES = [
  // TODO uncomment in bug 1641413
  //   {
  //     description: "page values should appear before saved values",
  //     savedLogins: [{ username: "savedUsername", password: "savedPassword" }],
  //     possibleUsernames: ["pageUsername"],
  //     expectedSuggestions: [
  //       { text: "pageUsername", style: "possible-username" },
  //       { text: "savedUsername", style: "login" },
  //     ],
  //   },
  {
    description: "duplicate page values should be deduped",
    savedLogins: [],
    possibleUsernames: ["pageUsername", "pageUsername", "pageUsername2"],
    expectedSuggestions: [
      { text: "pageUsername", style: "" },
      { text: "pageUsername2", style: "" },
    ],
  },
  // TODO uncomment in bug 1641413
  //   {
  //     description: "page values should dedupe and win over saved values",
  //     savedLogins: [{ username: "username", password: "savedPassword" }],
  //     possibleUsernames: ["username"],
  //     expectedSuggestions: [{ text: "username", style: "possible-username" }],
  //   },
];

const LOGIN = LoginTestUtils.testData.formLogin({
  origin: "https://example.com",
  formActionOrigin: "https://example.com",
  username: "LOGIN is used only for its origin",
  password: "LOGIN is used only for its origin",
});

function _saveLogins(logins) {
  logins
    .map(loginData =>
      LoginTestUtils.testData.formLogin({
        origin: "https://example.com",
        formActionOrigin: "https://example.com",
        username: loginData.username,
        password: loginData.password,
      })
    )
    .forEach(login => Services.logins.addLogin(login));
}

function _compare(expectedArr, actualArr) {
  ok(!!expectedArr, "Expect expectedArr to be truthy");
  ok(!!actualArr, "Expect actualArr to be truthy");
  ok(
    expectedArr.length == actualArr.length,
    "Expect expectedArr and actualArr to be the same length"
  );
  for (let i = 0; i < expectedArr.length; i++) {
    let expected = expectedArr[i];
    let actual = actualArr[i];

    ok(
      expected.text == actual.text,
      `Expect element #${i} text to match.  Expected: '${expected.text}', Actual '${actual.text}'`
    );
    ok(
      expected.style == actual.style,
      `Expect element #${i} text to match.  Expected: '${expected.style}', Actual '${actual.style}'`
    );
  }
}

function _test(testCase) {
  info(`Starting test case: ${testCase.description}`);
  info(`Storing saved logins: ${JSON.stringify(testCase.savedLogins)}`);
  _saveLogins(testCase.savedLogins);

  info("Computing results");
  let result = LoginManagerPrompter._getUsernameSuggestions(
    LOGIN,
    testCase.possibleUsernames
  );

  _compare(testCase.expectedSuggestions, result);

  info("Cleaning up state");
  LoginTestUtils.clearData();
}

add_task(function test_LoginManagerPrompter_getUsernameSuggestions() {
  for (let tc of TEST_CASES) {
    _test(tc);
  }
});
