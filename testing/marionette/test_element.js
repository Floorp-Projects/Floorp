/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const {utils: Cu} = Components;

Cu.import("chrome://marionette/content/element.js");

let el = {
  getBoundingClientRect: function() {
    return {
      top: 0,
      left: 0,
      width: 100,
      height: 100,
    };
  }
};

add_test(function test_coordinates() {
  let p = element.coordinates(el);
  ok(p.hasOwnProperty("x"));
  ok(p.hasOwnProperty("y"));
  equal("number", typeof p.x);
  equal("number", typeof p.y);

  deepEqual({x: 50, y: 50}, element.coordinates(el));
  deepEqual({x: 10, y: 10}, element.coordinates(el, 10, 10));
  deepEqual({x: -5, y: -5}, element.coordinates(el, -5, -5));

  Assert.throws(() => element.coordinates(null));

  Assert.throws(() => element.coordinates(el, "string", undefined));
  Assert.throws(() => element.coordinates(el, undefined, "string"));
  Assert.throws(() => element.coordinates(el, "string", "string"));
  Assert.throws(() => element.coordinates(el, {}, undefined));
  Assert.throws(() => element.coordinates(el, undefined, {}));
  Assert.throws(() => element.coordinates(el, {}, {}));
  Assert.throws(() => element.coordinates(el, [], undefined));
  Assert.throws(() => element.coordinates(el, undefined, []));
  Assert.throws(() => element.coordinates(el, [], []));

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
