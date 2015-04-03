Components.utils.import("resource://gre/modules/ObjectUtils.jsm");

function run_test() {
  run_next_test();
}

add_task(function* test_deepEqual() {
  let deepEqual = ObjectUtils.deepEqual.bind(ObjectUtils);
  // CommonJS 7.2
  Assert.ok(deepEqual(new Date(2000, 3, 14), new Date(2000, 3, 14)), "deepEqual date");
  Assert.ok(deepEqual(new Date(NaN), new Date(NaN)), "deepEqual invalid dates");

  Assert.ok(!deepEqual(new Date(), new Date(2000, 3, 14)), "deepEqual date");

  // 7.3
  Assert.ok(deepEqual(/a/, /a/));
  Assert.ok(deepEqual(/a/g, /a/g));
  Assert.ok(deepEqual(/a/i, /a/i));
  Assert.ok(deepEqual(/a/m, /a/m));
  Assert.ok(deepEqual(/a/igm, /a/igm));
  Assert.ok(!deepEqual(/ab/, /a/));
  Assert.ok(!deepEqual(/a/g, /a/));
  Assert.ok(!deepEqual(/a/i, /a/));
  Assert.ok(!deepEqual(/a/m, /a/));
  Assert.ok(!deepEqual(/a/igm, /a/im));

  let re1 = /a/;
  re1.lastIndex = 3;
  Assert.ok(!deepEqual(re1, /a/));

  // 7.4
  Assert.ok(deepEqual(4, "4"), "deepEqual == check");
  Assert.ok(deepEqual(true, 1), "deepEqual == check");
  Assert.ok(!deepEqual(4, "5"), "deepEqual == check");

  // 7.5
  // having the same number of owned properties && the same set of keys
  Assert.ok(deepEqual({a: 4}, {a: 4}));
  Assert.ok(deepEqual({a: 4, b: "2"}, {a: 4, b: "2"}));
  Assert.ok(deepEqual([4], ["4"]));
  Assert.ok(!deepEqual({a: 4}, {a: 4, b: true}));
  Assert.ok(deepEqual(["a"], {0: "a"}));

  let a1 = [1, 2, 3];
  let a2 = [1, 2, 3];
  a1.a = "test";
  a1.b = true;
  a2.b = true;
  a2.a = "test";
  Assert.ok(!deepEqual(Object.keys(a1), Object.keys(a2)));
  Assert.ok(deepEqual(a1, a2));

  let nbRoot = {
    toString: function() { return this.first + " " + this.last; }
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
});
