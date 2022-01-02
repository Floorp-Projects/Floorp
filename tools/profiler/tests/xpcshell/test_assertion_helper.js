add_task(function setup() {
  // With the default reporter, an assertion doesn't throw if it fails, it
  // merely report the result to the reporter and then go on. But in this test
  // we want that a failure really throws, so that we can actually assert that
  // it throws in case of failures!
  // That's why we disable the default repoter here.
  // I noticed that this line needs to be in an add_task (or possibly run_test)
  // function. If put outside this will crash the test.
  Assert.setReporter(null);
});

add_task(function test_objectContains() {
  const fixture = {
    foo: "foo",
    bar: "bar",
  };

  Assert.objectContains(fixture, { foo: "foo" }, "Matches one property value");
  Assert.objectContains(
    fixture,
    { foo: "foo", bar: "bar" },
    "Matches both properties"
  );
  Assert.objectContainsOnly(
    fixture,
    { foo: "foo", bar: "bar" },
    "Matches both properties"
  );
  Assert.throws(
    () => Assert.objectContainsOnly(fixture, { foo: "foo" }),
    /AssertionError/,
    "Fails if some properties are missing"
  );
  Assert.throws(
    () => Assert.objectContains(fixture, { foo: "bar" }),
    /AssertionError/,
    "Fails if the value for a present property is wrong"
  );
  Assert.throws(
    () => Assert.objectContains(fixture, { hello: "world" }),
    /AssertionError/,
    "Fails if an expected property is missing"
  );
  Assert.throws(
    () => Assert.objectContains(fixture, { foo: "foo", hello: "world" }),
    /AssertionError/,
    "Fails if some properties are present but others are missing"
  );
});

add_task(function test_objectContains_expectations() {
  const fixture = {
    foo: "foo",
    bar: "bar",
    num: 42,
    nested: {
      nestedFoo: "nestedFoo",
      nestedBar: "nestedBar",
    },
  };

  Assert.objectContains(
    fixture,
    {
      foo: Expect.stringMatches(/^fo/),
      bar: Expect.stringContains("ar"),
      num: Expect.number(),
      nested: Expect.objectContainsOnly({
        nestedFoo: Expect.stringMatches(/[Ff]oo/),
        nestedBar: Expect.stringMatches(/[Bb]ar/),
      }),
    },
    "Supports expectations"
  );
  Assert.objectContainsOnly(
    fixture,
    {
      foo: Expect.stringMatches(/^fo/),
      bar: Expect.stringContains("ar"),
      num: Expect.number(),
      nested: Expect.objectContains({
        nestedFoo: Expect.stringMatches(/[Ff]oo/),
      }),
    },
    "Supports expectations"
  );

  Assert.objectContains(fixture, {
    num: val => Assert.greater(val, 40),
  });

  // Failed expectations
  Assert.throws(
    () =>
      Assert.objectContains(fixture, {
        foo: Expect.stringMatches(/bar/),
      }),
    /AssertionError/,
    "Expect.stringMatches shouldn't match when the value is unexpected"
  );
  Assert.throws(
    () =>
      Assert.objectContains(fixture, {
        foo: Expect.stringContains("bar"),
      }),
    /AssertionError/,
    "Expect.stringContains shouldn't match when the value is unexpected"
  );
  Assert.throws(
    () =>
      Assert.objectContains(fixture, {
        foo: Expect.number(),
      }),
    /AssertionError/,
    "Expect.number shouldn't match when the value isn't a number"
  );
  Assert.throws(
    () =>
      Assert.objectContains(fixture, {
        nested: Expect.objectContains({
          nestedFoo: "bar",
        }),
      }),
    /AssertionError/,
    "Expect.objectContains should throw when the value is unexpected"
  );

  Assert.throws(
    () =>
      Assert.objectContains(fixture, {
        num: val => Assert.less(val, 40),
      }),
    /AssertionError/,
    "Expect.objectContains should throw when a function assertion fails"
  );
});

add_task(function test_type_expectations() {
  const fixture = {
    any: "foo",
    string: "foo",
    number: 42,
    boolean: true,
    bigint: 42n,
    symbol: Symbol("foo"),
    object: { foo: "foo" },
    function1() {},
    function2: () => {},
  };

  Assert.objectContains(fixture, {
    any: Expect.any(),
    string: Expect.string(),
    number: Expect.number(),
    boolean: Expect.boolean(),
    bigint: Expect.bigint(),
    symbol: Expect.symbol(),
    object: Expect.object(),
    function1: Expect.function(),
    function2: Expect.function(),
  });
});
