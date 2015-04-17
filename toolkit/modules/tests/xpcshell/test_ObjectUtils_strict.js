"use strict";

let {ObjectUtils} = Components.utils.import("resource://gre/modules/ObjectUtils.jsm", {});
let {Promise} = Components.utils.import("resource://gre/modules/Promise.jsm", {});

add_task(function* init() {
  // The code will cause uncaught rejections on purpose.
  Promise.Debugging.clearUncaughtErrorObservers();
});

add_task(function* test_strict() {
  let loose = { a: 1 };
  let strict = ObjectUtils.strict(loose);

  loose.a; // Should not throw.
  loose.b || undefined; // Should not throw.

  strict.a; // Should not throw.
  Assert.throws(() => strict.b, /No such property: "b"/);
  "b" in strict; // Should not throw.
  strict.b = 2;
  strict.b; // Should not throw.

  Assert.throws(() => strict.c, /No such property: "c"/);
  "c" in strict; // Should not throw.
  loose.c = 3;
  strict.c; // Should not throw.
});

function run_test() {
  run_next_test();
}
