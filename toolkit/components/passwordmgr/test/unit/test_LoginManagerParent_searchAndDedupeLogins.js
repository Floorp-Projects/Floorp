/**
 * Test LoginManagerParent._searchAndDedupeLogins()
 */

"use strict";

const { LoginManagerParent: LMP } = ChromeUtils.import(
  "resource://gre/modules/LoginManagerParent.jsm"
);

const DOMAIN1_HTTP_ORIGIN = "http://www3.example.com";
const DOMAIN1_HTTPS_ORIGIN = "https://www3.example.com";

const DOMAIN1_HTTP_TO_HTTP_U1_P1 = TestData.formLogin({});
const DOMAIN1_HTTP_TO_HTTP_U2_P1 = TestData.formLogin({
  username: "user2",
});
const DOMAIN1_HTTP_TO_HTTP_U3_P1 = TestData.formLogin({
  username: "user3",
});
const DOMAIN1_HTTPS_TO_HTTPS_U1_P1 = TestData.formLogin({
  origin: DOMAIN1_HTTPS_ORIGIN,
  formActionOrigin: "https://login.example.com",
});
const DOMAIN1_HTTPS_TO_HTTPS_U2_P1 = TestData.formLogin({
  origin: DOMAIN1_HTTPS_ORIGIN,
  formActionOrigin: "https://login.example.com",
  username: "user2",
});
const DOMAIN1_HTTPS_TO_HTTPS_U1_P2 = TestData.formLogin({
  origin: DOMAIN1_HTTPS_ORIGIN,
  formActionOrigin: "https://login.example.com",
  password: "password two",
});
const DOMAIN1_HTTPS_TO_HTTPS_U1_P2_DIFFERENT_PORT = TestData.formLogin({
  origin: "https://www3.example.com:8080",
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

add_task(function setup() {
  // Not enabled by default in all.js:
  Services.prefs.setBoolPref("signon.schemeUpgrades", true);
});

add_task(async function test_searchAndDedupeLogins_acceptDifferentSubdomains() {
  let testcases = [
    {
      description: "HTTPS form, same hostPort, same username, different scheme",
      formActionOrigin: DOMAIN1_HTTPS_ORIGIN,
      logins: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTP_TO_HTTP_U1_P1],
      expected: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1],
    },
    {
      description: "HTTP form, same hostPort, same username, different scheme",
      formActionOrigin: DOMAIN1_HTTP_ORIGIN,
      logins: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTP_TO_HTTP_U1_P1],
      expected: [DOMAIN1_HTTP_TO_HTTP_U1_P1],
    },
    {
      description: "HTTPS form, different passwords, different scheme",
      formActionOrigin: DOMAIN1_HTTPS_ORIGIN,
      logins: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTP_TO_HTTP_U1_P2],
      expected: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1],
    },
    {
      description: "HTTP form, different passwords, different scheme",
      formActionOrigin: DOMAIN1_HTTP_ORIGIN,
      logins: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTP_TO_HTTP_U1_P2],
      expected: [DOMAIN1_HTTP_TO_HTTP_U1_P2],
    },
    {
      description: "HTTPS form, both https, same username, different password",
      formActionOrigin: DOMAIN1_HTTPS_ORIGIN,
      logins: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTPS_TO_HTTPS_U1_P2],
      expected: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTPS_TO_HTTPS_U1_P2],
    },
    {
      description: "HTTP form, both https, same username, different password",
      formActionOrigin: DOMAIN1_HTTP_ORIGIN,
      logins: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTPS_TO_HTTPS_U1_P2],
      expected: [],
    },
    {
      description: "HTTPS form, same origin, different port, both schemes",
      formActionOrigin: DOMAIN1_HTTPS_ORIGIN,
      logins: [
        DOMAIN1_HTTPS_TO_HTTPS_U1_P1,
        DOMAIN1_HTTP_TO_HTTP_U1_P1_DIFFERENT_PORT,
        DOMAIN1_HTTPS_TO_HTTPS_U1_P2_DIFFERENT_PORT,
      ],
      expected: [
        DOMAIN1_HTTPS_TO_HTTPS_U1_P1,
        DOMAIN1_HTTPS_TO_HTTPS_U1_P2_DIFFERENT_PORT,
      ],
    },
    {
      description: "HTTP form, same origin, different port, both schemes",
      formActionOrigin: DOMAIN1_HTTP_ORIGIN,
      logins: [
        DOMAIN1_HTTPS_TO_HTTPS_U1_P1,
        DOMAIN1_HTTP_TO_HTTP_U1_P1_DIFFERENT_PORT,
        DOMAIN1_HTTPS_TO_HTTPS_U1_P2_DIFFERENT_PORT,
      ],
      expected: [DOMAIN1_HTTP_TO_HTTP_U1_P1_DIFFERENT_PORT],
    },
    {
      description: "HTTPS form, different origin, different scheme",
      formActionOrigin: DOMAIN1_HTTPS_ORIGIN,
      logins: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN2_HTTP_TO_HTTP_U1_P1],
      expected: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1],
    },
    {
      description:
        "HTTPS form, different origin, different scheme, same password, same hostPort preferred",
      formActionOrigin: DOMAIN1_HTTPS_ORIGIN,
      logins: [DOMAIN1_HTTP_TO_HTTP_U1_P1, DOMAIN2_HTTPS_TO_HTTPS_U1_P1],
      expected: [DOMAIN1_HTTP_TO_HTTP_U1_P1],
    },
    {
      description: "HTTP form, different origin, different scheme",
      formActionOrigin: DOMAIN1_HTTP_ORIGIN,
      logins: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN2_HTTP_TO_HTTP_U1_P1],
      expected: [DOMAIN2_HTTP_TO_HTTP_U1_P1],
    },
    {
      description: "HTTPS form, different username, different scheme",
      formActionOrigin: DOMAIN1_HTTPS_ORIGIN,
      logins: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTP_TO_HTTP_U2_P1],
      expected: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTP_TO_HTTP_U2_P1],
    },
    {
      description: "HTTP form, different username, different scheme",
      formActionOrigin: DOMAIN1_HTTP_ORIGIN,
      logins: [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTP_TO_HTTP_U2_P1],
      expected: [DOMAIN1_HTTP_TO_HTTP_U2_P1],
    },
    {
      description: "HTTPS form, different usernames, different schemes",
      formActionOrigin: DOMAIN1_HTTPS_ORIGIN,
      logins: [
        DOMAIN1_HTTPS_TO_HTTPS_U1_P2,
        DOMAIN1_HTTPS_TO_HTTPS_U2_P1,
        DOMAIN1_HTTP_TO_HTTP_U1_P1,
        DOMAIN1_HTTP_TO_HTTP_U3_P1,
      ],
      expected: [
        DOMAIN1_HTTPS_TO_HTTPS_U1_P2,
        DOMAIN1_HTTPS_TO_HTTPS_U2_P1,
        DOMAIN1_HTTP_TO_HTTP_U3_P1,
      ],
    },
  ];

  for (let tc of testcases) {
    info(tc.description);

    let guids = await Services.logins.addLogins(tc.logins);
    Assert.strictEqual(
      guids.length,
      tc.logins.length,
      "Check length of added logins"
    );

    let actual = await LMP.searchAndDedupeLogins(tc.formActionOrigin, {
      formActionOrigin: tc.formActionOrigin,
      looseActionOriginMatch: true,
      acceptDifferentSubdomains: true,
    });
    info(`actual:\n ${JSON.stringify(actual, null, 2)}`);
    info(`expected:\n ${JSON.stringify(tc.expected, null, 2)}`);
    Assert.strictEqual(
      actual.length,
      tc.expected.length,
      `Check result length`
    );
    for (let [i, login] of tc.expected.entries()) {
      Assert.ok(actual[i].equals(login), `Check index ${i}`);
    }

    Services.logins.removeAllLogins();
  }
});
