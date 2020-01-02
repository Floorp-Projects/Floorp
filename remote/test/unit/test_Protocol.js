/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Protocol } = ChromeUtils.import("chrome://remote/content/Protocol.jsm");

add_test(function test_sanitize() {
  const tests = [
    [42, 42],
    [{}, {}],
    [[], []],
    [undefined, undefined],
    ["foo", "foo"],
    ["", undefined],
  ];

  // we currently don't do anything based on the key
  const bogusKey = "bogusKey";

  for (const [input, output] of tests) {
    deepEqual(Protocol.sanitize(bogusKey, input), output);
  }

  run_next_test();
});

add_test(function test_sanitize_JSON() {
  const tests = [
    [{}, {}],
    [[], []],
    [{ foo: "bar" }, { foo: "bar" }],
    [{ foo: undefined }, {}],
    [{ foo: { bar: undefined } }, { foo: {} }],
    [{ foo: "" }, {}],
    [{ foo: { bar: "" } }, { foo: {} }],
  ];

  for (const [input, output] of tests) {
    const actual = JSON.stringify(input, Protocol.sanitize);
    const expected = JSON.stringify(output);
    deepEqual(actual, expected);
  }

  run_next_test();
});
