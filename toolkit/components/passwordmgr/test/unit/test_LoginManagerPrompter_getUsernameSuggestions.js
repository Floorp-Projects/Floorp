const { LoginManagerPrompter } = ChromeUtils.importESModule(
  "resource://gre/modules/LoginManagerPrompter.sys.mjs"
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
    isLoggedIn: true,
  },
  {
    description: "duplicate page values should be deduped",
    savedLogins: [],
    possibleUsernames: ["pageUsername", "pageUsername", "pageUsername2"],
    expectedSuggestions: [
      { text: "pageUsername", style: "possible-username" },
      { text: "pageUsername2", style: "possible-username" },
    ],
    isLoggedIn: true,
  },
  {
    description: "page values should dedupe and win over saved values",
    savedLogins: [{ username: "username", password: "savedPassword" }],
    possibleUsernames: ["username"],
    expectedSuggestions: [{ text: "username", style: "possible-username" }],
    isLoggedIn: true,
  },
  {
    description: "empty usernames should be filtered out",
    savedLogins: [{ username: "", password: "savedPassword" }],
    possibleUsernames: [""],
    expectedSuggestions: [],
    isLoggedIn: true,
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
    isLoggedIn: true,
  },
  {
    description: "saved logins from subdomains should be displayed",
    savedLogins: [
      {
        username: "savedUsername",
        password: "savedPassword",
        origin: "https://subdomain.example.com",
      },
    ],
    possibleUsernames: [],
    expectedSuggestions: [{ text: "savedUsername", style: "login" }],
    isLoggedIn: true,
  },
  {
    description: "usernames from different subdomains should be deduped",
    savedLogins: [
      {
        username: "savedUsername",
        password: "savedPassword",
        origin: "https://subdomain.example.com",
      },
      {
        username: "savedUsername",
        password: "savedPassword",
        origin: "https://example.com",
      },
    ],
    possibleUsernames: [],
    expectedSuggestions: [{ text: "savedUsername", style: "login" }],
    isLoggedIn: true,
  },
  {
    description: "No results with saved login when Primary Password is locked",
    savedLogins: [
      {
        username: "savedUsername",
        password: "savedPassword",
        origin: "https://example.com",
      },
    ],
    possibleUsernames: [],
    expectedSuggestions: [],
    isLoggedIn: false,
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

async function _saveLogins(loginDatas) {
  const logins = loginDatas.map(loginData => {
    let login;
    if (loginData.isAuth) {
      login = TestData.authLogin({
        origin: loginData.origin ?? "https://example.com",
        httpRealm: "example-realm",
        username: loginData.username,
        password: loginData.password,
      });
    } else {
      login = TestData.formLogin({
        origin: loginData.origin ?? "https://example.com",
        formActionOrigin: "https://example.com",
        username: loginData.username,
        password: loginData.password,
      });
    }
    return login;
  });
  await Services.logins.addLogins(logins);
}

function _compare(expectedArr, actualArr) {
  Assert.ok(!!expectedArr, "Expect expectedArr to be truthy");
  Assert.ok(!!actualArr, "Expect actualArr to be truthy");
  Assert.ok(
    expectedArr.length == actualArr.length,
    "Expect expectedArr and actualArr to be the same length"
  );
  for (let i = 0; i < expectedArr.length; i++) {
    const expected = expectedArr[i];
    const actual = actualArr[i];

    Assert.ok(
      expected.text == actual.text,
      `Expect element #${i} text to match.  Expected: '${expected.text}', Actual '${actual.text}'`
    );
    Assert.ok(
      expected.style == actual.style,
      `Expect element #${i} text to match.  Expected: '${expected.style}', Actual '${actual.style}'`
    );
  }
}

async function _test(testCase) {
  info(`Starting test case: ${testCase.description}`);
  info(`Storing saved logins: ${JSON.stringify(testCase.savedLogins)}`);
  await _saveLogins(testCase.savedLogins);

  if (!testCase.isLoggedIn) {
    // Primary Password should be enabled and locked
    LoginTestUtils.primaryPassword.enable();
  }

  info("Computing results");
  const result = await LoginManagerPrompter._getUsernameSuggestions(
    LOGIN,
    testCase.possibleUsernames
  );

  _compare(testCase.expectedSuggestions, result);

  info("Cleaning up state");
  if (!testCase.isLoggedIn) {
    LoginTestUtils.primaryPassword.disable();
  }
  LoginTestUtils.clearData();
}

add_task(async function test_LoginManagerPrompter_getUsernameSuggestions() {
  _setPrefs();
  for (const tc of TEST_CASES) {
    await _test(tc);
  }
});
