/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["capture"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  Log: "chrome://remote/content/shared/Log.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.MARIONETTE)
);

const CONTEXT_2D = "2d";
const BG_COLOUR = "rgb(255,255,255)";
const MAX_CANVAS_DIMENSION = 32767;
const MAX_CANVAS_AREA = 472907776;
const PNG_MIME = "image/png";
const XHTML_NS = "http://www.w3.org/1999/xhtml";

/**
 * Provides primitives to capture screenshots.
 *
 * @namespace
 */
const capture = {};

capture.Format = {
  Base64: 0,
  Hash: 1,
};

/**
 * Draw a rectangle off the framebuffer.
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
 * @param {number=} dX
 *     Horizontal offset between the browser window and content area. Defaults to 0.
 * @param {number=} dY
 *     Vertical offset between the browser window and content area. Defaults to 0.
 * @param {boolean=} readback
 *     If true, read back a snapshot of the pixel data currently in the
 *     compositor/window. Defaults to false.
 *
 * @return {HTMLCanvasElement}
 *     The canvas on which the selection from the window's framebuffer
 *     has been painted on.
 */
capture.canvas = async function(
  win,
  browsingContext,
  left,
  top,
  width,
  height,
  { canvas = null, flags = null, dX = 0, dY = 0, readback = false } = {}
) {
  // FIXME(bug 1761032): This looks a bit sketchy, overrideDPPX doesn't
  // influence rendering...
  const scale = win.browsingContext.overrideDPPX || win.devicePixelRatio;

  let canvasHeight = height * scale;
  let canvasWidth = width * scale;

  // Cap the screenshot size for width and height at 2^16 pixels,
  // which is the maximum allowed canvas size. Higher dimensions will
  // trigger exceptions in Gecko.
  if (canvasWidth > MAX_CANVAS_DIMENSION) {
    lazy.logger.warn(
      "Limiting screen capture width to maximum allowed " +
        MAX_CANVAS_DIMENSION +
        " pixels"
    );
    width = Math.floor(MAX_CANVAS_DIMENSION / scale);
    canvasWidth = width * scale;
  }

  if (canvasHeight > MAX_CANVAS_DIMENSION) {
    lazy.logger.warn(
      "Limiting screen capture height to maximum allowed " +
        MAX_CANVAS_DIMENSION +
        " pixels"
    );
    height = Math.floor(MAX_CANVAS_DIMENSION / scale);
    canvasHeight = height * scale;
  }

  // If the area is larger, reduce the height to keep the full width.
  if (canvasWidth * canvasHeight > MAX_CANVAS_AREA) {
    lazy.logger.warn(
      "Limiting screen capture area to maximum allowed " +
        MAX_CANVAS_AREA +
        " pixels"
    );
    height = Math.floor(MAX_CANVAS_AREA / (canvasWidth * scale));
    canvasHeight = height * scale;
  }

  if (canvas === null) {
    canvas = win.document.createElementNS(XHTML_NS, "canvas");
    canvas.width = canvasWidth;
    canvas.height = canvasHeight;
  }

  const ctx = canvas.getContext(CONTEXT_2D);

  if (readback) {
    if (flags === null) {
      flags =
        ctx.DRAWWINDOW_DRAW_CARET |
        ctx.DRAWWINDOW_DRAW_VIEW |
        ctx.DRAWWINDOW_USE_WIDGET_LAYERS;
    }

    // drawWindow doesn't take scaling into account.
    ctx.scale(scale, scale);
    ctx.drawWindow(win, left + dX, top + dY, width, height, BG_COLOUR, flags);
  } else {
    let rect = new DOMRect(left, top, width, height);
    let snapshot = await browsingContext.currentWindowGlobal.drawSnapshot(
      rect,
      scale,
      BG_COLOUR
    );

    ctx.drawImage(snapshot, 0, 0);

    // Bug 1574935 - Huge dimensions can trigger an OOM because multiple copies
    // of the bitmap will exist in memory. Force the removal of the snapshot
    // because it is no longer needed.
    snapshot.close();
  }

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
