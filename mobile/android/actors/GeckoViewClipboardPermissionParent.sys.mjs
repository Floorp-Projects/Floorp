/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewActorParent } from "resource://gre/modules/GeckoViewActorParent.sys.mjs";

export class GeckoViewClipboardPermissionParent extends GeckoViewActorParent {
  getLastOverWindowPointerLocation() {
    const mouseXInCSSPixels = {};
    const mouseYInCSSPixels = {};
    const windowUtils = this.window.windowUtils;
    windowUtils.getLastOverWindowPointerLocationInCSSPixels(
      mouseXInCSSPixels,
      mouseYInCSSPixels
    );
    const screenRect = windowUtils.toScreenRect(
      mouseXInCSSPixels.value,
      mouseYInCSSPixels.value,
      0,
      0
    );

    return {
      x: screenRect.x,
      y: screenRect.y,
    };
  }

  receiveMessage(aMessage) {
    debug`receiveMessage: ${aMessage.name}`;

    switch (aMessage.name) {
      case "ClipboardReadTextPaste:GetLastPointerLocation":
        return this.getLastOverWindowPointerLocation();

      default:
        return super.receiveMessage(aMessage);
    }
  }
}

const { debug, warn } = GeckoViewClipboardPermissionParent.initLogging(
  "GeckoViewClipboardPermissionParent"
);
