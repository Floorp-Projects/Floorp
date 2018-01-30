/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var tempScope = {};
Components.utils.import("resource://gre/modules/Geometry.jsm", tempScope);
var Point = tempScope.Point;
var Rect = tempScope.Rect;

function test() {
  ok(Rect, "Rect class exists");
  for (var fname in tests) {
    tests[fname]();
  }
}

var tests = {
  testGetDimensions() {
    let r = new Rect(5, 10, 100, 50);
    ok(r.left == 5, "rect has correct left value");
    ok(r.top == 10, "rect has correct top value");
    ok(r.right == 105, "rect has correct right value");
    ok(r.bottom == 60, "rect has correct bottom value");
    ok(r.width == 100, "rect has correct width value");
    ok(r.height == 50, "rect has correct height value");
    ok(r.x == 5, "rect has correct x value");
    ok(r.y == 10, "rect has correct y value");
  },

  testIsEmpty() {
    let r = new Rect(0, 0, 0, 10);
    ok(r.isEmpty(), "rect with nonpositive width is empty");
    r = new Rect(0, 0, 10, 0);
    ok(r.isEmpty(), "rect with nonpositive height is empty");
    r = new Rect(0, 0, 10, 10);
    ok(!r.isEmpty(), "rect with positive dimensions is not empty");
  },

  testRestrictTo() {
    let r1 = new Rect(10, 10, 100, 100);
    let r2 = new Rect(50, 50, 100, 100);
    r1.restrictTo(r2);
    ok(r1.equals(new Rect(50, 50, 60, 60)), "intersection is non-empty");

    r1 = new Rect(10, 10, 100, 100);
    r2 = new Rect(120, 120, 100, 100);
    r1.restrictTo(r2);
    ok(r1.isEmpty(), "intersection is empty");

    r1 = new Rect(10, 10, 100, 100);
    r2 = new Rect(0, 0, 0, 0);
    r1.restrictTo(r2);
    ok(r1.isEmpty(), "intersection of rect and empty is empty");

    r1 = new Rect(0, 0, 0, 0);
    r2 = new Rect(0, 0, 0, 0);
    r1.restrictTo(r2);
    ok(r1.isEmpty(), "intersection of empty and empty is empty");
  },

  testExpandToContain() {
    let r1 = new Rect(10, 10, 100, 100);
    let r2 = new Rect(50, 50, 100, 100);
    r1.expandToContain(r2);
    ok(r1.equals(new Rect(10, 10, 140, 140)), "correct expandToContain on intersecting rectangles");

    r1 = new Rect(10, 10, 100, 100);
    r2 = new Rect(120, 120, 100, 100);
    r1.expandToContain(r2);
    ok(r1.equals(new Rect(10, 10, 210, 210)), "correct expandToContain on non-intersecting rectangles");

    r1 = new Rect(10, 10, 100, 100);
    r2 = new Rect(0, 0, 0, 0);
    r1.expandToContain(r2);
    ok(r1.equals(new Rect(10, 10, 100, 100)), "expandToContain of rect and empty is rect");

    r1 = new Rect(10, 10, 0, 0);
    r2 = new Rect(0, 0, 0, 0);
    r1.expandToContain(r2);
    ok(r1.isEmpty(), "expandToContain of empty and empty is empty");
  },

  testSubtract: function testSubtract() {
    function equals(rects1, rects2) {
      return rects1.length == rects2.length && rects1.every(function(r, i) {
        return r.equals(rects2[i]);
      });
    }

    let r1 = new Rect(0, 0, 100, 100);
    let r2 = new Rect(500, 500, 100, 100);
    ok(equals(r1.subtract(r2), [r1]), "subtract area outside of region yields same region");

    r1 = new Rect(0, 0, 100, 100);
    r2 = new Rect(-10, -10, 50, 120);
    ok(equals(r1.subtract(r2), [new Rect(40, 0, 60, 100)]), "subtracting vertical bar from edge leaves one rect");

    r1 = new Rect(0, 0, 100, 100);
    r2 = new Rect(-10, -10, 120, 50);
    ok(equals(r1.subtract(r2), [new Rect(0, 40, 100, 60)]), "subtracting horizontal bar from edge leaves one rect");

    r1 = new Rect(0, 0, 100, 100);
    r2 = new Rect(40, 40, 20, 20);
    ok(equals(r1.subtract(r2), [
      new Rect(0, 0, 40, 100),
      new Rect(40, 0, 20, 40),
      new Rect(40, 60, 20, 40),
      new Rect(60, 0, 40, 100)]),
      "subtracting rect in middle leaves union of rects");
  },
};
