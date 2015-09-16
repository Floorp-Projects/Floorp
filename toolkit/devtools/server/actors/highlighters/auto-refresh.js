/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const EventEmitter = require("devtools/toolkit/event-emitter");
const { isNodeValid } = require("./utils/markup");
const { getAdjustedQuads } = require("devtools/toolkit/layout/utils");

// Note that the order of items in this array is important because it is used
// for drawing the BoxModelHighlighter's path elements correctly.
const BOX_MODEL_REGIONS = ["margin", "border", "padding", "content"];

/**
 * Base class for auto-refresh-on-change highlighters. Sub classes will have a
 * chance to update whenever the current node's geometry changes.
 *
 * Sub classes must implement the following methods:
 * _show: called when the highlighter should be shown,
 * _hide: called when the highlighter should be hidden,
 * _update: called while the highlighter is shown and the geometry of the
 *          current node changes.
 *
 * Sub classes will have access to the following properties:
 * - this.currentNode: the node to be shown
 * - this.currentQuads: all of the node's box model region quads
 * - this.win: the current window
 *
 * Emits the following events:
 * - shown
 * - hidden
 * - updated
 */
function AutoRefreshHighlighter(highlighterEnv) {
  EventEmitter.decorate(this);

  this.highlighterEnv = highlighterEnv;
  this.win = highlighterEnv.window;

  this.currentNode = null;
  this.currentQuads = {};

  this.update = this.update.bind(this);
}

AutoRefreshHighlighter.prototype = {
  /**
   * Show the highlighter on a given node
   * @param {DOMNode} node
   * @param {Object} options
   *        Object used for passing options
   */
  show: function(node, options = {}) {
    let isSameNode = node === this.currentNode;
    let isSameOptions = this._isSameOptions(options);

    if (!isNodeValid(node) || (isSameNode && isSameOptions)) {
      return false;
    }

    this.options = options;

    this._stopRefreshLoop();
    this.currentNode = node;
    this._updateAdjustedQuads();
    this._startRefreshLoop();

    let shown = this._show();
    if (shown) {
      this.emit("shown");
    }
    return shown;
  },

  /**
   * Hide the highlighter
   */
  hide: function() {
    if (!isNodeValid(this.currentNode)) {
      return;
    }

    this._hide();
    this._stopRefreshLoop();
    this.currentNode = null;
    this.currentQuads = {};
    this.options = null;

    this.emit("hidden");
  },

  /**
   * Are the provided options the same as the currently stored options?
   * Returns false if there are no options stored currently.
   */
  _isSameOptions: function(options) {
    if (!this.options) {
      return false;
    }

    let keys = Object.keys(options);

    if (keys.length !== Object.keys(this.options).length) {
      return false;
    }

    for (let key of keys) {
      if (this.options[key] !== options[key]) {
        return false;
      }
    }

    return true;
  },

  /**
   * Update the stored box quads by reading the current node's box quads.
   */
  _updateAdjustedQuads: function() {
    for (let region of BOX_MODEL_REGIONS) {
      this.currentQuads[region] = getAdjustedQuads(
        this.win,
        this.currentNode, region);
    }
  },

  /**
   * Update the knowledge we have of the current node's boxquads and return true
   * if any of the points x/y or bounds have change since.
   * @return {Boolean}
   */
  _hasMoved: function() {
    let oldQuads = JSON.stringify(this.currentQuads);
    this._updateAdjustedQuads();
    let newQuads = JSON.stringify(this.currentQuads);
    return oldQuads !== newQuads;
  },

  /**
   * Update the highlighter if the node has moved since the last update.
   */
  update: function() {
    if (!isNodeValid(this.currentNode) || !this._hasMoved()) {
      return;
    }

    this._update();
    this.emit("updated");
  },

  _show: function() {
    // To be implemented by sub classes
    // When called, sub classes should actually show the highlighter for
    // this.currentNode, potentially using options in this.options
    throw new Error("Custom highlighter class had to implement _show method");
  },

  _update: function() {
    // To be implemented by sub classes
    // When called, sub classes should update the highlighter shown for
    // this.currentNode
    // This is called as a result of a page scroll, zoom or repaint
    throw new Error("Custom highlighter class had to implement _update method");
  },

  _hide: function() {
    // To be implemented by sub classes
    // When called, sub classes should actually hide the highlighter
    throw new Error("Custom highlighter class had to implement _hide method");
  },

  _startRefreshLoop: function() {
    let win = this.currentNode.ownerDocument.defaultView;
    this.rafID = win.requestAnimationFrame(this._startRefreshLoop.bind(this));
    this.rafWin = win;
    this.update();
  },

  _stopRefreshLoop: function() {
    if (this.rafID && !Cu.isDeadWrapper(this.rafWin)) {
      this.rafWin.cancelAnimationFrame(this.rafID);
    }
    this.rafID = this.rafWin = null;
  },

  destroy: function() {
    this.hide();

    this.highlighterEnv = null;
    this.win = null;
    this.currentNode = null;
  }
};
exports.AutoRefreshHighlighter = AutoRefreshHighlighter;
