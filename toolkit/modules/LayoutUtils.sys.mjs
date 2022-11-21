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
    let x = rect.left;
    let y = rect.top;

    // We need to compensate for ancestor iframes in the same process
    // that might shift things over.
    let parentFrame = win.browsingContext?.embedderElement;
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

      parentFrame = win.browsingContext?.embedderElement;
    }

    return {
      x,
      y,
      width: rect.width,
      height: rect.height,
    };
  },
};
