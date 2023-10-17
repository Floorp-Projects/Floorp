/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { parseChallengeHeader } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/ChallengeHeaderParser.sys.mjs"
);

add_task(async function test_single_scheme() {
  const TEST_HEADERS = [
    {
      // double quotes
      header: 'Basic realm="test"',
      params: [{ name: "realm", value: "test" }],
    },
    {
      // single quote
      header: "Basic realm='test'",
      params: [{ name: "realm", value: "test" }],
    },
    {
      // multiline
      header: `Basic
       realm='test'`,
      params: [{ name: "realm", value: "test" }],
    },
    {
      // with additional parameter.
      header: 'Basic realm="test", charset="UTF-8"',
      params: [
        { name: "realm", value: "test" },
        { name: "charset", value: "UTF-8" },
      ],
    },
  ];
  for (const { header, params } of TEST_HEADERS) {
    const challenges = parseChallengeHeader(header);
    equal(challenges.length, 1);
    equal(challenges[0].scheme, "Basic");
    deepEqual(challenges[0].params, params);
  }
});

add_task(async function test_realmless_scheme() {
  const TEST_HEADERS = [
    {
      // no parameter
      header: "Custom",
      params: [],
    },
    {
      // one non-realm parameter
      header: "Custom charset='UTF-8'",
      params: [{ name: "charset", value: "UTF-8" }],
    },
  ];

  for (const { header, params } of TEST_HEADERS) {
    const challenges = parseChallengeHeader(header);
    equal(challenges.length, 1);
    equal(challenges[0].scheme, "Custom");
    deepEqual(challenges[0].params, params);
  }
});

add_task(async function test_multiple_schemes() {
  const TEST_HEADERS = [
    {
      header: 'Scheme1 realm="foo", Scheme2 realm="bar"',
      params: [
        [{ name: "realm", value: "foo" }],
        [{ name: "realm", value: "bar" }],
      ],
    },
    {
      header: 'Scheme1 realm="foo", charset="UTF-8", Scheme2 realm="bar"',
      params: [
        [
          { name: "realm", value: "foo" },
          { name: "charset", value: "UTF-8" },
        ],
        [{ name: "realm", value: "bar" }],
      ],
    },
    {
      header: `Scheme1 realm="foo",
                       charset="UTF-8",
               Scheme2 realm="bar"`,
      params: [
        [
          { name: "realm", value: "foo" },
          { name: "charset", value: "UTF-8" },
        ],
        [{ name: "realm", value: "bar" }],
      ],
    },
  ];
  for (const { header, params } of TEST_HEADERS) {
    const challenges = parseChallengeHeader(header);
    equal(challenges.length, 2);
    equal(challenges[0].scheme, "Scheme1");
    deepEqual(challenges[0].params, params[0]);
    equal(challenges[1].scheme, "Scheme2");
    deepEqual(challenges[1].params, params[1]);
  }
});

add_task(async function test_digest_scheme() {
  const header = `Digest
    realm="http-auth@example.org",
    qop="auth, auth-int",
    algorithm=SHA-256,
    nonce="7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v",
    opaque="FQhe/qaU925kfnzjCev0ciny7QMkPqMAFRtzCUYo5tdS"`;

  const challenges = parseChallengeHeader(header);
  equal(challenges.length, 1);
  equal(challenges[0].scheme, "Digest");

  // Note: we are not doing a deepEqual check here, because one of the params
  // actually contains a `,` inside quotes for its value, which will not be
  // handled properly by the current ChallengeHeaderParser. See Bug 1857847.
  const realmParam = challenges[0].params.find(param => param.name === "realm");
  ok(realmParam);
  equal(realmParam.value, "http-auth@example.org");

  // Once Bug 1857847 is addressed, this should start failing and can be
  // switched to deepEqual.
  notDeepEqual(
    challenges[0].params,
    [
      { name: "realm", value: "http-auth@example.org" },
      { name: "qop", value: "auth, auth-int" },
      { name: "algorithm", value: "SHA-256" },
      { name: "nonce", value: "7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v" },
      { name: "opaque", value: "FQhe/qaU925kfnzjCev0ciny7QMkPqMAFRtzCUYo5tdS" },
    ],
    "notDeepEqual should be changed to deepEqual when Bug 1857847 is fixed"
  );
});
