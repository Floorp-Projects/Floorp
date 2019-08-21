/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { InvalidArgumentError } = ChromeUtils.import(
  "chrome://marionette/content/error.js"
);
const { Log } = ChromeUtils.import("chrome://marionette/content/log.js");

XPCOMUtils.defineLazyGetter(this, "logger", Log.get);
XPCOMUtils.defineLazyGlobalGetters(this, ["crypto"]);

this.EXPORTED_SYMBOLS = ["capture"];

const CONTEXT_2D = "2d";
const BG_COLOUR = "rgb(255,255,255)";
const MAX_SKIA_DIMENSIONS = 32767;
const PNG_MIME = "image/png";
const XHTML_NS = "http://www.w3.org/1999/xhtml";

/**
 * Provides primitives to capture screenshots.
 *
 * @namespace
 */
this.capture = {};

capture.Format = {
  Base64: 0,
  Hash: 1,
};

/**
 * Take a screenshot of a single element.
 *
 * @param {Node} node
 *     The node to take a screenshot of.
 *
 * @return {HTMLCanvasElement}
 *     The canvas element where the element has been painted on.
 */
capture.element = function(node) {
  let win = node.ownerGlobal;
  let rect = node.getBoundingClientRect();

  return capture.canvas(win, rect.left, rect.top, rect.width, rect.height);
};

/**
 * Take a screenshot of the window's viewport by taking into account
 * the current offsets.
 *
 * @param {DOMWindow} win
 *     The DOM window providing the document element to capture,
 *     and the offsets for the viewport.
 *
 * @return {HTMLCanvasElement}
 *     The canvas element where the viewport has been painted on.
 */
capture.viewport = function(win) {
  return capture.canvas(
    win,
    win.pageXOffset,
    win.pageYOffset,
    win.innerWidth,
    win.innerHeight
  );
};

/**
 * Low-level interface to draw a rectangle off the framebuffer.
 *
 * @param {DOMWindow} win
 *     The DOM window used for the framebuffer, and providing the interfaces
 *     for creating an HTMLCanvasElement.
 * @param {number} left
 *     The left, X axis offset of the rectangle.
 * @param {number} top
 *     The top, Y axis offset of the rectangle.
 * @param {number} width
 *     The width dimension of the rectangle to paint.
 * @param {number} height
 *     The height dimension of the rectangle to paint.
 * @param {HTMLCanvasElement=} canvas
 *     Optional canvas to reuse for the screenshot.
 * @param {number=} flags
 *     Optional integer representing flags to pass to drawWindow; these
 *     are defined on CanvasRenderingContext2D.
 *
 * @return {HTMLCanvasElement}
 *     The canvas on which the selection from the window's framebuffer
 *     has been painted on.
 */
capture.canvas = function(
  win,
  left,
  top,
  width,
  height,
  { canvas = null, flags = null } = {}
) {
  const scale = win.devicePixelRatio;

  if (canvas === null) {
    let canvasWidth = width * scale;
    let canvasHeight = height * scale;

    if (canvasWidth > MAX_SKIA_DIMENSIONS) {
      logger.warn(
        "Reducing screenshot width because it exceeds " +
          MAX_SKIA_DIMENSIONS +
          " pixels"
      );
      canvasWidth = MAX_SKIA_DIMENSIONS;
    }

    if (canvasHeight > MAX_SKIA_DIMENSIONS) {
      logger.warn(
        "Reducing screenshot height because it exceeds " +
          MAX_SKIA_DIMENSIONS +
          " pixels"
      );
      canvasHeight = MAX_SKIA_DIMENSIONS;
    }

    canvas = win.document.createElementNS(XHTML_NS, "canvas");
    canvas.width = canvasWidth;
    canvas.height = canvasHeight;
  }

  let ctx = canvas.getContext(CONTEXT_2D);
  if (flags === null) {
    flags = ctx.DRAWWINDOW_DRAW_CARET;

    // Enabling those flags for drawWindow by default causes
    // drawing failures. Wait until drawSnapshot is used and supports
    // these flags (bug 1571341)
    // ctx.DRAWWINDOW_DRAW_VIEW;
    // ctx.DRAWWINDOW_USE_WIDGET_LAYERS;
  }

  ctx.scale(scale, scale);
  ctx.drawWindow(win, left, top, width, height, BG_COLOUR, flags);

  return canvas;
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

/**
 * Hash the contents of an HTMLCanvasElement to a SHA-256 hex digest.
 *
 * @param {HTMLCanvasElement} canvas
 *     The canvas to encode.
 *
 * @return {string}
 *     A hex digest of the SHA-256 hash of the base64 encoded string.
 */
capture.toHash = function(canvas) {
  let u = capture.toBase64(canvas);
  let buffer = new TextEncoder("utf-8").encode(u);
  return crypto.subtle.digest("SHA-256", buffer).then(hash => hex(hash));
};

/**
 * Convert buffer into to hex.
 *
 * @param {ArrayBuffer} buffer
 *     The buffer containing the data to convert to hex.
 *
 * @return {string}
 *     A hex digest of the input buffer.
 */
function hex(buffer) {
  let hexCodes = [];
  let view = new DataView(buffer);
  for (let i = 0; i < view.byteLength; i += 4) {
    let value = view.getUint32(i);
    let stringValue = value.toString(16);
    let padding = "00000000";
    let paddedValue = (padding + stringValue).slice(-padding.length);
    hexCodes.push(paddedValue);
  }
  return hexCodes.join("");
}
