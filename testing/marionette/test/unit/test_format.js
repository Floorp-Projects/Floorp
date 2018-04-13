/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const {pprint, truncate} = ChromeUtils.import("chrome://marionette/content/format.js", {});

const MAX_STRING_LENGTH = 250;
const HALF = "x".repeat(MAX_STRING_LENGTH / 2);

add_test(function test_pprint() {
  equal('[object Object] {"foo":"bar"}', pprint`${{foo: "bar"}}`);

  equal("[object Number] 42", pprint`${42}`);
  equal("[object Boolean] true", pprint`${true}`);
  equal("[object Undefined] undefined", pprint`${undefined}`);
  equal("[object Null] null", pprint`${null}`);

  let complexObj = {toJSON: () => "foo"};
  equal('[object Object] "foo"', pprint`${complexObj}`);

  let cyclic = {};
  cyclic.me = cyclic;
  equal("[object Object] <cyclic object value>", pprint`${cyclic}`);

  let el = {
    hasAttribute: attr => attr in el,
    getAttribute: attr => attr in el ? el[attr] : null,
    nodeType: 1,
    localName: "input",
    id: "foo",
    class: "a b",
    href: "#",
    name: "bar",
    src: "s",
    type: "t",
  };
  equal('<input id="foo" class="a b" href="#" name="bar" src="s" type="t">',
      pprint`${el}`);

  run_next_test();
});

add_test(function test_truncate_empty() {
  equal(truncate``, "");
  run_next_test();
});

add_test(function test_truncate_noFields() {
  equal(truncate`foo bar`, "foo bar");
  run_next_test();
});

add_test(function test_truncate_multipleFields() {
  equal(truncate`${0}`, "0");
  equal(truncate`${1}${2}${3}`, "123");
  equal(truncate`a${1}b${2}c${3}`, "a1b2c3");
  run_next_test();
});

add_test(function test_truncate_primitiveFields() {
  equal(truncate`${123}`, "123");
  equal(truncate`${true}`, "true");
  equal(truncate`${null}`, "");
  equal(truncate`${undefined}`, "");
  run_next_test();
});

add_test(function test_truncate_string() {
  equal(truncate`${"foo"}`, "foo");
  equal(truncate`${"x".repeat(250)}`, "x".repeat(250));
  equal(truncate`${"x".repeat(260)}`, `${HALF} ... ${HALF}`);
  run_next_test();
});

add_test(function test_truncate_array() {
  equal(truncate`${["foo"]}`, JSON.stringify(["foo"]));
  equal(truncate`${"foo"} ${["bar"]}`, `foo ${JSON.stringify(["bar"])}`);
  equal(truncate`${["x".repeat(260)]}`, JSON.stringify([`${HALF} ... ${HALF}`]));

  run_next_test();
});

add_test(function test_truncate_object() {
  equal(truncate`${{}}`, JSON.stringify({}));
  equal(truncate`${{foo: "bar"}}`, JSON.stringify({foo: "bar"}));
  equal(truncate`${{foo: "x".repeat(260)}}`, JSON.stringify({foo: `${HALF} ... ${HALF}`}));
  equal(truncate`${{foo: ["bar"]}}`, JSON.stringify({foo: ["bar"]}));
  equal(truncate`${{foo: ["bar", {baz: 42}]}}`, JSON.stringify({foo: ["bar", {baz: 42}]}));

  let complex = {
    toString() { return "hello world"; },
  };
  equal(truncate`${complex}`, "hello world");

  let longComplex = {
    toString() { return "x".repeat(260); },
  };
  equal(truncate`${longComplex}`, `${HALF} ... ${HALF}`);

  run_next_test();
});
