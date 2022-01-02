const { ObjectUtils } = ChromeUtils.import(
  "resource://gre/modules/ObjectUtils.jsm"
);

add_task(async function test_deepEqual() {
  let deepEqual = ObjectUtils.deepEqual.bind(ObjectUtils);
  // CommonJS 7.2
  Assert.ok(
    deepEqual(new Date(2000, 3, 14), new Date(2000, 3, 14)),
    "deepEqual date"
  );
  Assert.ok(deepEqual(new Date(NaN), new Date(NaN)), "deepEqual invalid dates");

  Assert.ok(!deepEqual(new Date(), new Date(2000, 3, 14)), "deepEqual date");

  let now = Date.now();
  Assert.ok(
    !deepEqual(new Date(now), now),
    "Dates and times should not be equal"
  );
  Assert.ok(
    !deepEqual(now, new Date(now)),
    "Times and dates should not be equal"
  );

  Assert.ok(!deepEqual("", {}), "Objects and primitives should not be equal");
  Assert.ok(!deepEqual(/a/, "a"), "RegExps and strings should not be equal");
  Assert.ok(!deepEqual("a", /a/), "Strings and RegExps should not be equal");

  // 7.3
  Assert.ok(deepEqual(/a/, /a/));
  Assert.ok(deepEqual(/a/g, /a/g));
  Assert.ok(deepEqual(/a/i, /a/i));
  Assert.ok(deepEqual(/a/m, /a/m));
  Assert.ok(deepEqual(/a/gim, /a/gim));
  Assert.ok(!deepEqual(/ab/, /a/));
  Assert.ok(!deepEqual(/a/g, /a/));
  Assert.ok(!deepEqual(/a/i, /a/));
  Assert.ok(!deepEqual(/a/m, /a/));
  Assert.ok(!deepEqual(/a/gim, /a/im));

  let re1 = /a/;
  re1.lastIndex = 3;
  Assert.ok(!deepEqual(re1, /a/));

  // 7.4
  Assert.ok(deepEqual(4, "4"), "deepEqual == check");
  Assert.ok(deepEqual(true, 1), "deepEqual == check");
  Assert.ok(!deepEqual(4, "5"), "deepEqual == check");

  // 7.5
  // having the same number of owned properties && the same set of keys
  Assert.ok(deepEqual({ a: 4 }, { a: 4 }));
  Assert.ok(deepEqual({ a: 4, b: "2" }, { a: 4, b: "2" }));
  Assert.ok(deepEqual([4], ["4"]));
  Assert.ok(!deepEqual({ a: 4 }, { a: 4, b: true }));
  Assert.ok(deepEqual(["a"], { 0: "a" }));

  let a1 = [1, 2, 3];
  let a2 = [1, 2, 3];
  a1.a = "test";
  a1.b = true;
  a2.b = true;
  a2.a = "test";
  Assert.ok(!deepEqual(Object.keys(a1), Object.keys(a2)));
  Assert.ok(deepEqual(a1, a2));

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

  Assert.ok(deepEqual(nb1, nb2));

  nameBuilder2.prototype = Object;
  nb2 = new nameBuilder2("Ryan", "Dahl");
  Assert.ok(!deepEqual(nb1, nb2));

  // String literal + object
  Assert.ok(!deepEqual("a", {}));

  // Make sure deepEqual doesn't loop forever on circular refs

  let b = {};
  b.b = b;

  let c = {};
  c.b = c;

  try {
    Assert.ok(!deepEqual(b, c));
  } catch (e) {
    Assert.ok(true, "Didn't recurse infinitely.");
  }

  let e = { a: 3, b: 4 };
  let f = { b: 4, a: 3 };

  function checkEquiv() {
    return arguments;
  }

  Assert.ok(deepEqual(checkEquiv(e, f), checkEquiv(f, e)));
});

add_task(async function test_isEmpty() {
  Assert.ok(ObjectUtils.isEmpty(""), "Empty strings should be empty");
  Assert.ok(ObjectUtils.isEmpty(0), "0 should be empty");
  Assert.ok(ObjectUtils.isEmpty(NaN), "NaN should be empty");
  Assert.ok(ObjectUtils.isEmpty(), "Undefined should be empty");
  Assert.ok(ObjectUtils.isEmpty(null), "Null should be empty");
  Assert.ok(ObjectUtils.isEmpty(false), "False should be empty");

  Assert.ok(
    ObjectUtils.isEmpty({}),
    "Objects without properties should be empty"
  );
  Assert.ok(
    ObjectUtils.isEmpty(
      Object.defineProperty({}, "a", {
        value: 1,
        enumerable: false,
        configurable: true,
        writable: true,
      })
    ),
    "Objects without enumerable properties should be empty"
  );
  Assert.ok(ObjectUtils.isEmpty([]), "Arrays without elements should be empty");

  Assert.ok(
    !ObjectUtils.isEmpty([1]),
    "Arrays with elements should not be empty"
  );
  Assert.ok(
    !ObjectUtils.isEmpty({ a: 1 }),
    "Objects with properties should not be empty"
  );

  function A() {}
  A.prototype.b = 2;
  Assert.ok(
    !ObjectUtils.isEmpty(new A()),
    "Objects with inherited enumerable properties should not be empty"
  );
});
