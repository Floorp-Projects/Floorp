let { LoginManagerPrompter } = ChromeUtils.import(
  "resource://gre/modules/LoginManagerPrompter.jsm"
);

const TEST_CASES = [
  {
    description: "page values should appear before saved values",
    savedLogins: [{ username: "savedUsername", password: "savedPassword" }],
    possibleUsernames: ["pageUsername"],
    expectedSuggestions: [
      { text: "pageUsername", style: "possible-username" },
      { text: "savedUsername", style: "login" },
    ],
  },
  {
    description: "duplicate page values should be deduped",
    savedLogins: [],
    possibleUsernames: ["pageUsername", "pageUsername", "pageUsername2"],
    expectedSuggestions: [
      { text: "pageUsername", style: "possible-username" },
      { text: "pageUsername2", style: "possible-username" },
    ],
  },
  {
    description: "page values should dedupe and win over saved values",
    savedLogins: [{ username: "username", password: "savedPassword" }],
    possibleUsernames: ["username"],
    expectedSuggestions: [{ text: "username", style: "possible-username" }],
  },
  {
    description: "empty usernames should be filtered out",
    savedLogins: [{ username: "", password: "savedPassword" }],
    possibleUsernames: [""],
    expectedSuggestions: [],
  },
  {
    description: "auth logins should be displayed alongside normal ones",
    savedLogins: [
      { username: "normalUsername", password: "normalPassword" },
      { isAuth: true, username: "authUsername", password: "authPassword" },
    ],
    possibleUsernames: [""],
    expectedSuggestions: [
      { text: "normalUsername", style: "login" },
      { text: "authUsername", style: "login" },
    ],
  },
];

const LOGIN = TestData.formLogin({
  origin: "https://example.com",
  formActionOrigin: "https://example.com",
  username: "LOGIN is used only for its origin",
  password: "LOGIN is used only for its origin",
});

function _setPrefs() {
  Services.prefs.setBoolPref("signon.capture.inputChanges.enabled", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("signon.capture.inputChanges.enabled");
  });
}

function _saveLogins(logins) {
  logins
    .map(loginData => {
      let login;
      if (loginData.isAuth) {
        login = TestData.authLogin({
          origin: "https://example.com",
          httpRealm: "example-realm",
          username: loginData.username,
          password: loginData.password,
        });
      } else {
        login = TestData.formLogin({
          origin: "https://example.com",
          formActionOrigin: "https://example.com",
          username: loginData.username,
          password: loginData.password,
        });
      }
      return login;
    })
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

async function _test(testCase) {
  info(`Starting test case: ${testCase.description}`);
  info(`Storing saved logins: ${JSON.stringify(testCase.savedLogins)}`);
  _saveLogins(testCase.savedLogins);

  info("Computing results");
  let result = await LoginManagerPrompter._getUsernameSuggestions(
    LOGIN,
    testCase.possibleUsernames
  );

  _compare(testCase.expectedSuggestions, result);

  info("Cleaning up state");
  LoginTestUtils.clearData();
}

add_task(async function test_LoginManagerPrompter_getUsernameSuggestions() {
  _setPrefs();
  for (let tc of TEST_CASES) {
    await _test(tc);
  }
});
