/*
 * Test LoginHelper.dedupeLogins
 */

"use strict";

Cu.import("resource://gre/modules/LoginHelper.jsm");

const DOMAIN1_HTTP_TO_HTTP_U1_P1 = TestData.formLogin({
  timePasswordChanged: 3000,
  timeLastUsed: 2000,
});
const DOMAIN1_HTTP_TO_HTTP_U1_P2 = TestData.formLogin({
  password: "password two",
});
const DOMAIN1_HTTP_TO_HTTP_U2_P2 = TestData.formLogin({
  password: "password two",
  username: "username two",
});
const DOMAIN1_HTTPS_TO_HTTPS_U1_P1 = TestData.formLogin({
  formSubmitURL: "http://www.example.com",
  hostname: "https://www3.example.com",
  timePasswordChanged: 4000,
  timeLastUsed: 1000,
});
const DOMAIN1_HTTPS_TO_EMPTY_U1_P1 = TestData.formLogin({
  formSubmitURL: "",
  hostname: "https://www3.example.com",
});
const DOMAIN1_HTTPS_TO_EMPTYU_P1 = TestData.formLogin({
  hostname: "https://www3.example.com",
  username: "",
});
const DOMAIN1_HTTP_AUTH = TestData.authLogin({
  hostname: "http://www3.example.com",
});
const DOMAIN1_HTTPS_AUTH = TestData.authLogin({
  hostname: "https://www3.example.com",
});


add_task(function test_dedupeLogins() {
  // [description, expectedOutput, dedupe arg. 0, dedupe arg 1, ...]
  let testcases = [
    [
      "exact dupes",
      [DOMAIN1_HTTP_TO_HTTP_U1_P1],
      [DOMAIN1_HTTP_TO_HTTP_U1_P1, DOMAIN1_HTTP_TO_HTTP_U1_P1],
      undefined,
      [], // force no resolveBy logic to test behavior of preferring the first..
    ],
    [
      "default uniqueKeys is un + pw",
      [DOMAIN1_HTTP_TO_HTTP_U1_P1, DOMAIN1_HTTP_TO_HTTP_U1_P2],
      [DOMAIN1_HTTP_TO_HTTP_U1_P1, DOMAIN1_HTTP_TO_HTTP_U1_P2],
      undefined,
      [],
    ],
    [
      "same usernames, different passwords, dedupe username only",
      [DOMAIN1_HTTP_TO_HTTP_U1_P1],
      [DOMAIN1_HTTP_TO_HTTP_U1_P1, DOMAIN1_HTTP_TO_HTTP_U1_P2],
      ["username"],
      [],
    ],
    [
      "same un+pw, different scheme",
      [DOMAIN1_HTTP_TO_HTTP_U1_P1],
      [DOMAIN1_HTTP_TO_HTTP_U1_P1, DOMAIN1_HTTPS_TO_HTTPS_U1_P1],
      undefined,
      [],
    ],
    [
      "same un+pw, different scheme, reverse order",
      [DOMAIN1_HTTPS_TO_HTTPS_U1_P1],
      [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTP_TO_HTTP_U1_P1],
      undefined,
      [],
    ],
    [
      "same un+pw, different scheme, include hostname",
      [DOMAIN1_HTTP_TO_HTTP_U1_P1, DOMAIN1_HTTPS_TO_HTTPS_U1_P1],
      [DOMAIN1_HTTP_TO_HTTP_U1_P1, DOMAIN1_HTTPS_TO_HTTPS_U1_P1],
      ["hostname", "username", "password"],
      [],
    ],
    [
      "empty username is not deduped with non-empty",
      [DOMAIN1_HTTP_TO_HTTP_U1_P1, DOMAIN1_HTTPS_TO_EMPTYU_P1],
      [DOMAIN1_HTTP_TO_HTTP_U1_P1, DOMAIN1_HTTPS_TO_EMPTYU_P1],
      undefined,
      [],
    ],
    [
      "empty username is deduped with same passwords",
      [DOMAIN1_HTTPS_TO_EMPTYU_P1],
      [DOMAIN1_HTTPS_TO_EMPTYU_P1, DOMAIN1_HTTP_TO_HTTP_U1_P1],
      ["password"],
      [],
    ],
    [
      "mix of form and HTTP auth",
      [DOMAIN1_HTTP_TO_HTTP_U1_P1],
      [DOMAIN1_HTTP_TO_HTTP_U1_P1, DOMAIN1_HTTP_AUTH],
      undefined,
      [],
    ],
  ];

  for (let tc of testcases) {
    let description = tc.shift();
    let expected = tc.shift();
    let actual = LoginHelper.dedupeLogins(...tc);
    Assert.strictEqual(actual.length, expected.length, `Check: ${description}`);
    for (let [i, login] of expected.entries()) {
      Assert.strictEqual(actual[i], login, `Check index ${i}`);
    }
  }
});


add_task(function* test_dedupeLogins_resolveBy() {
  Assert.ok(DOMAIN1_HTTP_TO_HTTP_U1_P1.timeLastUsed > DOMAIN1_HTTPS_TO_HTTPS_U1_P1.timeLastUsed,
            "Sanity check timeLastUsed difference");
  Assert.ok(DOMAIN1_HTTP_TO_HTTP_U1_P1.timePasswordChanged < DOMAIN1_HTTPS_TO_HTTPS_U1_P1.timePasswordChanged,
            "Sanity check timePasswordChanged difference");

  let testcases = [
    [
      "default resolveBy is timeLastUsed",
      [DOMAIN1_HTTP_TO_HTTP_U1_P1],
      [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTP_TO_HTTP_U1_P1],
    ],
    [
      "default resolveBy is timeLastUsed, reversed input",
      [DOMAIN1_HTTP_TO_HTTP_U1_P1],
      [DOMAIN1_HTTP_TO_HTTP_U1_P1, DOMAIN1_HTTPS_TO_HTTPS_U1_P1],
    ],
    [
      "resolveBy timeLastUsed + timePasswordChanged",
      [DOMAIN1_HTTP_TO_HTTP_U1_P1],
      [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTP_TO_HTTP_U1_P1],
      undefined,
      ["timeLastUsed", "timePasswordChanged"],
    ],
    [
      "resolveBy timeLastUsed + timePasswordChanged, reversed input",
      [DOMAIN1_HTTP_TO_HTTP_U1_P1],
      [DOMAIN1_HTTP_TO_HTTP_U1_P1, DOMAIN1_HTTPS_TO_HTTPS_U1_P1],
      undefined,
      ["timeLastUsed", "timePasswordChanged"],
    ],
    [
      "resolveBy timePasswordChanged",
      [DOMAIN1_HTTPS_TO_HTTPS_U1_P1],
      [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTP_TO_HTTP_U1_P1],
      undefined,
      ["timePasswordChanged"],
    ],
    [
      "resolveBy timePasswordChanged, reversed",
      [DOMAIN1_HTTPS_TO_HTTPS_U1_P1],
      [DOMAIN1_HTTP_TO_HTTP_U1_P1, DOMAIN1_HTTPS_TO_HTTPS_U1_P1],
      undefined,
      ["timePasswordChanged"],
    ],
    [
      "resolveBy timePasswordChanged + timeLastUsed",
      [DOMAIN1_HTTPS_TO_HTTPS_U1_P1],
      [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTP_TO_HTTP_U1_P1],
      undefined,
      ["timePasswordChanged", "timeLastUsed"],
    ],
    [
      "resolveBy timePasswordChanged + timeLastUsed, reversed",
      [DOMAIN1_HTTPS_TO_HTTPS_U1_P1],
      [DOMAIN1_HTTP_TO_HTTP_U1_P1, DOMAIN1_HTTPS_TO_HTTPS_U1_P1],
      undefined,
      ["timePasswordChanged", "timeLastUsed"],
    ],
    [
      "resolveBy scheme + timePasswordChanged, prefer HTTP",
      [DOMAIN1_HTTP_TO_HTTP_U1_P1],
      [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTP_TO_HTTP_U1_P1],
      undefined,
      ["scheme", "timePasswordChanged"],
      DOMAIN1_HTTP_TO_HTTP_U1_P1.hostname,
    ],
    [
      "resolveBy scheme + timePasswordChanged, prefer HTTP, reversed input",
      [DOMAIN1_HTTP_TO_HTTP_U1_P1],
      [DOMAIN1_HTTP_TO_HTTP_U1_P1, DOMAIN1_HTTPS_TO_HTTPS_U1_P1],
      undefined,
      ["scheme", "timePasswordChanged"],
      DOMAIN1_HTTP_TO_HTTP_U1_P1.hostname,
    ],
    [
      "resolveBy scheme + timePasswordChanged, prefer HTTPS",
      [DOMAIN1_HTTPS_TO_HTTPS_U1_P1],
      [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTP_TO_HTTP_U1_P1],
      undefined,
      ["scheme", "timePasswordChanged"],
      DOMAIN1_HTTPS_TO_HTTPS_U1_P1.hostname,
    ],
    [
      "resolveBy scheme + timePasswordChanged, prefer HTTPS, reversed input",
      [DOMAIN1_HTTPS_TO_HTTPS_U1_P1],
      [DOMAIN1_HTTP_TO_HTTP_U1_P1, DOMAIN1_HTTPS_TO_HTTPS_U1_P1],
      undefined,
      ["scheme", "timePasswordChanged"],
      DOMAIN1_HTTPS_TO_HTTPS_U1_P1.hostname,
    ],
    [
      "resolveBy scheme HTTP auth",
      [DOMAIN1_HTTPS_AUTH],
      [DOMAIN1_HTTP_AUTH, DOMAIN1_HTTPS_AUTH],
      undefined,
      ["scheme"],
      DOMAIN1_HTTPS_AUTH.hostname,
    ],
    [
      "resolveBy scheme HTTP auth, reversed input",
      [DOMAIN1_HTTPS_AUTH],
      [DOMAIN1_HTTPS_AUTH, DOMAIN1_HTTP_AUTH],
      undefined,
      ["scheme"],
      DOMAIN1_HTTPS_AUTH.hostname,
    ],
    [
      "resolveBy scheme, empty form submit URL",
      [DOMAIN1_HTTPS_TO_HTTPS_U1_P1],
      [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTPS_TO_EMPTY_U1_P1],
      undefined,
      ["scheme"],
      DOMAIN1_HTTPS_TO_HTTPS_U1_P1.hostname,
    ],
  ];

  for (let tc of testcases) {
    let description = tc.shift();
    let expected = tc.shift();
    let actual = LoginHelper.dedupeLogins(...tc);
    Assert.strictEqual(actual.length, expected.length, `Check: ${description}`);
    for (let [i, login] of expected.entries()) {
      Assert.strictEqual(actual[i], login, `Check index ${i}`);
    }
  }

});

add_task(function* test_dedupeLogins_preferredOriginMissing() {
  let testcases = [
    [
      "resolveBy scheme + timePasswordChanged, missing preferredOrigin",
      /preferredOrigin/,
      [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTP_TO_HTTP_U1_P1],
      undefined,
      ["scheme", "timePasswordChanged"],
    ],
    [
      "resolveBy timePasswordChanged + scheme, missing preferredOrigin",
      /preferredOrigin/,
      [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTP_TO_HTTP_U1_P1],
      undefined,
      ["timePasswordChanged", "scheme"],
    ],
    [
      "resolveBy scheme + timePasswordChanged, empty preferredOrigin",
      /preferredOrigin/,
      [DOMAIN1_HTTPS_TO_HTTPS_U1_P1, DOMAIN1_HTTP_TO_HTTP_U1_P1],
      undefined,
      ["scheme", "timePasswordChanged"],
      "",
    ],
  ];

  for (let tc of testcases) {
    let description = tc.shift();
    let expectedException = tc.shift();
    Assert.throws(() => {
      LoginHelper.dedupeLogins(...tc);
    }, expectedException, `Check: ${description}`);
  }
});
