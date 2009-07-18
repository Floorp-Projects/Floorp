// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/*
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roy Frostig <rfrostig@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

let Ci = Components.interfaces;

// -----------------------------------------------------------
// General util/convenience tools
//

let Util = {

  bind: function bind(f, thisObj) {
    return function() {
      return f.apply(thisObj, arguments);
    };
  },

  bindAll: function bindAll(instance) {
    let bind = Util.bind;
    for (let key in instance)
      if (instance[key] instanceof Function)
        instance[key] = bind(instance[key], instance);
  }

};


// -----------------------------------------------------------
// Util.Rect is a simple data structure for representation of a rectangle supporting
// many basic geometric operations.
//

Util.Rect = function Rect(x, y, w, h) {
  this.left = x;
  this.top = y;
  this.right = x+w;
  this.bottom = y+h;
};

Util.Rect.prototype = {
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

  setRect: function(x, y, w, h) {
    this.left = x;
    this.top = y;
    this.right = x+w;
    this.bottom = y+h;

    return this;
  },

  setBounds: function(t, l, b, r) {
    this.top = t;
    this.left = l;
    this.bottom = b;
    this.right = r;

    return this;
  },

  equals: function equals(r) {
    return (r != null       &&
            this.top == r.top &&
            this.left == r.left &&
            this.bottom == r.bottom &&
            this.right == r.right);
  },

  clone: function clone() {
    return new wsRect(this.left, this.top, this.right - this.left, this.bottom - this.top);
  },

  center: function center() {
    return [this.left + (this.right - this.left) / 2,
            this.top + (this.bottom - this.top) / 2];
  },

  centerRounded: function centerRounded() {
    return this.center().map(Math.round);
  },

  copyFrom: function(r) {
    this.top = r.top;
    this.left = r.left;
    this.bottom = r.bottom;
    this.right = r.right;

    return this;
  },

  copyFromTLBR: function(r) {
    this.left = r.left;
    this.top = r.top;
    this.right = r.right;
    this.bottom = r.bottom;

    return this;
  },

  translate: function(x, y) {
    this.left += x;
    this.right += x;
    this.top += y;
    this.bottom += y;

    return this;
  },

  // return a new wsRect that is the union of that one and this one
  union: function(rect) {
    let l = Math.min(this.left, rect.left);
    let r = Math.max(this.right, rect.right);
    let t = Math.min(this.top, rect.top);
    let b = Math.max(this.bottom, rect.bottom);

    return new wsRect(l, t, r-l, b-t);
  },

  toString: function() {
    return "[" + this.x + "," + this.y + "," + this.width + "," + this.height + "]";
  },

  expandBy: function(b) {
    this.left += b.left;
    this.right += b.right;
    this.top += b.top;
    this.bottom += b.bottom;
    return this;
  },

  contains: function(other) {
    return !!(other.left >= this.left &&
              other.right <= this.right &&
              other.top >= this.top &&
              other.bottom <= this.bottom);
  },

  intersect: function(r2) {
    let xmost1 = this.right;
    let xmost2 = r2.right;

    let x = Math.max(this.left, r2.left);

    let temp = Math.min(xmost1, xmost2);
    if (temp <= x)
      return null;

    let width = temp - x;

    let ymost1 = this.bottom;
    let ymost2 = r2.bottom;
    let y = Math.max(this.top, r2.top);

    temp = Math.min(ymost1, ymost2);
    if (temp <= y)
      return null;

    let height = temp - y;

    return new wsRect(x, y, width, height);
  },

  intersects: function(other) {
    let xok = (other.left > this.left && other.left < this.right) ||
      (other.right > this.left && other.right < this.right) ||
      (other.left <= this.left && other.right >= this.right);
    let yok = (other.top > this.top && other.top < this.bottom) ||
      (other.bottom > this.top && other.bottom < this.bottom) ||
      (other.top <= this.top && other.bottom >= this.bottom);
    return xok && yok;
  },

  /**
   * Similar to (and most code stolen from) intersect().  A restriction
   * is an intersection, but this modifies the receiving object instead
   * of returning a new rect.
   */
  restrictTo: function restrictTo(r2) {
    let xmost1 = this.right;
    let xmost2 = r2.right;

    let x = Math.max(this.left, r2.left);

    let temp = Math.min(xmost1, xmost2);
    if (temp <= x)
      throw "Intersection is empty but rects cannot be empty";

    let width = temp - x;

    let ymost1 = this.bottom;
    let ymost2 = r2.bottom;
    let y = Math.max(this.top, r2.top);

    temp = Math.min(ymost1, ymost2);
    if (temp <= y)
      throw "Intersection is empty but rects cannot be empty";

    let height = temp - y;

    return this.setRect(x, y, width, height);
  },

  /**
   * Similar to (and most code stolen from) union().  An extension is a
   * union (in our sense of the term, not the common set-theoretic sense),
   * but this modifies the receiving object instead of returning a new rect.
   * Effectively, this rectangle is expanded minimally to contain all of the
   * other rect.  "Expanded minimally" means that the rect may shrink if
   * given a strict subset rect as the argument.
   */
  expandToContain: function extendTo(rect) {
    let l = Math.min(this.left, rect.left);
    let r = Math.max(this.right, rect.right);
    let t = Math.min(this.top, rect.top);
    let b = Math.max(this.bottom, rect.bottom);

    return this.setRect(l, t, r-l, b-t);
  },

  round: function round(scale) {
    if (!scale) scale = 1;

    this.left = Math.floor(this.left * scale) / scale;
    this.top = Math.floor(this.top * scale) / scale;
    this.right = Math.ceil(this.right * scale) / scale;
    this.bottom = Math.ceil(this.bottom * scale) / scale;

    return this;
  },

  scale: function scale(xscl, yscl) {
    this.left *= xscl;
    this.right *= xscl;
    this.top *= yscl;
    this.bottom *= yscl;

    return this;
  }
};


// -----------------------------------------------------------
// Deprecated, global version of wsRect
//

function wsRect(x, y, w, h) {
  this.left = x;
  this.top = y;
  this.right = x+w;
  this.bottom = y+h;
}

wsRect.prototype = {

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

  setRect: function(x, y, w, h) {
    this.left = x;
    this.top = y;
    this.right = x+w;
    this.bottom = y+h;

    return this;
  },

  setBounds: function(t, l, b, r) {
    this.top = t;
    this.left = l;
    this.bottom = b;
    this.right = r;

    return this;
  },

  equals: function equals(r) {
    return (r != null       &&
            this.top == r.top &&
            this.left == r.left &&
            this.bottom == r.bottom &&
            this.right == r.right);
  },

  clone: function clone() {
    return new wsRect(this.left, this.top, this.right - this.left, this.bottom - this.top);
  },

  center: function center() {
    return [this.left + (this.right - this.left) / 2,
            this.top + (this.bottom - this.top) / 2];
  },

  centerRounded: function centerRounded() {
    return this.center().map(Math.round);
  },

  copyFrom: function(r) {
    this.top = r.top;
    this.left = r.left;
    this.bottom = r.bottom;
    this.right = r.right;

    return this;
  },

  copyFromTLBR: function(r) {
    this.left = r.left;
    this.top = r.top;
    this.right = r.right;
    this.bottom = r.bottom;

    return this;
  },

  translate: function(x, y) {
    this.left += x;
    this.right += x;
    this.top += y;
    this.bottom += y;

    return this;
  },

  // return a new wsRect that is the union of that one and this one
  union: function(rect) {
    let l = Math.min(this.left, rect.left);
    let r = Math.max(this.right, rect.right);
    let t = Math.min(this.top, rect.top);
    let b = Math.max(this.bottom, rect.bottom);

    return new wsRect(l, t, r-l, b-t);
  },

  toString: function() {
    return "[" + this.x + "," + this.y + "," + this.width + "," + this.height + "]";
  },

  expandBy: function(b) {
    this.left += b.left;
    this.right += b.right;
    this.top += b.top;
    this.bottom += b.bottom;
    return this;
  },

  contains: function(other) {
    return !!(other.left >= this.left &&
              other.right <= this.right &&
              other.top >= this.top &&
              other.bottom <= this.bottom);
  },

  intersect: function(r2) {
    let xmost1 = this.right;
    let xmost2 = r2.right;

    let x = Math.max(this.left, r2.left);

    let temp = Math.min(xmost1, xmost2);
    if (temp <= x)
      return null;

    let width = temp - x;

    let ymost1 = this.bottom;
    let ymost2 = r2.bottom;
    let y = Math.max(this.top, r2.top);

    temp = Math.min(ymost1, ymost2);
    if (temp <= y)
      return null;

    let height = temp - y;

    return new wsRect(x, y, width, height);
  },

  intersects: function(other) {
    let xok = (other.left > this.left && other.left < this.right) ||
      (other.right > this.left && other.right < this.right) ||
      (other.left <= this.left && other.right >= this.right);
    let yok = (other.top > this.top && other.top < this.bottom) ||
      (other.bottom > this.top && other.bottom < this.bottom) ||
      (other.top <= this.top && other.bottom >= this.bottom);
    return xok && yok;
  },

  /**
   * Similar to (and most code stolen from) intersect().  A restriction
   * is an intersection, but this modifies the receiving object instead
   * of returning a new rect.
   */
  restrictTo: function restrictTo(r2) {
    let xmost1 = this.right;
    let xmost2 = r2.right;

    let x = Math.max(this.left, r2.left);

    let temp = Math.min(xmost1, xmost2);
    if (temp <= x)
      throw "Intersection is empty but rects cannot be empty";

    let width = temp - x;

    let ymost1 = this.bottom;
    let ymost2 = r2.bottom;
    let y = Math.max(this.top, r2.top);

    temp = Math.min(ymost1, ymost2);
    if (temp <= y)
      throw "Intersection is empty but rects cannot be empty";

    let height = temp - y;

    return this.setRect(x, y, width, height);
  },

  /**
   * Similar to (and most code stolen from) union().  An extension is a
   * union (in our sense of the term, not the common set-theoretic sense),
   * but this modifies the receiving object instead of returning a new rect.
   * Effectively, this rectangle is expanded minimally to contain all of the
   * other rect.  "Expanded minimally" means that the rect may shrink if
   * given a strict subset rect as the argument.
   */
  expandToContain: function extendTo(rect) {
    let l = Math.min(this.left, rect.left);
    let r = Math.max(this.right, rect.right);
    let t = Math.min(this.top, rect.top);
    let b = Math.max(this.bottom, rect.bottom);

    return this.setRect(l, t, r-l, b-t);
  },

  round: function round(scale) {
    if (!scale) scale = 1;

    this.left = Math.floor(this.left * scale) / scale;
    this.top = Math.floor(this.top * scale) / scale;
    this.right = Math.ceil(this.right * scale) / scale;
    this.bottom = Math.ceil(this.bottom * scale) / scale;

    return this;
  },

  scale: function scale(xscl, yscl) {
    this.left *= xscl;
    this.right *= xscl;
    this.top *= yscl;
    this.bottom *= yscl;

    return this;
  }
};

