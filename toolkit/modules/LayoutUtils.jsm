/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["LayoutUtils"];

var LayoutUtils = {
  /**
   * For a given DOM element, returns its position in "screen"
   * coordinates. In a content process, the coordinates returned will
   * be relative to the left/top of the tab. In the chrome process,
   * the coordinates are relative to the user's screen.
   */
  getElementBoundingScreenRect(aElement) {
    return this.getElementBoundingRect(aElement, true);
  },

  /**
   * For a given DOM element, returns its position as an offset from the topmost
   * window. In a content process, the coordinates returned will be relative to
   * the left/top of the topmost content area. If aInScreenCoords is true,
   * screen coordinates will be returned instead.
   */
  getElementBoundingRect(aElement, aInScreenCoords) {
    let rect = aElement.getBoundingClientRect();
    let win = aElement.ownerGlobal;

    let x = rect.left;
    let y = rect.top;

    // We need to compensate for any iframes that might shift things
    // over. We also need to compensate for zooming.
    let parentFrame = win.frameElement;
    while (parentFrame) {
      win = parentFrame.ownerGlobal;
      let cstyle = win.getComputedStyle(parentFrame);

      let framerect = parentFrame.getBoundingClientRect();
      x +=
        framerect.left +
        parseFloat(cstyle.borderLeftWidth) +
        parseFloat(cstyle.paddingLeft);
      y +=
        framerect.top +
        parseFloat(cstyle.borderTopWidth) +
        parseFloat(cstyle.paddingTop);

      parentFrame = win.frameElement;
    }

    rect = {
      left: x,
      top: y,
      width: rect.width,
      height: rect.height,
    };
    rect = win.windowUtils.transformRectLayoutToVisual(
      rect.left,
      rect.top,
      rect.width,
      rect.height
    );

    if (aInScreenCoords) {
      rect = {
        left: rect.left + win.mozInnerScreenX,
        top: rect.top + win.mozInnerScreenY,
        width: rect.width,
        height: rect.height,
      };
    }

    let fullZoom = win.windowUtils.fullZoom;
    rect = {
      left: rect.left * fullZoom,
      top: rect.top * fullZoom,
      width: rect.width * fullZoom,
      height: rect.height * fullZoom,
    };

    return rect;
  },
};
