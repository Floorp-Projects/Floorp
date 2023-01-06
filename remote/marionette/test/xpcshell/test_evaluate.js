const { evaluate } = ChromeUtils.importESModule(
  "chrome://remote/content/marionette/evaluate.sys.mjs"
);

add_test(function test_acyclic() {
  evaluate.assertAcyclic({});

  Assert.throws(() => {
    const obj = {};
    obj.reference = obj;
    evaluate.assertAcyclic(obj);
  }, /JavaScriptError/);

  // custom message
  const cyclic = {};
  cyclic.reference = cyclic;
  Assert.throws(
    () => evaluate.assertAcyclic(cyclic, "", RangeError),
    RangeError
  );
  Assert.throws(
    () => evaluate.assertAcyclic(cyclic, "foo"),
    /JavaScriptError: foo/
  );
  Assert.throws(
    () => evaluate.assertAcyclic(cyclic, "bar", RangeError),
    /RangeError: bar/
  );

  run_next_test();
});

add_test(function test_isCyclic_noncyclic() {
  for (const type of [true, 42, "foo", [], {}, null, undefined]) {
    ok(!evaluate.isCyclic(type));
  }

  run_next_test();
});

add_test(function test_isCyclic_object() {
  const obj = {};
  obj.reference = obj;
  ok(evaluate.isCyclic(obj));

  run_next_test();
});

add_test(function test_isCyclic_array() {
  const arr = [];
  arr.push(arr);
  ok(evaluate.isCyclic(arr));

  run_next_test();
});

add_test(function test_isCyclic_arrayInObject() {
  const arr = [];
  arr.push(arr);
  ok(evaluate.isCyclic({ arr }));

  run_next_test();
});

add_test(function test_isCyclic_objectInArray() {
  const obj = {};
  obj.reference = obj;
  ok(evaluate.isCyclic([obj]));

  run_next_test();
});
