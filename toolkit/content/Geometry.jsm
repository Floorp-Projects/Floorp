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
 *   Ben Combee <bcombee@mozilla.com>
 *   Matt Brubeck <mbrubeck@mozilla.com>
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

// Blindly copied from Safari documentation for now.
const kViewportMinScale  = 0;
const kViewportMaxScale  = 10;
const kViewportMinWidth  = 200;
const kViewportMaxWidth  = 10000;
const kViewportMinHeight = 223;
const kViewportMaxHeight = 10000;

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
  },

  /** printf-like dump function */
  dumpf: function dumpf(str) {
    var args = arguments;
    var i = 1;
    dump(str.replace(/%s/g, function() {
      if (i >= args.length) {
        throw "dumps received too many placeholders and not enough arguments";
      }
      return args[i++].toString();
    }));
  },

  /** Like dump, but each arg is handled and there's an automatic newline */
  dumpLn: function dumpLn() {
    for (var i = 0; i < arguments.length; i++) { dump(arguments[i] + " "); }
    dump("\n");
  },

  getWindowUtils: function getWindowUtils(aWindow) {
    return aWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
  },

  getScrollOffset: function getScrollOffset(aWindow) {
    var cwu = Util.getWindowUtils(aWindow);
    var scrollX = {};
    var scrollY = {};
    cwu.getScrollXY(false, scrollX, scrollY);
    return new Point(scrollX.value, scrollY.value);
  },

  /** Executes aFunc after other events have been processed. */
  executeSoon: function executeSoon(aFunc) {
    Services.tm.mainThread.dispatch({
      run: function() {
        aFunc();
      }
    }, Ci.nsIThread.DISPATCH_NORMAL);
  },

  getHrefForElement: function getHrefForElement(target) {
    // XXX: This is kind of a hack to work around a Gecko bug (see bug 266932)
    // We're going to walk up the DOM looking for a parent link node.
    // This shouldn't be necessary, but we're matching the existing behaviour for left click

    let link = null;
    while (target) {
      if (target instanceof Ci.nsIDOMHTMLAnchorElement || 
          target instanceof Ci.nsIDOMHTMLAreaElement ||
          target instanceof Ci.nsIDOMHTMLLinkElement) {
          if (target.hasAttribute("href"))
            link = target;
      }
      target = target.parentNode;
    }

    if (link && link.hasAttribute("href"))
      return link.href;
    else
      return null;
  },

  makeURI: function makeURI(aURL, aOriginCharset, aBaseURI) {
    return Services.io.newURI(aURL, aOriginCharset, aBaseURI);
  },

  makeURLAbsolute: function makeURLAbsolute(base, url) {
    // Note:  makeURI() will throw if url is not a valid URI
    return this.makeURI(url, null, this.makeURI(base)).spec;
  },

  isLocalScheme: function isLocalScheme(aURL) {
    return (aURL.indexOf("about:") == 0 && aURL != "about:blank") || aURL.indexOf("chrome:") == 0;
  },

  clamp: function(num, min, max) {
    return Math.max(min, Math.min(max, num));
  },

  /**
   * Determines whether a home page override is needed.
   * Returns:
   *  "new profile" if this is the first run with a new profile.
   *  "new version" if this is the first run with a build with a different
   *                      Gecko milestone (i.e. right after an upgrade).
   *  "none" otherwise.
   */
  needHomepageOverride: function needHomepageOverride() {
    let savedmstone = null;
    try {
      savedmstone = Services.prefs.getCharPref("browser.startup.homepage_override.mstone");
    } catch (e) {}

    if (savedmstone == "ignore")
      return "none";

#expand    let ourmstone = "__MOZ_APP_VERSION__";

    if (ourmstone != savedmstone) {
      Services.prefs.setCharPref("browser.startup.homepage_override.mstone", ourmstone);

      return (savedmstone ? "new version" : "new profile");
    }

    return "none";
  },

  /** Don't display anything in the urlbar for these special URIs. */
  isURLEmpty: function isURLEmpty(aURL) {
    return (!aURL || aURL == "about:blank" || aURL == "about:home");
  },

  /** Recursively find all documents, including root document. */
  getAllDocuments: function getAllDocuments(doc, resultSoFar) {
    resultSoFar = resultSoFar || [doc];
    if (!doc.defaultView)
      return resultSoFar;
    let frames = doc.defaultView.frames;
    if (!frames)
      return resultSoFar;

    let i;
    let currentDoc;
    for (i = 0; i < frames.length; i++) {
      currentDoc = frames[i].document;
      resultSoFar.push(currentDoc);
      this.getAllDocuments(currentDoc, resultSoFar);
    }

    return resultSoFar;
  },

  // Put the Mozilla networking code into a state that will kick the auto-connection
  // process.
  forceOnline: function forceOnline() {
#ifdef MOZ_ENABLE_LIBCONIC
    Services.io.offline = false;
#endif
  },

  /** Capitalize first letter of a string. */
  capitalize: function(str) {
    return str.charAt(0).toUpperCase() + str.substring(1);
  },
  
  isPortrait: function isPortrait() {
    return (window.innerWidth < 500);
  }
};


/**
 * Helper class to nsITimer that adds a little more pizazz.  Callback can be an
 * object with a notify method or a function.
 */
Util.Timeout = function(aCallback) {
  this._callback = aCallback;
  this._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  this._active = false;
};

Util.Timeout.prototype = {
  /** Timer callback. Don't call this manually. */
  notify: function notify() {
    this._active = false;
    if (this._callback.notify)
      this._callback.notify();
    else
      this._callback.apply(null);
  },

  /** Do the callback once.  Cancels other timeouts on this object. */
  once: function once(aDelay, aCallback) {
    if (aCallback)
      this._callback = aCallback;
    this.clear();
    this._timer.initWithCallback(this, aDelay, this._timer.TYPE_ONE_SHOT);
    this._active = true;
    return this;
  },

  /** Do the callback every aDelay msecs. Cancels other timeouts on this object. */
  interval: function interval(aDelay, aCallback) {
    if (aCallback)
      this._callback = aCallback;
    this.clear();
    this._timer.initWithCallback(this, aDelay, this._timer.TYPE_REPEATING_SLACK);
    this._active = true;
    return this;
  },

  /** Clear any pending timeouts. */
  clear: function clear() {
    if (this._active) {
      this._timer.cancel();
      this._active = false;
    }
    return this;
  },

  /** If there is a pending timeout, call it and cancel the timeout. */
  flush: function flush() {
    if (this._active) {
      this.clear();
      this.notify();
    }
    return this;
  },

  /** Return true iff we are waiting for a callback. */
  isPending: function isPending() {
    return this._active;
  }
};



/**
 * Cache of commonly used elements.
 */
let Elements = {};

[
  ["browserBundle",      "bundle_browser"],
  ["contentShowing",     "bcast_contentShowing"],
  ["stack",              "stack"],
  ["panelUI",            "panel-container"],
  ["viewBuffer",         "view-buffer"],
  ["toolbarContainer",   "toolbar-container"],
].forEach(function (elementGlobal) {
  let [name, id] = elementGlobal;
  Elements.__defineGetter__(name, function () {
    let element = document.getElementById(id);
    if (!element)
      return null;
    delete Elements[name];
    return Elements[name] = element;
  });
});


/**
 * Simple Point class.
 *
 * Any method that takes an x and y may also take a point.
 */
function Point(x, y) {
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

function Rect(x, y, w, h) {
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
    let offsetX = (this.left < other.left ? other.left - this.left :
        (this.right > other.right ? other.right - this.right : 0));
    let offsetY = (this.top < other.top ? other.top - this.top :
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
