/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const {utils: Cu} = Components;

Cu.import("chrome://marionette/content/element.js");

class DOMElement {
  constructor(tagName, attrs = {}) {
    this.tagName = tagName;
    this.localName = tagName;

    for (let attr in attrs) {
      this[attr] = attrs[attr];
    }

    if (this.localName == "option") {
      this.selected = false;
    }

    if (this.localName == "input" && ["checkbox", "radio"].includes(this.type)) {
      this.checked = false;
    }
  }

  get nodeType() { return 1; }
  get ELEMENT_NODE() { return 1; }

  getBoundingClientRect() {
    return {
      top: 0,
      left: 0,
      width: 100,
      height: 100,
    };
  }
};

const domEl = new DOMElement("p");

add_test(function test_isSelected() {
  let checkbox = new DOMElement("input", {type: "checkbox"});
  ok(!element.isSelected(checkbox));
  checkbox.checked = true;
  ok(element.isSelected(checkbox));

  // selected is not a property of <input type=checkbox>
  checkbox.selected = true;
  checkbox.checked = false;
  ok(!element.isSelected(checkbox));

  let option = new DOMElement("option");
  ok(!element.isSelected(option));
  option.selected = true;
  ok(element.isSelected(option));

  // checked is not a property of <option>
  option.checked = true;
  option.selected = false;
  ok(!element.isSelected(option));

  // anything else should not be selected
  for (let typ of [domEl, undefined, null, "foo", true, [], {}]) {
    ok(!element.isSelected(typ));
  }

  run_next_test();
});

add_test(function test_coordinates() {
  let p = element.coordinates(domEl);
  ok(p.hasOwnProperty("x"));
  ok(p.hasOwnProperty("y"));
  equal("number", typeof p.x);
  equal("number", typeof p.y);

  deepEqual({x: 50, y: 50}, element.coordinates(domEl));
  deepEqual({x: 10, y: 10}, element.coordinates(domEl, 10, 10));
  deepEqual({x: -5, y: -5}, element.coordinates(domEl, -5, -5));

  Assert.throws(() => element.coordinates(null));

  Assert.throws(() => element.coordinates(domEl, "string", undefined));
  Assert.throws(() => element.coordinates(domEl, undefined, "string"));
  Assert.throws(() => element.coordinates(domEl, "string", "string"));
  Assert.throws(() => element.coordinates(domEl, {}, undefined));
  Assert.throws(() => element.coordinates(domEl, undefined, {}));
  Assert.throws(() => element.coordinates(domEl, {}, {}));
  Assert.throws(() => element.coordinates(domEl, [], undefined));
  Assert.throws(() => element.coordinates(domEl, undefined, []));
  Assert.throws(() => element.coordinates(domEl, [], []));

  run_next_test();
});

add_test(function test_isWebElementReference() {
  strictEqual(element.isWebElementReference({[element.Key]: "some_id"}), true);
  strictEqual(element.isWebElementReference({[element.LegacyKey]: "some_id"}), true);
  strictEqual(element.isWebElementReference(
      {[element.LegacyKey]: "some_id", [element.Key]: "2"}), true);
  strictEqual(element.isWebElementReference({}), false);
  strictEqual(element.isWebElementReference({"key": "blah"}), false);

  run_next_test();
});
