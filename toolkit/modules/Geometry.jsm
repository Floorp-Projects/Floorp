/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["Point", "Rect"];

/**
 * Simple Point class.
 *
 * Any method that takes an x and y may also take a point.
 */
this.Point = function Point(x, y) {
  this.set(x, y);
}

Point.prototype = {
  clone: function clone() {
    return new Point(this.x, this.y);
  },

  set: function set(x, y) {
    this.x = x;
    this.y = y;
    return this;
  },

  equals: function equals(x, y) {
    return this.x == x && this.y == y;
  },

  toString: function toString() {
    return "(" + this.x + "," + this.y + ")";
  },

  map: function map(f) {
    this.x = f.call(this, this.x);
    this.y = f.call(this, this.y);
    return this;
  },

  add: function add(x, y) {
    this.x += x;
    this.y += y;
    return this;
  },

  subtract: function subtract(x, y) {
    this.x -= x;
    this.y -= y;
    return this;
  },

  scale: function scale(s) {
    this.x *= s;
    this.y *= s;
    return this;
  },

  isZero: function() {
    return this.x == 0 && this.y == 0;
  }
};

(function() {
  function takePointOrArgs(f) {
    return function(arg1, arg2) {
      if (arg2 === undefined)
        return f.call(this, arg1.x, arg1.y);
      else
        return f.call(this, arg1, arg2);
    };
  }

  for each (let f in ['add', 'subtract', 'equals', 'set'])
    Point.prototype[f] = takePointOrArgs(Point.prototype[f]);
})();


/**
 * Rect is a simple data structure for representation of a rectangle supporting
 * many basic geometric operations.
 *
 * NOTE: Since its operations are closed, rectangles may be empty and will report
 * non-positive widths and heights in that case.
 */

this.Rect = function Rect(x, y, w, h) {
  this.left = x;
  this.top = y;
  this.right = x + w;
  this.bottom = y + h;
};

Rect.fromRect = function fromRect(r) {
  return new Rect(r.left, r.top, r.right - r.left, r.bottom - r.top);
};

Rect.prototype = {
  get x() { return this.left; },
  get y() { return this.top; },
  get width() { return this.right - this.left; },
  get height() { return this.bottom - this.top; },
  set x(v) {
    let diff = this.left - v;
    this.left = v;
    this.right -= diff;
  },
  set y(v) {
    let diff = this.top - v;
    this.top = v;
    this.bottom -= diff;
  },
  set width(v) { this.right = this.left + v; },
  set height(v) { this.bottom = this.top + v; },

  isEmpty: function isEmpty() {
    return this.left >= this.right || this.top >= this.bottom;
  },

  setRect: function(x, y, w, h) {
    this.left = x;
    this.top = y;
    this.right = x+w;
    this.bottom = y+h;

    return this;
  },

  setBounds: function(l, t, r, b) {
    this.top = t;
    this.left = l;
    this.bottom = b;
    this.right = r;

    return this;
  },

  equals: function equals(other) {
    return other != null &&
            (this.isEmpty() && other.isEmpty() ||
            this.top == other.top &&
            this.left == other.left &&
            this.bottom == other.bottom &&
            this.right == other.right);
  },

  clone: function clone() {
    return new Rect(this.left, this.top, this.right - this.left, this.bottom - this.top);
  },

  center: function center() {
    if (this.isEmpty())
      throw "Empty rectangles do not have centers";
    return new Point(this.left + (this.right - this.left) / 2,
                          this.top + (this.bottom - this.top) / 2);
  },

  copyFrom: function(other) {
    this.top = other.top;
    this.left = other.left;
    this.bottom = other.bottom;
    this.right = other.right;

    return this;
  },

  translate: function(x, y) {
    this.left += x;
    this.right += x;
    this.top += y;
    this.bottom += y;

    return this;
  },

  toString: function() {
    return "[" + this.x + "," + this.y + "," + this.width + "," + this.height + "]";
  },

  /** return a new rect that is the union of that one and this one */
  union: function(other) {
    return this.clone().expandToContain(other);
  },

  contains: function(other) {
    if (other.isEmpty()) return true;
    if (this.isEmpty()) return false;

    return (other.left >= this.left &&
            other.right <= this.right &&
            other.top >= this.top &&
            other.bottom <= this.bottom);
  },

  intersect: function(other) {
    return this.clone().restrictTo(other);
  },

  intersects: function(other) {
    if (this.isEmpty() || other.isEmpty())
      return false;

    let x1 = Math.max(this.left, other.left);
    let x2 = Math.min(this.right, other.right);
    let y1 = Math.max(this.top, other.top);
    let y2 = Math.min(this.bottom, other.bottom);
    return x1 < x2 && y1 < y2;
  },

  /** Restrict area of this rectangle to the intersection of both rectangles. */
  restrictTo: function restrictTo(other) {
    if (this.isEmpty() || other.isEmpty())
      return this.setRect(0, 0, 0, 0);

    let x1 = Math.max(this.left, other.left);
    let x2 = Math.min(this.right, other.right);
    let y1 = Math.max(this.top, other.top);
    let y2 = Math.min(this.bottom, other.bottom);
    // If width or height is 0, the intersection was empty.
    return this.setRect(x1, y1, Math.max(0, x2 - x1), Math.max(0, y2 - y1));
  },

  /** Expand this rectangle to the union of both rectangles. */
  expandToContain: function expandToContain(other) {
    if (this.isEmpty()) return this.copyFrom(other);
    if (other.isEmpty()) return this;

    let l = Math.min(this.left, other.left);
    let r = Math.max(this.right, other.right);
    let t = Math.min(this.top, other.top);
    let b = Math.max(this.bottom, other.bottom);
    return this.setRect(l, t, r-l, b-t);
  },

  /**
   * Expands to the smallest rectangle that contains original rectangle and is bounded
   * by lines with integer coefficients.
   */
  expandToIntegers: function round() {
    this.left = Math.floor(this.left);
    this.top = Math.floor(this.top);
    this.right = Math.ceil(this.right);
    this.bottom = Math.ceil(this.bottom);
    return this;
  },

  scale: function scale(xscl, yscl) {
    this.left *= xscl;
    this.right *= xscl;
    this.top *= yscl;
    this.bottom *= yscl;
    return this;
  },

  map: function map(f) {
    this.left = f.call(this, this.left);
    this.top = f.call(this, this.top);
    this.right = f.call(this, this.right);
    this.bottom = f.call(this, this.bottom);
    return this;
  },

  /** Ensure this rectangle is inside the other, if possible. Preserves w, h. */
  translateInside: function translateInside(other) {
    let offsetX = (this.left <= other.left ? other.left - this.left :
                  (this.right > other.right ? other.right - this.right : 0));
    let offsetY = (this.top <= other.top ? other.top - this.top :
                  (this.bottom > other.bottom ? other.bottom - this.bottom : 0));
    return this.translate(offsetX, offsetY);
  },

  /** Subtract other area from this. Returns array of rects whose union is this-other. */
  subtract: function subtract(other) {
    let r = new Rect(0, 0, 0, 0);
    let result = [];
    other = other.intersect(this);
    if (other.isEmpty())
      return [this.clone()];

    // left strip
    r.setBounds(this.left, this.top, other.left, this.bottom);
    if (!r.isEmpty())
      result.push(r.clone());
    // inside strip
    r.setBounds(other.left, this.top, other.right, other.top);
    if (!r.isEmpty())
      result.push(r.clone());
    r.setBounds(other.left, other.bottom, other.right, this.bottom);
    if (!r.isEmpty())
      result.push(r.clone());
    // right strip
    r.setBounds(other.right, this.top, this.right, this.bottom);
    if (!r.isEmpty())
      result.push(r.clone());

    return result;
  },

  /**
   * Blends two rectangles together.
   * @param rect Rectangle to blend this one with
   * @param scalar Ratio from 0 (returns a clone of this rect) to 1 (clone of rect).
   * @return New blended rectangle.
   */
  blend: function blend(rect, scalar) {
    return new Rect(
      this.left   + (rect.left   - this.left  ) * scalar,
      this.top    + (rect.top    - this.top   ) * scalar,
      this.width  + (rect.width  - this.width ) * scalar,
      this.height + (rect.height - this.height) * scalar);
  },

  /**
   * Grows or shrinks the rectangle while keeping the center point.
   * Accepts single multipler, or separate for both axes.
   */
  inflate: function inflate(xscl, yscl) {
    let xAdj = (this.width * xscl - this.width) / 2;
    let s = (arguments.length > 1) ? yscl : xscl;
    let yAdj = (this.height * s - this.height) / 2;
    this.left -= xAdj;
    this.right += xAdj;
    this.top -= yAdj;
    this.bottom += yAdj;
    return this;
  }
};
