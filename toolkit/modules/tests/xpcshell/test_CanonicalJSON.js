const { CanonicalJSON } = Components.utils.import("resource://gre/modules/CanonicalJSON.jsm", {});

function stringRepresentation(obj) {
  const clone = JSON.parse(JSON.stringify(obj));
  return JSON.stringify(clone);
}

add_task(function* test_canonicalJSON_should_preserve_array_order() {
  const input = ['one', 'two', 'three'];
  // No sorting should be done on arrays.
  do_check_eq(CanonicalJSON.stringify(input), '["one","two","three"]');
});

add_task(function* test_canonicalJSON_orders_object_keys() {
  const input = [{
    b: ['two', 'three'],
    a: ['zero', 'one']
  }];
  do_check_eq(
    CanonicalJSON.stringify(input),
    '[{"a":["zero","one"],"b":["two","three"]}]'
  );
});

add_task(function* test_canonicalJSON_orders_nested_object_keys() {
  const input = [{
    b: {d: 'd', c: 'c'},
    a: {b: 'b', a: 'a'}
  }];
  do_check_eq(
    CanonicalJSON.stringify(input),
    '[{"a":{"a":"a","b":"b"},"b":{"c":"c","d":"d"}}]'
  );
});

add_task(function* test_canonicalJSON_escapes_unicode_values() {
  do_check_eq(
    CanonicalJSON.stringify([{key: '✓'}]),
    '[{"key":"\\u2713"}]'
  );
  // Unicode codepoints should be output in lowercase.
  do_check_eq(
    CanonicalJSON.stringify([{key: 'é'}]),
    '[{"key":"\\u00e9"}]'
  );
});

add_task(function* test_canonicalJSON_escapes_unicode_object_keys() {
  do_check_eq(
    CanonicalJSON.stringify([{'é': 'check'}]),
    '[{"\\u00e9":"check"}]'
  );
});


add_task(function* test_canonicalJSON_does_not_alter_input() {
  const records = [
    {'foo': 'bar', 'last_modified': '12345', 'id': '1'},
    {'bar': 'baz', 'last_modified': '45678', 'id': '2'}
  ];
  const serializedJSON = JSON.stringify(records);
  CanonicalJSON.stringify(records);
  do_check_eq(JSON.stringify(records), serializedJSON);
});


add_task(function* test_canonicalJSON_preserves_data() {
  const records = [
    {'foo': 'bar', 'last_modified': '12345', 'id': '1'},
    {'bar': 'baz', 'last_modified': '45678', 'id': '2'},
  ]
  const expected = '[{"foo":"bar","id":"1","last_modified":"12345"},' +
                   '{"bar":"baz","id":"2","last_modified":"45678"}]';
  do_check_eq(CanonicalJSON.stringify(records), expected);
});

add_task(function* test_canonicalJSON_does_not_add_space_separators() {
  const records = [
    {'foo': 'bar', 'last_modified': '12345', 'id': '1'},
    {'bar': 'baz', 'last_modified': '45678', 'id': '2'},
  ]
  const serialized = CanonicalJSON.stringify(records);
  do_check_false(serialized.includes(" "));
});

add_task(function* test_canonicalJSON_serializes_empty_object() {
  do_check_eq(CanonicalJSON.stringify({}), "{}");
});

add_task(function* test_canonicalJSON_serializes_empty_array() {
  do_check_eq(CanonicalJSON.stringify([]), "[]");
});

add_task(function* test_canonicalJSON_serializes_NaN() {
  do_check_eq(CanonicalJSON.stringify(NaN), "null");
});

add_task(function* test_canonicalJSON_serializes_inf() {
  // This isn't part of the JSON standard.
  do_check_eq(CanonicalJSON.stringify(Infinity), "null");
});


add_task(function* test_canonicalJSON_serializes_empty_string() {
  do_check_eq(CanonicalJSON.stringify(""), '""');
});

add_task(function* test_canonicalJSON_escapes_backslashes() {
  do_check_eq(CanonicalJSON.stringify("This\\and this"), '"This\\\\and this"');
});

add_task(function* test_canonicalJSON_handles_signed_zeros() {
  // do_check_eq doesn't support comparison of -0 and 0 properly.
  do_check_true(CanonicalJSON.stringify(-0) === '-0');
  do_check_true(CanonicalJSON.stringify(0) === '0');
});


add_task(function* test_canonicalJSON_with_deeply_nested_dicts() {
  const records = [{
    'a': {
      'b': 'b',
      'a': 'a',
      'c': {
        'b': 'b',
        'a': 'a',
        'c': ['b', 'a', 'c'],
        'd': {'b': 'b', 'a': 'a'},
        'id': '1',
        'e': 1,
        'f': [2, 3, 1],
        'g': {2: 2, 3: 3, 1: {
          'b': 'b', 'a': 'a', 'c': 'c'}}}},
    'id': '1'}]
  const expected =
    '[{"a":{"a":"a","b":"b","c":{"a":"a","b":"b","c":["b","a","c"],' +
    '"d":{"a":"a","b":"b"},"e":1,"f":[2,3,1],"g":{' +
    '"1":{"a":"a","b":"b","c":"c"},"2":2,"3":3},"id":"1"}},"id":"1"}]';

  do_check_eq(CanonicalJSON.stringify(records), expected);
});

function run_test() {
  run_next_test();
}
