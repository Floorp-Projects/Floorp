/* -*- Mode: js2; tab-width: 40; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

var gWsDoLog = false;
var gWsLogDiv = null;

function logbase() {
  if (!gWsDoLog)
    return;

  if (gWsLogDiv == null && "console" in window) {
    console.log.apply(console, arguments);
  } else {
    var s = "";
    for (var i = 0; i < arguments.length; i++) {
      s += arguments[i] + " ";
    }
    s += "\n";
    if (gWsLogDiv) {
      gWsLogDiv.appendChild(document.createElementNS("http://www.w3.org/1999/xhtml", "br"));
      gWsLogDiv.appendChild(document.createTextNode(s));
    }

    dump(s);
  }
}

function dumpJSStack(stopAtNamedFunction) {
  let caller = Components.stack.caller;
  dump("\tStack: " + caller.name);
  while ((caller = caller.caller)) {
    dump(" <- " + caller.name);
    if (stopAtNamedFunction && caller.name != "anonymous")
      break;
  }
  dump("\n");
}

function log() {
  return;
  logbase.apply(window, arguments);
}

function log2() {
  return;
  logbase.apply(window, arguments);
}

let reportError = log;

/*
 * wsBorder class
 *
 * Simple container for top,left,bottom,right "border" values
 */
function wsBorder(t, l, b, r) {
  this.setBorder(t, l, b, r);
}

wsBorder.prototype = {
  _t: 0, _l: 0, _b: 0, _r: 0,

  get left() { return this._l; },
  get right() { return this._r; },
  get top() { return this._t; },
  get bottom() { return this._b; },

  set left(v) { this._l = v; },
  set right(v) { this._r = v; },
  set top(v) { this._t = v; },
  set bottom(v) { this._b = v; },

  setBorder: function (t, l, b, r) {
    this._t = t;
    this._l = l;
    this._b = b;
    this._r = r;
  },

  toString: function () {
    return "[l:" + this._l + ",t:" + this._t + ",r:" + this._r + ",b:" + this._b + "]";
  }
};

/*
 * wsRect class
 *
 * Rectangle class, with both x/y/w/h and t/l/b/r accessors.
 */
function wsRect(x, y, w, h) {
  this.setRect(x, y, w, h);
}

wsRect.prototype = {
  _l: 0, _t: 0, _b: 0, _r: 0,

  get x() { return this._l; },
  get y() { return this._t; },
  get width() { return this._r - this._l; },
  get height() { return this._b - this._t; },
  get w() { return this.width; },
  get h() { return this.height; },

  set x(v) { let w = this.w; this._l = v; this._r = v + w; },
  set y(v) { let h = this.h; this._t = v; this._b = v + h; },
  set width(v) { this._r = this._l + v; },
  set height(v) { this._b = this._t + v; },
  set w(v) { this.w = v; },
  set h(v) { this.h = v; },

  get left() { return this._l; },
  get right() { return this._r; },
  get top() { return this._t; },
  get bottom() { return this._b; },

  set left(v) { this._l = v; },
  set right(v) { this._r = v; },
  set top(v) { this._t = v; },
  set bottom(v) { this._b = v; },

  setRect: function(x, y, w, h) {
    this._l = x;
    this._t = y;
    this._r = x+w;
    this._b = y+h;

    return this;
  },

  setBounds: function(t, l, b, r) {
    this._t = t;
    this._l = l;
    this._b = b;
    this._r = r;

    return this;
  },

  clone: function() {
    return new wsRect(this.x, this.y, this.width, this.height);
  },

  copyFrom: function(r) {
    this._t = r._t;
    this._l = r._l;
    this._b = r._b;
    this._r = r._r;

    return this;
  },

  copyFromTLBR: function(r) {
    this._l = r.left;
    this._t = r.top;
    this._r = r.right;
    this._b = r.bottom;

    return this;
  },

  translate: function(x, y) {
    this._l += x;
    this._r += x;
    this._t += y;
    this._b += y;

    return this;
  },

  // return a new wsRect that is the union of that one and this one
  union: function(rect) {
    let l = Math.min(this._l, rect._l);
    let r = Math.max(this._r, rect._r);
    let t = Math.min(this._t, rect._t);
    let b = Math.max(this._b, rect._b);

    return new wsRect(l, t, r-l, b-t);
  },

  toString: function() {
    return "[" + this.x + "," + this.y + "," + this.width + "," + this.height + "]";
  },

  expandBy: function(b) {
    this._l += b.left;
    this._r += b.right;
    this._t += b.top;
    this._b += b.bottom;
    return this;
  },

  contains: function(other) {
    return !!(other._l >= this._l &&
              other._r <= this._r &&
              other._t >= this._t &&
              other._b <= this._b);
  },

  intersect: function(r2) {
    let xmost1 = this.x + this.width;
    let ymost1 = this.y + this.height;
    let xmost2 = r2.x + r2.width;
    let ymost2 = r2.y + r2.height;

    let x = Math.max(this.x, r2.x);
    let y = Math.max(this.y, r2.y);

    let temp = Math.min(xmost1, xmost2);
    if (temp <= x)
      return null;

    let width = temp - x;

    temp = Math.min(ymost1, ymost2);
    if (temp <= y)
      return null;

    let height = temp - y;

    return new wsRect(x, y, width, height);
  },

  intersects: function(other) {
    let xok = (other._l > this._l && other._l < this._r) ||
      (other._r > this._l && other._r < this._r) ||
      (other._l <= this._l && other._r >= this._r);
    let yok = (other._t > this._t && other._t < this._b) ||
      (other._b > this._t && other._b < this._b) ||
      (other._t <= this._t && other._b >= this._b);
    return xok && yok;
  },
  
  round: function(scale) {
    this._l = Math.floor(this._l * scale) / scale;
    this._t = Math.floor(this._t * scale) / scale;
    this._r = Math.ceil(this._r * scale) / scale;
    this._b = Math.ceil(this._b * scale) / scale;
  }
};

/*
 * The "Widget Stack"
 *
 * Manages a <xul:stack>'s children, allowing them to be dragged around
 * the stack, subject to specified constraints.  Optionally supports
 * one widget designated as the viewport, which can be panned over a virtual
 * area without needing to draw that area entirely.  The viewport widget
 * is designated by a 'viewport' attribute on the child element.
 *
 * Widgets are subject to various constraints, specified in xul via the
 * 'constraint' attribute.  Current constraints are:
 *   ignore-x: When panning, ignore any changes to the widget's x position
 *   ignore-y: When panning, ignore any changes to the widget's y position
 *   vp-relative: This widget's position should be claculated relative to
 *     the viewport widget.  It will always keep the same offset from that
 *     widget as initially laid out, regardless of changes to the viewport
 *     bounds.
 *   frozen: This widget is in a fixed position and should never pan.
 */
function WidgetStack(el, ew, eh) {
  this.init(el, ew, eh);
}

WidgetStack.prototype = {
  // the <stack> element
  _el: null,

  // object indexed by widget id, with state struct for each object (see _addNewWidget)
  _widgetState: null,

  // any barriers
  _barriers: null,

  // If a viewport widget is present, this will point to its state object;
  // otherwise null.
  _viewport: null,

  // a wsRect; the inner bounds of the viewport content
  _viewportBounds: null,
  // a wsBorder; the overflow area to the side of the bounds where our
  // viewport-relative widgets go
  _viewportOverflow: null,

  // a wsRect; the viewportBounds expanded by the viewportOverflow
  _pannableBounds: null,
  get pannableBounds() {
    if (!this._pannableBounds) {
      this._pannableBounds = this._viewportBounds.clone()
                                 .expandBy(this._viewportOverflow);
    }
    return this._pannableBounds.clone();
  },

  // a wsRect; the currently visible part of pannableBounds.
  _viewingRect: null,

  // the amount of current global offset applied to all widgets (whether
  // static or not).  Set via offsetAll().  Can be used to push things
  // out of the way for overlaying some other UI.
  globalOffsetX: 0,
  globalOffsetY: 0,

  // if true (default), panning is constrained to the pannable bounds.
  _constrainToViewport: true,

  _viewportUpdateInterval: -1,
  _viewportUpdateTimeout: -1,

  _viewportUpdateHandler: null,
  _panHandler: null,

  _dragState: null,

  //
  // init:
  //   el: the <stack> element whose children are to be managed
  //
  init: function (el, ew, eh) {
    this._el = el;
    this._widgetState = {};
    this._barriers = [];

    let rect = this._el.getBoundingClientRect();
    let width = rect.width;
    let height = rect.height;

    if (ew != undefined && eh != undefined) {
      width = ew;
      height = eh;
    }

    this._viewportOverflow = new wsBorder(0, 0, 0, 0);

    this._viewingRect = new wsRect(0, 0, width, height);

    // listen for DOMNodeInserted/DOMNodeRemoved/DOMAttrModified
    let children = this._el.childNodes;
    for (let i = 0; i < children.length; i++) {
      let c = this._el.childNodes[i];
      if (c.tagName == "spacer")
        this._addNewBarrierFromSpacer(c);
      else
        this._addNewWidget(c);
    }

    // this also updates the viewportOverflow and pannableBounds
    this._updateWidgets();

    if (this._viewport) {
      this._viewportBounds = new wsRect(0, 0, this._viewport.rect.width, this._viewport.rect.height);
    } else {
      this._viewportBounds = new wsRect(0, 0, 0, 0);
    }
  },

  // handleEvents: if this is called, WS will install its own event handlers
  // on the stack element for mouse clicks and motion.  Otherwise, the application
  // must call dragStart/dragMove/dragStop as appropriate.
  handleEvents: function () {
    let self = this;

    let e = window;
    e.addEventListener("mousedown", function (ev) { return self._onMouseDown(ev); }, true);
    e.addEventListener("mouseup", function (ev) { return self._onMouseUp(ev); }, true);
    e.addEventListener("mousemove", function (ev) { return self._onMouseMove(ev); }, true);
  },

  // moveWidgetBy: move the widget with the given id by x,y.  Should
  // not be used on vp-relative or otherwise frozen widgets (using it
  // on the x coordinate for x-ignore widgets and similarily for y is
  // ok, as long as the other coordinate remains 0.)
  moveWidgetBy: function (wid, x, y) {
    let state = this._getState(wid);

    state.rect.x += x;
    state.rect.y += y;

    this._commitState(state);
  },

  // panBy: pan the entire set of widgets by the given x and y amounts.
  // This does the same thing as if the user dragged by the given amount.
  // If this is called with an outstanding drag, weirdness might happen,
  // but it also might work, so not disabling that.
  //
  // if ignoreBarriers is true, then barriers are ignored for the pan.
  panBy: function panBy(dx, dy, ignoreBarriers) {
    if (dx == 0 && dy ==0)
      return;

    let needsDragWrap = !this._dragging;

    if (needsDragWrap)
      this.dragStart(0, 0);

    this._panBy(dx, dy, ignoreBarriers);

    if (needsDragWrap)
      this.dragStop();
  },

  // panTo: pan the entire set of widgets so that the given x,y coordinates
  // are in the upper left of the stack.
  panTo: function (x, y) {
    this.panBy(x - this._viewingRect.x, y - this._viewingRect.y, true);
  },

  // freeze: set a widget as frozen.  A frozen widget won't be moved
  // in the stack -- its x,y position will still be tracked in the
  // state, but the left/top attributes won't be overwritten.  Call unfreeze
  // to move the widget back to where the ws thinks it should be.
  freeze: function (wid) {
    let state = this._getState(wid);

    state.frozen = true;
  },

  unfreeze: function (wid) {
    let state = this._getState(wid);
    if (!state.frozen)
      return;

    state.frozen = false;
    this._commitState(state);
  },

  // moveFrozenTo: move a frozen widget with id wid to x, y in the stack.
  // can only be used on frozen widgets
  moveFrozenTo: function (wid, x, y) {
    let state = this._getState(wid);
    if (!state.frozen)
      throw "moveFrozenTo on non-frozen widget " + wid;

    state.widget.setAttribute("left", x);
    state.widget.setAttribute("top", y);
  },

  // we're relying on viewportBounds and viewingRect having the same origin
  get viewportVisibleRect () {
    let rect = this._viewportBounds.intersect(this._viewingRect);
    if (!rect)
        rect = new wsRect(0, 0, 0, 0);
    return rect;
  },

  // isWidgetVisible: return true if any portion of widget with id wid is
  // visible; otherwise return false.
  isWidgetVisible: function (wid) {
    let state = this._getState(wid);
    let visibleStackRect = new wsRect(0, 0, this._viewingRect.width, this._viewingRect.height);

    return visibleStackRect.intersects(state.rect);
  },

  // getWidgetVisibility: returns the percentage that the widget is visible
  getWidgetVisibility: function (wid) {
    let state = this._getState(wid);
    let visibleStackRect = new wsRect(0, 0, this._viewingRect.width, this._viewingRect.height);

    let visibleRect = visibleStackRect.intersect(state.rect);
    if (visibleRect)
      return [visibleRect.width / state.rect.width, visibleRect.height / state.rect.height]

    return [0, 0];
  },

  // offsetAll: add an offset to all widgets
  offsetAll: function (x, y) {
    this.globalOffsetX += x;
    this.globalOffsetY += y;

    for each (let state in this._widgetState) {
      state.rect.x += x;
      state.rect.y += y;

      this._commitState(state);
    }
  },

  // setViewportBounds
  //  nb: an object containing top, left, bottom, right properties
  //    OR
  //  width, height: integer values; origin assumed to be 0,0
  //    OR
  //  top, left, bottom, right: integer values
  //
  // Set the bounds of the viewport area; that is, set the size of the
  // actual content that the viewport widget will be providing a view
  // over.  For example, in the case of a 100x100 viewport showing a
  // view into a 100x500 webpage, the viewport bounds would be
  // { top: 0, left: 0, bottom: 500, right: 100 }.
  //
  // setViewportBounds will move all the viewport-relative widgets into
  // place based on the new viewport bounds.
  setViewportBounds: function setViewportBounds() {
    let oldBounds = this._viewportBounds.clone();

    if (arguments.length == 1) {
      this._viewportBounds.copyFromTLBR(arguments[0]);
    } else if (arguments.length == 2) {
      this._viewportBounds.setRect(0, 0, arguments[0], arguments[1]);
    } else if (arguments.length == 4) {
      this._viewportBounds.setBounds(arguments[0],
      arguments[1],
      arguments[2],
      arguments[3]);
    } else {
      throw "Invalid number of arguments to setViewportBounds";
    }

    let vp = this._viewport;

    let dleft = this._viewportBounds.left - oldBounds.left;
    let dright = this._viewportBounds.right - oldBounds.right;
    let dtop = this._viewportBounds.top - oldBounds.top;
    let dbottom = this._viewportBounds.bottom - oldBounds.bottom;

    log2("setViewportBounds dltrb", dleft, dtop, dright, dbottom);

    // move all vp-relative widgets to be the right offset from the bounds again
    for each (let state in this._widgetState) {
      if (state.vpRelative) {
        log2("vpRelative widget", state.id, state.rect.x, dleft, dright);
        if (state.vpOffsetXBefore) {
          state.rect.x += dleft;
        } else {
          state.rect.x += dright;
        }

        if (state.vpOffsetYBefore) {
          state.rect.y += dtop;
        } else {
          state.rect.y += dbottom;
        }

        log2("vpRelative widget", state.id, state.rect.x, dleft, dright);
        this._commitState(state);
      }
    }

    for (let bid in this._barriers) {
      let barrier = this._barriers[bid];

      log2("setViewportBounds: looking at barrier", bid, barrier.vpRelative, barrier.type);

      if (barrier.vpRelative) {
        if (barrier.type == "vertical") {
          let q = "v barrier moving from " + barrier.x + " to ";
          if (barrier.vpOffsetXBefore) {
            barrier.x += dleft;
          } else {
            barrier.x += dright;
          }
          log2(q += barrier.x);
        } else if (barrier.type == "horizontal") {
          let q = "h barrier moving from " + barrier.y + " to ";
          if (barrier.vpOffsetYBefore) {
            barrier.y += dtop;
          } else {
            barrier.y += dbottom;
          }
          log2(q += barrier.y);
        }
      }
    }

    // clear the pannable bounds cache to make sure it gets rebuilt
    this._pannableBounds = null;

    // now let's make sure that the viewing rect and inner bounds are still valid
    this._adjustViewingRect();

    this._callViewportUpdateHandler(true);
  },

  // setViewportHandler
  //  uh: A function object
  //
  // The given function object is called at the end of every drag and viewport
  // bounds change, passing in the new rect that's to be displayed in the
  // viewport.
  //
  setViewportHandler: function (uh) {
    this._viewportUpdateHandler = uh;
  },

  // setPanHandler
  // uh: A function object
  //
  // The given functin object is called whenever elements pan; it provides
  // the new area of the pannable bounds that's visible in the stack.
  setPanHandler: function (uh) {
    this._panHandler = uh;
  },

  // dragStart: start a drag, with the current coordinates being clientX,clientY
  dragStart: function (clientX, clientY) {
    log("(dragStart)", clientX, clientY);

    if (this._dragState) {
      reportError("dragStart with drag already in progress? what?");
      this._dragState = null;
    }

    this._dragState = { };

    let t = Date.now();

    this._dragState.barrierState = [];

    this._dragState.startTime = t;
    // outer x, that is outer from the viewport coordinates.  In stack-relative coords.
    this._dragState.outerStartX = clientX;
    this._dragState.outerStartY = clientY;

    this._dragCoordsFromClient(clientX, clientY, t);

    this._dragState.outerLastUpdateDX = 0;
    this._dragState.outerLastUpdateDY = 0;

    if (this._viewport) {
      // create a copy of these so that we can compute
      // deltas correctly to update the viewport
      this._viewport.dragStartRect = this._viewport.rect.clone();
    }

    this._dragState.dragging = true;
  },

  // dragStop: stop any drag in progress
  dragStop: function () {
    log("(dragStop)");

    if (!this._dragging)
      return;

    if (this._viewportUpdateTimeout != -1)
      clearTimeout(this._viewportUpdateTimeout);

    this._viewportUpdate();

    this._dragState = null;
  },

  // dragMove: process a mouse move to clientX,clientY for an ongoing drag
  dragMove: function dragStop(clientX, clientY) {
    if (!this._dragging)
      return;

    log("(dragMove)", clientX, clientY);

    this._dragCoordsFromClient(clientX, clientY);

    this._dragUpdate();

    if (this._viewportUpdateInterval != -1) {
      if (this._viewportUpdateTimeout != -1)
        clearTimeout(this._viewportUpdateTimeout);
      let self = this;
      this._viewportUpdateTimeout = setTimeout(function () { self._viewportUpdate(); }, this._viewportUpdateInterval);
    }
  },

  // updateSize: tell the WidgetStack to update its size, because it
  // was either resized or some other event took place.
  updateSize: function updateSize(width, height) {
    if (width == undefined || height == undefined) {
      let rect = this._el.getBoundingClientRect();
      width = rect.width;
      height = rect.height;
    }

    // update widget rects and viewportOverflow, since the resize might have
    // caused them to change (widgets first, since the viewportOverflow depends
    // on them).

    // XXX these methods aren't working correctly yet, but they aren't strictly
    // necessary in Fennec's default config
    //for each (let s in this._widgetState)
    //  this._updateWidgetRect(s);
    //this._updateViewportOverflow();

    this._viewingRect.width = width;
    this._viewingRect.height = height;

    this._adjustViewingRect();
  },

  beginUpdateBatch: function startUpdate() {
    this._skipViewportUpdates = true;
    this._startViewportBoundsString = this._viewportBounds.toString();
  },
  
  endUpdateBatch: function endUpdate() {
    this._skipViewportUpdates = false;
    let boundsSizeChanged =
      this._startViewportBoundsString != this._viewportBounds.toString();
    this._callViewportUpdateHandler(boundsSizeChanged);
  },

  //
  // Internal code
  //

  _updateWidgetRect: function(state) {
    // don't need to support updating the viewport rect at the moment
    // (we'd need to duplicate the vptarget* code from _addNewWidget if we did)
    if (state == this._viewport)
      return;

    let w = state.widget;
    let x = w.getAttribute("left") || 0;
    let y = w.getAttribute("top") || 0;
    let rect = w.getBoundingClientRect();
    state.rect = new wsRect(parseInt(x), parseInt(y),
                            rect.right - rect.left,
                            rect.bottom - rect.top);
    if (w.hasAttribute("widgetwidth") && w.hasAttribute("widgetheight")) {
      state.rect.width = parseInt(w.getAttribute("widgetwidth"));
      state.rect.height = parseInt(w.getAttribute("widgetheight"));
    }
  },

  _dumpRects: function () {
    dump("WidgetStack:\n");
    dump("\tthis._viewportBounds: " + this._viewportBounds + "\n");
    dump("\tthis._viewingRect: " + this._viewingRect + "\n");
    dump("\tthis._viewport.viewportInnerBounds: " + this._viewport.viewportInnerBounds + "\n");
    dump("\tthis._viewport.rect: " + this._viewport.rect + "\n");
    dump("\tthis.pannableBounds: " + this.pannableBounds + "\n");
  },

  // Ensures that _viewingRect is within _pannableBounds (call this when either
  // one is resized)
  _adjustViewingRect: function _adjustViewingRect() {
    let vr = this._viewingRect;
    let pb = this.pannableBounds;

    if (pb.contains(vr))
      return; // nothing to do here

    // don't bother adjusting _viewingRect if it can't fit into
    // _pannableBounds
    if (vr.height > pb.height || vr.width > pb.width)
      return;

    let panX = 0, panY = 0;
    if (vr.right > pb.right)
      panX = pb.right - vr.right;
    else if (vr.left < pb.left)
      panX = pb.left - vr.left;

    if (vr.bottom > pb.bottom)
      panY = pb.bottom - vr.bottom;
    else if(vr.top < pb.top)
      panY = pb.top - vr.top;

    this.panBy(panX, panY, true);
  },

  _getState: function (wid) {
    if (!(wid in this._widgetState))
      throw "Unknown widget id '" + wid + "'; widget not in stack";

    return this._widgetState[wid];
  },

  _onMouseDown: function (ev) {
    log("(mousedown)");
    this.dragStart(ev.screenX, ev.screenY);

    // we haven't yet recognized a drag; whatever happens make
    // sure we start one after 50ms.
    this._dragState.dragging = false;

    let self = this;
    this._dragState.dragStartTimeout = setTimeout(function () {
                                                    self._dragState.dragStartTimeout = -1;
                                                    self._delayedDragStart();
                                                    self._dragUpdate();
                                                  }, 50);
  },

  _onMouseUp: function (ev) {
    log("(mouseup)");
    if (!this._dragState)
      return;

    if (this._dragState.dragStartTimeout != -1)
      clearTimeout(this._dragState.dragStartTimeout);

    this.dragStop();
  },

  _onMouseMove: function (ev) {
    if (!this._dragging)
      return;

    this._dragCoordsFromClient(ev.screenX, ev.screenY);

    if (!this._dragging && this._dragState.outerDistSquared > 100)
      this._delayedDragStart();

    this.dragMove(ev.screenX, ev.screenY);
  },

  get _dragging() {
    return this._dragState && this._dragState.dragging;
  },

  // dragStart: a drag was started, either by distance or by time.
  _delayedDragStart: function () {
    log("(dragStart)");
    if (this._dragging)
      return;

    if (this._dragState.dragStartTimeout != -1)
      clearTimeout(this._dragState.dragStartTimeout);

    this._dragState.dragging = true;
  },

  _viewportUpdate: function _viewportUpdate() {
    if (!this._viewport)
      return;

    this._viewportUpdateTimeout = -1;

    let vws = this._viewport;
    let vwib = vws.viewportInnerBounds;
    let vpb = this._viewportBounds;

    // recover the amount the inner bounds moved by the amount the viewport
    // widget moved, but don't include offsets that we're making up from previous
    // drags that didn't affect viewportInnerBounds
    let [ignoreX, ignoreY] = this._offsets || [0, 0];
    let rx = (vws.dragStartRect.x - vws.rect.x) - ignoreX;
    let ry = (vws.dragStartRect.y - vws.rect.y) - ignoreY;

    let [dX, dY] = this._rectTranslateConstrain(rx, ry, vwib, vpb);

    // record the offsets that correpond to the amount of the drag we're ignoring
    // to ensure the viewportInnerBounds remains within the viewportBounds
    this._offsets = [dX - rx, dY - ry];

    // adjust the viewportInnerBounds, and snap the viewport back
    vwib.translate(dX, dY);
    vws.rect.translate(dX, dY);
    this._commitState(vws);

    // update this so that we can call this function again during the same drag
    // and get the right values.
    vws.dragStartRect = vws.rect.clone();

    this._callViewportUpdateHandler(false);
  },

  _callViewportUpdateHandler: function _callViewportUpdateHandler(boundsChanged) {
    if (!this._viewport || !this._viewportUpdateHandler || this._skipViewportUpdates)
      return;

    let vwib = this._viewport.viewportInnerBounds.clone();

    vwib.left += this._viewport.offsetLeft;
    vwib.top += this._viewport.offsetTop;
    vwib.right += this._viewport.offsetRight;
    vwib.bottom += this._viewport.offsetBottom;

    this._viewportUpdateHandler.apply(window, [vwib, boundsChanged]);
  },

  _dragCoordsFromClient: function (cx, cy, t) {
    this._dragState.curTime = t ? t : Date.now();
    this._dragState.outerCurX = cx;
    this._dragState.outerCurY = cy;

    let dx = this._dragState.outerCurX - this._dragState.outerStartX;
    let dy = this._dragState.outerCurY - this._dragState.outerStartY;
    this._dragState.outerDX = dx;
    this._dragState.outerDY = dy;
    this._dragState.outerDistSquared = dx*dx + dy*dy;
  },

  _panHandleBarriers: function (dx, dy) {
    // XXX unless the barriers are sorted by position, this will break
    // with multiple barriers that are near enough to eachother that a
    // drag could cross more than one.

    let vr = this._viewingRect;

    // XXX this just stops at the first horizontal and vertical barrier it finds

    // barrier_[xy] is the barrier that was used to get to the final
    // barrier_d[xy] value.  if null, no barrier, and dx/dy shouldn't
    // be replaced with barrier_d[xy].
    let barrier_y = null, barrier_x = null;
    let barrier_dy = 0, barrier_dx = 0;

    for (let i = 0; i < this._barriers.length; i++) {
      let b = this._barriers[i];

      //log2("barrier", i, b.type, b.x, b.y);

      if (dx != 0 && b.type == "vertical") {
        if (barrier_x != null) {
          delete this._dragState.barrierState[i];
          continue;
        }

        let alreadyKnownDistance = this._dragState.barrierState[i] || 0;

        //log2("alreadyKnownDistance", alreadyKnownDistance);

        let dbx = 0;

        //100 <= 100 && 100-(-5) > 100

        if ((vr.left <= b.x && vr.left+dx > b.x) ||
            (vr.left >= b.x && vr.left+dx < b.x))
        {
          dbx = b.x - vr.left;
        } else if ((vr.right <= b.x && vr.right+dx > b.x) ||
                   (vr.right >= b.x && vr.right+dx < b.x))
        {
          dbx = b.x - vr.right;
        } else {
          delete this._dragState.barrierState[i];
          continue;
        }

        let leftoverDistance = dbx - dx;

        //log2("initial dbx", dbx, leftoverDistance);

        let dist = Math.abs(leftoverDistance + alreadyKnownDistance) - b.size;

        if (dist >= 0) {
          if (dx < 0)
            dbx -= dist;
          else
            dbx += dist;
          delete this._dragState.barrierState[i];
        } else {
          dbx = 0;
          this._dragState.barrierState[i] = leftoverDistance + alreadyKnownDistance;
        }

        //log2("final dbx", dbx, "state", this._dragState.barrierState[i]);

        if (Math.abs(barrier_dx) <= Math.abs(dbx)) {
          barrier_x = b;
          barrier_dx = dbx;

          //log2("new barrier_dx", barrier_dx);
        }
      }

      if (dy != 0 && b.type == "horizontal") {
        if (barrier_y != null) {
          delete this._dragState.barrierState[i];
          continue;
        }

        let alreadyKnownDistance = this._dragState.barrierState[i] || 0;

        //log2("alreadyKnownDistance", alreadyKnownDistance);

        let dby = 0;

        //100 <= 100 && 100-(-5) > 100

        if ((vr.top <= b.y && vr.top+dy > b.y) ||
            (vr.top >= b.y && vr.top+dy < b.y))
        {
          dby = b.y - vr.top;
        } else if ((vr.bottom <= b.y && vr.bottom+dy > b.y) ||
                   (vr.bottom >= b.y && vr.bottom+dy < b.y))
        {
          dby = b.y - vr.bottom;
        } else {
          delete this._dragState.barrierState[i];
          continue;
        }

        let leftoverDistance = dby - dy;

        //log2("initial dby", dby, leftoverDistance);

        let dist = Math.abs(leftoverDistance + alreadyKnownDistance) - b.size;

        if (dist >= 0) {
          if (dy < 0)
            dby -= dist;
          else
            dby += dist;
          delete this._dragState.barrierState[i];
        } else {
          dby = 0;
          this._dragState.barrierState[i] = leftoverDistance + alreadyKnownDistance;
        }

        //log2("final dby", dby, "state", this._dragState.barrierState[i]);

        if (Math.abs(barrier_dy) <= Math.abs(dby)) {
          barrier_y = b;
          barrier_dy = dby;

          //log2("new barrier_dy", barrier_dy);
        }
      }
    }

    if (barrier_x) {
      //log2("did barrier_x", barrier_x, "barrier_dx", barrier_dx);
      dx = barrier_dx;
    }

    if (barrier_y) {
      dy = barrier_dy;
    }

    return [dx, dy];
  },

  _panBy: function _panBy(dx, dy, ignoreBarriers) {
    let vr = this._viewingRect;

    // check if any barriers would be crossed by this pan, and take them
    // into account.  do this first.
    if (!ignoreBarriers)
      [dx, dy] = this._panHandleBarriers(dx, dy);

    // constrain the full drag of the viewingRect to the pannableBounds.
    // note that the viewingRect needs to move in the opposite
    // direction of the pan, so we fiddle with the signs here (as you
    // pan to the upper left, more of the bottom right becomes visible,
    // so the viewing rect moves to the bottom right of the virtual surface).
    [dx, dy] = this._rectTranslateConstrain(dx, dy, vr, this.pannableBounds);

    // If the net result is that we don't have any room to move, then
    // just return.
    if (dx == 0 && dy == 0)
      return;

    // the viewingRect moves opposite of the actual pan direction, see above
    vr.x += dx;
    vr.y += dy;

    // Go through each widget and move it by dx,dy.  Frozen widgets
    // will be ignored in commitState.
    // The widget rects are in real stack space though, so we need to subtract
    // our (now negated) dx, dy from their coordinates.
    for each (let state in this._widgetState) {
      if (!state.ignoreX)
        state.rect.x -= dx;
      if (!state.ignoreY)
        state.rect.y -= dy;

      this._commitState(state);
    }

    if (this._panHandler)
      this._panHandler.apply(window, [vr.clone(), this._skipViewportUpdates]);
  },

  _dragUpdate: function () {
    let dx = this._dragState.outerLastUpdateDX - this._dragState.outerDX;
    let dy = this._dragState.outerLastUpdateDY - this._dragState.outerDY;

    this._dragState.outerLastUpdateDX = this._dragState.outerDX;
    this._dragState.outerLastUpdateDY = this._dragState.outerDY;

    this.panBy(dx, dy);
  },

  //
  // widget addition/removal
  //
  _addNewWidget: function (w) {
    let wid = w.getAttribute("id");
    if (!wid) {
      reportError("WidgetStack: child widget without id!");
      return;
    }

    if (w.getAttribute("hidden") == "true")
      return;

    let state = {
      widget: w,
      id: wid,

      viewport: false,
      ignoreX: false,
      ignoreY: false,
      sticky: false,
      frozen: false,
      vpRelative: false,

      offsetLeft: 0,
      offsetTop: 0,
      offsetRight: 0,
      offsetBottom: 0
    };

    this._updateWidgetRect(state);

    if (w.hasAttribute("constraint")) {
      let cs = w.getAttribute("constraint").split(",");
      for each (let s in cs) {
        if (s == "ignore-x")
          state.ignoreX = true;
        else if (s == "ignore-y")
          state.ignoreY = true;
        else if (s == "sticky")
          state.sticky = true;
        else if (s == "frozen") {
          state.frozen = true;
        } else if (s == "vp-relative")
          state.vpRelative = true;
      }
    }

    if (w.hasAttribute("viewport")) {
      if (this._viewport)
        reportError("WidgetStack: more than one viewport canvas in stack!");

      this._viewport = state;
      state.viewport = true;

      if (w.hasAttribute("vptargetx") && w.hasAttribute("vptargety") &&
          w.hasAttribute("vptargetw") && w.hasAttribute("vptargeth"))
      {
        let wx = parseInt(w.getAttribute("vptargetx"));
        let wy = parseInt(w.getAttribute("vptargety"));
        let ww = parseInt(w.getAttribute("vptargetw"));
        let wh = parseInt(w.getAttribute("vptargeth"));

        state.offsetLeft = state.rect.left - wx;
        state.offsetTop = state.rect.top - wy;
        state.offsetRight = state.rect.right - (wx + ww);
        state.offsetBottom = state.rect.bottom - (wy + wh);

        state.rect = new wsRect(wx, wy, ww, wh);
      }

      // initialize inner bounds to top-left
      state.viewportInnerBounds = new wsRect(0, 0, state.rect.width, state.rect.height);
    }

    this._widgetState[wid] = state;

    log ("(New widget: " + wid + (state.viewport ? " [viewport]" : "") + " at: " + state.rect + ")");
  },

  _removeWidget: function (w) {
    let wid = w.getAttribute("id");
    delete this._widgetState[wid];
    this._updateWidgets();
  },

  // updateWidgets:
  //   Go through all the widgets and figure out their viewport-relative offsets.
  // If the widget goes to the left or above the viewport widget, then
  // vpOffsetXBefore or vpOffsetYBefore is set.
  // See setViewportBounds for use of vpOffset* state variables, and for how
  // the actual x and y coords of each widget are calculated based on their offsets
  // and the viewport bounds.
  _updateWidgets: function () {
    let vp = this._viewport;

    let ofRect = this._viewingRect.clone();

    for each (let state in this._widgetState) {
      if (vp && state.vpRelative) {
        // compute the vpOffset from 0,0 assuming that the viewport rect is 0,0
        if (state.rect.left >= vp.rect.right) {
          state.vpOffsetXBefore = false;
          state.vpOffsetX = state.rect.left - vp.rect.width;
        } else {
          state.vpOffsetXBefore = true;
          state.vpOffsetX = state.rect.left - vp.rect.left;
        }

        if (state.rect.top >= vp.rect.bottom) {
          state.vpOffsetYBefore = false;
          state.vpOffsetY = state.rect.top - vp.rect.height;
        } else {
          state.vpOffsetYBefore = true;
          state.vpOffsetY = state.rect.top - vp.rect.top;
        }

        log("widget", state.id, "offset", state.vpOffsetX, state.vpOffsetXBefore ? "b" : "a", state.vpOffsetY, state.vpOffsetYBefore ? "b" : "a", "rect", state.rect);
      }
    }

    this._updateViewportOverflow();
  },

  // updates the viewportOverflow/pannableBounds
  _updateViewportOverflow: function() {
    let vp = this._viewport;
    if (!vp)
      return;

    let ofRect = new wsRect(0, 0, this._viewingRect.width, this._viewingRect.height);

    for each (let state in this._widgetState) {
      if (vp && state.vpRelative) {
        ofRect.left = Math.min(ofRect.left, state.rect.left);
        ofRect.top = Math.min(ofRect.top, state.rect.top);
        ofRect.right = Math.max(ofRect.right, state.rect.right);
        ofRect.bottom = Math.max(ofRect.bottom, state.rect.bottom);
      }
    }

    // prevent the viewportOverflow from having positive top/left or negative
    // bottom/right values, which would otherwise happen if there aren't widgets
    // beyond each of those edges
    this._viewportOverflow = new wsBorder(
      /*top*/ Math.min(ofRect.top, 0),
      /*left*/ Math.min(ofRect.left, 0),
      /*bottom*/ Math.max(ofRect.bottom - vp.rect.height, 0),
      /*right*/ Math.max(ofRect.right - vp.rect.width, 0)
    );

    // clear the _pannableBounds cache, since it depends on the
    // viewportOverflow
    this._pannableBounds = null;
  },

  _widgetBounds: function () {
    let r = new wsRect(0,0,0,0);

    for each (let state in this._widgetState)
      r = r.union(state.rect);

    return r;
  },

  _commitState: function (state) {
    // if the widget is frozen, don't actually update its left/top;
    // presumably the caller is managing those directly for now.
    if (state.frozen)
      return;

    state.widget.setAttribute("left", state.rect.x + state.offsetLeft);
    state.widget.setAttribute("top", state.rect.y + state.offsetTop);
  },

  // constrain translate of rect by dx dy to bounds; return dx dy that can
  // be used to bring rect up to the edge of bounds if we'd go over.
  _rectTranslateConstrain: function (dx, dy, rect, bounds) {
    let newX, newY;

    // If the rect is larger than the bounds, allow it to increase its overlap
    let woverflow = rect.width > bounds.width;
    let hoverflow = rect.height > bounds.height;
    if (woverflow || hoverflow) {
      intersection = rect.intersect(bounds);
      newIntersection = rect.clone().translate(dx, dy).intersect(bounds);
      if (woverflow)
        newX = (newIntersection.width > intersection.width) ? rect.x + dx : rect.x;
      if (hoverflow)
        newY = (newIntersection.height > intersection.height) ? rect.y + dy : rect.y;
    }

    // Common case, rect fits within the bounds
    // clamp new X to within [bounds.left, bounds.right - rect.width],
    //       new Y to within [bounds.top, bounds.bottom - rect.height]
    if (isNaN(newX))
      newX = Math.min(Math.max(bounds.left, rect.x + dx), bounds.right - rect.width);
    if (isNaN(newY))
      newY = Math.min(Math.max(bounds.top, rect.y + dy), bounds.bottom - rect.height);

    return [newX - rect.x, newY - rect.y];
  },

  // add a new barrier from a <spacer>
  _addNewBarrierFromSpacer: function (el) {
    let t = el.getAttribute("barriertype");

    // XXX implement these at some point
    // t != "lr" && t != "rl" &&
    // t != "tb" && t != "bt" &&

    if (t != "horizontal" &&
        t != "vertical")
    {
      throw "Invalid barrier type: " + t;
    }

    let x, y;

    let barrier = {};
    let vp = this._viewport;

    barrier.type = t;

    if (el.getAttribute("left"))
      barrier.x = parseInt(el.getAttribute("left"));
    else if (el.getAttribute("top"))
      barrier.y = parseInt(el.getAttribute("top"));
    else
      throw "Barrier without top or left attribute";

    if (el.getAttribute("size"))
      barrier.size = parseInt(el.getAttribute("size"));
    else
      barrier.size = 10;

    if (el.hasAttribute("constraint")) {
      let cs = el.getAttribute("constraint").split(",");
      for each (let s in cs) {
        if (s == "ignore-x")
          barrier.ignoreX = true;
        else if (s == "ignore-y")
          barrier.ignoreY = true;
        else if (s == "sticky")
          barrier.sticky = true;
        else if (s == "frozen") {
          barrier.frozen = true;
        } else if (s == "vp-relative")
          barrier.vpRelative = true;
      }
    }

    if (barrier.vpRelative) {
      if (barrier.type == "vertical") {
        if (barrier.x >= vp.rect.right) {
          barrier.vpOffsetXBefore = false;
          barrier.vpOffsetX = barrier.x - vp.rect.right;
        } else {
          barrier.vpOffsetXBefore = true;
          barrier.vpOffsetX = barrier.x - vp.rect.left;
        }
      } else if (barrier.type == "horizontal") {
        if (barrier.y >= vp.rect.bottom) {
          barrier.vpOffsetYBefore = false;
          barrier.vpOffsetY = barrier.y - vp.rect.bottom;
        } else {
          barrier.vpOffsetYBefore = true;
          barrier.vpOffsetY = barrier.y - vp.rect.top;
        }

        //log2("h barrier relative", barrier.vpOffsetYBefore, barrier.vpOffsetY);
      }
    }

    this._barriers.push(barrier);
  }
};
