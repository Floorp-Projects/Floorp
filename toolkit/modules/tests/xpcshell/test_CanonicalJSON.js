const { CanonicalJSON } = ChromeUtils.import(
  "resource://gre/modules/CanonicalJSON.jsm"
);

function stringRepresentation(obj) {
  const clone = JSON.parse(JSON.stringify(obj));
  return JSON.stringify(clone);
}

add_task(async function test_canonicalJSON_should_preserve_array_order() {
  const input = ["one", "two", "three"];
  // No sorting should be done on arrays.
  Assert.equal(CanonicalJSON.stringify(input), '["one","two","three"]');
});

add_task(async function test_canonicalJSON_orders_object_keys() {
  const input = [
    {
      b: ["two", "three"],
      a: ["zero", "one"],
    },
  ];
  Assert.equal(
    CanonicalJSON.stringify(input),
    '[{"a":["zero","one"],"b":["two","three"]}]'
  );
});

add_task(async function test_canonicalJSON_orders_nested_object_keys() {
  const input = [
    {
      b: { d: "d", c: "c" },
      a: { b: "b", a: "a" },
    },
  ];
  Assert.equal(
    CanonicalJSON.stringify(input),
    '[{"a":{"a":"a","b":"b"},"b":{"c":"c","d":"d"}}]'
  );
});

add_task(async function test_canonicalJSON_escapes_unicode_values() {
  Assert.equal(CanonicalJSON.stringify([{ key: "✓" }]), '[{"key":"\\u2713"}]');
  // Unicode codepoints should be output in lowercase.
  Assert.equal(CanonicalJSON.stringify([{ key: "é" }]), '[{"key":"\\u00e9"}]');
});

add_task(async function test_canonicalJSON_escapes_unicode_object_keys() {
  Assert.equal(
    CanonicalJSON.stringify([{ é: "check" }]),
    '[{"\\u00e9":"check"}]'
  );
});

add_task(async function test_canonicalJSON_does_not_alter_input() {
  const records = [
    { foo: "bar", last_modified: "12345", id: "1" },
    { bar: "baz", last_modified: "45678", id: "2" },
  ];
  const serializedJSON = JSON.stringify(records);
  CanonicalJSON.stringify(records);
  Assert.equal(JSON.stringify(records), serializedJSON);
});

add_task(async function test_canonicalJSON_preserves_data() {
  const records = [
    { foo: "bar", last_modified: "12345", id: "1" },
    { bar: "baz", last_modified: "45678", id: "2" },
  ];
  const expected =
    '[{"foo":"bar","id":"1","last_modified":"12345"},' +
    '{"bar":"baz","id":"2","last_modified":"45678"}]';
  Assert.equal(CanonicalJSON.stringify(records), expected);
});

add_task(async function test_canonicalJSON_does_not_add_space_separators() {
  const records = [
    { foo: "bar", last_modified: "12345", id: "1" },
    { bar: "baz", last_modified: "45678", id: "2" },
  ];
  const serialized = CanonicalJSON.stringify(records);
  Assert.ok(!serialized.includes(" "));
});

add_task(async function test_canonicalJSON_serializes_empty_object() {
  Assert.equal(CanonicalJSON.stringify({}), "{}");
});

add_task(async function test_canonicalJSON_serializes_empty_array() {
  Assert.equal(CanonicalJSON.stringify([]), "[]");
});

add_task(async function test_canonicalJSON_serializes_NaN() {
  Assert.equal(CanonicalJSON.stringify(NaN), "null");
});

add_task(async function test_canonicalJSON_serializes_inf() {
  // This isn't part of the JSON standard.
  Assert.equal(CanonicalJSON.stringify(Infinity), "null");
});

add_task(async function test_canonicalJSON_serializes_empty_string() {
  Assert.equal(CanonicalJSON.stringify(""), '""');
});

add_task(async function test_canonicalJSON_escapes_backslashes() {
  Assert.equal(CanonicalJSON.stringify("This\\and this"), '"This\\\\and this"');
});

add_task(async function test_canonicalJSON_handles_signed_zeros() {
  // do_check_eq doesn't support comparison of -0 and 0 properly.
  Assert.ok(CanonicalJSON.stringify(-0) === "-0");
  Assert.ok(CanonicalJSON.stringify(0) === "0");
});

add_task(async function test_canonicalJSON_with_deeply_nested_dicts() {
  const records = [
    {
      a: {
        b: "b",
        a: "a",
        c: {
          b: "b",
          a: "a",
          c: ["b", "a", "c"],
          d: { b: "b", a: "a" },
          id: "1",
          e: 1,
          f: [2, 3, 1],
          g: {
            2: 2,
            3: 3,
            1: {
              b: "b",
              a: "a",
              c: "c",
            },
          },
        },
      },
      id: "1",
    },
  ];
  const expected =
    '[{"a":{"a":"a","b":"b","c":{"a":"a","b":"b","c":["b","a","c"],' +
    '"d":{"a":"a","b":"b"},"e":1,"f":[2,3,1],"g":{' +
    '"1":{"a":"a","b":"b","c":"c"},"2":2,"3":3},"id":"1"}},"id":"1"}]';

  Assert.equal(CanonicalJSON.stringify(records), expected);
});

add_task(async function test_canonicalJSON_should_use_scientific_notation() {
  /*
  We globally follow the Python float representation, except for exponents < 10
  where we drop the leading zero
  */
  Assert.equal(CanonicalJSON.stringify(0.00099), "0.00099");
  Assert.equal(CanonicalJSON.stringify(0.000011), "0.000011");
  Assert.equal(CanonicalJSON.stringify(0.0000011), "0.0000011");
  Assert.equal(CanonicalJSON.stringify(0.000001), "0.000001");
  Assert.equal(CanonicalJSON.stringify(0.00000099), "9.9e-7");
  Assert.equal(CanonicalJSON.stringify(0.0000001), "1e-7");
  Assert.equal(CanonicalJSON.stringify(0.000000930258908), "9.30258908e-7");
  Assert.equal(CanonicalJSON.stringify(0.00000000000068272), "6.8272e-13");
  Assert.equal(
    CanonicalJSON.stringify(Math.pow(10, 20)),
    "100000000000000000000"
  );
  Assert.equal(CanonicalJSON.stringify(Math.pow(10, 21)), "1e+21");
  Assert.equal(
    CanonicalJSON.stringify(Math.pow(10, 15) + 0.1),
    "1000000000000000.1"
  );
  Assert.equal(
    CanonicalJSON.stringify(Math.pow(10, 16) * 1.1),
    "11000000000000000"
  );
});
