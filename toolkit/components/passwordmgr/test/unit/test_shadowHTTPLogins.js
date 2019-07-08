/**
 * Test LoginHelper.shadowHTTPLogins
 */

"use strict";

const DOMAIN1_HTTP_TO_HTTP_U1_P1 = TestData.formLogin({});
const DOMAIN1_HTTP_TO_HTTP_U2_P1 = TestData.formLogin({
  username: "user2",
});
const DOMAIN1_HTTPS_TO_HTTPS_U1_P1 = TestData.formLogin({
  origin: "https://www3.example.com",
  formActionOrigin: "https://login.example.com",
});
const DOMAIN1_HTTPS_TO_HTTPS_U1_P2 = TestData.formLogin({
  origin: "https://www3.example.com",
  formActionOrigin: "https://login.example.com",
  password: "password two",
});
const DOMAIN1_HTTP_TO_HTTP_U1_P2 = TestData.formLogin({
  password: "password two",
});
const DOMAIN1_HTTP_TO_HTTP_U1_P1_DIFFERENT_PORT = TestData.formLogin({
  origin: "http://www3.example.com:8080",
});
const DOMAIN2_HTTP_TO_HTTP_U1_P1 = TestData.formLogin({
  origin: "http://different.example.com",
});
const DOMAIN2_HTTPS_TO_HTTPS_U1_P1 = TestData.formLogin({
  origin: "https://different.example.com",
  formActionOrigin: "https://login.example.com",
});

add_task(function test_shadowHTTPLogins() {
  let testcases = [
    {
      description: "same hostPort, same username, different scheme",
      logins: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTP_TO_HTTP_U1_P1],
      expected: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1],
    },
    {
      description: "different passwords, different scheme",
      logins: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTP_TO_HTTP_U1_P2],
      expected: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1],
    },
    {
      description: "both https, same username, different password",
      logins: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTPS_TO_HTTPS_U1_P2],
      expected: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTPS_TO_HTTPS_U1_P2],
    },
    {
      description: "same origin, different port, different scheme",
      logins: [
        DOMAIN1_HTTPS_TO_HTTPS_U1_P1,
        DOMAIN1_HTTP_TO_HTTP_U1_P1_DIFFERENT_PORT,
      ],
      expected: [
        DOMAIN1_HTTPS_TO_HTTPS_U1_P1,
        DOMAIN1_HTTP_TO_HTTP_U1_P1_DIFFERENT_PORT,
      ],
    },
    {
      description: "different origin, different scheme",
      logins: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN2_HTTP_TO_HTTP_U1_P1],
      expected: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN2_HTTP_TO_HTTP_U1_P1],
    },
    {
      description: "different username, different scheme",
      logins: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTP_TO_HTTP_U2_P1],
      expected: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTP_TO_HTTP_U2_P1],
    },
  ];

  for (let tc of testcases) {
    info(tc.description);
    let actual = LoginHelper.shadowHTTPLogins(tc.logins);
    Assert.strictEqual(
      actual.length,
      tc.expected.length,
      `Check result length`
    );
    for (let [i, login] of tc.expected.entries()) {
      Assert.strictEqual(actual[i], login, `Check index ${i}`);
    }
  }
});
