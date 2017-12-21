/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Test conversion between nsIPropertyBag and JS values.
 */

var PropertyBagConverter = asyncShutdownService.wrappedJSObject._propertyBagConverter;

function run_test() {
  test_conversions();
}

function normalize(obj) {
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

    // Objects
    {
      a: 1,
      b: true,
      c: "string",
      d: .5,
      e: [2, false, "another string", .3],
      f: [],
      g: {
        a2: 1,
        b2: true,
        c2: "string",
        d2: .5,
        e2: [2, false, "another string", .3],
        f2: [],
        g2: [{
          a3: 1,
          b3: true,
          c3: "string",
          d3: .5,
          e3: [2, false, "another string", .3],
          f3: [],
          g3: {}
        }]
      }
    }];

  for (let sample of SAMPLES) {
    let stringified = JSON.stringify(normalize(sample), null, "\t");
    info("Testing conversions of " + stringified);
    let rewrites = [sample];
    for (let i = 1; i < 3; ++i) {
      let source = rewrites[i - 1];
      let bag = PropertyBagConverter.fromValue(source);
      info(" => " + bag);
      if (source == null) {
        Assert.ok(bag == null, "The bag is null");
      } else if (typeof source == "object") {
        Assert.ok(bag instanceof Ci.nsIPropertyBag, "The bag is a property bag");
      } else {
        Assert.ok(typeof bag != "object", "The bag is not an object");
      }
      let dest = PropertyBagConverter.toValue(bag);
      let restringified = JSON.stringify(normalize(dest), null, "\t");
      info("Comparing");
      info(stringified);
      info(restringified);
      Assert.deepEqual(sample, dest, "Testing after " + i + " conversions");
      rewrites.push(dest);
    }
  }
}
