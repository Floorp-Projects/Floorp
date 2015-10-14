/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["capture"];

const CONTEXT_2D = "2d";
const BG_COLOUR = "rgb(255,255,255)";
const PNG_MIME = "image/png";
const XHTML_NS = "http://www.w3.org/1999/xhtml";

/** Provides primitives to capture screenshots. */
this.capture = {};

/**
 * Take a screenshot of a single element.
 *
 * @param {Node} node
 *     The node to take a screenshot of.
 * @param {Array.<Node>=} highlights
 *     Optional array of nodes, around which a border will be marked to
 *     highlight them in the screenshot.
 *
 * @return {HTMLCanvasElement}
 *     The canvas element where the element has been painted on.
 */
capture.element = function(node, highlights=[]) {
  let doc = node.ownerDocument;
  let win = doc.defaultView;
  let rect = node.getBoundingClientRect();

  return capture.canvas(
      doc,
      rect.left,
      rect.top,
      rect.width,
      rect.height,
      highlights);
};

/**
 * Take a screenshot of the document's viewport, taking into account
 * the current window's offset.
 *
 * @param {Document} document
 *     The DOM document providing the document element to capture,
 *     and a window for determining the offset of the viewport.
 * @param {Array.<Node>=} highlights
 *     Optional array of nodes, around which a border will be marked to
 *     highlight them in the screenshot.
 *
 * @return {HTMLCanvasElement}
 *     The canvas element where the viewport has been painted on.
 */
capture.viewport = function(document, highlights=[]) {
  let win = document.defaultView;
  let docEl = document.documentElement;

  return capture.canvas(
      document,
      win.pageXOffset,
      win.pageYOffset,
      docEl.clientWidth,
      docEl.clientHeight,
      highlights);
};

/**
 * Low-level interface to draw a rectangle off the framebuffer.
 *
 * @param {Document} document
 *     A DOM document providing the window used to the framebuffer,
 *     and interfaces for creating an HTMLCanvasElement.
 * @param {number} left
 *     The left, X axis offset of the rectangle.
 * @param {number} top
 *     The top, Y axis offset of the rectangle.
 * @param {number} width
 *     The width dimension of the rectangle to paint.
 * @param {number} height
 *     The height dimension of the rectangle to paint.
 * @param {Array.<Node>=} highlights
 *     Optional array of nodes, around which a border will be marked to
 *     highlight them in the screenshot.
 *
 * @return {HTMLCanvasElement}
 *     The canvas on which the selection from the window's framebuffer
 *     has been painted on.
 */
capture.canvas = function(document, left, top, width, height, highlights=[]) {
  let win = document.defaultView;

  let canvas = document.createElementNS(XHTML_NS, "canvas");
  canvas.width = width;
  canvas.height = height;

  let ctx = canvas.getContext(CONTEXT_2D);
  ctx.drawWindow(win, left, top, width, height, BG_COLOUR);
  ctx = capture.highlight_(ctx, highlights, top, left);

  return canvas;
};

capture.highlight_ = function(context, highlights, top=0, left=0) {
  if (!highlights) {
    return;
  }

  context.lineWidth = "2";
  context.strokeStyle = "red";
  context.save();

  for (let el of highlights) {
    let rect = el.getBoundingClientRect();
    let oy = -top;
    let ox = -left;

    context.strokeRect(
        rect.left + ox,
        rect.top + oy,
        rect.width,
        rect.height);
  }

  return context;
};

/**
 * Encode the contents of an HTMLCanvasElement to a Base64 encoded string.
 *
 * @param {HTMLCanvasElement} canvas
 *     The canvas to encode.
 *
 * @return {string}
 *     A Base64 encoded string.
 */
capture.toBase64 = function(canvas) {
  let u = canvas.toDataURL(PNG_MIME);
  return u.substring(u.indexOf(",") + 1);
};
