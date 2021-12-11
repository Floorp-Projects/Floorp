/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["LayoutUtils"];

var LayoutUtils = {
  /**
   * For a given DOM element, returns its position in "screen"
   * coordinates.
   */
  getElementBoundingScreenRect(aElement) {
    let rect = aElement.getBoundingClientRect();
    let win = aElement.ownerGlobal;

    let x = rect.left;
    let y = rect.top;

    // We need to compensate for ancestor iframes in the same process
    // that might shift things over.
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

    return aElement.ownerGlobal.windowUtils.toScreenRectInCSSUnits(
      x,
      y,
      rect.width,
      rect.height
    );
  },
};
