/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test cases borrowed and adapted from:
// https://github.com/joyent/node/blob/6101eb184db77d0b11eb96e48744e57ecce4b73d/test/simple/test-assert.js
// MIT license: http://opensource.org/licenses/MIT

function run_test() {
  let ns = {};
  ChromeUtils.import("resource://testing-common/Assert.jsm", ns);
  let assert = new ns.Assert();

  function makeBlock(f, ...args) {
    return function() {
      return f.apply(assert, args);
    };
  }

  function protoCtrChain(o) {
    let result = [];
    while ((o = o.__proto__)) {
      result.push(o.constructor);
    }
    return result.join();
  }

  function indirectInstanceOf(obj, cls) {
    if (obj instanceof cls) {
      return true;
    }
    let clsChain = protoCtrChain(cls.prototype);
    let objChain = protoCtrChain(obj);
    return objChain.slice(-clsChain.length) === clsChain;
  }

  assert.ok(
    indirectInstanceOf(ns.Assert.AssertionError.prototype, Error),
    "Assert.AssertionError instanceof Error"
  );

  assert.throws(
    makeBlock(assert.ok, false),
    ns.Assert.AssertionError,
    "ok(false)"
  );

  assert.ok(true, "ok(true)");

  assert.ok("test", "ok('test')");

  assert.throws(
    makeBlock(assert.equal, true, false),
    ns.Assert.AssertionError,
    "equal"
  );

  assert.equal(null, null, "equal");

  assert.equal(undefined, undefined, "equal");

  assert.equal(null, undefined, "equal");

  assert.equal(true, true, "equal");

  assert.notEqual(true, false, "notEqual");

  assert.throws(
    makeBlock(assert.notEqual, true, true),
    ns.Assert.AssertionError,
    "notEqual"
  );

  assert.throws(
    makeBlock(assert.strictEqual, 2, "2"),
    ns.Assert.AssertionError,
    "strictEqual"
  );

  assert.throws(
    makeBlock(assert.strictEqual, null, undefined),
    ns.Assert.AssertionError,
    "strictEqual"
  );

  assert.notStrictEqual(2, "2", "notStrictEqual");

  // deepEquals joy!
  // 7.2
  assert.deepEqual(
    new Date(2000, 3, 14),
    new Date(2000, 3, 14),
    "deepEqual date"
  );
  assert.deepEqual(new Date(NaN), new Date(NaN), "deepEqual invalid dates");

  assert.throws(
    makeBlock(assert.deepEqual, new Date(), new Date(2000, 3, 14)),
    ns.Assert.AssertionError,
    "deepEqual date"
  );

  // 7.3
  assert.deepEqual(/a/, /a/);
  assert.deepEqual(/a/g, /a/g);
  assert.deepEqual(/a/i, /a/i);
  assert.deepEqual(/a/m, /a/m);
  assert.deepEqual(/a/gim, /a/gim);
  assert.throws(
    makeBlock(assert.deepEqual, /ab/, /a/),
    ns.Assert.AssertionError
  );
  assert.throws(
    makeBlock(assert.deepEqual, /a/g, /a/),
    ns.Assert.AssertionError
  );
  assert.throws(
    makeBlock(assert.deepEqual, /a/i, /a/),
    ns.Assert.AssertionError
  );
  assert.throws(
    makeBlock(assert.deepEqual, /a/m, /a/),
    ns.Assert.AssertionError
  );
  assert.throws(
    makeBlock(assert.deepEqual, /a/gim, /a/im),
    ns.Assert.AssertionError
  );

  let re1 = /a/;
  re1.lastIndex = 3;
  assert.throws(
    makeBlock(assert.deepEqual, re1, /a/),
    ns.Assert.AssertionError
  );

  // 7.4
  assert.deepEqual(4, "4", "deepEqual == check");
  assert.deepEqual(true, 1, "deepEqual == check");
  assert.throws(
    makeBlock(assert.deepEqual, 4, "5"),
    ns.Assert.AssertionError,
    "deepEqual == check"
  );

  // 7.5
  // having the same number of owned properties && the same set of keys
  assert.deepEqual({ a: 4 }, { a: 4 });
  assert.deepEqual({ a: 4, b: "2" }, { a: 4, b: "2" });
  assert.deepEqual([4], ["4"]);
  assert.throws(
    makeBlock(assert.deepEqual, { a: 4 }, { a: 4, b: true }),
    ns.Assert.AssertionError
  );
  assert.deepEqual(["a"], { 0: "a" });

  let a1 = [1, 2, 3];
  let a2 = [1, 2, 3];
  a1.a = "test";
  a1.b = true;
  a2.b = true;
  a2.a = "test";
  assert.throws(
    makeBlock(assert.deepEqual, Object.keys(a1), Object.keys(a2)),
    ns.Assert.AssertionError
  );
  assert.deepEqual(a1, a2);

  let nbRoot = {
    toString() {
      return this.first + " " + this.last;
    },
  };

  function nameBuilder(first, last) {
    this.first = first;
    this.last = last;
    return this;
  }
  nameBuilder.prototype = nbRoot;

  function nameBuilder2(first, last) {
    this.first = first;
    this.last = last;
    return this;
  }
  nameBuilder2.prototype = nbRoot;

  let nb1 = new nameBuilder("Ryan", "Dahl");
  let nb2 = new nameBuilder2("Ryan", "Dahl");

  assert.deepEqual(nb1, nb2);

  nameBuilder2.prototype = Object;
  nb2 = new nameBuilder2("Ryan", "Dahl");
  assert.throws(
    makeBlock(assert.deepEqual, nb1, nb2),
    ns.Assert.AssertionError
  );

  // String literal + object
  assert.throws(makeBlock(assert.deepEqual, "a", {}), ns.Assert.AssertionError);

  // Testing the throwing
  function thrower(errorConstructor) {
    throw new errorConstructor("test");
  }
  makeBlock(thrower, ns.Assert.AssertionError);
  makeBlock(thrower, ns.Assert.AssertionError);

  // the basic calls work
  assert.throws(
    makeBlock(thrower, ns.Assert.AssertionError),
    ns.Assert.AssertionError,
    "message"
  );
  assert.throws(
    makeBlock(thrower, ns.Assert.AssertionError),
    ns.Assert.AssertionError
  );
  assert.throws(
    makeBlock(thrower, ns.Assert.AssertionError),
    ns.Assert.AssertionError
  );

  // if not passing an error, catch all.
  assert.throws(makeBlock(thrower, TypeError), TypeError);

  // when passing a type, only catch errors of the appropriate type
  let threw = false;
  try {
    assert.throws(makeBlock(thrower, TypeError), ns.Assert.AssertionError);
  } catch (e) {
    threw = true;
    assert.ok(e instanceof TypeError, "type");
  }
  assert.equal(
    true,
    threw,
    "Assert.throws with an explicit error is eating extra errors",
    ns.Assert.AssertionError
  );
  threw = false;

  function ifError(err) {
    if (err) {
      throw err;
    }
  }
  assert.throws(function() {
    ifError(new Error("test error"));
  }, /test error/);

  // make sure that validating using constructor really works
  threw = false;
  try {
    assert.throws(function() {
      throw new Error({});
    }, Array);
  } catch (e) {
    threw = true;
  }
  assert.ok(threw, "wrong constructor validation");

  // use a RegExp to validate error message
  assert.throws(makeBlock(thrower, TypeError), /test/);

  // use a fn to validate error object
  assert.throws(makeBlock(thrower, TypeError), function(err) {
    if (err instanceof TypeError && /test/.test(err)) {
      return true;
    }
    return false;
  });
  // do the same with an arrow function
  assert.throws(makeBlock(thrower, TypeError), err => {
    if (err instanceof TypeError && /test/.test(err)) {
      return true;
    }
    return false;
  });

  function testAssertionMessage(actual, expected) {
    try {
      assert.equal(actual, "");
    } catch (e) {
      assert.equal(
        e.toString(),
        ["AssertionError:", expected, "==", '""'].join(" ")
      );
    }
  }
  testAssertionMessage(undefined, '"undefined"');
  testAssertionMessage(null, "null");
  testAssertionMessage(true, "true");
  testAssertionMessage(false, "false");
  testAssertionMessage(0, "0");
  testAssertionMessage(100, "100");
  testAssertionMessage(NaN, '"NaN"');
  testAssertionMessage(Infinity, '"Infinity"');
  testAssertionMessage(-Infinity, '"-Infinity"');
  testAssertionMessage("", '""');
  testAssertionMessage("foo", '"foo"');
  testAssertionMessage([], "[]");
  testAssertionMessage([1, 2, 3], "[1,2,3]");
  testAssertionMessage(/a/, '"/a/"');
  testAssertionMessage(/abc/gim, '"/abc/gim"');
  testAssertionMessage(function f() {}, '"function f() {}"');
  testAssertionMessage({}, "{}");
  testAssertionMessage({ a: undefined, b: null }, '{"a":"undefined","b":null}');
  testAssertionMessage(
    { a: NaN, b: Infinity, c: -Infinity },
    '{"a":"NaN","b":"Infinity","c":"-Infinity"}'
  );

  // https://github.com/joyent/node/issues/2893
  try {
    assert.throws(function() {
      ifError(null);
    });
  } catch (e) {
    threw = true;
    assert.equal(
      e.message,
      "Error: The 'expected' argument was not supplied to Assert.throws() - false == true"
    );
  }
  assert.ok(threw);

  // https://github.com/joyent/node/issues/5292
  try {
    assert.equal(1, 2);
  } catch (e) {
    assert.equal(e.toString().split("\n")[0], "AssertionError: 1 == 2");
  }

  try {
    assert.equal(1, 2, "oh no");
  } catch (e) {
    assert.equal(e.toString().split("\n")[0], "AssertionError: oh no - 1 == 2");
  }

  // Need to JSON.stringify so that their length is > 128 characters.
  let longArray0 = Array.from(Array(50), (v, i) => i);
  let longArray1 = longArray0.concat([51]);
  try {
    assert.deepEqual(longArray0, longArray1);
  } catch (e) {
    let message = e.toString();
    // Just check that they're both entirely present in the message
    assert.ok(message.includes(JSON.stringify(longArray0)));
    assert.ok(message.includes(JSON.stringify(longArray1)));
  }

  // Test XPCShell-test integration:
  ok(true, "OK, this went well");
  deepEqual(/a/g, /a/g, "deep equal should work on RegExp");
  deepEqual(/a/gim, /a/gim, "deep equal should work on RegExp");
  deepEqual(
    { a: 4, b: "1" },
    { b: "1", a: 4 },
    "deep equal should work on regular Object"
  );
  deepEqual(a1, a2, "deep equal should work on Array with Object properties");

  // Test robustness of reporting:
  equal(
    new ns.Assert.AssertionError({
      actual: {
        toJSON() {
          throw new Error("bam!");
        },
      },
      expected: "foo",
      operator: "=",
    }).message,
    '[object Object] = "foo"'
  );

  let message;
  assert.greater(3, 2);
  try {
    assert.greater(2, 2);
  } catch (e) {
    message = e.toString().split("\n")[0];
  }
  assert.equal(message, "AssertionError: 2 > 2");

  assert.greaterOrEqual(2, 2);
  try {
    assert.greaterOrEqual(1, 2);
  } catch (e) {
    message = e.toString().split("\n")[0];
  }
  assert.equal(message, "AssertionError: 1 >= 2");

  assert.less(1, 2);
  try {
    assert.less(2, 2);
  } catch (e) {
    message = e.toString().split("\n")[0];
  }
  assert.equal(message, "AssertionError: 2 < 2");

  assert.lessOrEqual(2, 2);
  try {
    assert.lessOrEqual(2, 1);
  } catch (e) {
    message = e.toString().split("\n")[0];
  }
  assert.equal(message, "AssertionError: 2 <= 1");

  /* ---- stringMatches ---- */
  assert.stringMatches("hello world", /llo\s/);
  assert.stringMatches("hello world", "llo\\s");
  assert.throws(
    () => assert.stringMatches("hello world", /foo/),
    /^AssertionError: "hello world" matches "\/foo\/"/
  );
  assert.throws(
    () => assert.stringMatches(5, /foo/),
    /^AssertionError: Expected a string for lhs, but "5" isn't a string./
  );
  assert.throws(
    () => assert.stringMatches("foo bar", "+"),
    /^AssertionError: Expected a valid regular expression for rhs, but "\+" isn't one./
  );

  /* ---- stringContains ---- */
  assert.stringContains("hello world", "llo");
  assert.throws(
    () => assert.stringContains(5, "foo"),
    /^AssertionError: Expected a string for both lhs and rhs, but either "5" or "foo" is not a string./
  );

  run_next_test();
}

add_task(async function test_rejects() {
  let ns = {};
  ChromeUtils.import("resource://testing-common/Assert.jsm", ns);
  let assert = new ns.Assert();

  // A helper function to test failures.
  async function checkRejectsFails(err, expected) {
    try {
      await assert.rejects(Promise.reject(err), expected);
      ok(false, "should have thrown");
    } catch (ex) {
      deepEqual(ex, err, "Assert.rejects threw the original unexpected error");
    }
  }

  // A "throwable" error that's not an actual Error().
  let SomeErrorLikeThing = function() {};

  // The actual tests...

  // An explicit error object:
  // An instance to check against.
  await assert.rejects(Promise.reject(new Error("oh no")), Error, "rejected");
  // A regex to match against the message.
  await assert.rejects(Promise.reject(new Error("oh no")), /oh no/, "rejected");

  // Failure cases:
  // An instance to check against that doesn't match.
  await checkRejectsFails(new Error("something else"), SomeErrorLikeThing);
  // A regex that doesn't match.
  await checkRejectsFails(new Error("something else"), /oh no/);

  // Check simple string messages.
  await assert.rejects(Promise.reject("oh no"), /oh no/, "rejected");
  // Wrong message.
  await checkRejectsFails("something else", /oh no/);
});
