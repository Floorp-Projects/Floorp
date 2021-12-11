"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "FilterExpressions",
  "resource://gre/modules/components-utils/FilterExpressions.jsm"
);

// Basic JEXL tests
add_task(async function() {
  let val;
  // Test that basic expressions work
  val = await FilterExpressions.eval("2+2");
  equal(val, 4, "basic expression works");

  // Test that multiline expressions work
  val = await FilterExpressions.eval(`
    2
    +
    2
  `);
  equal(val, 4, "multiline expression works");

  // Test that it reads from the context correctly.
  val = await FilterExpressions.eval("first + second + 3", {
    first: 1,
    second: 2,
  });
  equal(val, 6, "context is available to filter expressions");
});

// Date tests
add_task(async function() {
  let val;
  // Test has a date transform
  val = await FilterExpressions.eval('"2016-04-22"|date');
  const d = new Date(Date.UTC(2016, 3, 22)); // months are 0 based
  equal(val.toString(), d.toString(), "Date transform works");

  // Test dates are comparable
  const context = { someTime: Date.UTC(2016, 0, 1) };
  val = await FilterExpressions.eval('"2015-01-01"|date < someTime', context);
  ok(val, "dates are comparable with less-than");
  val = await FilterExpressions.eval('"2017-01-01"|date > someTime', context);
  ok(val, "dates are comparable with greater-than");
});

// Sampling tests
add_task(async function() {
  let val;
  // Test stable sample returns true for matching samples
  val = await FilterExpressions.eval('["test"]|stableSample(1)');
  ok(val, "Stable sample returns true for 100% sample");

  // Test stable sample returns true for matching samples
  val = await FilterExpressions.eval('["test"]|stableSample(0)');
  ok(!val, "Stable sample returns false for 0% sample");

  // Test stable sample for known samples
  val = await FilterExpressions.eval('["test-1"]|stableSample(0.5)');
  ok(val, "Stable sample returns true for a known sample");
  val = await FilterExpressions.eval('["test-4"]|stableSample(0.5)');
  ok(!val, "Stable sample returns false for a known sample");

  // Test bucket sample for known samples
  val = await FilterExpressions.eval('["test-1"]|bucketSample(0, 5, 10)');
  ok(val, "Bucket sample returns true for a known sample");
  val = await FilterExpressions.eval('["test-4"]|bucketSample(0, 5, 10)');
  ok(!val, "Bucket sample returns false for a known sample");
});

// Preference tests
add_task(async function() {
  let val;
  // Compare the value of the preference
  Services.prefs.setIntPref("normandy.test.value", 3);
  registerCleanupFunction(() =>
    Services.prefs.clearUserPref("normandy.test.value")
  );

  val = await FilterExpressions.eval(
    '"normandy.test.value"|preferenceValue == 3'
  );
  ok(val, "preferenceValue expression compares against preference values");
  val = await FilterExpressions.eval(
    '"normandy.test.value"|preferenceValue == "test"'
  );
  ok(!val, "preferenceValue expression fails value checks appropriately");

  // preferenceValue can take a default value as an optional argument, which
  // defaults to `undefined`.
  val = await FilterExpressions.eval(
    '"normandy.test.default"|preferenceValue(false) == false'
  );
  ok(
    val,
    "preferenceValue takes optional 'default value' param for prefs without set values"
  );
  val = await FilterExpressions.eval(
    '"normandy.test.value"|preferenceValue(5) == 5'
  );
  ok(
    !val,
    "preferenceValue default param is not returned for prefs with set values"
  );

  // Compare if the preference is user set
  val = await FilterExpressions.eval(
    '"normandy.test.isSet"|preferenceIsUserSet == true'
  );
  ok(
    !val,
    "preferenceIsUserSet expression determines if preference is set at all"
  );
  val = await FilterExpressions.eval(
    '"normandy.test.value"|preferenceIsUserSet == true'
  );
  ok(
    val,
    "preferenceIsUserSet expression determines if user's preference has been set"
  );

  // Compare if the preference has _any_ value, whether it's user-set or default,
  val = await FilterExpressions.eval(
    '"normandy.test.nonexistant"|preferenceExists == true'
  );
  ok(
    !val,
    "preferenceExists expression determines if preference exists at all"
  );
  val = await FilterExpressions.eval(
    '"normandy.test.value"|preferenceExists == true'
  );
  ok(val, "preferenceExists expression fails existence check appropriately");
});

// keys tests
add_task(async function testKeys() {
  let val;

  // Test an object defined in JEXL
  val = await FilterExpressions.eval("{foo: 1, bar: 2}|keys");
  Assert.deepEqual(
    new Set(val),
    new Set(["foo", "bar"]),
    "keys returns the keys from an object in JEXL"
  );

  // Test an object in the context
  let context = { ctxObject: { baz: "string", biff: NaN } };
  val = await FilterExpressions.eval("ctxObject|keys", context);

  Assert.deepEqual(
    new Set(val),
    new Set(["baz", "biff"]),
    "keys returns the keys from an object in the context"
  );

  // Test that values from the prototype are not included
  context = { ctxObject: Object.create({ fooProto: 7 }) };
  context.ctxObject.baz = 8;
  context.ctxObject.biff = 5;
  equal(
    await FilterExpressions.eval("ctxObject.fooProto", context),
    7,
    "Prototype properties are accessible via property access"
  );
  val = await FilterExpressions.eval("ctxObject|keys", context);
  Assert.deepEqual(
    new Set(val),
    new Set(["baz", "biff"]),
    "keys does not return properties from the object's prototype chain"
  );

  // Return undefined for non-objects
  equal(
    await FilterExpressions.eval("ctxObject|keys", { ctxObject: 45 }),
    undefined,
    "keys returns undefined for numbers"
  );
  equal(
    await FilterExpressions.eval("ctxObject|keys", { ctxObject: null }),
    undefined,
    "keys returns undefined for null"
  );

  // Object properties are not cached
  let pong = 0;
  context = {
    ctxObject: {
      get ping() {
        return ++pong;
      },
    },
  };
  await FilterExpressions.eval(
    "ctxObject.ping == 0 || ctxObject.ping == 1",
    context
  );
  equal(pong, 2, "Properties are not reifed");
});

add_task(async function testLength() {
  equal(
    await FilterExpressions.eval("[1, null, {a: 2, b: 3}, Infinity]|length"),
    4,
    "length returns the length of the array it's applied to"
  );

  equal(
    await FilterExpressions.eval("[]|length"),
    0,
    "length is zero for an empty array"
  );

  // Should be undefined for non-Arrays
  equal(
    await FilterExpressions.eval("5|length"),
    undefined,
    "length is undefined when applied to numbers"
  );
  equal(
    await FilterExpressions.eval("null|length"),
    undefined,
    "length is undefined when applied to null"
  );
  equal(
    await FilterExpressions.eval("undefined|length"),
    undefined,
    "length is undefined when applied to undefined"
  );
  equal(
    await FilterExpressions.eval("{a: 1, b: 2, c: 3}|length"),
    undefined,
    "length is undefined when applied to non-Array objects"
  );
});

add_task(async function testMapToProperty() {
  Assert.deepEqual(
    await FilterExpressions.eval(
      '[{a: 1}, {a: {b: 10}}, {a: [5,6,7,8]}]|mapToProperty("a")'
    ),
    [1, { b: 10 }, [5, 6, 7, 8]],
    "mapToProperty returns an array of values when applied to an array of objects all with the property defined"
  );

  Assert.deepEqual(
    await FilterExpressions.eval('[]|mapToProperty("a")'),
    [],
    "mapToProperty returns an empty array when applied to an empty array"
  );

  Assert.deepEqual(
    await FilterExpressions.eval('[{a: 1}, {b: 2}, {a: 3}]|mapToProperty("a")'),
    [1, undefined, 3],
    "mapToProperty returns an array with undefined entries where the property is undefined"
  );

  // Should be undefined for non-Arrays
  equal(
    await FilterExpressions.eval('5|mapToProperty("a")'),
    undefined,
    "mapToProperty returns undefined when applied numbers"
  );
  equal(
    await FilterExpressions.eval('null|mapToProperty("a")'),
    undefined,
    "mapToProperty returns undefined when applied null"
  );
  equal(
    await FilterExpressions.eval('undefined|mapToProperty("a")'),
    undefined,
    "mapToProperty returns undefined when applied undefined"
  );
  equal(
    await FilterExpressions.eval('{a: 1, b: 2, c: 3}|mapToProperty("a")'),
    undefined,
    "mapToProperty returns undefined when applied non-Array objects"
  );
});

// intersect tests
add_task(async function testIntersect() {
  let val;

  val = await FilterExpressions.eval("[1, 2, 3] intersect [4, 2, 6, 7, 3]");
  Assert.deepEqual(
    new Set(val),
    new Set([2, 3]),
    "intersect finds the common elements between two lists in JEXL"
  );

  const context = { left: [5, 7], right: [4, 5, 3] };
  val = await FilterExpressions.eval("left intersect right", context);
  Assert.deepEqual(
    new Set(val),
    new Set([5]),
    "intersect finds the common elements between two lists in the context"
  );

  val = await FilterExpressions.eval(
    "['string', 2] intersect [4, 'string', 'other', 3]"
  );
  Assert.deepEqual(
    new Set(val),
    new Set(["string"]),
    "intersect can compare strings"
  );

  // Return undefined when intersecting things that aren't lists.
  equal(
    await FilterExpressions.eval("5 intersect 7"),
    undefined,
    "intersect returns undefined for numbers"
  );
  equal(
    await FilterExpressions.eval("val intersect other", {
      val: null,
      other: null,
    }),
    undefined,
    "intersect returns undefined for null"
  );
  equal(
    await FilterExpressions.eval("5 intersect [1, 2, 5]"),
    undefined,
    "intersect returns undefined if only one operand is a list"
  );
});

add_task(async function test_regExpMatch() {
  let val;

  val = await FilterExpressions.eval('"foobar"|regExpMatch("^foo(.+?)$")');
  Assert.deepEqual(
    new Set(val),
    new Set(["foobar", "bar"]),
    "regExpMatch returns the matches in an array"
  );

  val = await FilterExpressions.eval('"FOObar"|regExpMatch("^foo(.+?)$", "i")');
  Assert.deepEqual(
    new Set(val),
    new Set(["FOObar", "bar"]),
    "regExpMatch accepts flags for matching"
  );

  val = await FilterExpressions.eval('"F00bar"|regExpMatch("^foo(.+?)$", "i")');
  Assert.equal(val, null, "regExpMatch returns null if there are no matches");
});

add_task(async function test_versionCompare() {
  let val;

  val = await FilterExpressions.eval('"1.0.0"|versionCompare("1")');
  ok(val === 0);

  val = await FilterExpressions.eval('"1.0.0"|versionCompare("1.1")');
  ok(val < 0);

  val = await FilterExpressions.eval('"1.0.0"|versionCompare("0.1")');
  ok(val > 0);
});
