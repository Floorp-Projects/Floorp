/**
 * Test LoginHelper.doLoginsMatch
 */

add_task(function test_formSubmitURL_ignoreSchemes() {
  let httpActionLogin = TestData.formLogin();
  let httpsActionLogin = TestData.formLogin({
    formSubmitURL: "https://www.example.com",
  });
  let jsActionLogin = TestData.formLogin({
    formSubmitURL: "javascript:",
  });
  let emptyActionLogin = TestData.formLogin({
    formSubmitURL: "",
  });

  Assert.notEqual(httpActionLogin.formSubmitURL, httpsActionLogin.formSubmitURL,
                  "Ensure actions differ");

  const TEST_CASES = [
    [httpActionLogin, httpActionLogin, true],
    [httpsActionLogin, httpsActionLogin, true],
    [jsActionLogin, jsActionLogin, true],
    [emptyActionLogin, emptyActionLogin, true],
    // only differing by scheme:
    [httpsActionLogin, httpActionLogin, true],
    [httpActionLogin, httpsActionLogin, true],

    // empty matches everything
    [httpsActionLogin, emptyActionLogin, true],
    [emptyActionLogin, httpsActionLogin, true],
    [jsActionLogin, emptyActionLogin, true],
    [emptyActionLogin, jsActionLogin, true],

    // Begin false cases:
    [httpsActionLogin, jsActionLogin, false],
    [jsActionLogin, httpsActionLogin, false],
    [httpActionLogin, jsActionLogin, false],
    [jsActionLogin, httpActionLogin, false],
  ];

  for (let [login1, login2, expected] of TEST_CASES) {
    Assert.strictEqual(LoginHelper.doLoginsMatch(login1, login2, {
      ignorePassword: false,
      ignoreSchemes: true,
    }), expected, `LoginHelper.doLoginsMatch:
\t${JSON.stringify(login1)}
\t${JSON.stringify(login2)}`);
  }
});
