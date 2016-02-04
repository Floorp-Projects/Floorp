"use strict";

var {ObjectUtils} = Components.utils.import("resource://gre/modules/ObjectUtils.jsm", {});
var {PromiseTestUtils} = Components.utils.import("resource://testing-common/PromiseTestUtils.jsm", {});

add_task(function* test_strict() {
  let loose = { a: 1 };
  let strict = ObjectUtils.strict(loose);

  loose.a; // Should not throw.
  loose.b || undefined; // Should not throw.

  strict.a; // Should not throw.
  PromiseTestUtils.expectUncaughtRejection(/No such property: "b"/);
  Assert.throws(() => strict.b, /No such property: "b"/);
  "b" in strict; // Should not throw.
  strict.b = 2;
  strict.b; // Should not throw.

  PromiseTestUtils.expectUncaughtRejection(/No such property: "c"/);
  Assert.throws(() => strict.c, /No such property: "c"/);
  "c" in strict; // Should not throw.
  loose.c = 3;
  strict.c; // Should not throw.
});

function run_test() {
  run_next_test();
}
