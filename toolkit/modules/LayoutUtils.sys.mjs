/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export var LayoutUtils = {
  /**
   * For a given DOM element, returns its position in screen coordinates of CSS units
   * (<https://developer.mozilla.org/en-US/docs/Web/CSS/CSSOM_View/Coordinate_systems#screen>).
   */
  getElementBoundingScreenRect(aElement) {
    let rect = aElement.getBoundingClientRect();
    let win = aElement.ownerGlobal;

    const { x, y, width, height } = this._rectToClientRect(win, rect);
    return win.windowUtils.toScreenRectInCSSUnits(x, y, width, height);
  },

  /**
   * Similar to getElementBoundingScreenRect using window and rect,
   * returns screen coordinates in screen units.
   */
  rectToScreenRect(win, rect) {
    const { x, y, width, height } = this._rectToClientRect(win, rect);
    return win.ownerGlobal.windowUtils.toScreenRect(x, y, width, height);
  },

  _rectToClientRect(win, rect) {
    // We need to compensate the position for ancestor iframes in the same
    // process that might shift things over. Those might have different CSS
    // pixel scales, so we compute the position in device pixels and then go
    // back to css pixels at the end.
    let winDpr = win.devicePixelRatio;
    let x = rect.left * winDpr;
    let y = rect.top * winDpr;

    let parentFrame = win.browsingContext?.embedderElement;
    while (parentFrame) {
      win = parentFrame.ownerGlobal;
      let cstyle = win.getComputedStyle(parentFrame);

      let framerect = parentFrame.getBoundingClientRect();
      let xDelta =
        framerect.left +
        parseFloat(cstyle.borderLeftWidth) +
        parseFloat(cstyle.paddingLeft);
      let yDelta =
        framerect.top +
        parseFloat(cstyle.borderTopWidth) +
        parseFloat(cstyle.paddingTop);

      x += xDelta * win.devicePixelRatio;
      y += yDelta * win.devicePixelRatio;

      parentFrame = win.browsingContext?.embedderElement;
    }

    return {
      x: x / winDpr,
      y: y / winDpr,
      width: rect.width,
      height: rect.height,
    };
  },
};
