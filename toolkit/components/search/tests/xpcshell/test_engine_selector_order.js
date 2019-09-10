/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  SearchEngineSelector: "resource://gre/modules/SearchEngineSelector.jsm",
});

/**
 * This constant defines the tests for the order. The input is an array of
 * engines that will be constructed. The engine definitions are arrays with
 * fields in order:
 *   name, orderHint, default, defaultPrivate
 *
 * The expected is an array of engine names.
 */
const TESTS = [
  {
    // Basic tests to ensure correct order for default engine.
    input: [
      ["A", 750, "no", "no"],
      ["B", 3000, "no", "no"],
      ["C", 2000, "yes", "no"],
      ["D", 1000, "yes-if-no-other", "no"],
      ["E", 500, "no", "no"],
    ],
    expected: ["C", "B", "D", "A", "E"],
    expectedPrivate: undefined,
  },
  {
    input: [
      ["A", 750, "no", "no"],
      ["B", 3000, "no", "no"],
      ["C", 2000, "yes-if-no-other", "no"],
      ["D", 1000, "yes", "no"],
      ["E", 500, "no", "no"],
    ],
    expected: ["D", "B", "C", "A", "E"],
    expectedPrivate: undefined,
  },
  // Check that yes-if-no-other works correctly.
  {
    input: [
      ["A", 750, "no", "no"],
      ["B", 3000, "no", "no"],
      ["C", 2000, "no", "no"],
      ["D", 1000, "yes-if-no-other", "no"],
      ["E", 500, "no", "no"],
    ],
    expected: ["D", "B", "C", "A", "E"],
    expectedPrivate: undefined,
  },
  // Basic tests to ensure correct order with private engine.
  {
    input: [
      ["A", 750, "no", "no"],
      ["B", 3000, "yes-if-no-other", "no"],
      ["C", 2000, "no", "yes"],
      ["D", 1000, "yes", "yes-if-no-other"],
      ["E", 500, "no", "no"],
    ],
    expected: ["D", "C", "B", "A", "E"],
    expectedPrivate: "C",
  },
  {
    input: [
      ["A", 750, "no", "yes-if-no-other"],
      ["B", 3000, "yes-if-no-other", "no"],
      ["C", 2000, "no", "yes"],
      ["D", 1000, "yes", "no"],
      ["E", 500, "no", "no"],
    ],
    expected: ["D", "C", "B", "A", "E"],
    expectedPrivate: "C",
  },
  // Private engine test for yes-if-no-other.
  {
    input: [
      ["A", 750, "no", "yes-if-no-other"],
      ["B", 3000, "yes-if-no-other", "no"],
      ["C", 2000, "no", "no"],
      ["D", 1000, "yes", "no"],
      ["E", 500, "no", "no"],
    ],
    expected: ["D", "A", "B", "C", "E"],
    expectedPrivate: "A",
  },
];

function getConfigUrl(testInput) {
  return (
    "data:application/json," +
    JSON.stringify({
      data: testInput.map(info => ({
        engineName: info[0],
        orderHint: info[1],
        default: info[2],
        defaultPrivate: info[3],
        appliesTo: [
          {
            included: { everywhere: true },
          },
        ],
      })),
    })
  );
}

const engineSelector = new SearchEngineSelector();

add_task(async function() {
  let i = 0;
  for (const test of TESTS) {
    await engineSelector.init(getConfigUrl(test.input));

    const { engines, privateDefault } = engineSelector.fetchEngineConfiguration(
      "us",
      "en-US"
    );
    let names = engines.map(obj => obj.engineName);
    Assert.deepEqual(
      names,
      test.expected,
      `Should have the correct order for the engines: test ${i}`
    );
    Assert.equal(
      privateDefault,
      test.expectedPrivate,
      `Should have the correct selection for the private engine: test ${i++}`
    );
  }
});
