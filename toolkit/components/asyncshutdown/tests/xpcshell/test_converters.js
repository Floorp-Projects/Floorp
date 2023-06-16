/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Test conversion between nsIPropertyBag and JS values.
 */

var PropertyBagConverter =
  new asyncShutdownService.wrappedJSObject._propertyBagConverter();

function run_test() {
  test_conversions();
}

function normalize(obj) {
  if (obj === undefined) {
    return null;
  }
  if (obj == null || typeof obj != "object") {
    return obj;
  }
  if (Array.isArray(obj)) {
    return obj.map(normalize);
  }
  let result = {};
  for (let k of Object.keys(obj).sort()) {
    result[k] = normalize(obj[k]);
  }
  return result;
}

function test_conversions() {
  const SAMPLES = [
    // Simple values
    1,
    true,
    "string",
    null,
    undefined,
    // Object
    {
      a: 1,
      b: true,
      c: "string",
      d: 0.5,
      e: [2, false, "another string", 0.3],
      f: [],
      g: {
        a2: 1,
        b2: true,
        c2: "string",
        d2: 0.5,
        e2: [2, false, "another string", 0.3],
        f2: [],
        g2: [
          {
            a3: 1,
            b3: true,
            c3: "string",
            d3: 0.5,
            e3: [2, false, "another string", 0.3],
            f3: [],
            g3: {},
          },
        ],
        h2: null,
      },
      h: null,
    },
    // Array
    [1, 2, 3],
    // Array of objects
    [[1, 2], { a: 1, b: "string" }, null],
  ];

  for (let sample of SAMPLES) {
    let stringified = JSON.stringify(normalize(sample), null, "\t");
    info("Testing conversions of " + stringified);
    let rewrites = [sample];
    for (let i = 1; i < 3; ++i) {
      let source = rewrites[i - 1];
      let bag = PropertyBagConverter.jsValueToPropertyBag(source);
      Assert.ok(bag instanceof Ci.nsIPropertyBag, "The bag is a property bag");
      let dest = PropertyBagConverter.propertyBagToJsValue(bag);
      let restringified = JSON.stringify(normalize(dest), null, "\t");
      info("Comparing");
      info(stringified);
      info(restringified);
      Assert.deepEqual(sample, dest, "Testing after " + i + " conversions");
      rewrites.push(dest);
    }
  }
}
